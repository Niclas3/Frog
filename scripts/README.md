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
`/dev/sdbp1` at `/test`, and compares the production image's
`/share/poudland/cursor.bmp` (visible to the guest as
`/test/share/poudland/cursor.bmp`) byte-for-byte with the manifest input. This
guest-path check passes at the default 16 MiB memory budget; it does not perform
a Root Switch.
The source image is never attached directly to the guest. Build and verify it
without QEMU using `make frog-root.img` and `make frog-root-verify`; run the
host corruption, oversize, atomic-publication, and reuse tests with
`make frog-root-test`.

`./scripts/qemu-test.sh root-locator-smoke` is the focused Root Locator kernel
profile. It registers boot-local synthetic disks through the real MBR scanner
and attaches an otherwise blank disposable secondary disk, so no tracked image
can affect the cases. It checks that enumeration exposes the synthetic
partitions but not whole disks, then covers missing, unique, duplicate,
target-label bad-magic and structural corruption, unrelated corruption, and
unreadable-partition classification.
The unique case is deliberately fail-closed: an unreadable partition prevents
selection because it could be a second `frog-root`, while two already valid
matches remain a definitive duplicate. The profile is headless and does not
use QMP; it defaults to the first-machine 16 MiB memory budget.

`./scripts/qemu-test.sh root-switch-smoke` is the focused one-time VFS Root
Switch profile. It copies the generated production FrogFS image, mounts it at
`/sysroot` only in the test fixture, and runs at 16 MiB without QMP. Before the
commit it checks every actual fallible preparation checkpoint leaves the old
root and all mount identities and flags unchanged. After the commit it proves
the staged root is `/`, the old paths are unreachable, devfs and packagefs use
the same mounted roots and superblocks, a queued packagefs message plus a
reply traverse the original open files, and `/dev/input/event0` retains its
dentry, inode, open, and empty-read behavior. A second switch must return
`-EALREADY`. This remains isolated primitive evidence.

`./scripts/qemu-test.sh root-namespace-smoke` is the production Root Namespace
profile and Checkpoint B evidence. At 16 MiB it locates exactly one
`frog-root`, passes the registry-owned block object directly through the typed
mount-source interface, mounts it at `/sysroot`, verifies the same source and
the on-disk read-only flag, and performs the one-time Root Switch. It proves
`/sysroot` and a Bootstrap-Root-only path are unreachable; reads the installed
`/bin/compositor` and `/sbin/init` ELF headers; rejects existing write,
truncate, create, mkdir, unlink, and rmdir with `-EROFS`; preserves dentry and
inode identity plus valid open/read/ioctl behavior for both input devices and
`/dev/fb0`; and completes a packagefs round trip through the original mounted
root and open files. It does not load or execute disk PID1.

`./scripts/qemu-test.sh production-root-negative-smoke` is the real-disk
startup rejection profile. Its `missing`, `corrupt`, and `duplicate` stages
all call the same `disk_system_init_start()` transaction as normal boot at
16 MiB. The missing stage attaches a real empty image; corrupt preserves the
target label while invalidating only the FrogFS zone size; duplicate attaches
two complete `frog-root` images. The guest requires the exact typed locator
status and proves the Root Switch did not occur. QEMU attaches every staged
root with `snapshot=on`; `result.json` persists base, primary, and applicable
extra-disk SHA-256 values before and after every stage.

`./scripts/qemu-test.sh disk-init-loader-smoke` extends that real locator,
read-only Root Switch, and preserved-device chain through Task 4.1. At 16 MiB
it proves the installed `/sbin/init` is larger than 4 KiB, loads it through the
same ELF parser, mapper, page protection, and stack builder as ordinary
`execv`, and atomically publishes a fully prepared numeric PID1 with the
expected parent, name, address space, entry, stack, and arguments. Missing,
malformed, oversized, unmappable, argument-allocation, partial-map, and
post-PID-swap/pre-publication failures must publish no user address space,
leave the Bootstrap main thread as PID1, make the temporary PID immediately
reusable, and retain root/devfs/packagefs identity and readiness. Fault files
are added only to a temporary non-overriding overlay; the profile attaches its
base through a QEMU snapshot and verifies the base SHA-256 before and after.
It intentionally finishes before scheduling PID1, so it is not Task 4.2 System
Init selection evidence.

