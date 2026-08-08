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

Run `./scripts/qemu-test.sh input-smoke` after PS/2, interrupt, devfs, fd, or
input ABI changes. A separate ring-3 image opens `/dev/input/event0` and
`/dev/input/event1`, checks nonblocking empty reads and close, then performs
blocking reads while the host injects a qcode `a`, relative X movement `+7`,
and left-button down through QMP. Debugcon synchronization markers order each
injection. A successful run requires the keyboard byte and complete shared
`mouse_device_packet_t` values to match exactly; failed runs retain the QMP
transcript and guest logs.

Run `./scripts/qemu-test.sh time-smoke` for the ring-3 time syscall contract.
The fixture checks monotonic normalization and forward progress, user-pointer
and clock-ID errors, unavailable realtime behavior, PIT programming, and the
fractional nanosecond accumulator. This profile is headless and does not use
QMP.
