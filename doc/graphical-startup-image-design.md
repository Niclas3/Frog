# Graphical Startup and FrogFS Image Design

Status: Historical Poudland P0 contract; superseded for normal startup

This document is the historical P0 `/test` contract. Task 4.3 of
`tasks/root-filesystem-plan.md` has since moved production defaults to
`/bin/compositor`, `/bin/desktop`, and `/share/poudland/cursor.bmp` and proved
the exact disk-loaded init lifecycle. Explicit legacy P0 profiles still use
the paths recorded below; they no longer define production defaults.

Frog boots the Poudland Server and demo client as ordinary user ELF files installed in a reproducible FrogFS disk image. A graphical test does not rely on fixed raw sectors, kernel-linked application execution, or a guest preparation boot.

## Reusable FrogFS Disk

The build produces an 80 MiB sparse MBR disk image with one primary partition spanning the usable disk. Frog publishes it as `/dev/sdbp1` in the initial two-disk QEMU configuration and formats that partition as FrogFS.

The temporary P0 installation is:

```text
/test/compositor
/test/desktop
/test/b.bmp
```

At the P0 checkpoint the kernel mounted `/dev/sdbp1` at `/test`. The then-future
root-filesystem milestone is now complete: normal startup locates `frog-root`,
switches it to `/`, preserves devfs at `/dev`, and uses the production paths
documented in `doc/root-filesystem-implementation-handoff.md`.

## Host Image Builder

A tracked host tool and manifest implement the equivalent of:

```sh
make frog-root.img
```

The target:

1. creates a sparse 80 MiB image;
2. writes a deterministic DOS/MBR partition table with one primary partition;
3. formats that partition with the supported FrogFS on-disk format;
4. installs the freshly built compositor ELF, desktop ELF, and cursor bitmap;
5. reads the resulting directory, inode, indirect-table, bitmap, and file data
   back before publication;
6. consumes a versioned tracked manifest containing installed image paths,
   source paths, sizes, and content hashes.

The tool rejects an artifact that exceeds filesystem or executable-loader limits and performs overflow-checked offset and length calculations. It writes a temporary output and publishes the final image only after every verification succeeds, so a failed rebuild cannot replace the last valid image.

The image builder, filesystem-layout definitions, and content manifest are source-controlled. The generated 80 MiB image is a build artifact and is not committed to Git.

At the P0 checkpoint, the reusable base image contained only the P0
compositor, desktop, and cursor.

The Poudland end-to-end harness is supplied through the separate
`config/frog-test.overlay` image input, so normal `frog-root.img` construction
does not build or install a test ELF. This preserves the historical `/test`
P0 mount model for explicit legacy profiles; the production image and Root
Switch path are documented separately.

The phony image target always evaluates the versioned manifest and every source
it names. An unchanged image is reused without changing its hash or mtime;
changed or mismatched input cannot silently run stale application code.
Manifest `elf` entries are checked against the same bounded ELF32/i386
`ET_EXEC` shape accepted by the kernel loader before publication. A clean image
build produces deterministic `core/apps/build/compositor` and
`core/apps/build/desktop` artifacts; both contain the test-only invocation
argument `--exec-smoke` in the production ELF rather than using separate smoke
artifacts.

## Runtime Startup

One small user-mode graphical init performed the P0 startup sequence:

1. fork and `execv("/test/compositor", ...)`;
2. fork and `execv("/test/desktop", ...)`;
3. wait for child termination and report explicit launch or runtime failure.

At P0, this init was an early embedded image selected by normal boot and the
dedicated desktop profile; its flat binary remained within the one-page fixture
limit. Explicit historical profiles still select their embedded variants.
Current normal boot and the current `desktop-smoke`/`desktop-soak-10m` path use
disk-loaded System Init and Graphical Init instead.

The shared startup status contract reserves desktop exits 70, 71, and 72 for
connect timeout, compositor EOF/HUP (`-ECONNRESET`), and other runtime or
protocol failure. Graphical init reserves 80 through 86 for compositor fork,
desktop fork, compositor exec, desktop exec, compositor runtime, desktop
runtime, and wait failure. Child exec failure exits with the matching reserved
status, allowing the parent to distinguish it after `wait`. A compositor exit
status of zero paired with desktop HUP is normal cleanup regardless of child
reap order. Any other desktop status is a desktop runtime failure. The parent
does not restart or kill either child and reaps every child it successfully
started, including when desktop exits first.

The compositor makes that final reap finite without adding a kill syscall or a
test-only control path. It treats five seconds without any completed HELLO as a
startup failure. After at least one completed HELLO, it keeps all welcomed
clients alive and exits successfully only after the last welcomed peer's
packagefs DISCONNECT record has been drained. A desktop fork or exec failure
therefore ends through the compositor startup deadline; a desktop runtime
failure closes its packagefs fd, and the resulting disconnect ends the
compositor only when no other welcomed client remains.

