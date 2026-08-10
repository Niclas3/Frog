# Frog Root Filesystem Directory Structure

Status: Implemented and production-chain acceptance complete on 2026-08-10

This document defines the first production Root Filesystem layout and startup
contract for Frog. Normal non-test startup now reaches the read-only System
Image at `/`, preserves `/dev`, loads `/sbin/init` from disk as numeric PID1,
and exits the Bootstrap main thread. Production Graphical Init execution and
the final compositor, desktop, and cursor paths are proved by the complete
desktop smoke and soak chain. Older Poudland P0 QEMU profiles retain explicit
`/test` fixtures and embedded init variants only as isolated historical
regressions.

The terms in this document follow `CONTEXT.md`. The decisions are recorded in
ADRs 0004 through 0007 under `docs/adr/`. The detailed startup order and its
commit/failure boundary are specified in `doc/boot-process.md`.

## Image Roles

Frog keeps these responsibilities separate:

| Item | Responsibility | Initial policy |
| --- | --- | --- |
| Boot Image | Loads the bootloader and kernel. | Separate from the System Image. |
| Bootstrap Root | Supplies the temporary namespace needed to discover storage and devices. | Directory-only in-memory `rootfs`; never the production root. |
| System Image | Contains system programs, configuration, and shared resources. | Deterministic FrogFS volume named `frog-root`; mounted read-only. |
| Root Filesystem | The namespace visible at `/` after the Root Switch. | Backed initially by the System Image, with devfs reattached at `/dev`. |
| Test Image Overlay | Adds test-only files to a disposable copy of production image contents. | Never changes or overrides production manifest entries. |

The Boot Image and System Image remain separate artifacts for this milestone.
A later CompactFlash layout may contain both, but combining their storage does
not combine their responsibilities.

## Historical P0 Baseline and Current Production Contract

| Concern | Verified Poudland P0 baseline | Current production contract |
| --- | --- | --- |
| Root namespace | Directory-only in-memory filesystem. | Read-only FrogFS System Image at `/`. |
| System-image location | Fixed `/dev/sdbp1`. | Exactly one registered FrogFS partition named `frog-root`. |
| Image mount | `/test`. | `/sysroot` while staging, then `/` after Root Switch. |
| First userspace program | Embedded graphical init. | Disk-loaded `/sbin/init`. |
| Graphical programs | `/test/compositor`, `/test/desktop`. | `/bin/compositor`, `/bin/desktop`. |
| Cursor asset | `/test/b.bmp`. | `/share/poudland/cursor.bmp`. |
| Device namespace | devfs mounted below the current root. | The same devfs mount subtree is preserved at `/dev`. |

Every production-contract row above is implemented. The P0 column remains only
to explain explicit legacy-profile overrides; it is not an alternate normal
startup path.

## Initial Production Tree

The first System Image contains only directories with an immediate consumer:

```text
/
├── bin/
│   ├── compositor
│   └── desktop
├── dev/
├── etc/
│   └── frog/
│       └── init.conf
├── sbin/
│   ├── init
│   └── init-graphical
└── share/
    └── poudland/
        └── cursor.bmp
```

The paths have these responsibilities:

| Path | Responsibility |
| --- | --- |
| `/bin` | User programs required by the selected system mode. |
| `/dev` | Mount point for the preserved devfs subtree, including `/dev/pkg`. The directory stored in the image is only a mount point. |
| `/etc/frog` | Strict system configuration owned by Frog. |
| `/sbin` | System startup and mode-specific init programs. |
| `/share/poudland` | Architecture-independent Poudland resources. |

`/sysroot` belongs only to the Bootstrap Root and is not present in the System
Image. `/test` remains a mount point used by focused filesystem tests, not a
production install prefix and not part of the first production image.

`/home`, `/var`, `/tmp`, and `/usr` are also absent from the first image. When
real consumers exist, `/home` and `/var` will default to separately writable
FrogFS volumes. `/tmp` will use a future bounded tmpfs. The current
directory-only `core/fs/rootfs` implementation is the Bootstrap Root backend.
It is not a data-bearing tmpfs.

## Root Discovery and Switch

The normal startup sequence is:

```text
bootloader and kernel
        |
directory-only Bootstrap Root
        |
initialize devfs, packagefs, and block devices
        |
find exactly one FrogFS volume named "frog-root"
        |
mount it read-only at /sysroot
        |
validate mount metadata and read-only state
        |
preserve the existing devfs mount subtree at new-root /dev
        |
one-time Root Switch
        |
load /sbin/init from the Root Filesystem
```

The Root Locator uses the on-disk FrogFS volume name, not ATA discovery order
or a path such as `/dev/sdbp1`. Zero matches and multiple matches are both
fatal. A future boot configuration may override the locator by label or UUID,
but that is outside this milestone.

Validation before the switch establishes that FrogFS metadata is acceptable
and the mount is read-only. It does not pre-open or prevalidate every program.
ELF validation and file errors remain part of normal `exec` behavior after the
switch.

The existing devfs mount object and all of its nested mounts, including
packagefs, must remain the same live subtree after the Root Switch. Frog does
not destroy and recreate devices during this transition. Until complete
unmount support exists, the old Bootstrap Root may remain pinned but must be
unreachable from the new namespace.

