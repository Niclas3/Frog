# Implementation Plan: Poudland P0 and `desktop.c`

## Overview

Deliver a reproducible 16 MiB QEMU boot in which a user-mode graphical init loads `/test/compositor` and `/test/desktop` from a reusable FrogFS disk, Poudland accepts the Version 1 client protocol, `desktop.c` creates three solid-color windows and closes the third, two windows remain visible, the cursor moves, the focused window receives keyboard input, and the second window is dragged from `(220,200)` to `(260,225)`. The fast path ends only after guest state and exact framebuffer evidence pass; a separate ten-minute soak follows.

The implementation follows the accepted contracts in `doc/wait2-design.md`, `doc/timekeeping-design.md`, `doc/user-runtime-memory-design.md`, `doc/packagefs-design.md`, `doc/poudland-protocol-v1.md`, and `doc/graphical-startup-image-design.md`.

## Scope Boundaries

Included:

- PIT monotonic time, optional RTC realtime, `clock_gettime`, and `gettimeofday`;
- poll-shaped `wait2` and level-triggered readiness;
- private anonymous `mmap` plus user-space `malloc` and `free`;
- record-oriented packagefs with refcounted sessions and per-client S2C queues;
- a minimal modern Poudland Server and Version 1 client library;
- solid server-rendered windows, damage-aware backbuffer, cursor, focus, and drag;
- a host-built reusable FrogFS image and user ELF launch chain;
- automated `desktop-smoke` and a separate `desktop-soak-10m`.

Deferred:

- shared Window Surfaces, resize, clipboard, decorations, and binary compatibility with legacy Poudland messages;
- demand paging, copy-on-write, partial `munmap`, `realloc`, and multithreaded allocation;
- root mount/path migration from `/test` to production `/bin` and shared-data paths;
- compositor restart policy and RTC setting;
- migration of non-P0 features from the legacy compositor implementation.

## Architecture Decisions

- The Poudland Server is the only process that maps and writes the physical framebuffer.
- `/dev/input/event0` and `/dev/input/event1` are direct event sources; `/dev/tty0` is not Poudland input.
- `wait2` is a pollfd-shaped, level-triggered syscall with millisecond relative timeouts backed by monotonic time.
- PIT channel 0 targets a corrected 1000 Hz periodic rate; RTC is optional and never drives intervals.
- User allocation is implemented over eager zero-filled `MAP_PRIVATE | MAP_ANONYMOUS`, not kernel `malloc` syscalls.
- Packagefs is a named, bounded, record-oriented 1-server/multi-client transport with no kernel pointers.
- Broadcast is expanded into directed nonblocking sends to a snapshot of HELLO-completed clients.
- Poudland Version 1 uses fixed-width pointer-free messages, request IDs, explicit errors, and session-owned window IDs.
- The new minimal implementation lives alongside the legacy compositor code during route A; route B migrates useful legacy behavior only after P0 is green.
- The reusable FrogFS image is generated from source inputs and copied per test; there is no per-run guest prepare stage.

## Dependency Graph

```text
Baseline validation and checkpoint
    |
    +-- Timekeeping --------+-- wait2 core -- input/package readiness --+
    |                       |                                       |
    +-- Anonymous mmap -- user allocator ---------------------------+
    |                                                               |
    +-- FrogFS image tool --------------------------------------+    |
                                                                |    |
wait2 + allocator + VFS/devfs --> packagefs --> transport lib --+----+
                                                                |    |
framebuffer + input + time + allocator --> built-in compositor -+    |
                                                                     |
Poudland protocol/client + reusable image + compositor ---------------+
    |
desktop create/close slice
    |
pointer, focus, drag, keyboard, damage
    |
graphical init and desktop-smoke
    |
desktop-soak-10m
```

## Tasks

### Task 0: Re-establish and checkpoint the current baseline

**Description:** Preserve the existing user-owned input-chain work, rerun the current focused profiles serially, and create explicit checkpoint commits only after the worktree contents and passing evidence are understood.

**Acceptance criteria:**

- Current compile, process, framebuffer-mmap, and input profiles pass from the actual dirty checkout.
- Existing input changes and the accepted design documents are staged explicitly without unrelated artifacts.
- Failure artifacts are retained and diagnosed before any Poudland foundation work begins.

