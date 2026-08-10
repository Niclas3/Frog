# Implementation Plan: Production FrogFS Root Filesystem

Status: Complete. Task 5.3 and the final checkpoint passed on 2026-08-10.

## Overview

Migrate Frog from the verified Poudland P0 arrangement, where a generated
FrogFS disk is mounted at `/test`, to a deterministic read-only FrogFS System
Image mounted as `/`. Normal boot will locate exactly one volume named
`frog-root`, stage it at `/sysroot`, preserve the existing devfs/packagefs
mount subtree across a one-time Root Switch, load `/sbin/init` from disk, and
start Poudland from production paths.

This file is the accepted implementation plan and evidence ledger. Current
behavior remains documented in
`doc/graphical-startup-image-design.md` and `doc/frogfs-host-image.md`; the
accepted target is `doc/directory-structure.md` and ADRs 0004 through 0007.

## Scope

Included:

- behavior-preserving rename of the current directory-only `tmpfs` backend to
  `rootfs`;
- FrogFS manifest Version 2 with explicit directories and volume name;
- separate production manifest and non-overriding test overlay;
- safe block-device enumeration and `frog-root` discovery;
- read-only staging mount at `/sysroot` and one-time Root Switch;
- preservation of the live devfs subtree, including packagefs;
- loading PID1 from `/sbin/init` and strict graphical-mode selection;
- migration to `/bin/compositor`, `/bin/desktop`, and
  `/share/poudland/cursor.bmp`;
- bounded host and QEMU validation, including negative startup cases and the
  existing desktop smoke and soak outcomes.

Deferred:

- a true data-bearing tmpfs and `/tmp` mount;
- writable `/home` or `/var` volumes;
- general unmount, `pivot_root`, mount propagation, or namespace cloning;
- boot-command-line parsing and label/UUID overrides;
- combined boot/system CompactFlash image or installer;
- ownership/group metadata, symlinks, package management, or writable root;
- TTY Init until a real TTY path exists.

## Dependency Graph

```text
documented baseline
       |
       +-- rootfs naming cleanup
       |
       +-- manifest v2 --> production + test manifests --> host checkpoint
       |                         |
       +-- block iteration ------+--> root locator
       |                                  |
       +-- VFS Root Switch + devfs move --+--> namespace checkpoint
                                                   |
ELF loader reuse --> disk PID1 --> System Init --> graphical path migration
                                                   |
                 read-only/failure automation -----+--> production checkpoint
                                                           |
                      desktop smoke --> ten-minute soak --> final checkpoint
```

QEMU profiles must run serially because they share generated build locations.
Every profile uses bounded deadlines, structured `FROGTEST` evidence, and
disposable writable disks where mutation is intentional. A compile-only result
is never treated as runtime evidence.

## Phase 0: Preserve the Baseline

### Task 0.1: Record and verify the pre-migration contract

**Work:** Keep Poudland P0 documentation and its `/test` runtime evidence as the
known-good baseline. Add only links from historical documents to this plan.

**Likely files:** `doc/graphical-startup-image-design.md`,
`doc/frogfs-host-image.md`, `doc/poudland-p0-handoff.md`, `tasks/plan.md`,
`tasks/todo.md`.

**Acceptance:** A reader can distinguish implemented P0 behavior from the
accepted production-root target; unrelated `booter/Makefile` changes remain
untouched.

**Verification:** `git diff --check`; inspect only Markdown changes.

**Dependencies:** None.

## Phase 1: Name the Bootstrap Filesystem Correctly

### Task 1.1: Rename the directory-only backend from `tmpfs` to `rootfs`

**Status:** Completed 2026-08-09. The behavior-preserving source migration
uses `core/fs/rootfs/rootfs.c` and `rootfs.o`; the public `root_fs_init` API,
registered filesystem type, and numeric magic value are unchanged.

**Work:** Perform a behavior-preserving file, symbol, include, Makefile, test
marker, and documentation rename. Do not add file storage or true tmpfs
semantics in this task.

**Likely files:** `core/fs/tmpfs/`, `core/fs/Makefile`, VFS initialization and
filesystem regression sources, current operational documentation.

**Acceptance:** The same Bootstrap Root mounts and passes the same tests under
the `rootfs` name; no `tmpfs` name remains for this backend; no filesystem
behavior changes.

**Verification:** `./scripts/CI.sh`, the existing focused filesystem QEMU
profile, marker-name audit, `git diff --check`.

