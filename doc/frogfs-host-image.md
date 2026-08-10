# Deterministic FrogFS host image

Status: Implemented and full production-chain acceptance complete on 2026-08-10

This document describes the implemented host-only manifest Version 2 System
Image. Version 2 supplies the exact production tree, a named read-only System
Image, deterministic host construction, and a separate test overlay. The
production Root Switch, read-only activation, disk-backed PID1 loading,
strict System Init selection, Graphical Init production paths, and complete
desktop smoke/soak chain are implemented. Current evidence is documented in
`doc/root-filesystem-implementation-handoff.md`; the namespace and startup
contracts remain in `doc/directory-structure.md`, `doc/boot-process.md`, and
`tasks/root-filesystem-plan.md`.

`make frog-root.img` creates `build/frog-root.img` without a guest prepare
boot and without using `hd80M.img` as a template. The output is an 80 MiB
sparse MBR disk with one type-`0x83` primary partition:

- disk sectors: 163840
- partition: LBA 2048 through 163839 (161792 sectors)
- FrogFS zone size: 1024 bytes
- inode count: 4096
- root inode: 0
- volume name: `frog-root`
- superblock read-only flag: set
- allocation order: sorted manifest paths, then namespace/data order
- timestamps, uid/gid, unused bytes, and disk signature: fixed values

## Manifest

`config/frog-root.manifest` is whitespace-delimited:

```text
frogfs-manifest 2
volume frog-root
dir /image
file /image/path source/path/relative/to/manifest size sha256
elf /program/path source/path/relative/to/manifest size sha256
```

The first valid record must be `frogfs-manifest 2`. Exactly one `volume` record
names the image; names are 1--15 ASCII bytes, start with an alphanumeric byte,
and continue with alphanumerics, `.`, `_`, or `-`. A `dir` record has exactly
two fields. Root `/` is implicit and must not be declared.

Destinations are normalized absolute paths within the image. Every non-root
parent must have its own explicit `dir` record; `file` and `elf` records never
create a parent implicitly. Duplicate targets and directory/file type conflicts
are rejected. Records are sorted by target before inode or zone allocation, so
valid record order cannot change the image. The production manifest stores
`/bin/compositor`, `/bin/desktop`, and `/share/poudland/cursor.bmp`; after the
Root Switch those are their guest paths. A focused disposable test image may
instead be mounted at `/test`: its production entries then
resolve under `/test/bin` and `/test/share`, while only its overlay supplies
the temporary legacy aliases `/test/compositor`, `/test/desktop`, and
`/test/b.bmp`. Each component is at most 15 bytes, matching the current FrogFS
directory-entry ABI. Sources must be relative to the manifest, regular files,
must match the declared byte size and SHA-256, and must not exceed 1059840
bytes. `file` entries retain that behavior unchanged. An `elf` entry is also
validated against the production loader contract in
`core/kernel/thread/exec.c`: ELF32 little endian, `ET_EXEC`, i386, bounded and
non-overlapping `PT_LOAD` segments in the user range, a valid executable entry
point, and at most 4096 mapped pages. Unsupported dynamic, interpreter, and TLS
program headers are rejected before image publication.

## Test overlays

`mkfrogfs_image` accepts one optional overlay:

```sh
mkfrogfs_image --manifest BASE --overlay TEST_OVERLAY --output IMAGE [--verify]
```

An overlay begins with `frogfs-overlay 1` and then permits only Version 2
`dir`, `file`, and `elf` records. It has no `volume` record; its sources are
relative to the overlay file. An overlay can install below a directory already
declared by the base, but every new parent is explicit. The merged namespace
rejects any duplicate target or type conflict, including an attempt to replace
a base path.

`config/frog-root.manifest` contains exactly `/bin`, `/dev`, `/etc/frog`,
`/sbin`, and `/share/poudland` plus the programs, configuration, and cursor
listed in `doc/directory-structure.md`. `make frog-root.img` first rebuilds
the production compositor and desktop plus the standalone installed init
artifacts, then runs the exact-inventory host check before publication or
verification.

`config/frog-test.overlay` contains only test content: `/poudland-e2e` and the
explicit legacy P0 aliases `/compositor`, `/desktop`, and `/b.bmp`. Only
legacy profiles request those paths; they are never part of the production
image. `make frog-test-root.img`
creates a separate image from the same base manifest plus that non-overriding
overlay. `make frog-root-test` asserts that repeated base construction and all
overlay construction/verification leave the base image hash and mtime unchanged.

## Host-only installed init artifacts

`config/init.conf` is a tracked exact `mode=graphical` input. The explicit
`make -C core/user QEMU_TEST=0 FROG_TEST_PROFILE= installed-init` target builds
standalone `core/build/init.elf` and `core/build/init-graphical.elf`. System
Init parses by explicit byte length, accepts only that exact configuration, and
reports distinct malformed, unsupported-mode, open, read, close, and exec
failures before stopping. The installed Graphical Init artifact uses
`/bin/compositor` and `/bin/desktop`; embedded P0 artifacts request their
legacy `/test` paths explicitly at build time.