**Verification:**

- `./scripts/CI.sh`
- `./scripts/qemu-test.sh process-smoke`
- `./scripts/qemu-test.sh framebuffer-mmap-smoke`
- `./scripts/qemu-test.sh input-smoke`
- `git diff --check`

**Dependencies:** None

**Files likely touched:** None unless a baseline regression is found

**Estimated scope:** S

### Task 1: Establish the time64 monotonic and optional realtime ABI

**Description:** Add signed 64-bit time types, normalized `timespec/timeval`, corrected PIT programming and rational accumulation, optional validated RTC origin, clock syscalls, and coherent i386 snapshots.

**Acceptance criteria:**

- PIT high and low divisor bytes produce the intended approximately 1000 Hz rate.
- `CLOCK_MONOTONIC` works without RTC and never decreases; invalid realtime returns `-ENODATA`.
- `clock_gettime/gettimeofday` validate user pointers and return normalized time64 structures.

**Verification:**

- Focused kernel time tests including low-half carry, PIT remainder, RTC invalid data, and no-RTC boot
- `./scripts/qemu-test.sh process-smoke`
- `./scripts/CI.sh`

**Dependencies:** Task 0

**Files likely touched:**

- `core/include/frog/types.h`
- `core/include/frog/time.h`
- `core/arch/x86/irq/i8253.c`
- `core/drivers/rtc/cmos.c`
- `core/arch/x86/entry/syscall-init.c`

**Estimated scope:** M

### Task 2: Implement the poll-shaped `wait2` core

**Description:** Replace the commented select-like path with the accepted pollfd ABI, monotonic timeout conversion, strong fd references, level-triggered readiness, and count-zero finite sleep.

**Acceptance criteria:**

- `POLLIN`, `POLLOUT`, `POLLERR`, `POLLHUP`, and `POLLNVAL` follow `doc/wait2-design.md`.
- `timeout_ms` handles `-1`, `0`, positive values, finite count-zero sleep, and rejects empty infinite waits.
- Close/readiness races remain memory-safe and invalid nonnegative descriptors report per-entry `POLLNVAL`.

**Verification:**

- Focused wait2 syscall tests for timeout boundaries, multiple fds, invalid fds, and close races
- `./scripts/qemu-test.sh input-smoke`
- `./scripts/CI.sh`

**Dependencies:** Task 1

**Files likely touched:**

- `core/include/frog/poll.h`
- `core/fs/select.c`
- `core/include/kernel/fd.h`
- `core/kernel/fd.c`
- `core/arch/x86/entry/syscall-init.c`

**Estimated scope:** M

### Task 3: Connect input devices to common readiness

**Description:** Make keyboard and mouse file operations report complete-record readability through the common poll callback without changing the accepted `/dev/input/event0/1` read ABI.

**Acceptance criteria:**

- Empty queues are not readable; enqueuing input wakes waiters and remains readable until drained.
- Mouse readiness represents a complete `mouse_device_packet_t`, not a partial PS/2 byte sequence.
- Existing blocking and nonblocking input behavior remains intact.

**Verification:**

- `./scripts/qemu-test.sh input-smoke`
- New wait2-over-input cases pass with ordered QMP injection

**Dependencies:** Tasks 0 and 2

**Files likely touched:**

- `core/drivers/input/keyboard/ps2_kbd_driver.c`
- `core/drivers/input/mouse/ps2_mouse_driver.c`
- `core/include/kernel/vfs.h`

**Estimated scope:** S

## Checkpoint A: Time and readiness foundation

- All baseline profiles still pass serially.
- Time and wait2 focused tests pass without RTC dependency.
- No input busy polling remains in the intended compositor path.

### Task 4: Add eager private anonymous mappings

**Description:** Extend the current device-only mmap path with exact, eager, zero-filled private anonymous mappings and private fork copies while retaining exact `munmap`.

**Acceptance criteria:**

- Accepted anonymous flags, fd, offset, length, and address rules are enforced atomically.
- Forked mappings begin byte-identical and become independent after either process writes.
- Exec, exit, failed allocation rollback, and exact unmap release every owned frame and VMA.

