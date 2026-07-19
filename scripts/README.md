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
and FrogFS cases. These operations still run only against disposable image
copies. Set `FROG_QEMU_TIMEOUT` to change the 30-second deadline or
`FROG_QEMU_KEEP=1` to retain all artifacts from a passing run.

Run `./scripts/qemu-test.sh process-smoke` after process, scheduler, paging, or
syscall changes. It boots into ring 3 and checks fork return values, address
space isolation, exit-status delivery, child reaping, and the no-child wait
case. The guest cases live in `core/kernel/thread/process_regression.c`.

Run `./scripts/qemu-test.sh user-smoke` after changing user-image loading,
privilege transitions, or syscall entry. It copies a separately linked image
to `0x08048000`, enters it in ring 3, and verifies both invalid and
unimplemented syscall handling before completing the guest test.

Run `./scripts/qemu-test.sh framebuffer-smoke` after boot-video, VBE, paging, or
framebuffer changes. The guest maps the VBE linear framebuffer as kernel-only
MMIO and draws fixed red, green, and blue bands with a white center square. The
host waits for a debugcon synchronization marker, requests a P6 PPM screenshot
through a private QMP socket, and validates the full visible frame, including
every pixel.
Passing runs remain quiet and retain only
`build/qemu-test/framebuffer-smoke-result.json`; failed runs retain the
screenshot, debugcon log, QEMU trace, and validator diagnostics.