**Dependencies:** Task 0.1.

## Phase 2: Produce the Target Namespace Deterministically

### Task 2.1: Add manifest Version 2 directory and volume records

**Status:** Completed 2026-08-09. The host builder accepts only Version 2,
requires one valid volume name and explicit parent directories, writes the
volume name plus read-only flag to the superblock, and preserves deterministic
ordering and current file/ELF behavior.

**Work:** Teach the host builder to parse `volume frog-root` and explicit `dir`
entries. Require every non-root parent, reject duplicates and path/type
conflicts, and retain current fixed modes and ELF validation. Keep Version 1
handling only if an existing test or migration checkpoint needs it; remove it
once all tracked manifests have moved.

**Likely files:** `tools/mkfrogfs_image.c`, `tools/test-mkfrogfs-image.sh`,
manifest-format documentation, tracked manifest fixtures.

**Acceptance:** Host tests cover valid nested directories, missing parents,
duplicate/path conflicts, invalid or duplicate volume records, the exact
`frog-root` superblock value, deterministic line-order handling, unchanged
reuse, corruption detection, and atomic publication.

**Verification:** host builder tests, two independent builds with equal hashes,
`make frog-root-verify`, `git diff --check`.

**Dependencies:** Task 0.1.

### Task 2.2: Split production contents from test-only overlays

**Status:** Completed 2026-08-10. The production manifest has the exact
accepted initial tree, including the strict init input and standalone init
artifacts. A non-overriding disposable overlay supplies `/poudland-e2e` and
temporary legacy P0 root-path aliases only; host inventory, repeated-build,
and base-image reuse checks prove they cannot enter or mutate production.

**Work:** Define the production manifest for the exact tree in
`doc/directory-structure.md`. Compose test images from production contents plus
a separate overlay; reject any overlay entry that replaces a production path.
Remove `/poudland-e2e` from production contents.

**Likely files:** `config/` manifests and configuration inputs, application
Makefiles, root-image Makefiles, builder tests.

**Acceptance:** The production image contains exactly the declared production
tree; overlay images add only test paths; base production hash and mtime remain
unchanged when inputs are unchanged; collision attempts fail.

**Verification:** host manifest inventory test, overlay conflict tests,
deterministic hash/reuse test, `make frog-root-verify`.

**Dependencies:** Task 2.1.

### Checkpoint A: Host-built System Image

The deterministic image has explicit directories, volume name `frog-root`, no
test-only programs, and a verified non-overriding overlay mechanism. The guest
may still boot using the old `/test` path at this checkpoint.

**Status:** Completed 2026-08-10 by host-only evidence. This is not Root
Switch, disk PID1, or production compositor runtime evidence.

## Phase 3: Establish Root Discovery and Namespace Transition

### Task 3.1: Add bounded block-device enumeration for root discovery

**Status:** Completed 2026-08-10. The block registry exposes a counted,
startup-only partition callback without exposing its list or whole-disk
objects. FrogFS mount and discovery share one authoritative superblock probe.
The locator returns typed found, missing, duplicate, target-corrupt, and
unreadable results; it refuses to claim a unique root when an unreadable
partition or damaged `frog-root` candidate leaves that claim indeterminate.

**Work:** Add a small callback or iterator API that lets startup inspect
registered block partitions without exposing the internal block-device list.
Probe candidate FrogFS superblocks safely and return exactly one device whose
volume name is `frog-root`.

**Likely files:** block core/header, FrogFS metadata helper, focused kernel
regression code.

**Acceptance:** Zero, one, and multiple matching partitions are distinct
results; unreadable/non-FrogFS devices are not selected; enumeration has clear
locking/lifetime rules and does not leak registry internals.

**Verification:** `./scripts/qemu-test.sh root-locator-smoke` passes the
partitions-only, positive, missing-root, duplicate-root, bad-magic,
structural-corruption, unrelated-corruption, and unreadable cases using
boot-local synthetic block devices; `./scripts/CI.sh` supplies the normal
compile check.

**Dependencies:** Checkpoint A.

### Task 3.2: Add a one-time VFS Root Switch primitive

**Status:** Completed 2026-08-10. `vfs_switch_root_once()` performs a
zero-allocation, one-time transition under the VFS namespace lock. It
normalizes the staged mount as `/`, rebinds the existing devfs mount entry to
the new `/dev`, leaves the nested packagefs entry unchanged, and intentionally
pins the retired Bootstrap Root mount. Task 3.3 now calls this primitive from
the production activation policy.