`./scripts/qemu-test.sh system-init-selection-smoke` is the focused Task 4.2
ring-3 profile. At 16 MiB it repeats the production Root Locator, read-only
Root Switch, and disk PID1 startup, then schedules the exact production
`/sbin/init`. The `valid` stage proves that System Init strictly reads
`mode=graphical\n` and replaces itself with a test-only ELF installed at
`/sbin/init-graphical`; that target verifies `argc`/`argv`, numeric PID1,
parentless process identity, the replaced image name and address space, the
root configuration, `/dev/input/event0`, and a cleaned-up packagefs
bind/connect pair. It does not start compositor or desktop children.

The remaining stages are `missing-config`, `malformed`, `duplicate`,
`unknown`, `tty`, `open-failure`, `read-failure`, `close-failure`,
`exec-missing`, `exec-corrupt`, and `exec-returned`. Each must emit its exact
`system-init status=NN` line from ring 3 and then enter the permanent
`wait2(NULL, 0, 1000)` stop loop before the guest can report PASS. The runner
builds a complete temporary FrogFS image for each stage instead of overriding
the production manifest. Non-injection stages use the byte-identical
production `init.elf`; only the four I/O/returned-exec injections use a
separately named focused ELF. Every guest drive is a QEMU snapshot, and both
the stage image and reusable production image hashes must remain unchanged.
Set `FROG_QEMU_KEEP=1` when the individual stage manifests, debugcon logs, and
build logs are needed for an audit.

`./scripts/qemu-test.sh graphical-init-production-smoke` is the focused Task
4.3 lifecycle profile. At 16 MiB it runs the real Root Locator, read-only Root
Switch, exact production `/sbin/init`, and exact production
`/sbin/init-graphical`. The complete temporary System Image copies the
production configuration, cursor, and both init ELFs byte-for-byte; only
`/bin/compositor` and `/bin/desktop` are replaced by ring-3 lifecycle stubs.
Those stubs verify exact `argc`/`argv` and process IDs. A deterministic
test-only handshake makes desktop exit 71 and compositor exit 0 after both
have been observed. Kernel observers require both children to have PID1 as
their parent, valid address spaces and exact names, then verify both are
reaped, the supervisor emits exactly `graphical-init status=00`, and PID1
enters `wait2(NULL, 0, 1000)`. The reusable production image and temporary
stage hashes are checked before and after. Use `FROG_QEMU_KEEP=1` to retain a
passing log and its exact stage manifest. The focused stubs are not production
applications; `frogfs-exec-smoke` separately proves exact production
compositor and desktop ELF loading.

QEMU's IDE hard-disk frontend rejects a directly read-only block node. The
runner therefore attaches the reusable `build/frog-root.img` as the read-only
base of a temporary snapshot overlay, stores that overlay only under the
profile work directory, and requires the base SHA-256 to match before and
after. This does not claim that the guest IDE block device itself is hard
read-only; guest mutation denial is enforced by the FrogFS on-disk flag.
Normal `make run`, `make debug_run`, and `make debug_runv1` use the same
`snapshot=on` rule for `build/frog-root.img`. QEMU-test recipes keep their
ordinary `hd80M.img` drive unless the selected profile explicitly stages a
different image. `tools/test-qemu-runtime-drives.sh`, called by `CI.sh`, checks
the expanded drive variables and all three normal recipes through the
recipe-free `FORCE` target.

`./scripts/qemu-test.sh frogfs-exec-smoke` uses that same generated image at
16 MiB. The guest mounts `/dev/sdbp1` at `/test`, proves both production ELF
files are larger than 4 KiB, and sequentially executes the exact installed
`/test/bin/compositor` and `/test/bin/desktop` files with `--exec-smoke`. Their
expected exit statuses and the parent `wait` results feed the machine-readable
`FROGTEST` result, with a path-specific case name on failure. This profile
passes at 16 MiB without loading disk `/sbin/init` or switching the root. No
alternate test executable is built.

