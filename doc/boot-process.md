# Frog Production Root Boot Process

Status: Implemented and full production-chain acceptance complete on 2026-08-10

This document defines the startup transition from Frog's temporary Bootstrap
Root to its production Root Filesystem. Normal non-test startup now performs
Root Locator, read-only staging, and Root Switch, loads `/sbin/init` through
the production ELF loader as numeric PID1, records it as the init process, and
exits the Bootstrap main thread. The exact production System Init and Graphical
Init now run through that chain and use final application paths. Legacy
graphical QEMU profiles keep their documented embedded init and `/test`
fixtures only through explicit test-profile overrides.

The target directory tree is in `doc/directory-structure.md`; implementation
order and test gates are in `tasks/root-filesystem-plan.md`. ADRs 0004 through
0007 record the underlying decisions. Current commands, evidence, and resume
instructions are in `doc/root-filesystem-implementation-handoff.md`.

## Startup Invariants

The production path maintains these invariants:

- the kernel begins with a directory-only in-memory Bootstrap Root;
- the Boot Image and FrogFS System Image are separate artifacts;
- the selected System Image is exactly one registered FrogFS partition whose
  volume name is `frog-root`;
- the System Image is mounted and enforced read-only;
- the existing devfs mount subtree is preserved rather than recreated;
- `/sbin/init` is the only fixed first userspace path;
- no failure silently falls back to the embedded graphical startup;
- no process or filesystem test uses `/test` as an alias for production `/`.

## Phase 1: Establish the Bootstrap Namespace

The kernel initializes the directory-only Bootstrap Root first. The current
backend is registered as `rootfs` and implemented in `core/fs/rootfs/`. It
supports the directories needed to initialize startup mounts but is not a
general memory filesystem and does not provide `/tmp` semantics.

The kernel then initializes the existing device and storage path needed for
root discovery:

1. initialize VFS and the Bootstrap Root;
2. create and mount devfs at `/dev`;
3. establish nested device filesystems such as packagefs at `/dev/pkg`;
4. initialize block drivers and register discovered partitions;
5. register FrogFS as a filesystem type without choosing a production mount.

At the end of this phase, the Bootstrap Root is still `/`, `/dev` is live, and
no production PID1 has started.

## Phase 2: Locate the System Image

Startup enumerates registered partition devices through a bounded block-layer
interface. It examines enough FrogFS metadata to identify the on-disk volume
name while rejecting unreadable or invalid candidates.

The accepted initial Root Locator is the constant volume name `frog-root`:

- exactly one valid match proceeds;
- no match reports a missing root and stops;
- more than one match reports an ambiguous root and stops;
- block discovery order and names such as `/dev/sdbp1` do not affect selection.

This phase does not choose the first FrogFS partition and does not fall back to
an unlabeled disk. A future boot-provided label or UUID override is a separate
feature.

## Phase 3: Stage and Validate the Root

Startup creates `/sysroot` in the Bootstrap Root and mounts the selected
FrogFS partition there with a read-only mount contract. Validation is limited
to the properties needed to trust the mount transition:

- the FrogFS superblock and required root metadata are structurally valid;
- the mounted volume is the selected `frog-root` candidate;
- the mount and block path will reject mutation;
- a root directory is available for the namespace transition.

Startup does not pre-open `/sbin/init`, hash every installed program, or parse
each ELF before switching roots. Those errors belong to normal path lookup and
`exec` after the transition.

Failure in this phase unmounts or releases only objects created for staging
when the available VFS primitives permit it, leaves the Bootstrap Root as the
active namespace, reports a classified startup failure, and stops. It does not
continue into the old graphical path. In the current policy, a failed
`/sysroot` creation unregisters the newly registered FrogFS type; a failed
mount also removes the newly created empty `/sysroot` before unregistering.
The primary error and any cleanup error are reported separately. Once a mount
succeeds, later failures leave it pinned because Frog does not yet have a
general unmount primitive.