**Work:** Introduce the narrow startup operation needed to replace the global
root with the mounted `/sysroot` root and associate the existing `/dev` mount
subtree with the new root. Define ownership and rollback before changing global
state. Do not implement a general unmount or `pivot_root` API.

**Likely files:** VFS mount/root structures and headers, devfs/packagefs mount
integration, focused filesystem regression code.

**Acceptance:** On success, `/` refers to the staged FrogFS root and `/dev`, its
nested packagefs mount, and existing device objects retain identity and
readiness. `/sysroot` and the old Bootstrap Root are unreachable. Every failure
before the commit point leaves the old namespace usable; no fallible operation
occurs after the commit point unless startup stops safely.

**Verification:** `./scripts/qemu-test.sh root-switch-smoke` passes at 16 MiB
with the real generated FrogFS image staged at `/sysroot`. It covers every
actual pre-commit failure checkpoint, mount-entry/superblock/root identity,
queued packagefs C2S plus post-switch S2C traffic through the original open
files, `/dev/input/event0` identity/open/read behavior, unreachable old paths,
and the `-EALREADY` second-call contract. `./scripts/CI.sh` supplies the normal
compile check.

**Dependencies:** Task 1.1.

### Task 3.3: Separate FrogFS registration from boot mount policy

**Status:** Completed 2026-08-10. `frogfs_init()` now registers only the
filesystem type. Writable profiles explicitly create `/test`; production uses
the exact registry-owned block device returned by `frogfs_locate_root()`,
stages it at `/sysroot`, validates source identity and the on-disk read-only
flag, switches root once, and hands the switched namespace to Task 4.1's
disk-PID1 loader.

**Work:** Keep filesystem-type initialization independent of choosing a mount
point. Normal startup creates `/sysroot`, mounts the located device read-only,
validates mount state, preserves devfs, and invokes the Root Switch. Focused
filesystem tests retain disposable writable mounts at `/test`.

**Likely files:** FrogFS initialization, kernel startup, mount flags/contracts,
filesystem QEMU smoke setup.

**Acceptance:** Normal startup contains no `/dev/sdbp1` root assumption and no
production `/test` mount. Mutation tests can still mount writable disposable
FrogFS at `/test`. The production mount rejects create/write/truncate/unlink
with `-EROFS`.

**Verification:** `root-namespace-smoke` passes at 16 MiB and rejects existing
write, truncate, create, mkdir, unlink, and rmdir with `-EROFS`. The runner
opens the reusable image as the read-only base of a QEMU temporary snapshot
overlay because QEMU's IDE hard-disk frontend rejects a directly read-only
node; its SHA-256 is identical before and after. The same profile proves that
mkdir and mount failures unregister FrogFS and remove only the staging
directory created by the failed attempt. `disk-smoke` remains the writable
`/test` regression, and `./scripts/CI.sh` supplies the compile check.

**Dependencies:** Tasks 3.1 and 3.2.

### Checkpoint B: Production Root Namespace

**Status:** Completed 2026-08-10 by `root-namespace-smoke` at 16 MiB.

Frog reaches the switched read-only Root Filesystem by reserved volume name,
and `/dev/input/event0`, `/dev/input/event1`, `/dev/fb0`, and packagefs still
work. A dedicated embedded smoke init may validate this checkpoint. Task 4.1
now proves disk-backed PID1 publication. At Checkpoint B alone graphical
startup was still unclaimed; Phases 4 and 5 now provide that complete evidence.

## Phase 4: Load and Select Init from the System Image

### Task 4.1: Reuse the ELF loader for a disk-backed initial process

**Status:** Completed 2026-08-10. Normal non-test startup now loads the
production `/sbin/init` through a trusted kernel-owned VFS-path interface,
publishes it as numeric PID1, records it as the init process, and exits the
Bootstrap main thread. There is no embedded fallback on this path.

**Work:** Factor the trusted-kernel entry needed to load an initial ELF by VFS
path while reusing the production ELF mapping and validation logic. Do not
duplicate the loader or route a kernel pointer through the user-copy syscall
entry. Keep existing profile-specific embedded init paths until migrated tests
have equivalent evidence.

**Likely files:** process creation, exec loader/header, startup call site,
focused process tests.

