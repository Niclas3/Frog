# Deterministic FrogFS host image

`make frog-root.img` creates `build/frog-root.img` without a guest prepare
boot and without using `hd80M.img` as a template. The output is an 80 MiB
sparse MBR disk with one type-`0x83` primary partition:

- disk sectors: 163840
- partition: LBA 2048 through 163839 (161792 sectors)
- FrogFS zone size: 1024 bytes
- inode count: 4096
- root inode: 0
- allocation order: sorted manifest paths, then namespace/data order
- timestamps, uid/gid, unused bytes, and disk signature: fixed values

## Manifest

`config/frog-root.manifest` is whitespace-delimited:

```text
frogfs-manifest 1
file /image/path source/path/relative/to/manifest size sha256
elf /program/path source/path/relative/to/manifest size sha256
```

Destinations are normalized absolute paths within the image. The P0 image is
mounted at `/test`, so image path `/b.bmp` is guest path `/test/b.bmp`. Each
component is at most 15 bytes, matching the current FrogFS directory-entry ABI.
Sources must be relative to the manifest, regular files,
must match the declared byte size and SHA-256, and must not exceed 1059840
bytes. `file` entries retain that behavior unchanged. An `elf` entry is also
validated against the production loader contract in
`core/kernel/thread/exec.c`: ELF32 little endian, `ET_EXEC`, i386, bounded and
non-overlapping `PT_LOAD` segments in the user range, a valid executable entry
point, and at most 4096 mapped pages. Unsupported dynamic, interpreter, and TLS
program headers are rejected before image publication. Parent
directories are created implicitly. Entries are sorted by guest path before
inode or zone allocation, so manifest line order cannot change the image.

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
./scripts/qemu-test.sh frogfs-image-smoke
./scripts/qemu-test.sh frogfs-exec-smoke
```

The host test covers deterministic reuse, corruption detection and repair,
wrong source hashes, invalid executable metadata, oversized input rejection,
MBR geometry, and atomic failure behavior. The image profile attaches only a
private copy, mounts `/dev/sdbp1`, and reads the manifest asset byte-for-byte
through FrogFS. The exec profile runs under 16 MiB, checks that both installed
production ELFs exceed one page, then sequentially performs
`fork`/`execv`/`wait` on `/test/compositor` and `/test/desktop` with the
`--exec-smoke` argument.
