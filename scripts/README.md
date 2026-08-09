This is a helper scripts folder.

`bochs_config/`         contains bochs configure and debug configure.
`debug/`                contains gdb debug scripts.
`create_net_bridge.sh`  a script makes a net bridge and a tap type interface.
`debug.sh`              a script for debugging code
`run.sh`                a script for running without debug
`CI.sh`                 a script for checking a clean kernel image build
## Automated QEMU validation

Run `./scripts/qemu-test.sh boot-smoke` for the canonical noninteractive boot
check. It performs a clean test build, packages disposable copies of both disk
images, waits for the ring-3 and final guest markers, and quietly writes the
normalized result to `build/qemu-test/boot-smoke-result.json`. Failed runs retain their
build log, debugcon output, QEMU trace, and copied images under
`build/qemu-test/<timestamp>-boot-smoke-<classification>/`.

`./scripts/qemu-test.sh disk-smoke` additionally enables the destructive disk
and FrogFS cases. Its prepare stage installs and executes a real static i386
ET_EXEC with separate RX and RW load segments, then checks argc/argv,
initialized data, zero-filled BSS, and an inherited file descriptor. It also
checks invalid paths, pointers, argument counts, and ELF input, plus a test-only
failure after candidate address-space construction to prove that the old image
survives and a subsequent exec still succeeds. The one-shot failure state is
also checked across fork, process exit, and PID reuse. These operations still
run only against disposable image copies. Set `FROG_QEMU_TIMEOUT` to change the
30-second deadline or `FROG_QEMU_KEEP=1` to retain all artifacts from a passing
run.

`./scripts/qemu-test.sh frogfs-image-smoke` builds the deterministic sparse
`build/frog-root.img`, copies it for QEMU, mounts its sole primary partition as
`/dev/sdbp1` at `/test`, and compares the image's `/b.bmp` (visible to the
guest as `/test/b.bmp`) byte-for-byte with the manifest input.
The source image is never attached directly to the guest. Build and verify it
without QEMU using `make frog-root.img` and `make frog-root-verify`; run the
host corruption, oversize, atomic-publication, and reuse tests with
`make frog-root-test`.

`./scripts/qemu-test.sh frogfs-exec-smoke` uses that same generated image at
16 MiB. The guest mounts `/dev/sdbp1` at `/test`, proves both production ELF
files are larger than 4 KiB, and sequentially executes the exact installed
`/test/compositor` and `/test/desktop` files with `--exec-smoke`. Their expected
exit statuses and the parent `wait` results feed the machine-readable
`FROGTEST` result, with a path-specific case name on failure. No alternate test
executable is built.

`./scripts/qemu-test.sh poudland-e2e-smoke` runs the production compositor and
`desktop.c` ELFs from a private copy of the same generated FrogFS image at
16 MiB. A multi-page guest harness exercises HELLO, create/close, ownership,
geometry and protocol errors, the 16-window session limit, the 64-window server
limit, and disconnect cleanup. It then starts the real desktop, proves the two
remaining IDs are live and client-owned, and emits a synchronization marker.
The host polls P6 screenshots through a private QMP socket until the exact
two-window frame is present, quits QEMU, and validates every pixel including
the bitmap cursor's alpha blend. A stale third window or any old test-client
window prevents PASS. Set `FROG_QEMU_KEEP=1` to retain the successful
screenshot and transcript as evidence; failures retain them automatically.

Run `./scripts/qemu-test.sh process-smoke` after process, scheduler, paging, or
syscall changes. It boots into ring 3 and checks fork return values, address
space isolation, exit-status delivery, child reaping, and the no-child wait
case. It also verifies that a bad wait status pointer leaves the same zombie
available for retry, init adopts an already-zombie grandchild, and fork rebases
a real nonempty user-heap free list without sharing allocation state. Kernel
cases live in `core/kernel/thread/process_regression.c`; ring-3 cases live in
`core/user/user_smoke.c`.

All normal and QEMU-test boots now enter ring 3 through a separately linked
image copied to `0x08048000`; kernel-linked `init()` remains only as legacy
source. `boot-smoke` and `user-smoke` use the basic image, `process-smoke` uses
the process image, and the prepare stage of `disk-smoke` uses the fd-lifetime
image. Disk verify/corrupt stages use the basic image. Run
`./scripts/qemu-test.sh user-smoke` after changing image loading, privilege
transitions, or syscall entry; it verifies invalid and unimplemented syscall
handling before completing the guest test.

Run `./scripts/qemu-test.sh framebuffer-smoke` after boot-video, VBE, paging, or
framebuffer changes. The guest maps the VBE linear framebuffer as kernel-only
MMIO and draws fixed red, green, and blue bands with a white center square. The
host waits for a debugcon synchronization marker, requests a P6 PPM screenshot
through a private QMP socket, and validates the full visible frame, including
every pixel.
Passing runs remain quiet and retain only
`build/qemu-test/framebuffer-smoke-result.json`; failed runs retain the
screenshot, debugcon log, QEMU trace, and validator diagnostics.