**Acceptance:** The kernel can create PID1 from `/sbin/init`; ordinary `execv`
behavior is unchanged; malformed, missing, oversized, or unmappable init files
fail explicitly; the old one-page embedded limit is not applied to the disk
ELF.

**Verification:** `disk-init-loader-smoke` passes at 16 MiB. It starts from the
real Root Locator and production read-only Root Switch, proves the installed
`/sbin/init` is larger than 4 KiB, and checks PID1 name, parent, address space,
entry, stack, and two kernel-provided arguments before scheduling it. Missing,
malformed, oversized, unmappable, argument-allocation, partial-map, and
post-PID-swap/pre-publication failures leave the Bootstrap main thread as
PID1, publish no user address space, release the temporary PID for immediate
reuse, and preserve root/devfs/packagefs identity and readiness. The runner
uses a temporary non-overriding fixture overlay and verifies its read-only
base hash before and after. `process-smoke`, `frogfs-exec-smoke`,
`root-namespace-smoke`, `boot-smoke`, and `./scripts/CI.sh` provide regression
and compile evidence.

**Dependencies:** Checkpoint B.

### Task 4.2: Implement strict System Init selection

**Status:** Completed 2026-08-10. The exact production `/sbin/init` is now
scheduled from the production-like read-only root as numeric PID1, strictly
selects graphical mode, and replaces its image without changing PID. Stable
failure status and the permanent `wait2` stop loop are guest-verified for all
accepted rejection classes. Task 4.2 alone did not claim child launch; the now
completed Task 4.3 and Phase 5 provide that evidence.

**Work:** Add `/sbin/init` as a small user program that parses only the accepted
`mode=graphical` configuration and replaces itself with
`/sbin/init-graphical`. Preserve PID1. Reserve the interface for a future
`init.mode` Boot Override without implementing a fake kernel command line.

**Likely files:** user init sources, application Makefiles,
`/etc/frog/init.conf` source, production manifest.

**Acceptance:** Valid graphical configuration execs Graphical Init. Missing,
malformed, duplicate, unknown, unsupported TTY, and exec-failure cases report a
stable failure and stop. No fallback selects graphical mode.

**Verification:** `tools/test-system-init.sh` covers the parser and operation
contract on the host. `system-init-selection-smoke` runs 12 bounded stages at
16 MiB: valid, missing config, malformed, duplicate, unknown, unsupported TTY,
open/read/close failure, selected-init missing/corrupt, and the otherwise
impossible returned-exec case. The valid ring-3 replacement target verifies
PID1, parent, argv, replaced image identity, root, devfs, and packagefs. Every
failure emits the exact status before the same PID enters
`wait2(NULL, 0, 1000)`. Temporary complete images and the reusable production
image must retain their hashes; non-injection stages execute the exact
production System Init ELF.

**Dependencies:** Tasks 2.2 and 4.1.

### Task 4.3: Package Graphical Init and migrate Poudland paths

**Status:** Completed 2026-08-10. Production Graphical Init now supervises
`/bin/compositor` and `/bin/desktop`, and the production compositor loads
`/share/poudland/cursor.bmp`. Legacy embedded P0 and graphical QEMU profiles
request `/test/compositor`, `/test/desktop`, and `/test/b.bmp` explicitly and
build a separately named legacy compositor; those aliases remain confined to
the disposable test overlay.

**Work:** Package the already proved graphical supervisor as
`/sbin/init-graphical`; change it to supervise `/bin/compositor` and
`/bin/desktop`; change the compositor asset path to
`/share/poudland/cursor.bmp`. Preserve existing exit/status, child reap, and
packagefs disconnect behavior.

**Likely files:** graphical init, compositor, manifest inputs, current startup
documentation.

**Acceptance:** No production program refers to `/test/compositor`,
`/test/desktop`, or `/test/b.bmp`; lifecycle behavior and status codes match
Poudland P0; all artifacts are loaded from the Root Filesystem.