`./scripts/qemu-test.sh poudland-e2e-smoke` runs an explicit legacy-path
compositor built from the production compositor sources, the production
`desktop.c` ELF, and the test-only Poudland harness from a private copy of
`build/frog-test-root.img` at 16 MiB. That image is assembled from the base
manifest and `config/frog-test.overlay`; the separately named compositor keeps
the legacy `/test/b.bmp` asset path out of the production ELF.
`frogfs-image-smoke` and `frogfs-exec-smoke` continue to use only
`build/frog-root.img`. A multi-page
guest harness exercises HELLO, create/close, ownership,
geometry and protocol errors, the 16-window session limit, the 64-window server
limit, and disconnect cleanup. It then starts the real desktop, proves the two
remaining IDs are live and client-owned, and emits a synchronization marker.
The host polls P6 screenshots through a private QMP socket until the exact
two-window frame is present, quits QEMU, and validates every pixel including
the bitmap cursor's alpha blend. A stale third window or any old test-client
window prevents PASS. Set `FROG_QEMU_KEEP=1` to retain the successful
screenshot and transcript as evidence; failures retain them automatically.

`./scripts/qemu-test.sh desktop-smoke` is the complete production-chain
graphical interaction acceptance test. At 16 MiB it builds a complete
temporary `frog-root` image with byte-identical snapshots of the production
System Init and Graphical Init, exact production configuration and cursor, and
test-enabled builds from the real compositor and `desktop.c` sources at
`/bin/compositor` and `/bin/desktop`. It then runs Root Locator, read-only Root
Switch, disk PID1, strict mode selection, and Graphical Init before checking
both child `fork`/`execv` paths. Normal application binaries are rebuilt
without `FROGTEST` instrumentation before the guest starts, and the reusable
production image hash must remain unchanged.

The host injects a left-button press, qcode `a`, relative movement `(40, 25)`,
and button release through QMP. The guest must emit the complete ordered PASS
set for production root readiness, init/fork/exec identity, bind, handshake,
three creates, third close, two live windows, focus, keyboard routing,
drag/configure, client observation, final frame, one-second idle-present
stability, and live compositor/desktop process state. Only after the liveness
case passes may the guest emit `desktop-idle-stable`. The host then validates
every pixel of the 1024x768 frame, including the white focus border and cursor
alpha blend at `(270, 235)`.

The final classifier rescans PANIC, ASSERT, triple fault, any guest FAIL, and
unexpected System/Graphical Init status after QMP completion. The diagnostic
overrides `FROG_QEMU_DESKTOP_DRAG_X`, `FROG_QEMU_DESKTOP_DROP_CASE`,
`FROG_QEMU_DESKTOP_WRONG_PIXEL=1`,
`FROG_QEMU_DESKTOP_STALE_IMAGE=1`, and
`FROG_QEMU_DESKTOP_POST_READY_FAIL=1` support distinct negative
classifications; they are unset in a normal acceptance run.

`./scripts/qemu-test.sh desktop-soak-10m` uses the same complete temporary
production-root stage image and fixed QMP interaction in a separate 16 MiB
profile. After the fast desktop checks pass, the compositor validates the
complete final scene once per minute for ten minutes. The kernel requires both
compositor and desktop to remain live before the soak snapshot, every
heartbeat, and final verification. Each checkpoint also requires unchanged
frame and presented-pixel counts. The final checkpoint compares the user-page
allocator and live packagefs service/session/endpoint/queue counters with
snapshots taken at the start of the soak. The canonical result records 10
heartbeats, 600 seconds, 1024x768, and stable liveness/state/resources; set
`FROG_QEMU_KEEP=1` to retain the debugcon log, exact PPM screenshot, QMP
transcript, and stage disk. The default deadline is 660 seconds.
`FROG_QEMU_SOAK_WATCHDOG_TEST=1` deliberately fails the host wait after
`desktop-soak-start` so the bounded watchdog and retained failure artifacts
can be checked without waiting ten minutes.

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
including `poudland-e2e-smoke`, `desktop-smoke`, and `desktop-soak-10m`,
default to 16 MiB. Set `FROG_QEMU_MEMORY` to a positive integer with an `M` or
`G` suffix, for example
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
