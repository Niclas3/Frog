---
status: accepted
---

# Switch root and preserve the mounted device namespace

Frog will mount the FrogFS System Image at `/sysroot` while using the Bootstrap Root, validate only the filesystem metadata and read-only state required by that mount, then perform one Root Switch before starting PID1. The existing directory-only in-memory backend is registered and implemented as `rootfs` because it implements this Bootstrap Root; the name `tmpfs` is reserved for a future, bounded temporary file storage implementation. The existing devfs mount subtree, including its nested packagefs mount, will be associated with `/dev` in the new Root Filesystem instead of being destroyed and initialized again; the old Bootstrap Root will become unreachable and may remain pinned until Frog has complete unmount support. System Init and mode-specific programs will be loaded normally after the switch rather than being prevalidated by the kernel.

## Implemented primitive

Task 3.2 implements the decision as the single deep VFS interface
`vfs_switch_root_once()`. The interface accepts no mount objects or paths: it
consumes the fixed startup contract that a root is already mounted at
`/sysroot`, that the staged root contains an unmounted `/dev`, and that the
current `/dev` and nested `/dev/pkg` mounts are live.

All lookup and topology validation happens under the namespace lock before any
persistent rewrite. The operation allocates no memory. Its locked commit is an
infallible pointer, flag, and list transition whose final publication of the
new global root is the linearization point. A second call returns `-EALREADY`
before attempting to resolve the now-unreachable `/sysroot`.

Commit clears the old `/sysroot` and `/dev` mount flags, sets the new `/dev`
mount flag, rebinds only the existing devfs mount entry, and leaves the nested
packagefs mount entry, superblock, mounted root, open files, queues, and
sessions unchanged. The staged mount entry becomes the canonical root entry;
its mounted root has name `/`, is its own parent, and is also the superblock
mount point. The old root entry is removed from the active mount list and held
by a private retired-root pointer; no object is freed.

The focused `root-switch-smoke` profile proves this primitive with the
generated FrogFS image at 16 MiB. Normal startup now invokes it through the
typed production activation policy, and `root-namespace-smoke` proves the
complete Checkpoint B namespace. Task 4.1 subsequently added
`disk-init-loader-smoke`, which carries the same production namespace through
atomic disk-backed PID1 publication while leaving System Init execution to its
own checkpoint.
