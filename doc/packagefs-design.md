# Packagefs Design

Status: Accepted for the Poudland P0 control plane

Packagefs provides named, record-oriented, bidirectional communication between one server process and multiple client processes. Poudland is its first production consumer, but the transport must not expose compositor-specific types or kernel implementation details.

## Goals

- Let one process bind a named service and let multiple processes connect to it.
- Preserve message boundaries across `read` and `write`.
- Let a server identify a client without exposing a kernel pointer.
- Let a server send the same application record to all eligible clients.
- Support blocking I/O, nonblocking I/O, and readiness notification through the common poll core.
- Apply bounded buffering and backpressure instead of unbounded allocation.
- Release sessions, queues, and unread messages when an fd closes or a process exits.
- Notify a server when a client session's final endpoint reference closes.
- Validate all user-controlled lengths, identifiers, flags, and pointers at the syscall boundary.

## Non-goals

- Packagefs is not a regular filesystem and does not persist service nodes across reboot.
- Packagefs is not shared memory and does not replace the data mapping API.
- Packagefs does not interpret Poudland requests or other application protocols.
- Packagefs does not expose kernel addresses, intrusive-list fields, or internal object layouts.
- Packagefs is not the bulk pixel transport for a complete window surface.

## Model

The transport has four concepts:

- **Service name**: a bounded name in the packagefs namespace, such as `compositor`.
- **Server endpoint**: the unique endpoint bound to a service name.
- **Client session**: one connection created by opening an existing service.
- **Record**: one complete application message with a bounded payload.

A client write appends one record to the server's receive queue. A server read returns one record together with an opaque client ID. A server reply names that client ID and appends one record to that client's receive queue. Closing either endpoint changes session state and wakes affected waiters.

A duplicated or fork-inherited endpoint keeps the same session live. Closing one descriptor only releases one reference; disconnect occurs when the final endpoint reference closes.

## Namespace and Open Contract

The intended namespace remains `/dev/pkg/<service>` so existing Poudland paths can migrate without carrying forward the legacy implementation.

Accepted contract:

- A server binds a new service with `open("/dev/pkg/<service>", O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC)`.
- A client connects with `open("/dev/pkg/<service>", O_RDWR | O_CLOEXEC)`.
- Binding an existing live service fails instead of silently replacing its server.
- A client connects by opening an existing service with read/write access.
- Connecting to a missing or closing service fails.
- Service names are at most 31 bytes and reject empty names, separators, and traversal components.
- A packagefs endpoint cannot be used as a regular seekable file.
- `O_NONBLOCK` changes endpoint I/O behavior, not bind or connect behavior.
- Service nodes are ephemeral kernel objects and are never written to disk.

The final server endpoint reference removes the service name. A new server may then bind that name without inheriting any earlier session or client ID.

## Record Contract

Packagefs preserves records rather than exposing the underlying byte ring. A successful read consumes exactly one record. A successful write enqueues exactly one record.

The common record envelope is:

```c
enum frog_pkg_event {
        FROG_PKG_DATA = 1,
        FROG_PKG_DISCONNECT = 2,
        FROG_PKG_WRITABLE = 3,
};

struct frog_pkg_record {
        uint_32 peer_id;
        uint_32 event;
        uint_32 payload_size;
        uint_8 payload[];
};
```

On server reads and writes, `peer_id` identifies the client. On client reads and writes, it must be zero and denotes the server. Packagefs owns the envelope event values; application protocols such as Poudland put their message types inside the payload. The declared payload size and actual I/O length must match exactly.

Required semantics:

