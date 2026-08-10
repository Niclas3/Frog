# Production Root Filesystem Implementation Handoff

Status: Implemented, runtime-verified, and final gates complete on 2026-08-10

This is the current handoff for Frog's production Root Filesystem milestone.
It supersedes the operational parts of the historical Poudland P0 handoff but
does not rewrite that history. The implementation described here is present in
the current working tree and has not been assigned a commit hash.

## Delivered Outcome

Normal startup no longer treats `/test` or a fixed device name as the system
root. Frog now:

1. boots with the directory-only in-memory Bootstrap Root provided by
   `rootfs`;
2. initializes devfs, nested packagefs, and block devices;
3. locates exactly one structurally valid FrogFS partition whose volume name
   is `frog-root`;
4. mounts it read-only at `/sysroot` and verifies the selected block source and
   on-disk read-only flag;
5. performs one Root Switch while preserving the existing `/dev` mount subtree
   and nested packagefs state;
6. loads `/sbin/init` through the production ELF loader as numeric PID1;
7. strictly reads `/etc/frog/init.conf`, replaces PID1 with
   `/sbin/init-graphical`, and supervises `/bin/compositor` plus
   `/bin/desktop`;
8. loads the compositor cursor from `/share/poudland/cursor.bmp`.

The reusable System Image is deterministic, named, read-only, and separate
from the Boot Image. Writable filesystem tests still use disposable FrogFS
media at `/test`; `/test` is not a production install prefix.

## Final Startup Flow

```text
MBR and loader -> kernel
        |
directory-only Bootstrap Root (`rootfs`)
        |
devfs at /dev -> packagefs at /dev/pkg -> ATA partitions published
        |
frogfs_locate_root(): exactly one valid `frog-root`
        |
frogfs_activate_root(): register -> mkdir /sysroot -> read-only mount
        |
vfs_switch_root_once(): preserve the existing /dev subtree and commit /
        |
process_execute_init_path("/sbin/init", ...): publish numeric PID1
        |
/sbin/init reads exact `mode=graphical`
        |
exec /sbin/init-graphical without changing PID1
        |
fork/exec /bin/compositor and /bin/desktop
        |
Poudland service, input loop, 1024x768 composition, child supervision
```

`disk_system_init_start()` in `core/init/main.c` is the shared transaction for
Root Locator, root activation, and disk PID1 loading. Normal startup and the
production-chain tests use this wrapper; the negative root profile does not
reimplement the startup path.

## What Each Phase Delivered

| Phase | Result |
| --- | --- |
| 0 | Preserved the historical Poudland P0 `/test` baseline and separated historical claims from the production target. |
| 1 | Renamed the directory-only `tmpfs` backend to `rootfs` without changing its behavior or public `root_fs_init` entry. |
| 2 | Added FrogFS manifest Version 2, explicit directories, volume `frog-root`, a read-only superblock flag, deterministic publication/reuse, an exact production inventory, and a non-overriding test overlay. |
| 3 | Added bounded partition enumeration, typed Root Locator results, read-only activation at `/sysroot`, and a one-time Root Switch preserving devfs/packagefs identities and open state. |
| 4 | Reused the production ELF loader for disk PID1, added strict System Init selection, packaged Graphical Init, and migrated compositor, desktop, and cursor to production paths. |
| 5 | Split writable `/test` tests from root-boot profiles, added real-disk missing/corrupt/duplicate and writable-root rejection, moved desktop smoke/soak through the complete production chain, added process liveness gates and final host rescans, and synchronized operations documentation. |

## Key Interfaces and Failure Contracts

### Root Locator

`frogfs_locate_root()` returns `struct frogfs_root_result`. Its typed statuses
are:

- `FROGFS_ROOT_FOUND`: exactly one structurally valid `frog-root`; `bdev` is
  non-null and remains owned by the block registry for the boot;
- `FROGFS_ROOT_NOT_FOUND`: no valid matching volume;
- `FROGFS_ROOT_DUPLICATE`: at least two valid matching volumes;
- `FROGFS_ROOT_CORRUPT`: a readable candidate retains the target label but its
  FrogFS structure is invalid;
- `FROGFS_ROOT_UNREADABLE`: an unreadable partition prevents a unique-root
  claim unless duplication is already definitive.

The locator enumerates published partitions, not whole disks, and never picks
the first FrogFS volume or relies on `/dev/sdbp1`.

### Root Activation and Switch

`frogfs_activate_root()` returns `struct frogfs_root_activation` with the
selected block device, mounted superblock, primary `error`, and separate
`cleanup_error`. Status values distinguish bad locator input, FrogFS
registration, `/sysroot` creation, mount, source mismatch, missing read-only
policy, Root Switch failure, and success.