Any discovery, mount, validation, mount-preservation, Root Switch, or initial
exec failure is reported and stops startup. Normal boot does not fall back to
the embedded graphical init.

## Init Contract

After the Root Switch, the kernel starts exactly `/sbin/init`. System Init
strictly reads:

```ini
mode=graphical
```

from `/etc/frog/init.conf`. It then replaces itself with
`/sbin/init-graphical`, preserving the PID1 role. Graphical Init supervises
`/bin/compositor` and `/bin/desktop` using the lifecycle contract already
proved by Poudland P0.

Missing configuration, malformed syntax, unknown fields, an unsupported mode,
or failure to execute the selected init is fatal. TTY is a reserved future
startup mode and must fail explicitly until a real `/sbin/init-tty` exists.
A future `init.mode` Boot Override may take precedence over the file, but an
invalid override will still fail rather than silently selecting graphical
mode.

The strict parser and standalone `/sbin/init` plus `/sbin/init-graphical`
artifacts are host-verified and packaged. The kernel now loads `/sbin/init`
through the shared production ELF machinery and publishes it as PID1. The
focused selection profile schedules the exact production System Init and
proves strict selection, in-place replacement, numeric PID1 preservation, and
every stable failure-stop class. `graphical-init-production-smoke` continues
through the exact production Graphical Init, proves both production child
paths, reap results, status output, and the final PID1 stop loop. It replaces
only the two application ELFs with lifecycle stubs; exact production
application ELF execution remains separately covered by `frogfs-exec-smoke`.

## System Image Manifest Version 2

The accepted manifest update makes the namespace explicit. Its intended shape
is:

```text
frogfs-manifest 2
volume frog-root
dir /bin
dir /dev
dir /etc
dir /etc/frog
dir /sbin
dir /share
dir /share/poudland
elf /bin/compositor <source> <size> <sha256>
elf /bin/desktop <source> <size> <sha256>
file /etc/frog/init.conf <source> <size> <sha256>
elf /sbin/init <source> <size> <sha256>
elf /sbin/init-graphical <source> <size> <sha256>
file /share/poudland/cursor.bmp <source> <size> <sha256>
```

The exact source paths, byte counts, and hashes are generated from tracked
inputs. Version 2 has these rules:

- every non-root parent directory is declared with `dir` before publication;
- file and ELF entries do not create parents implicitly;
- the manifest declares the reserved volume name `frog-root`;
- the builder keeps the current fixed directory/file/ELF modes instead of
  adding owner, group, or general permission syntax;
- duplicate paths, missing parents, unsupported records, path conflicts, or a
  production/test-overlay collision fail the build;
- normalized entry ordering and fixed metadata keep output deterministic;
- the production manifest never contains `/poudland-e2e` or other test-only
  programs.

The host builder implements the Version 2 parser, explicit-directory contract,
`frog-root` volume name, read-only superblock, exact production manifest, and
non-overriding overlay mechanism. `config/frog-test.overlay` installs only the
test harness and explicit legacy P0 root-path aliases in a disposable image;
the production manifest has none of those paths. The runtime Root Switch and
production path migration are implemented; `doc/frogfs-host-image.md` is the
authoritative description of the host format and image validation.

## Test Layout

Tests currently have four explicit roles:

1. Focused FrogFS/VFS mutation tests mount a disposable writable FrogFS copy at
   `/test`. They do not treat `/test` as the system root.
2. Production-root startup profiles boot through the real Root Locator and
   Root Switch and use the production paths in this document. The exact
   installed application ELFs are also executed by the separate
   `frogfs-exec-smoke` path-alignment profile.
3. `desktop-smoke` and `desktop-soak-10m` build a complete temporary
   `frog-root` image, retain byte-identical production System Init and
   Graphical Init ELFs, install test-enabled builds from the real compositor
   and desktop sources at `/bin`, and exercise the complete production chain.
4. Historical P0 profiles such as `poudland-builtin-smoke` and
   `poudland-e2e-smoke` retain explicit legacy `/test` overrides. They do not
   define or validate normal startup.

Test-only executables and fixtures are supplied by a non-overriding Test Image
Overlay to a disposable image. The reusable production image remains
byte-for-byte unchanged and is opened read-only by normal QEMU runs. Tests that
need mutation receive writable disposable copies.

## Scope Boundaries

This milestone includes deterministic System Image construction, root-volume
discovery, a one-time Root Switch, devfs preservation, disk-loaded System Init,
and migration of the existing Poudland startup paths.

It does not include a general `pivot_root`, a general unmount facility, package
management, filesystem ownership metadata, symlinks, writable root operation,
a true tmpfs, persistent user state, boot-command-line parsing, UUID-based root
selection, or a combined CompactFlash installer image.

## Acceptance Summary

The target is complete: host tests prove manifest Version 2 and determinism,
and bounded QEMU profiles prove root discovery, read-only enforcement, Root
Switch, preserved input/packagefs devices, disk-loaded init, production-path
compositor/desktop startup, explicit failure modes, desktop smoke, and the
ten-minute soak. Current evidence and resumption instructions are in
`doc/root-filesystem-implementation-handoff.md`; the detailed gate ledger is
in `tasks/root-filesystem-plan.md` and `tasks/root-filesystem-todo.md`.