- A `FROG_PKG_DATA` payload may contain zero through 1024 bytes.
- A declared payload length inconsistent with the complete `write` length fails with `-EINVAL` and enqueues nothing.
- A payload larger than 1024 bytes fails with `-EMSGSIZE` and enqueues nothing.
- A read buffer smaller than the next complete record fails with `-EMSGSIZE` without consuming that record.
- `O_NONBLOCK` returns `-EAGAIN` when a read queue is empty or the selected peer's write queue lacks room.
- Blocking I/O sleeps on endpoint wait queues and rechecks the condition after wakeup.
- A peer closure wakes blocked readers and writers with a documented terminal result.
- Queued records survive peer closure and remain readable before end-of-file.
- Records from one session remain FIFO ordered, and its final disconnect indication follows all client-to-server data already accepted from that session.
- Interrupted operations do not partially enqueue or consume a record.

The initial limits are a 1024-byte payload, one page of receive-queue storage per endpoint, at most 16 clients per service, and at most 16 services system-wide. These are named limits and are tested at their exact boundaries. Window Surface pixels do not pass through this payload.

## Identity and Routing

The kernel assigns each client session an opaque, nonzero client ID scoped to one server endpoint. The ID is data, not a pointer. The server receives it in a fixed-width record envelope and uses it when sending a directed reply.

The kernel must reject:

- an unknown or closed client ID;
- an ID owned by another service;
- stale IDs from an earlier session generation;
- a payload length inconsistent with the supplied buffer;
- unsupported envelope versions or flags.

Broadcast is supported by the Poudland-facing transport library and is an explicit operation rather than a magic null pointer. The first implementation snapshots the clients that have completed the Poudland `HELLO` exchange and expands the broadcast into nonblocking directed packagefs sends. Clients that complete `HELLO` after the snapshot do not receive that broadcast; clients that close during delivery are reported as disconnected. A full or disconnected client does not prevent delivery to the other clients. The result distinguishes delivered, would-block, and disconnected recipients so Poudland can retry only the failed client IDs when the message type requires it. Packagefs does not provide a separate broadcast slot in the first version.

Packagefs does not generate a client-connect record. Poudland establishes application state with its existing `HELLO` message. Packagefs generates only the final `FROG_PKG_DISCONNECT` lifecycle record; an unknown client ID can therefore disconnect without ever having completed a Poudland handshake.

## Readiness and Backpressure

Packagefs participates in the same readiness mechanism as input devices.

- Readable: at least one complete record, or a terminal peer-close condition, can be observed without blocking.
- Writable: on a client endpoint, the server receive queue can accept a maximum-contract record without blocking. A server endpoint uses per-client `FROG_PKG_WRITABLE` records instead of one aggregate writable bit.
- Error/hangup: the peer or service has closed and no further normal transfer is possible.
- A peer-closed endpoint reports `POLLIN | POLLHUP` while complete records remain, then persistent `POLLHUP` after they drain.

Every readiness result is advisory: `read` and `write` must revalidate state after wakeup. The server has one receive queue for client-to-server records. Every client session has its own receive queue for server-to-client records; a server write uses `peer_id` to select that queue. Queue capacity is bounded per endpoint. Slow consumers therefore apply backpressure only to sends targeting that client instead of causing unbounded kernel memory use or blocking delivery to unrelated clients.

After a nonblocking directed server write fails with `-EAGAIN`, packagefs atomically arms a coalesced writable notification for that client ID. When that client's receive queue has enough room again, the server endpoint becomes readable and returns one `FROG_PKG_WRITABLE` record naming the client. Repeated full-to-writable transitions are coalesced until the server consumes the notification. The Poudland Server then retries only that client's pending records. A server endpoint does not use one ambiguous `POLLOUT` bit to describe multiple client queues; client endpoints retain the normal single-queue `POLLOUT` meaning.

## Lifecycle

The server owns the service name while its endpoint is live. Each client session owns its receive queue and client ID.

- Closing the final reference to a client endpoint removes it from the server's routing table, invalidates its ID, and enqueues a kernel-generated `FROG_PKG_DISCONNECT` record for the server. The notification contains the opaque client ID and no payload.
- Closing the server prevents new clients, wakes current clients, releases the service name, and frees the service after its last reference disappears.
- Process exit has the same effect as closing every packagefs fd owned by that process.
- Duplicated or inherited fds share one endpoint lifetime through reference counting; the final close performs teardown.