`vfs_switch_root_once()` performs all fallible lookup and validation before a
zero-allocation locked commit. Success returns 0; a second call returns
`-EALREADY`. The old Bootstrap Root remains intentionally pinned because Frog
does not yet provide general unmount, but it and `/sysroot` are unreachable by
pathname after the switch.

### Disk PID1 and Init

`process_execute_init_path()` accepts trusted kernel-owned path and argument
strings while reusing the same ELF validation, mapping, page protection, and
stack construction as `execv`. On success it publishes PID1 and returns it
through `pid_out`; on failure it publishes no user process and leaves
`pid_out == -1`.

Normal startup reports and stops at one of `root-locator`, `root-activation`,
`disk-pid1-loader`, or `boot-main-exit-returned`. System Init and Graphical Init
retain their stable numeric status output and permanent PID1 `wait2(NULL, 0,
1000)` stop loop. No failure falls back to the embedded P0 init.

## Production and Test Image Boundary

`config/frog-root.manifest` is the production inventory. It contains only:

```text
/bin/compositor
/bin/desktop
/dev
/etc/frog/init.conf
/sbin/init
/sbin/init-graphical
/share/poudland/cursor.bmp
```

plus their explicit parent directories. The image is generated as
`build/frog-root.img`; generated images are not tracked.

`config/frog-test.overlay` can only add non-production paths. The builder
rejects an overlay that replaces a production path or conflicts with its type.
Legacy P0 aliases and `/poudland-e2e` therefore exist only in disposable test
images.

`desktop-smoke` and `desktop-soak-10m` build a complete temporary
`frog-root` stage image. It uses byte-identical snapshots of the production
`/sbin/init` and `/sbin/init-graphical`, the exact production configuration and
cursor, and test-enabled builds from the real compositor and `desktop.c`
sources at `/bin/compositor` and `/bin/desktop`. This is not an embedded-init
or legacy-`/test` substitute.

## Normal QEMU Disk Semantics

The QEMU IDE frontend used here cannot attach the System Image as a directly
read-only hard-disk node. For `QEMU_TEST=0`, `make run`, `make debug_run`, and
`make debug_runv1` attach:

```text
format=raw,file=build/frog-root.img,if=ide,index=1,media=disk,snapshot=on
```

The snapshot overlay is disposable. The guest still receives a writable IDE
frontend, while FrogFS enforces the on-disk read-only flag. The reusable base
must keep the same SHA-256 before and after every production-root run.

For `QEMU_TEST=1`, ordinary test recipes retain the existing writable
`hd80M.img` data-drive semantics unless a profile supplies its own disposable
or snapshot-backed image. `tools/test-qemu-runtime-drives.sh` checks the
expanded variables and all three normal QEMU recipes without invoking a build
recipe.

## Canonical Build and Validation Commands

Run all QEMU profiles serially because they clean shared build outputs.

```sh
cd /home/zm/Development/C/Frog/src

# Host construction, deterministic reuse, corruption, and inventory.
make frog-root-test
make frog-root.img
make frog-root-verify

# Compile, normal-artifact isolation, profile-size gates, and host checks.
./scripts/CI.sh

# Focused root and startup runtime chain.
FROG_QEMU_KEEP=1 ./scripts/qemu-test.sh root-locator-smoke
FROG_QEMU_KEEP=1 ./scripts/qemu-test.sh root-switch-smoke
FROG_QEMU_KEEP=1 ./scripts/qemu-test.sh root-namespace-smoke
FROG_QEMU_KEEP=1 ./scripts/qemu-test.sh production-root-negative-smoke
FROG_QEMU_KEEP=1 ./scripts/qemu-test.sh disk-init-loader-smoke
FROG_QEMU_KEEP=1 ./scripts/qemu-test.sh system-init-selection-smoke
FROG_QEMU_KEEP=1 ./scripts/qemu-test.sh graphical-init-production-smoke

# Exact installed application ELF path alignment.
FROG_QEMU_KEEP=1 ./scripts/qemu-test.sh frogfs-image-smoke
FROG_QEMU_KEEP=1 ./scripts/qemu-test.sh frogfs-exec-smoke

# Complete production-chain graphical acceptance.
FROG_QEMU_KEEP=1 ./scripts/qemu-test.sh desktop-smoke
FROG_QEMU_KEEP=1 ./scripts/qemu-test.sh desktop-soak-10m

# Interactive normal boot; the System Image is snapshot-backed.
make run
```

`./scripts/CI.sh` is compile and host-artifact validation, not QEMU runtime
evidence. Runtime claims require the retained result directories above.

## Final Host and Normal-Build Closure

After the retained runtime evidence was reviewed, the final gate ran
`./scripts/CI.sh` successfully, rebuilt the kernel, normal compositor and
desktop, both installed init ELFs, and `frog-root.img` with `QEMU_TEST=0` and
an empty `FROG_TEST_PROFILE`, then reran image verification and inventory.