## Phase 4: Preserve `/dev` and Commit the Root Switch

Before committing, VFS prepares the same live devfs mount subtree to appear at
`/dev` beneath the staged root. This includes nested packagefs state and the
identity of already registered device objects. Reinitializing devfs is not an
acceptable substitute because it can discard open-object identity, queued
events, or packagefs state.

The Root Switch is a narrow one-time kernel operation, not a general
`pivot_root` interface. Its implementation must define:

- which root dentry, superblock, and mount references are transferred;
- how the `/dev` subtree is attached to the new namespace;
- a single commit point for replacing the global root;
- rollback for every failure before that commit point;
- which old-root references remain intentionally pinned afterward.

Task 3.2 now implements this primitive as `vfs_switch_root_once()`. It performs
all fallible lookup and validation before a zero-allocation locked commit. The
commit normalizes the staged mount as the new root, moves the same devfs mount
entry to the new `/dev`, leaves the nested packagefs entry untouched, and pins
the retired root entry privately. Publishing the new global root is the final
linearization point; a later call returns `-EALREADY` before path lookup.

Once committed:

- the staged FrogFS root is visible as `/`;
- the preserved device subtree is visible as `/dev`;
- `/sysroot` and the old Bootstrap Root are unreachable by pathname;
- the old root may remain pinned until general unmount support exists;
- startup does not attempt to roll back into the old root.

Any unexpected post-commit failure is fatal and stops startup. The transition
must not expose a half-switched namespace to userspace.

`root-switch-smoke` remains the isolated primitive test. Normal startup and
`root-namespace-smoke` now use the production activation policy: the latter
proves the exact located block object, read-only mount, switched namespace,
preserved devices/packagefs, and mutation denial at 16 MiB. Checkpoint B is
complete. The separate `disk-init-loader-smoke` profile extends that chain
through disk-loaded PID1 publication.

## Phase 5: Start PID1 from Disk

After the Root Switch, the kernel uses the existing production ELF loader to
create PID1 from `/sbin/init`. Its trusted kernel entry reuses the same ELF
validation, mapping, protection, and stack construction as `execv`; it neither
duplicates ELF parsing nor passes a kernel pointer through the user-copy
syscall entry. Initial publication atomically transfers numeric PID1 ownership
from the Bootstrap main thread to the private, fully prepared user process.
Any failure before publication releases the address space and temporary PID
and leaves the Bootstrap main thread as PID1.

System Init reads `/etc/frog/init.conf`. The first accepted configuration is:

```ini
mode=graphical
```

It replaces itself with `/sbin/init-graphical`, so the selected Graphical Init
retains PID1. Graphical Init then supervises:

```text
/bin/compositor
/bin/desktop
```

The compositor loads `/share/poudland/cursor.bmp`. Existing Poudland P0 child
status, reaping, packagefs disconnect, input, framebuffer, and idle behavior
remain unchanged by the path migration.

Missing or invalid configuration, unsupported TTY mode, missing or malformed
ELFs, mapping failure, or Graphical Init failure is reported and stops. No
embedded init or implicit graphical default masks the error.

## Failure Classification

Automation should distinguish at least these stages:

| Stage | Required failure classes |
| --- | --- |
| Root discovery | missing `frog-root`, duplicate `frog-root`, unreadable/corrupt candidate |
| Root staging | mount failure, invalid metadata, read-only enforcement failure |
| Root Switch | device-subtree preparation failure, pre-commit transition failure, post-commit fatal failure |
| Initial exec | missing `/sbin/init`, invalid ELF, address-space/process creation failure |
| Mode selection | missing/malformed config, unknown field, unsupported mode, selected-init exec failure |
| Graphical startup | compositor/desktop fork, exec, runtime, or wait failure using the existing P0 status contract |

Each automated case must have a bounded deadline and a machine-readable
`FROGTEST` PASS or classified FAIL result. A QMP/socket/host-runner failure is
reported separately from a guest startup failure.