**Verification:** `tools/test-production-graphical-paths.sh` audits the three
production ELFs for the exact final paths and rejects the four legacy path
strings. `graphical-init-production-smoke` passes at 16 MiB through the real
Root Locator, read-only Root Switch, exact production System Init, and exact
production Graphical Init. Its complete temporary System Image replaces only
the two application ELFs with ring-3 lifecycle stubs. The stubs verify
`argc`/`argv` and process identity; the kernel observes PID1 parentage, child
parentage, names, address spaces, both reap results, the exact
`graphical-init status=00\n` line, and the final `wait2(NULL, 0, 1000)` stop
loop. The production and stage image hashes remain unchanged. Separate
`frogfs-exec-smoke` evidence executes the exact production compositor and
desktop ELFs. Legacy `poudland-builtin-smoke` and `poudland-e2e-smoke`, plus
the production-chain `desktop-smoke`, `frogfs-image-smoke`,
`system-init-selection-smoke`, `disk-init-loader-smoke`, `boot-smoke`, host
tests, normal-artifact audits, and `./scripts/CI.sh`, all pass after the
migration.

**Dependencies:** Task 4.2.

### Checkpoint C: Disk-loaded Production Startup

Normal boot locates and switches to `frog-root`, starts `/sbin/init`, selects
Graphical Init from strict configuration, and launches compositor and desktop
from production paths. Missing/corrupt root and init failures stop explicitly.

**Status:** Completed. `desktop-smoke` now runs the shared Root Locator,
read-only Root Switch, disk PID1 loader, exact production `/sbin/init`, and
exact production `/sbin/init-graphical` before launching instrumented ELFs
built from the real compositor and desktop sources at their production paths.
The retained 16 MiB PASS at
`build/qemu-test/20260810T150906Z-desktop-smoke-PASS-695854/` contains the
complete stage image and manifest, both application ELFs and hashes, and
byte-identical production init snapshots. The retained real-disk
missing/corrupt/duplicate PASS is
`build/qemu-test/20260810T150712Z-production-root-negative-smoke-PASS-679409/`;
all three stages call the shared startup wrapper, return their exact typed
locator status, prove the Root Switch did not occur, and preserve every
recorded disk hash. Writable-root, loader, and init-selection failures remain
bounded and classified by their serial profiles.

## Phase 5: Migrate Automated Acceptance

### Task 5.1: Separate low-level FrogFS tests from root-boot profiles

**Status:** Completed. Writable mutation tests remain on disposable `/test`
media. Production-root profiles use QEMU snapshots or complete disposable
stage images and verify both reusable-base and stage hashes. The
`root-namespace-smoke` `writable-root` stage changes only the superblock
read-only bit, then requires exact `NOT_READ_ONLY`/`-EROFS` rejection and
proves that the Root Switch did not occur. The current retained bounded PASS is
`build/qemu-test/20260810T150654Z-root-namespace-smoke-PASS-677706/`; the
current missing/corrupt/duplicate disk boundary evidence is
`build/qemu-test/20260810T150712Z-production-root-negative-smoke-PASS-679409/`.

**Work:** Keep create/write/reopen/unlink and partition-boundary tests on a
disposable writable filesystem mounted at `/test`. Move installed-program and
startup tests to the real production root path. Make normal QEMU attach the
base System Image read-only; mutation profiles receive copies.

**Likely files:** `scripts/qemu-test.sh`, test-profile init selection, image
helpers, `scripts/README.md`.

**Acceptance:** Each profile states whether `/test` or the production Root
Filesystem is under test; writable cases cannot modify the reusable base;
normal boot fails if a write to the root succeeds.

**Verification:** serial execution of focused FrogFS mutation, root-read-only,
installed-exec, and negative root-location profiles; compare base hashes before
and after each run.

**Dependencies:** Checkpoint C.

### Task 5.2: Run the desktop acceptance path through the real root

**Status:** Completed. `desktop-smoke` and `desktop-soak-10m` use the same
production startup chain and final filesystem paths. Their complete temporary
System Image replaces only the compositor and desktop ELFs with test-enabled
builds from those same sources; neither init executable is replaced. The
current desktop PASS is
`build/qemu-test/20260810T150906Z-desktop-smoke-PASS-695854/`. It requires
both compositor and desktop to remain live at idle before emitting the final
ready marker. The final retained soak PASS at
`build/qemu-test/20260810T152529Z-desktop-soak-10m-PASS-697950/` records ten
ordered liveness-gated heartbeats, final process liveness, stable state and
resources, the exact 1024x768 framebuffer, and unchanged production and stage
hashes for 600 seconds, all at 16 MiB.

**Work:** Adapt the non-overriding test overlay and guest markers so
`desktop-smoke` and `desktop-soak-10m` exercise the same Root Locator, Root
Switch, System Init, Graphical Init, compositor, desktop, input, and packagefs
path as normal boot.