The closure audit rejected test-only kernel symbols and markers, `FROGTEST`,
desktop liveness/negative-test strings, and `/test/compositor`,
`/test/desktop`, and `/test/b.bmp` in the normal kernel and installed ELFs. It
also passed `tools/test-production-graphical-paths.sh`,
`tools/test-production-apps.sh`, `tools/test-frog-root-inventory.sh`, and
`tools/test-qemu-runtime-drives.sh`. The resulting normal artifact hashes are:

| Artifact | SHA-256 |
| --- | --- |
| `core/build/core.img` | `82a8acd6136ca832a84ac2fbe259e8402cb4aff7cddbe6c1a861383e0f91e6a1` |
| `core/build/core_symbol.img` | `a6a8702cdda411052082aa732aacc28f428062b85cba298cba93468866f613f5` |
| `core/apps/build/compositor` | `61bfaf061d0e4be320f46993022d5e10c920fa67ccc9b59b1171275ed8965e4a` |
| `core/apps/build/desktop` | `8b55d650f779174adde428e95bccb7f333866f1d49be267067334c2ab662864f` |
| `core/build/init.elf` | `ea9f2777f4654dee018bda9d9acfe152f84dbae5ea57c55d20218132fcd7a56f` |
| `core/build/init-graphical.elf` | `45583dcc7614689f827a317700488dc184ced19009ea0d9eadd36bc546aa6e1e` |
| `build/frog-root.img` | `f937af9017cc2ee964c298253ab6363640a90e0e7eab59a8d66e18e3f49a20e8` |

Documentation links, retained-evidence paths, shell syntax, trailing
whitespace, `git diff --check`, and tracked generated-artifact state were also
audited after the final status update. No additional QEMU run was needed for
this documentation-only closure.

## Machine-Readable Evidence Contract

The production desktop chain is ordered by these important records:

```text
FROGTEST CASE desktop.root-located PASS
FROGTEST CASE desktop.root-activated PASS
FROGTEST CASE desktop.disk-pid1-loaded PASS
FROGTEST SYNC desktop-production-root-ready
FROGTEST CASE desktop.production-init-chain PASS
...
FROGTEST CASE desktop.idle-present-stable PASS
FROGTEST CASE desktop.liveness.idle PASS
FROGTEST SYNC desktop-idle-stable
```

The idle and soak liveness checks require both recorded children to retain the
expected PID, PID1 parent, user address space, exact `compositor`/`desktop`
name, and a status other than `THREAD_TASK_HANGING` or `THREAD_TASK_DIED`.
Every soak heartbeat is emitted only after that check. Final soak order is:

```text
FROGTEST SYNC desktop-soak-start
FROGTEST HEARTBEAT desktop-soak minute=1
...
FROGTEST HEARTBEAT desktop-soak minute=10
FROGTEST CASE desktop-soak.state-stable PASS
FROGTEST CASE desktop-soak.liveness PASS
FROGTEST CASE desktop-soak.resources PASS
FROGTEST SYNC desktop-soak-complete
```

The host performs a final scan after QMP completion. PANIC, ASSERT, triple
fault, any `FROGTEST ... FAIL`, or unexpected System/Graphical Init status
cannot be hidden by an earlier ready marker. Important normalized
classifications include `PASS`, `PANIC`, `ASSERT_FAILED`, `TRIPLE_FAULT`,
`GUEST_TEST_FAILED`, `UNEXPECTED_INIT_STATUS`, `GUEST_STATE_MISMATCH`,
`EXPECTED_MARKER_MISSING`, `FRAMEBUFFER_MISMATCH`, `QMP_FAILED`,
`BOOT_TIMEOUT`, `EARLY_QEMU_EXIT`, and `BASE_DISK_MUTATED`.

## Final Retained Evidence

All paths are relative to this checkout's `build/qemu-test/` directory.

| Evidence | Retained directory | Result |
| --- | --- | --- |
| Production root missing/corrupt/duplicate on real staged disks | `20260810T150712Z-production-root-negative-smoke-PASS-679409/` | Three stages PASS at 16 MiB; shared startup wrapper, exact typed status, and root-not-switched markers. |
| Root namespace including writable-root rejection | `20260810T150654Z-root-namespace-smoke-PASS-677706/` | PASS at 16 MiB. |
| Final production-chain desktop smoke | `20260810T150906Z-desktop-smoke-PASS-695854/` | PASS at 16 MiB, 1024x768, exact state order, idle liveness, and framebuffer. |
| Final ten-minute production-chain soak | `20260810T152529Z-desktop-soak-10m-PASS-697950/` | PASS at 16 MiB; 600 seconds, 10 ordered heartbeats, 1024x768, stable liveness/state/resources. |