**Verification:**

- Focused anonymous mmap/fork/exec/exit tests under 16 MiB
- Existing `framebuffer-mmap-smoke` still passes
- Failure-injection cleanup counters return to baseline

**Dependencies:** Task 0

**Files likely touched:**

- `core/include/uapi/frog/mman.h`
- `core/fs/syscall_fs.c`
- `core/mm/vm.c`
- `core/include/frog/vm.h`
- `core/kernel/thread/fork.c`

**Estimated scope:** M

### Task 5: Move `malloc/free` into the user runtime

**Description:** Implement a 16-byte-aligned user allocator with small page arenas and dedicated large mappings; do not re-enable kernel `SYS_MALLOC/SYS_FREE`.

**Acceptance criteria:**

- Small and multi-megabyte allocations, reuse, arena release, `malloc(0)`, `free(NULL)`, alignment, and OOM behavior match the accepted contract.
- No user allocator path asks the kernel to interpret arena metadata.
- The allocator survives fork isolation and repeated process exit under 16 MiB.

**Verification:**

- Focused user allocator smoke profile
- `./scripts/qemu-test.sh process-smoke`
- Static app link contains the user allocator and no `SYS_MALLOC/SYS_FREE` call site

**Dependencies:** Task 4

**Files likely touched:**

- `core/lib/user_malloc.c`
- `core/include/frog/syscall.h`
- `core/kernel/syscall.c`
- `core/user/Makefile`
- `core/user/user_runtime_smoke.c`

**Estimated scope:** M

### Task 6: Implement packagefs bind, connect, and directed records

**Description:** Replace the stale pointer-bearing packagefs path with ephemeral named services, opaque client IDs, bounded complete records, and per-client receive queues.

**Acceptance criteria:**

- Accepted server/client `open` flags, name validation, limits, and errno results are enforced.
- C2S and directed S2C records preserve boundaries and never expose kernel addresses.
- Blocking and nonblocking queue behavior is bounded and all-or-nothing.

**Verification:**

- New `packagefs-smoke` bind/connect/two-client routing and boundary cases
- Invalid pointer, size, ID, flag, and stale-generation tests
- `./scripts/CI.sh`

**Dependencies:** Tasks 2 and 4

**Files likely touched:**

- `core/fs/packagefs/packagefs.c`
- `core/fs/packagefs/packagefs.h`
- `core/include/uapi/frog/packagefs.h`
- `core/init/main.c`
- `core/Makefile`

**Estimated scope:** M

### Task 7: Complete packagefs lifecycle and readiness

**Description:** Add final-reference disconnect, drain-before-EOF, HUP/error behavior, per-client writable notifications, and safe process-exit teardown.

**Acceptance criteria:**

- Duplicated and fork-inherited fds produce exactly one final disconnect.
- Queued DATA precedes DISCONNECT; reads drain before EOF and writes after close return `-EPIPE`.
- A failed nonblocking directed send arms one coalesced `FROG_PKG_WRITABLE` notification naming the correct client ID.

**Verification:**

- `packagefs-smoke` lifecycle, queue-full recovery, close races, and repeated bind cycles
- Leak/reference counters return to baseline after forced process exits
- wait2 readiness transitions match the design document

**Dependencies:** Task 6

**Files likely touched:**

- `core/fs/packagefs/packagefs.c`
- `core/fs/select.c`
- `core/kernel/thread/exit.c`
- `core/user/packagefs_smoke.c`
- `scripts/qemu-test.sh`

**Estimated scope:** M

## Checkpoint B: User runtime and transport

- Anonymous mmap, allocator, and packagefs focused profiles pass under 16 MiB.
- Existing framebuffer, input, process, and disk regressions remain green.
- No legacy queue-size ioctl or serialized pointer remains in the new transport.

### Task 8: Build the pointer-free packagefs user library

**Description:** Provide record construction, exact read/write, bind/connect helpers, and directed-send results for Poudland without retaining legacy packetx pointer layouts.

**Acceptance criteria:**

- Helpers preserve negative errno and never write a fixed 1024 bytes from a smaller allocation.
- Server helpers expose peer IDs as values and handle DATA, DISCONNECT, and WRITABLE records.
- Broadcast expansion reports delivered, would-block, and disconnected clients independently.

