# Graphical Startup and FrogFS Image Design

Status: Accepted for the Poudland P0 runtime

Frog boots the Poudland Server and demo client as ordinary user ELF files installed in a reproducible FrogFS disk image. A graphical test does not rely on fixed raw sectors, kernel-linked application execution, or a guest preparation boot.

## Reusable FrogFS Disk

The build produces an 80 MiB sparse MBR disk image with one primary partition spanning the usable disk. Frog publishes it as `/dev/sdbp1` in the initial two-disk QEMU configuration and formats that partition as FrogFS.

The temporary P0 installation is:

```text
/test/compositor
/test/desktop
/test/b.bmp
```

The kernel mounts `/dev/sdbp1` at `/test`. The post-P0 root-filesystem stabilization milestone will mount a verified FrogFS root at `/`, retain devfs at `/dev`, and migrate executables and shared data to their documented production paths.

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

The phony image target always evaluates the versioned manifest and every source
it names. An unchanged image is reused without changing its hash or mtime;
changed or mismatched input cannot silently run stale application code.

## Runtime Startup

One small user-mode graphical init performs the production startup sequence:

1. fork and `execv("/test/compositor", ...)`;
2. fork and `execv("/test/desktop", ...)`;
3. wait for child termination and report explicit launch or runtime failure.

Desktop connection tolerates the compositor bind race. `poudland_connect` retries only `-ENOENT` for at most 2000 milliseconds using finite `wait2(count = 0)` sleeps. Other errors return immediately.

P0 does not automatically restart a failed child. If the Poudland Server exits, packagefs closes its service endpoint; the desktop observes EOF/HUP, the client library returns `-ECONNRESET`, and desktop releases its local state and exits.

## Poudland Client Runtime

The client library owns the packagefs fd inside a caller-supplied `struct poudland_client`. Its public operations return zero on success or a negative Frog errno on failure. It owns request-ID allocation, matches responses, queues intervening asynchronous events, and uses `wait2` rather than a queue-size ioctl or busy polling.

The initial calls cover connect, window create, window close, next event, and disconnect. Callers do not pass a redundant fd or store it in a pointer field.

## Automated Use

`desktop-smoke` depends on the reusable FrogFS image. Each run copies the immutable base image into its unique temporary work directory and gives only that disposable copy to QEMU. This copy is test isolation, not a guest preparation phase.

The normal run boot uses the same filesystem and exec path as production. QEMU test builds may call the existing test-report syscall from the same init, compositor, and desktop sources. The kernel converts those test-only reports into line-oriented `FROGTEST` records on port `0xe9`; the host runner captures them through `isa-debugcon`. Production builds compile out the test reports.

The host runner preserves a failing disposable disk, debugcon log, QEMU log, QMP transcript, screenshot, and normalized `result.json`. It never mutates the reusable base image.

## Root Filesystem Follow-up

After desktop P0 passes, the recorded root-filesystem stabilization work audits the current FrogFS/VFS implementation, adopts stable root paths, and makes the same image builder produce the production root layout. This follow-up may change mount and install paths but must not change the ordinary file-read and ELF-exec model proven here.