After peer closure, existing records are drained before `read` returns zero. A subsequent write returns `-EPIPE`; Frog does not raise `SIGPIPE` until a signal model exists. The transport never depends on a cooperative application-level goodbye message, so abrupt process termination follows the same path as an explicit close.

No packagefs object may be stored in a persistent on-disk inode or addressed through a raw kernel pointer in user memory.

## Poudland Use

For the existing `desktop.c` scenario, packagefs is the control plane for connection, window creation, window close, and replies. The Poudland Server owns the scene and physical framebuffer.

Poudland expands a broadcast into directed sends to the fixed recipient snapshot. It keeps at most one page of pending outbound records per client. Reliable pending records remain FIFO ordered, so a later reliable record cannot overtake an earlier one. Replaceable state may overwrite an older pending value of the same Poudland message type, but cannot overtake a reliable state transition. If a client's pending page cannot accept another required record, Poudland treats the client as unresponsive and disconnects it.

The initial Poudland delivery rules are selected by application message type rather than encoded as packagefs transport flags:

- `POUDLAND_V1_MSG_WELCOME`, `POUDLAND_V1_MSG_WINDOW_INIT`, keyboard events,
  and pointer click/down/raise/enter/leave transitions are retained and retried
  in FIFO order.
- Pointer move/drag and absolute `POUDLAND_V1_MSG_WINDOW_CONFIGURE` state
  retain only the newest pending value of the same message type for each
  affected window without overtaking an earlier reliable transition.
- No message in the first `desktop.c` milestone is unconditionally discarded after `-EAGAIN`.

Broadcast describes only the fixed set of recipients. Its retry or replacement behavior follows the contained Poudland message type; broadcast itself does not impose a delivery class.

The initial compatibility slice does not require client pixel buffers: the current demo requests colored windows that the server can render. A later surface API may map shared window buffers into a client and the server; packagefs would then carry small control records such as surface attach, commit, and damage. Mapping alone cannot replace service discovery, session identity, message boundaries, wakeups, or lifecycle handling.

## Migration from Legacy Packagefs

The legacy code is a behavioral reference, not a structure to re-enable wholesale. Migration must remove:

- kernel pointers used as client identities;
- user-visible structs containing kernel layout details;
- fixed-size writes that ignore the caller's actual message length;
- queue-size ioctls used instead of readiness notification;
- missing server/client close paths;
- service objects stored through legacy inode private fields;
- packagefs-specific branches in obsolete filesystem code.

Compatibility should be provided in the user library where inexpensive. The kernel contract should expose only the modern record and session model.

## Validation

At minimum, automated tests must cover:

- bind, connect, client-to-server record, directed reply, and close;
- two clients with correct reply routing and no cross-session delivery;
- message sizes zero, one, maximum, and maximum plus one;
- undersized read buffers without record loss;
- blocking wakeup and `O_NONBLOCK` `-EAGAIN` behavior;
- queue-full backpressure and recovery after the peer reads;
- server close, client close, process exit, and stale client IDs;
- queued-data plus hangup, drain-before-EOF, and write-after-close `-EPIPE`;
- duplicated and fork-inherited endpoint references, with exactly one final disconnect notification;
- readiness transitions for readable, writable, error, and hangup states;
- invalid user pointers and concurrent close/read/write races;
- repeated bind/close cycles without leaked pages, fds, sessions, or service names.

The Poudland end-to-end test additionally binds `compositor`, launches `desktop.c`, observes the expected window messages, and verifies the rendered result through guest state and a QEMU screenshot.

## Open Decisions

- The future shared-surface handle, mapping, commit, and damage protocol, which is outside the accepted P0 control-plane contract.