**Verification:**

- Library tests run over the real packagefs syscall path
- Static inspection finds no `uint_32 *source/target` transport identity

**Dependencies:** Task 7

**Files likely touched:**

- `core/apps/include/frog/packagefs.h`
- `core/apps/lib/packagefs.c`
- `core/apps/Makefile`
- `core/user/packagefs_smoke.c`

**Estimated scope:** S

### Task 9: Implement the Poudland Version 1 client runtime

**Description:** Add fixed headers, HELLO/WELCOME negotiation, request matching, explicit errors, bounded intervening-event queues, and caller-owned client context.

**Acceptance criteria:**

- Connect retries only `-ENOENT` for the requested timeout and uses wait2 rather than busy polling.
- Request IDs skip zero and pending IDs; mismatched, malformed, and version-incompatible responses fail deterministically.
- Create, close, next-event, disconnect, EOF, and `-ECONNRESET` behavior match the accepted public interface.

**Verification:**

- Client/server fixture covers response reordering, async events between responses, timeout, malformed payload, ERROR, and HUP
- No legacy `struct list_head` or fd-as-pointer crosses the protocol

**Dependencies:** Task 8

**Files likely touched:**

- `core/apps/include/gua/poudland.h`
- `core/apps/lib/gua/poudland.c`
- `core/apps/include/gua/poudland_protocol.h`
- `core/apps/Makefile`

**Estimated scope:** M

### Task 10: Establish the modern built-in compositor slice

**Description:** Create the route-A minimal event-loop implementation alongside the legacy compositor, using framebuffer mode metadata, one damage-aware backbuffer, solid geometry-only windows, cursor bitmap, input fds, monotonic deadlines, focus, and whole-window drag.

**Acceptance criteria:**

- A user ELF opens `/dev/fb0`, maps it, loads `/test/b.bmp`, and renders background, cursor, and two built-in windows under 16 MiB.
- Cursor and drag operate through wait2 input readiness; no `/dev/tty0` input dependency remains.
- Static scene produces no repeated presents after 500 ms idle.

**Verification:**

- Focused built-in compositor QEMU profile with QMP input and screenshot
- Frame/pixel counters prove damage-bounded presentation and idle quiescence
- Existing framebuffer-mmap and input profiles remain green

**Dependencies:** Tasks 1, 3, and 5

**Files likely touched:**

- `core/apps/poudland/main.c`
- `core/apps/poudland/scene.c`
- `core/apps/poudland/render.c`
- `core/apps/include/gua/poudland-server.h`
- `core/apps/Makefile`

**Estimated scope:** M

### Task 11: Create the deterministic FrogFS host image builder

**Description:** Build the 80 MiB sparse MBR image with one primary FrogFS partition, install manifest-selected artifacts, verify them, and publish atomically.

**Acceptance criteria:**

- A clean build can create `/dev/sdbp1` layout without using a guest prepare boot or an existing `hd80M.img` template.
- The builder validates every offset, filesystem limit, installed file size, and content hash.
- Failed builds leave no apparently valid new image; unchanged inputs reuse the existing output.

**Verification:**

- Host unit tests for MBR/FrogFS layout and corrupt/oversized inputs
- QEMU disk profile mounts the generated image and reads the verified manifest files
- Rebuilding twice without input change preserves the content hash

**Dependencies:** Task 0; final content installation depends on Task 12

**Files likely touched:**

- `tools/mkfrogfs_image.c`
- `tools/Makefile`
- `config/frog-root.manifest`
- `Makefile`

**Estimated scope:** M

## Checkpoint C: Independent vertical slices

- Modern built-in compositor renders and drags without packagefs.
- Poudland client protocol fixture passes independently of graphics.
- Host tool creates and guest mounts a deterministic FrogFS disk.

### Task 12: Produce installable compositor and desktop ELFs

**Description:** Repair the app build around the new runtime and minimal modules, create deterministic static i386 ET_EXEC artifacts, and install them plus `b.bmp` into the generated image.

**Acceptance criteria:**