The production System Image SHA-256 in these runs is:

```text
f937af9017cc2ee964c298253ab6363640a90e0e7eab59a8d66e18e3f49a20e8
```

The complete desktop stage image SHA-256 is:

```text
f795cfa6cf2e5bb3c07f3b4eec26c98436c6feb482fbdd644fa80470278bd623
```

Both hashes are unchanged before and after the final desktop smoke and soak.
The root-negative result also records immutable per-stage hashes:

- missing primary: `30e14955ebf1352266dc2ff8067e68104607e750abb9d3b36582b8af909fcb58`;
- corrupt primary: `e7bcceaa15da516f603ade58351fe7272ade83afbb865aff9860eb0e2cfd5824`;
- duplicate primary and extra: the production image hash above.

## RED and Negative-Gate Evidence

- `20260810T145353Z-production-root-negative-smoke-EXPECTED_MARKER_MISSING-651017/`
  is the focused RED before the real shared-wrapper markers existed. The
  missing-disk guest still ran the old boot smoke and could not satisfy the
  production-root contract.
- `20260810T150111Z-desktop-smoke-GUEST_STATE_MISMATCH-662078/` is the
  liveness RED: the complete desktop reached idle, QEMU exited 0, and the host
  rejected the absent `desktop.liveness.idle` record.
- `20260810T150356Z-desktop-smoke-GUEST_STATE_MISMATCH-665009/` deliberately
  removes the liveness record from validator input and proves missing liveness
  cannot pass.
- `20260810T150421Z-desktop-smoke-GUEST_TEST_FAILED-666258/` injects a guest
  FAIL after the ready marker; the final rescan classifies it as
  `GUEST_TEST_FAILED` despite QEMU exit 0.

`20260810T150034Z-desktop-smoke-QMP_FAILED-2/` is deliberately not product
evidence. QEMU failed to bind its private Unix QMP socket with `Operation not
permitted` in the restricted sandbox. `QMP_FAILED` is an infrastructure
classification; rerunning the same profile where Unix sockets were permitted
produced the valid liveness RED and later PASS evidence.

## Repository State and Ownership

This implementation and handoff are uncommitted. Do not invent or record a
commit SHA until the owner creates one. Generated images, ELFs, screenshots,
QMP transcripts, and `build/qemu-test` results remain untracked artifacts.

`booter/Makefile` is an unrelated owner change that predates this milestone.
It was not edited, restored, staged, or otherwise claimed by this work. Future
cleanup or commit preparation must continue to preserve it.

## Risks and Explicitly Deferred Work

- The retired Bootstrap Root stays pinned; general unmount, `pivot_root`,
  mount propagation, and namespace cloning are deferred.
- `rootfs` is directory-only bootstrap storage, not a data-bearing tmpfs. A
  bounded true tmpfs and `/tmp` mount are deferred.
- Writable `/home` and `/var` FrogFS volumes, ownership/group metadata,
  symlinks, package management, and writable production root are deferred.
- Boot `root=`/label override transport, UUID selection, TTY Init, and a
  combined boot/system/install CompactFlash layout are deferred.
- Shared client Window Surface mapping/commit/damage, compositor restart,
  richer desktop-shell behavior, and useful legacy feature migration remain
  later Poudland work.
- QEMU proves the i386/16 MiB software contract, but not the timing, BIOS,
  storage, or video behavior of the first physical board.

## Next Step: M6117/386SX Physical Bring-up

The first hardware target is the 40 MHz 386SX-compatible M6117 system with
16 MiB RAM, a 1 GB CompactFlash card, RS-232, DB25 parallel/Covox, 16-bit ISA,
RTL8019AS, Yamaha YMF262-M plus Covox, and SVGA.

Bring it up conservatively:

1. capture exact chipset, Super I/O, SVGA, IDE/CF, IRQ, and BIOS details;
2. establish an interrupt-driven RS-232 diagnostic path because QEMU
   debugcon/QMP do not exist on the board;
3. validate the BIOS memory map and constrain allocation to 16 MiB;
4. measure 8259/8254 scheduling and monotonic timing; treat RTC as optional
   until its oscillator, battery, and retained state are confirmed;
5. probe the CompactFlash read-only first, confirm MBR/LBA behavior, and verify
   exactly one `frog-root` before enabling any write path;
6. identify a conservative SVGA/VBE mode and framebuffer mapping before
   attempting the full graphical chain;
7. validate PS/2 event delivery, then reproduce the serial equivalents of the
   root-located, root-activated, disk-PID1-loaded, init, compositor, and desktop
   liveness checkpoints.

See `doc/hardware-targets.md` for the full board inventory and bring-up order.