Run `./scripts/qemu-test.sh framebuffer-mmap-smoke` for the public API path. A
separate low-address ring-3 image opens `/dev/fb0`, validates framebuffer info
and mmap errors, maps the complete aperture, draws the base frame, closes the
fd, and then proves the VMA remains live across fork and exact unmap. Child
processes add fixed magenta and yellow rectangles, including child-first and
parent-first exit order plus a child-only post-unmap fault. The test-only ready
syscall checks that all mapping, file, device, and physical-resource references
returned to their baseline before QMP captures and validates every pixel.
Failures retain the QMP transcript in addition to the graphical diagnostics.

Run `./scripts/qemu-test.sh anonymous-mmap-smoke` for the eager private-RAM
mapping contract. Its low-address ring-3 fixture checks the exact anonymous
request shape, one-page and multi-page zero filling and writes, exact unmap,
private fork copies, exit cleanup, successful `execv` cleanup through a
test-only read-only ELF fixture, and all-or-nothing rollback after injected
PTE-install and fork-copy failures. Under its default 1 GiB QEMU memory it also
checks allocation, PTE-install, and fork-copy rollback without leaked user
frames. The exec case replaces a child with a live three-page anonymous image
and verifies that its final user-frame count returns to the parent's baseline.
It fully scans a zero-filled 3 MiB compositor backbuffer, checks writes at both
ends and across a page boundary, exactly unmaps it, and verifies the user-frame
baseline. Under the default 1 GiB QEMU memory it also requires a zero-filled
mapping at the 16 MiB parameter ceiling. With the explicit 16 MiB test
configuration that ceiling request may validly return `-ENOMEM`, but no other
error; the 3 MiB compositor budget must still succeed. This profile is headless
and does not use QMP. The
one-page limit on built-in smoke image blobs is only a fixture packaging rule;
it is not an `mmap` or `exec` ABI limit.

Run `./scripts/qemu-test.sh user-allocator-smoke` for the single-threaded user
allocator built on private anonymous mappings. The ring-3 fixture checks
`malloc(0)`, `free(NULL)`, 16-byte alignment, power-of-two small arenas,
in-arena reuse, empty-arena release, a zero-filled 3 MiB dedicated mapping,
forced mmap failure cleanup, fork isolation, and repeated child exit without
freeing allocations. It also verifies that the legacy `SYS_MALLOC` and
`SYS_FREE` numbers remain unsupported. Run the same profile with
`FROG_QEMU_MEMORY=16M` for the P0 memory budget.

Run `./scripts/qemu-test.sh packagefs-smoke` after changing VFS atomic open,
devfs mounts, fd lifetime, exec, wait queues, or packagefs itself. The headless
ring-3 fixture covers exact bind/connect flags and limits, pointer-free bounded
records, two-client directed routing, invalid pointers and IDs, nonblocking
backpressure, blocking wakeup, stale IDs after rebind, and `O_CLOEXEC` across
`execv`. A kernel-side case also forces a byte-ring wrap and reads every record
back in FIFO order. Repeat with
`FROG_QEMU_MEMORY=16M ./scripts/qemu-test.sh packagefs-smoke` for the P0 memory
budget. This profile does not use QMP.

Most QEMU profiles default to `-m 1G`; generated-image graphical/exec profiles,
including `poudland-e2e-smoke`, default to 16 MiB. Set `FROG_QEMU_MEMORY` to a
positive integer with an `M` or `G` suffix, for example
`FROG_QEMU_MEMORY=16M ./scripts/qemu-test.sh anonymous-mmap-smoke`. The chosen
value is used by every runner path and recorded as `qemu_memory` in the result
JSON.

Run `./scripts/qemu-test.sh input-smoke` after PS/2, interrupt, devfs, fd, or
input ABI changes. A separate ring-3 image opens `/dev/input/event0` and
`/dev/input/event1`, checks nonblocking empty reads and close, then performs
infinite `wait2` calls while the host injects a qcode `a`, relative X movement
`+7`, and left-button down through QMP. Each source must remain level-ready
until read, clear after drain, and wake only its own poll entry. A final ordered
injection delivers keyboard and mouse input together and requires one `wait2`
call to report both entries. Debugcon synchronization markers order every
bounded injection. A successful run also requires the keyboard byte and each
complete shared `mouse_device_packet_t` value to match exactly; failed runs
retain the QMP transcript and guest logs.

Run `./scripts/qemu-test.sh time-smoke` for the ring-3 time syscall contract.
The fixture checks monotonic normalization and forward progress, user-pointer
and clock-ID errors, unavailable realtime behavior, PIT programming, and the
fractional nanosecond accumulator. This profile is headless and does not use
QMP.

Run `./scripts/qemu-test.sh wait2-smoke` after changing poll, wait queues, fd
lifetime, uaccess, scheduler, or monotonic timeout handling. The headless
ring-3 fixture checks the frozen 8-byte `pollfd` ABI, whole-call validation,
per-entry `POLLNVAL`, revents clearing and ready counts, nonblocking scans,
absolute finite deadlines, and cleanup after an injected copy-out fault. It
does not use QMP; input-driven `wait2` wakeup is deferred to the input
profile's dedicated integration phase.