**Likely files:** QEMU runner/profile logic, guest test reporting, test overlay
manifest, operational test documentation.

**Acceptance:** The existing exact framebuffer, focus, key, drag, close,
two-live-window, idle-stability, cleanup, and ten-minute stability contracts
still pass under 16 MiB. No test substitutes for disk init or the real
compositor/desktop. Negative profiles distinguish guest failure from host/QMP
infrastructure failure.

**Verification:** run the relevant profiles serially, including
`desktop-smoke` and `desktop-soak-10m`; inspect structured result artifacts and
preserved failure logs; verify the reusable image hash.

**Dependencies:** Task 5.1.

### Task 5.3: Synchronize operational documentation and handoff

**Status:** Completed 2026-08-10. Current-state and historical documents are
synchronized, the durable implementation handoff records the exact commands
and retained evidence, `./scripts/CI.sh` passes, and a fresh normal
`QEMU_TEST=0` build contains no test-only kernel symbols, markers, or legacy
`/test` paths. The normal System Image still hashes to
`f937af9017cc2ee964c298253ab6363640a90e0e7eab59a8d66e18e3f49a20e8`.

**Work:** Only after runtime acceptance, update current-state docs and canonical
commands, retire obsolete `/test` production wording, and write the requested
implementation handoff with the exact working-tree state, commands, markers,
results, and remaining risks. Record a commit only if one actually exists.

**Likely files:** `doc/frogfs-host-image.md`,
`doc/graphical-startup-image-design.md`, `doc/poudland-p0-handoff.md`,
`scripts/README.md`, task checklists, and
`doc/root-filesystem-implementation-handoff.md`.

**Acceptance:** Every current-state claim matches the checkout and fresh
evidence; historical P0 evidence remains identifiable; no generated image,
binary, screenshot, or transient log is tracked.

**Verification:** documentation link/path audit, `git diff --check`, final
worktree review, and rerun of the canonical acceptance command if documentation
changes it.

**Dependencies:** Task 5.2.

### Final Checkpoint

**Status:** Completed 2026-08-10. All implementation, 16 MiB runtime,
documentation, generated-artifact, CI, and normal-build isolation gates below
have passed. The work remains uncommitted; no commit SHA is claimed.

The production Root Filesystem contract is complete only when all of these are
true:

- a clean build deterministically produces a read-only `frog-root` System
  Image with the exact documented tree;
- normal boot selects it without relying on disk enumeration order;
- `/dev` and nested packagefs remain live across the Root Switch;
- `/sbin/init` and `/sbin/init-graphical` are loaded from disk and Poudland uses
  only production paths;
- missing, duplicate, corrupt, writable-root, and init-selection failures are
  bounded and machine-classified;
- focused writable FrogFS tests still run at `/test` on disposable media;
- desktop smoke and the separate ten-minute soak pass at 16 MiB through the
  full production chain;
- current-state documentation and the implementation handoff cite fresh
  commands and evidence.

## Risks and Controls

| Risk | Control |
| --- | --- |
| Root Switch leaves dangling dentries or mounts. | Define ownership, commit point, and rollback first; add focused identity and failure-injection tests. |
| Recreating devfs loses open devices or packagefs state. | Move/reattach the existing mount subtree; verify object identity and readiness. |
| Root discovery exposes or races the block registry. | Use a bounded callback/iterator with explicit locking and lifetime rules. |
| Initial-process loading forks ELF behavior. | Reuse the existing loader behind an internal trusted-path entry. |
| A read-only filesystem is only policy, not enforcement. | Enforce in VFS/filesystem operations and open the normal QEMU backend read-only. |
| Tests accidentally validate `/test` instead of `/`. | Split profile roles and require production-path markers for startup/e2e tests. |
| Documentation describes future behavior as current. | Keep target docs marked deferred and update operational docs only after their checkpoint passes. |
| QEMU runs interfere through shared build output. | Run profiles serially with unique disposable work directories and bounded cleanup. |
| The 16 MiB target regresses. | Keep 16 MiB on installed-exec, desktop smoke, and soak acceptance paths. |

## Deferred Decisions

There are no open decisions blocking this plan. Future work will separately
decide the boot-override transport and UUID format, true tmpfs storage and
quota semantics, writable volume discovery/mount policy, TTY Init behavior,
and a combined CompactFlash partition/install layout.