`production-root-negative-smoke` exercises the shared
`disk_system_init_start()` transaction with real staged IDE disks. Its
`missing`, `corrupt`, and `duplicate` stages require
`FROGFS_ROOT_NOT_FOUND`, `FROGFS_ROOT_CORRUPT`, and
`FROGFS_ROOT_DUPLICATE` respectively, a null selected block device, and proof
that `/sysroot` and the production programs never became visible. Each stage
records all applicable base, primary, and extra-disk hashes before and after.

## Test Modes

Focused filesystem mutation tests continue to boot an appropriate smoke init
and mount a writable disposable FrogFS at `/test`. They validate operations on
that test filesystem and do not claim a Root Switch.

`root-namespace-smoke` uses the production root-namespace path and stops before
loading PID1. `disk-init-loader-smoke` repeats the real locator, read-only
activation, and Root Switch, then proves `/sbin/init` is larger than 4 KiB and
validates the complete unpublished-to-published PID1 state transition. Its
malformed and fault-injection fixtures exist only in a temporary,
non-overriding image overlay. The profile intentionally finishes before PID1
is scheduled. `system-init-selection-smoke` then schedules the exact production
System Init in ring 3. Its valid stage proves in-place replacement by a
test-only `/sbin/init-graphical` target while keeping PID1 and checking the
root, devfs, and packagefs. Eleven rejection stages require their exact status
line and a subsequent `wait2(NULL, 0, 1000)` call. The profile uses complete
temporary manifests because overlays cannot replace production paths, and it
checks both the stage-image and production-image hashes.

`graphical-init-production-smoke` then uses the same production chain with the
exact production `/sbin/init` and `/sbin/init-graphical`. Its complete
temporary System Image differs from production only at `/bin/compositor` and
`/bin/desktop`, where small ring-3 lifecycle stubs verify their exact argv and
process identities. A deterministic handshake makes desktop exit 71 and
compositor exit 0 only after both have been observed. The real supervisor must
reap both, emit exactly `graphical-init status=00`, and enter
`wait2(NULL, 0, 1000)`. Kernel observers verify PID1, parent/child relations,
names, address spaces, reaping, output, and the stop loop. The stage and
reusable production hashes must remain unchanged. Exact production
compositor/desktop ELF loading is independently covered by
`frogfs-exec-smoke`; the focused stubs are never production artifacts.

QEMU's IDE
hard-disk frontend rejects a directly read-only block node, so this profile
opens the reusable System Image as the read-only base of a temporary snapshot
overlay and verifies its SHA-256 before and after. The guest still enforces the
FrogFS on-disk read-only flag. A profile that needs mutation receives its own
writable copy and mounts it explicitly at `/test`.

The desktop profiles retain the 16 MiB target, exact graphical evidence, and
serial execution requirement. Both now build a complete temporary
`frog-root`, retain the exact production System Init and Graphical Init, and
run test-enabled builds from the real compositor and desktop sources at their
production paths. Before the idle-ready marker, and before every soak
heartbeat and final verification, the kernel proves that both recorded child
processes still have PID1 parentage, a user address space, their exact names,
and a live scheduler state. The final host classifier rescans panic, assertion,
triple-fault, guest FAIL, and unexpected init-status records after QMP has
finished, so an earlier ready marker cannot make a later failure green.

The final retained smoke and separate ten-minute soak pass through the real
Root Switch and disk-loaded init at 16 MiB. The soak records 10 ordered
heartbeats, 600 seconds, stable process/state/resource checks, and a 1024x768
framebuffer. Exact retained paths and hashes are in the implementation
handoff.

## Non-goals

This design does not add a general unmount API, a true tmpfs, boot command-line
parsing, UUID selection, TTY Init, writable root operation, separate mutable
volumes, or a combined CompactFlash installer. Those features must not be
smuggled into the Root Switch implementation.