These artifacts and logic are host-verified and packaged into the System Image.
The Root Switch, production-path reads, loading `/sbin/init` through the
shared ELF loader, strict mode selection, and production Graphical Init
lifecycle are guest-verified. The lifecycle profile uses focused child stubs;
the exact production compositor and desktop ELFs are separately executed by
the installed-ELF profile.

## Publication and reuse

The builder writes a unique temporary file beside the requested output. The
FrogFS superblock is written only after all namespace, data, inode, and bitmap
content is complete and synced. Before the atomic rename commit point, a
failure removes the temporary file and leaves the previous image untouched.
After rename, a parent-directory sync failure is reported as a durability
warning while the command remains successful; the new visible image is already
complete and cannot be safely rolled back.

The builder reconstructs the expected bytes on every forced invocation. If
they already match the output, the temporary file is discarded, preserving the
existing image's content hash and mtime. `--verify` performs the same comparison
without publishing.

## Validation

```sh
make frog-root-test
make frog-root.img
make frog-root-verify
make frog-test-root.img
make frog-test-root-verify
# Guest path alignment at the temporary /test mount; not Root Switch evidence:
./scripts/qemu-test.sh frogfs-image-smoke
./scripts/qemu-test.sh frogfs-exec-smoke
# Production Root Locator + read-only mount + Root Switch evidence:
./scripts/qemu-test.sh root-namespace-smoke
# Real staged missing/corrupt/duplicate root disks through the shared wrapper:
./scripts/qemu-test.sh production-root-negative-smoke
# The same production chain through disk-backed PID1 publication:
./scripts/qemu-test.sh disk-init-loader-smoke
# Exact production init chain and Graphical Init lifecycle at 16 MiB:
FROG_QEMU_KEEP=1 ./scripts/qemu-test.sh graphical-init-production-smoke
# Complete production-chain interaction and separate ten-minute stability:
FROG_QEMU_KEEP=1 ./scripts/qemu-test.sh desktop-smoke
FROG_QEMU_KEEP=1 ./scripts/qemu-test.sh desktop-soak-10m
```

The host test covers explicit directory hierarchy and order independence,
volume and read-only-superblock verification, invalid/duplicate volume records,
missing parents, duplicate/type conflicts, malformed directory records,
deterministic reuse, corruption detection and repair, wrong source hashes,
invalid executable metadata, oversized input rejection, MBR geometry, and
atomic failure behavior. It also covers overlay source resolution, added paths,
base/overlay collisions, overlay duplicate and parent failures, forbidden
overlay headers/volume records, duplicate CLI overlays, and base-image
immutability.

The two guest path-alignment commands pass at 16 MiB using a temporary `/test`
mount of the production image. The image profile validates
`/test/share/poudland/cursor.bmp`; the exec profile proves that both
`/test/bin/compositor` and `/test/bin/desktop` are multi-page ELF files and
checks their `fork`/`execv`/`wait` paths. This is guest-path-alignment evidence,
not Root Switch or disk-loaded `/sbin/init` evidence. The separate
`root-namespace-smoke` command proves the Root Switch and reads
`/bin/compositor` plus `/sbin/init`, but deliberately does not execute PID1.
`disk-init-loader-smoke` then proves the installed multi-page `/sbin/init` ELF
is mapped with its entry, stack, and kernel-provided arguments and atomically
published as numeric PID1. It covers missing, malformed, oversized,
unmappable, allocation, partial-map, and post-PID-swap/pre-publication
rollback without scheduling System Init. After the Root Switch, the
corresponding production paths are `/share/poudland/cursor.bmp`,
`/bin/compositor`, and `/bin/desktop`. Any still-unmigrated P0 profile must use
only the disposable overlay aliases, never infer that those aliases exist in
the production manifest.

`graphical-init-production-smoke` uses a complete temporary `frog-root` image
containing byte-identical production `init.elf` and `init-graphical.elf` files.
Only `/bin/compositor` and `/bin/desktop` are focused lifecycle stubs. The
profile proves both final exec paths, child identity and parentage, both reap
statuses, exact `graphical-init status=00` output, and the permanent PID1
`wait2(NULL, 0, 1000)` loop. Both temporary and reusable image hashes are
checked before and after; the reusable production image validated for this
task has SHA-256
`f937af9017cc2ee964c298253ab6363640a90e0e7eab59a8d66e18e3f49a20e8`.

Normal `make run`, `make debug_run`, and `make debug_runv1` attach that
reusable image as a QEMU temporary snapshot base because the IDE frontend does
not accept a directly read-only hard-disk node. The guest enforces the FrogFS
read-only flag, while the host verifies the reusable base remains unchanged.
The final desktop smoke and soak use a complete temporary `frog-root` image
with byte-identical production init ELFs and test-enabled builds from the real
application sources; their exact hashes and retained result paths are in
`doc/root-filesystem-implementation-handoff.md`.