The embedded process is the kernel's PID 1 and must never invoke `SYS_EXIT`.
After both children have been reaped, it emits `graphical-init status=NN`
through the descriptor-free `SYS_PUTC` console/debug channel and remains
resident in a one-second `wait2(count = 0)` loop. Normal `make run` captures
that channel in `build/frog-run-debugcon.log`; the debug targets use matching
`frog-debug*-debugcon.log` files. The 80--86 result is therefore visible even
though PID 1 starts with no open file descriptors, without turning a completed
or failed graphical session into the kernel's `init process must not exit`
panic.

Desktop connection tolerates the compositor bind race. `poudland_connect` retries only `-ENOENT` for at most 2000 milliseconds using finite `wait2(count = 0)` sleeps. Other errors return immediately.

P0 does not automatically restart a failed child. If the Poudland Server exits, packagefs closes its service endpoint; the desktop observes EOF/HUP, the client library returns `-ECONNRESET`, and desktop releases its local state and exits.

At P0, normal boot initialized FrogFS and mounted `/dev/sdbp1` at `/test`.
Current normal boot uses the Root Locator and Root Switch; focused historical,
generated-image, and exec profiles may still use fixed disposable `/test`
mounts without claiming the production namespace.

## Poudland Client Runtime

The client library owns the packagefs fd inside a caller-supplied `struct poudland_client`. Its public operations return zero on success or a negative Frog errno on failure. It owns request-ID allocation, matches responses, queues intervening asynchronous events, and uses `wait2` rather than a queue-size ioctl or busy polling.

The initial calls cover connect, window create, window close, next event, and disconnect. Callers do not pass a redundant fd or store it in a pointer field.

## Automated Use

`frogfs-exec-smoke` is the focused installed-ELF launch proof. At 16 MiB it mounts a
private copy of the generated image, proves both ELF files exceed the old raw
one-page fixture limit, and sequentially validates `fork`, exact-path `execv`,
and `wait` status for both installed production artifacts.

The current `desktop-smoke` is still the full graphical acceptance path, but
its startup boundary has moved beyond this historical P0 design. It builds a
complete temporary `frog-root` image with exact production init ELFs and
test-enabled variants of the same compositor and `desktop.c` sources at their
production `/bin` paths. It then exercises Root Locator, Root Switch, disk PID1,
strict selection, and Graphical Init before the unchanged interaction scenario.
Only the disposable application variants contain test reporting; normal ELFs
remain free of it. QMP injects focus, key, drag, and release events only after
guest synchronization points. The final 1024x768 screenshot remains a
pixel-for-pixel check of stacking, focus border, and cursor blending.

The host requires every positive state record exactly once, validates causal
order and process liveness, and rescans late guest failures after QMP
completion. It records both reusable production and complete stage-image
SHA-256 values before and after QEMU. Wrong drag coordinates, missing state,
post-ready FAIL, a stale image, and a deliberately wrong pixel have distinct
negative gates.

Every generated-image run gives only a disposable disk to QEMU. This is test
isolation, not a guest preparation phase.

`make run` builds and verifies `build/frog-root.img`, copies the immutable
`../hd.img` template to `build/frog-boot.img`, freshly assembles the MBR and
loader through writable temporary files, and burns only the MBR, loader,
kernel, and font into that private boot disk. It selects the 1024x768x32
framebuffer loader and launches QEMU with 16 MiB, `-vga std`, and the generated
FrogFS image as a snapshot-backed second IDE disk. Normal boot therefore neither mutates a
stale checkout `hd.img` nor silently reuses an MBR older than `boot.inc`, and it
no longer depends on fixed raw sectors for compositor or bitmap assets. Debug
run targets retain their larger memory and debugging settings but use the same
private boot disk, framebuffer loader, and generated second disk. Existing
QEMU smoke profiles keep their current compile-time init selection, disposable
data disk rules, and `FROGTEST` text; the production graphical init contains no
`SYS_TEST_REPORT` or `FROGTEST` reporting.

The host runner preserves a failing disposable disk, debugcon log, QEMU log, QMP transcript, screenshot, and normalized `result.json`. It never mutates the reusable base image.

## Root Filesystem Follow-up

After desktop P0 passes, the recorded root-filesystem stabilization work audits the current FrogFS/VFS implementation, adopts stable root paths, and makes the same image builder produce the production root layout. This follow-up may change mount and install paths but must not change the ordinary file-read and ELF-exec model proven here.

That follow-up is complete through Phase 5: rootfs naming, manifest Version 2,
Root Locator, read-only Root Switch, disk-loaded System Init, production
graphical paths, real-disk negative startup, desktop smoke, and the ten-minute
soak all pass. See `doc/directory-structure.md` for the current namespace,
`doc/boot-process.md` for the startup transition, and
`doc/root-filesystem-implementation-handoff.md` for current commands and
evidence. This document remains the authoritative historical record of the P0
`/test` path, not the current production path contract.