- Clean build produces fresh compositor and desktop ELFs with correct entry point and no unresolved legacy dependency.
- Image manifest contains the exact new hashes and paths under `/test`.
- Normal exec loader starts each ELF from FrogFS with no raw-image 4 KiB restriction.

**Verification:**

- ELF header/program-header inspection
- `make frog-root.img`
- A focused exec-from-generated-FrogFS profile under 16 MiB

**Dependencies:** Tasks 5, 9, 10, and 11

**Files likely touched:**

- `core/apps/Makefile`
- `core/apps/start.s`
- `core/apps/desktop.c`
- `config/frog-root.manifest`
- `Makefile`

**Estimated scope:** M

### Task 13: Connect Poudland create and close end to end

**Description:** Bind `/dev/pkg/compositor`, implement Version 1 HELLO/create/close/error handling, replace built-in validation windows with client-owned solid windows, and preserve server ownership rules.

**Acceptance criteria:**

- `desktop.c` completes HELLO, creates three accepted windows, receives three IDs, closes the third with confirmation, and leaves exactly two live windows.
- Foreign IDs, stale IDs, invalid geometry, protocol errors, and window limits return the accepted errors.
- Client disconnect destroys all owned windows and damages their former regions.

**Verification:**

- Guest FROGTEST cases for handshake, three creates, third close, two live, ownership, and disconnect cleanup
- Screenshot before input shows only the accepted two-window scene

**Dependencies:** Task 12

**Files likely touched:**

- `core/apps/poudland/protocol.c`
- `core/apps/poudland/scene.c`
- `core/apps/lib/gua/poudland.c`
- `core/apps/desktop.c`

**Estimated scope:** M

### Task 14: Complete pointer, keyboard, focus, drag, and damage behavior

**Description:** Route direct input events through the Poudland scene, emit explicit screen/local pointer events and forward-compatible key events, move the focused window, and present only resulting damage.

**Acceptance criteria:**

- Left press anywhere visible on window 2 focuses it; relative `(+40,+25)` moves it from `(220,200)` to `(260,225)` and release ends drag.
- Desktop receives the corresponding absolute WINDOW_CONFIGURE and receives ASCII `a` as a PRESS key event for the focused window.
- Pointer motion/drag and configure replacement rules do not overtake reliable button or keyboard transitions.

**Verification:**

- Guest state markers include old/new window and cursor coordinates, focused window ID, and delivered key event
- Damage/present counters and screenshot key pixels match the fixed scene

**Dependencies:** Task 13

**Files likely touched:**

- `core/apps/poudland/input.c`
- `core/apps/poudland/scene.c`
- `core/apps/poudland/protocol.c`
- `core/apps/desktop.c`
- `core/apps/include/gua/poudland_protocol.h`

**Estimated scope:** M

### Task 15: Launch and supervise the graphical children from user init

**Description:** Replace the production init spin with the accepted fork/exec/wait sequence while keeping test reports conditional and restart disabled.

**Acceptance criteria:**

- Init launches `/test/compositor` then `/test/desktop`; the desktop tolerates the service bind race for up to two seconds.
- Exec failure, connect timeout, unexpected child exit, compositor HUP, and normal cleanup produce distinct status evidence.
- Production builds contain no QEMU-only test output path.

**Verification:**

- Focused launch tests for missing/corrupt ELF and compositor early exit
- Process and exec regression profiles remain green
- Normal build inspection excludes test-report calls

**Dependencies:** Tasks 12 and 13

**Files likely touched:**

- `core/user/graphical_init.c`
- `core/user/Makefile`
- `core/init/main.c`
- `core/include/uapi/frog/test.h`

**Estimated scope:** M

## Checkpoint D: Functional desktop goal

- The normal guest path loads both ELFs from the reusable FrogFS image.
- Three create/one close leaves two client windows.
- QMP mouse and keyboard input completes the fixed drag/focus/key scenario.
- Static scene stops presenting.

### Task 16: Add guest desktop-smoke state reporting

**Description:** Add test-only case/sync reports to the real init, compositor, and desktop control flow without introducing a separate graphical implementation.

**Acceptance criteria:**

- Guest cases report launch, service bind, handshake, three creates, third close, two live, drag coordinates, focus, key delivery, and idle present stability.
- `desktop-final-frame` is emitted only after the final damage reaches the framebuffer.
- Production preprocessing removes all test reports.

