# `wait2` Design

Status: Accepted

`wait2` is Frog's small, level-triggered I/O readiness interface. It lets one process wait for input devices, packagefs endpoints, and future file-descriptor-backed event sources without embedding device-specific logic in the syscall.

## UAPI

```c
struct pollfd {
        int_32 fd;
        uint_16 events;
        uint_16 revents;
};

int_32 wait2(struct pollfd *fds,
             uint_32 count,
             int_32 timeout_ms);
```

The initial event set is intentionally small:

```c
#define POLLIN   0x0001
#define POLLOUT  0x0004
#define POLLERR  0x0008
#define POLLHUP  0x0010
#define POLLNVAL 0x0020
```

Callers request only `POLLIN` and `POLLOUT` in `events`. The kernel may report `POLLERR`, `POLLHUP`, and `POLLNVAL` in `revents` whether or not the caller requested them. Unsupported event bits are rejected rather than silently ignored.

## Return Contract

- A positive result is the number of entries whose `revents` is nonzero.
- Zero means the timeout expired without a reportable event.
- A negative result is a whole-call error such as `-EFAULT`, `-EINVAL`, or `-ENOMEM`.
- A negative fd is ignored and receives `revents = 0`.
- An invalid nonnegative fd receives `POLLNVAL`, which counts as a ready entry instead of failing the whole call.
- Every `revents` field is cleared by the kernel before readiness is evaluated.

`wait2` reports every ready entry in one call. It does not select one index and therefore does not impose index-order starvation on the caller.

## Timeout Contract

`timeout_ms` is a relative duration measured against Frog's monotonic timeline:

- `-1`: wait without a deadline;
- `0`: scan once and return immediately;
- greater than zero: wait for at most that many milliseconds;
- less than `-1`: return `-EINVAL`.

A positive duration is rounded up to the next hardware timer tick so the call does not expire earlier than requested. Milliseconds are a stable UAPI unit and do not change when Frog runs on a faster CPU or gains a finer clocksource.

When `count == 0`, `fds` may be null and a finite timeout acts as a monotonic sleep. `wait2(NULL, 0, -1)` returns `-EINVAL` because no event or deadline could wake it.

## Readiness Semantics

`wait2` is level-triggered:

- `POLLIN`: a read can return data, end-of-file, or an error without blocking;
- `POLLOUT`: a write of the endpoint's documented minimum unit can proceed without blocking;
- `POLLERR`: the endpoint has an error condition;
- `POLLHUP`: the peer has closed or the device has disconnected;
- `POLLNVAL`: the descriptor is invalid or cannot participate in `wait2`.

Callers must drain a readable source until it returns `-EAGAIN`. If unread data and peer closure coexist, an endpoint may report `POLLIN | POLLHUP`. Hangup persists after queued data is drained.

Edge-triggered notification is explicitly deferred. If Frog later adds it, it must be an additive interface with separate registration state; the level-triggered `wait2` contract remains unchanged.

## Descriptor and Object Lifetime

The kernel resolves every nonnegative fd to a strong file-object reference before sleeping and releases those references before returning. Closing or reusing a numeric fd after entry cannot turn a pinned object into a dangling pointer or retarget the active wait.

Frog currently has no user threads. Concurrently closing a descriptor being monitored by another thread is therefore unreachable in the first implementation and remains unspecified for a future threading API. Even when behavior is unspecified, the kernel must remain memory-safe.

Duplicated or inherited endpoint descriptors share endpoint references. Closing one descriptor does not signal a peer disconnect. The endpoint disconnects only when its final reference closes.

## Peer Closure

Stream- or session-like endpoints follow drain-before-EOF behavior:

1. mark the peer closed;
2. wake read, write, and poll wait queues;
3. report `POLLIN | POLLHUP` while complete queued records remain;
4. report `POLLHUP` after the queue drains;
5. make subsequent reads return zero;
6. make subsequent writes return `-EPIPE`.

Frog has no signals yet, so an `-EPIPE` write does not also raise `SIGPIPE`. Future signal interruption may cause `wait2` to return `-EINTR`; no partial-success result is returned in that case.

## Poudland Example

```c
struct pollfd sources[] = {
        { .fd = keyboard_fd, .events = POLLIN },
        { .fd = mouse_fd, .events = POLLIN },
        { .fd = package_server_fd, .events = POLLIN },
};

for (;;) {
        int_32 ready = wait2(sources, 3, 16);

        if (ready < 0)
                break;
        if (ready == 0) {
                present_damage();
                continue;
        }

        for (uint_32 i = 0; i < 3; i++) {
                if (sources[i].revents & POLLIN)
                        drain_source(&sources[i]);
                if (sources[i].revents & POLLHUP)
                        handle_peer_close(&sources[i]);
                if (sources[i].revents & (POLLERR | POLLNVAL))
                        disable_source(&sources[i]);
        }
}
```

The compositor recomputes its timeout from the next Frame Deadline on every loop. It does not assume that a timeout occurs exactly at the requested millisecond.

## Kernel Requirements

- Copy the complete pollfd array from user memory and reject overflow before allocation.
- Limit `count` to the process descriptor limit, initially 32.
- Hold strong file references across registration, sleep, rescan, and copy-out.
- Register wait queues through each file operation's poll callback.
- Rescan after registration and after every wakeup to prevent lost-wakeup races.
- Use a monotonic timer and round positive millisecond durations up to ticks.
- Remove all wait-queue entries and references on every success and error path.
- Copy only `revents` results back after all kernel-side validation succeeds.
- Regular files may be defined as always readable and writable; devices and transports must provide meaningful callbacks rather than inheriting a permissive fallback.
- Invalid user input returns errors or `POLLNVAL`; it must never trigger an assertion or kernel fault.

## Required Tests

- immediate, finite, and infinite waits;
- `count == 0` finite sleep and invalid infinite empty wait;
- simultaneous keyboard, mouse, and packagefs readiness;
- multiple ready entries in one return;
- invalid negative and nonnegative descriptors;
- unsupported event bits and count overflow;
- timeout rounding at 1 ms and across PIT tick boundaries;
- wakeup between initial scan and wait registration;
- spurious wakeup followed by rescan;
- peer close with queued data, drain, EOF, and `-EPIPE`;
- duplicated/fork-inherited descriptors and final-reference disconnect;
- invalid user pointers and cleanup after copy-out failure;
- repeated wait/close cycles without leaked file references or wait entries.

## Migration

The dormant index-return implementation in `core/fs/select.c` is not re-enabled. The syscall number and `wait2` name may remain, but the user declaration, wrapper, implementation, compositor caller, and device poll callbacks migrate together to this contract. There is no active production caller whose index-return behavior must be preserved.