**Verification:**

- Debugcon transcript contains ordered, profile-specific FROGTEST records
- Negative injections fail the correct case rather than timing out generically

**Dependencies:** Tasks 14 and 15

**Files likely touched:**

- `core/include/uapi/frog/test.h`
- `core/apps/poudland/test_report.c`
- `core/apps/desktop.c`
- `core/user/graphical_init.c`
- `Makefile.os_rules`

**Estimated scope:** M

### Task 17: Add the 16 MiB host desktop-smoke profile

**Description:** Extend the bounded QEMU runner to copy the reusable image, inject the fixed mouse/key sequence, wait for the final-frame marker, capture the framebuffer, validate exact dimensions and key pixels, and classify the result.

**Acceptance criteria:**

- QEMU runs with `-m 16M`, a private QMP socket, unique disk copy, bounded timeout, and no global process killing.
- The host requires all guest state cases plus the exact final screenshot scene; either evidence source failing prevents PASS.
- The base image is unchanged and failures retain disk, logs, QMP transcript, screenshot, and result JSON.

**Verification:**

- `./scripts/qemu-test.sh desktop-smoke`
- Deliberate wrong coordinate, stale image, missing marker, and wrong pixel each produce a non-PASS classification

**Dependencies:** Task 16

**Files likely touched:**

- `scripts/qemu-test.sh`
- `scripts/README.md`
- `doc/qemu-automated-validation.md`
- `Makefile.os_rules`

**Estimated scope:** M

### Task 18: Add the ten-minute desktop soak

**Description:** Reuse the passing image and graphical path for a separately invoked bounded stability profile with periodic state heartbeats and final resource checks.

**Acceptance criteria:**

- Ten minutes complete without panic, assertion, unexpected child exit, unbounded present growth while idle, or allocator/packagefs reference growth.
- The runner retains concise heartbeat and final resource evidence without excessive logs.
- The soak remains separate from the fast development profile.

**Verification:**

- `./scripts/qemu-test.sh desktop-soak-10m`
- Failure-injected run proves the watchdog and artifact preservation paths

**Dependencies:** Task 17

**Files likely touched:**

- `scripts/qemu-test.sh`
- `core/apps/poudland/test_report.c`
- `doc/qemu-automated-validation.md`

**Estimated scope:** S

## Final Checkpoint: Poudland P0 complete

- `./scripts/CI.sh` passes.
- Existing process, disk, framebuffer-mmap, and input profiles pass serially.
- `desktop-smoke` passes from a generated reusable FrogFS image under 16 MiB.
- `desktop-soak-10m` passes separately.
- `git diff --check` is clean, generated images are not staged, and commits preserve unrelated user changes.
- The two remaining windows, cursor, drag result, and absent third window are supported by both guest state and screenshot evidence.

## Risks and Mitigations

| Risk | Impact | Mitigation |
| --- | --- | --- |
| Anonymous VM fork/teardown regresses current process work | High | Land and validate it before allocator or compositor use; keep failure-injection cleanup tests |
| 16 MiB cannot hold kernel plus a 3 MiB backbuffer | High | Remove per-window pixel buffers, run every graphical profile at 16 MiB, report explicit OOM |
| Packagefs lifecycle races leak sessions or pages | High | Strong refs, final-close tests, repeated bind/exit loops, resource counters |
| Legacy compositor code pulls stale APIs back into P0 | Medium | Build the route-A minimal modules separately; migrate legacy behavior only after green P0 |
| Host image tool drifts from kernel FrogFS format | High | Share fixed on-disk definitions where possible and verify every generated image in the guest |
| One server fd cannot express per-client POLLOUT | Medium | Use accepted per-client `FROG_PKG_WRITABLE` input records after `-EAGAIN` |
| Test-only behavior differs from production | High | Compile thin reports into the same real control paths and run normal exec/image paths |
| Existing dirty input changes are accidentally overwritten | High | Validate and checkpoint with explicit path staging before overlapping edits |

## Open Questions

None for the accepted P0 scope. Root filesystem migration and Window Surface design are explicitly deferred.
