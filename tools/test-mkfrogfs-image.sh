#!/usr/bin/env bash
set -eu

tool=${1:-./mkfrogfs_image}
work_dir=$(mktemp -d "${TMPDIR:-/tmp}/frog-mkfs-test.XXXXXX")
trap 'rm -rf "$work_dir"' EXIT HUP INT TERM

source_file="$work_dir/payload.bin"
manifest="$work_dir/manifest"
image="$work_dir/root.img"
printf 'deterministic frogfs payload\n' >"$source_file"
source_hash=$(sha256sum "$source_file" | awk '{print $1}')
source_size=$(stat -c %s "$source_file")
{
    printf 'frogfs-manifest 2\n'
    printf 'file /test/payload payload.bin %s %s\n' \
        "$source_size" "$source_hash"
    printf 'dir /test\n'
    printf 'volume frog-root\n'
} >"$manifest"

"$tool" --manifest "$manifest" --output "$image"
"$tool" --manifest "$manifest" --output "$image" --verify
python3 - "$image" <<'PY'
import sys

with open(sys.argv[1], "rb") as stream:
    stream.seek(2048 * 512)
    superblock = stream.read(512)

if superblock[4:20].split(b"\0", 1)[0] != b"frog-root":
    raise SystemExit("manifest volume was not written to the superblock")
if superblock[84] != 1:
    raise SystemExit("System Image superblock is not read-only")
PY
first_hash=$(sha256sum "$image" | awk '{print $1}')
first_mtime=$(stat -c %Y "$image")

overlay_dir="$work_dir/overlay"
overlay_manifest="$overlay_dir/frog-test.overlay"
overlay_image="$work_dir/overlay.img"
mkdir "$overlay_dir"
printf 'overlay payload\n' >"$overlay_dir/overlay.bin"
overlay_hash=$(sha256sum "$overlay_dir/overlay.bin" | awk '{print $1}')
overlay_size=$(stat -c %s "$overlay_dir/overlay.bin")
{
    printf 'frogfs-overlay 1\n'
    printf 'dir /test-overlay\n'
    printf 'file /test-overlay/overlay.bin overlay.bin %s %s\n' \
        "$overlay_size" "$overlay_hash"
} >"$overlay_manifest"
"$tool" --manifest "$manifest" --overlay "$overlay_manifest" \
    --output "$overlay_image"
"$tool" --manifest "$manifest" --overlay "$overlay_manifest" \
    --output "$overlay_image" --verify
test "$(sha256sum "$image" | awk '{print $1}')" = "$first_hash"
test "$(stat -c %Y "$image")" = "$first_mtime"

# An overlay may add a child beneath a directory explicitly declared by base.
base_parent_overlay="$overlay_dir/base-parent.overlay"
base_parent_image="$work_dir/base-parent-overlay.img"
{
    printf 'frogfs-overlay 1\n'
    printf 'file /test/overlay.bin overlay.bin %s %s\n' \
        "$overlay_size" "$overlay_hash"
} >"$base_parent_overlay"
"$tool" --manifest "$manifest" --overlay "$base_parent_overlay" \
    --output "$base_parent_image"
"$tool" --manifest "$manifest" --overlay "$base_parent_overlay" \
    --output "$base_parent_image" --verify

python3 - "$overlay_image" <<'PY'
import struct
import sys

with open(sys.argv[1], "rb") as stream:
    stream.seek(2048 * 512 + 20)
    fields = struct.unpack("<15I", stream.read(60))
    zone_size = fields[3]
    inode_table_block = fields[8]
    stream.seek(inode_table_block * zone_size)
    root = stream.read(100)
    root_zone = struct.unpack_from("<I", root, 16)[0]
    root_size = struct.unpack_from("<I", root, 8)[0]
    stream.seek(root_zone * zone_size)
    names = [stream.read(24)[:16].split(b"\0", 1)[0]
             for _ in range(root_size // 24)]
if b"test-overlay" not in names:
    raise SystemExit("overlay directory is absent from the image")
PY
sleep 1
"$tool" --manifest "$manifest" --output "$image"
test "$(sha256sum "$image" | awk '{print $1}')" = "$first_hash"

expect_rejected_manifest()
{
    local candidate=$1
    local candidate_image=$2

    if "$tool" --manifest "$candidate" --output "$candidate_image" \
            >/dev/null 2>&1; then
        echo "invalid manifest unexpectedly built: $candidate" >&2
        exit 1
    fi
    test ! -e "$candidate_image"
}

expect_rejected_overlay()
{
    local candidate=$1
    local candidate_image=$2

    if "$tool" --manifest "$manifest" --overlay "$candidate" \
            --output "$candidate_image" >/dev/null 2>&1; then
        echo "invalid overlay unexpectedly built: $candidate" >&2
        exit 1
    fi
    test ! -e "$candidate_image"
}

missing_volume_manifest="$work_dir/missing-volume.manifest"
{
    printf 'frogfs-manifest 2\n'
    printf 'dir /test\n'
} >"$missing_volume_manifest"
expect_rejected_manifest "$missing_volume_manifest" "$work_dir/missing-volume.img"

duplicate_volume_manifest="$work_dir/duplicate-volume.manifest"
{
    printf 'frogfs-manifest 2\nvolume frog-root\nvolume frog-data\n'
} >"$duplicate_volume_manifest"
expect_rejected_manifest "$duplicate_volume_manifest" "$work_dir/duplicate-volume.img"

invalid_volume_manifest="$work_dir/invalid-volume.manifest"
{
    printf 'frogfs-manifest 2\nvolume -frog-root\n'
} >"$invalid_volume_manifest"
expect_rejected_manifest "$invalid_volume_manifest" "$work_dir/invalid-volume.img"

max_volume_manifest="$work_dir/max-volume.manifest"
printf 'frogfs-manifest 2\nvolume abcdefghijklmno\n' >"$max_volume_manifest"
"$tool" --manifest "$max_volume_manifest" --output "$work_dir/max-volume.img"

long_volume_manifest="$work_dir/long-volume.manifest"
printf 'frogfs-manifest 2\nvolume abcdefghijklmnop\n' >"$long_volume_manifest"
expect_rejected_manifest "$long_volume_manifest" "$work_dir/long-volume.img"

invalid_volume_character_manifest="$work_dir/invalid-volume-character.manifest"
printf 'frogfs-manifest 2\nvolume frog/root\n' \
    >"$invalid_volume_character_manifest"
expect_rejected_manifest "$invalid_volume_character_manifest" \
    "$work_dir/invalid-volume-character.img"

header_order_manifest="$work_dir/header-order.manifest"
printf 'volume frog-root\nfrogfs-manifest 2\n' >"$header_order_manifest"
expect_rejected_manifest "$header_order_manifest" "$work_dir/header-order.img"

missing_parent_manifest="$work_dir/missing-parent.manifest"
{
    printf 'frogfs-manifest 2\nvolume frog-root\n'
    printf 'file /missing/payload payload.bin %s %s\n' \
        "$source_size" "$source_hash"
} >"$missing_parent_manifest"
expect_rejected_manifest "$missing_parent_manifest" "$work_dir/missing-parent.img"

duplicate_dir_manifest="$work_dir/duplicate-dir.manifest"
{
    printf 'frogfs-manifest 2\nvolume frog-root\ndir /same\ndir /same\n'
} >"$duplicate_dir_manifest"
expect_rejected_manifest "$duplicate_dir_manifest" "$work_dir/duplicate-dir.img"

type_conflict_manifest="$work_dir/type-conflict.manifest"
{
    printf 'frogfs-manifest 2\nvolume frog-root\ndir /same\n'
    printf 'file /same payload.bin %s %s\n' "$source_size" "$source_hash"
} >"$type_conflict_manifest"
expect_rejected_manifest "$type_conflict_manifest" "$work_dir/type-conflict.img"

invalid_dir_manifest="$work_dir/invalid-dir.manifest"
printf 'frogfs-manifest 2\nvolume frog-root\ndir /extra field\n' \
    >"$invalid_dir_manifest"
expect_rejected_manifest "$invalid_dir_manifest" "$work_dir/invalid-dir.img"

root_dir_manifest="$work_dir/root-dir.manifest"
printf 'frogfs-manifest 2\nvolume frog-root\ndir /\n' >"$root_dir_manifest"
expect_rejected_manifest "$root_dir_manifest" "$work_dir/root-dir.img"

overlay_collision="$work_dir/overlay-collision"
{
    printf 'frogfs-overlay 1\n'
    printf 'file /test/payload payload.bin %s %s\n' \
        "$source_size" "$source_hash"
} >"$overlay_collision"
expect_rejected_overlay "$overlay_collision" "$work_dir/overlay-collision.img"

overlay_duplicate="$work_dir/overlay-duplicate"
printf 'frogfs-overlay 1\ndir /overlay-duplicate\ndir /overlay-duplicate\n' \
    >"$overlay_duplicate"
expect_rejected_overlay "$overlay_duplicate" "$work_dir/overlay-duplicate.img"

overlay_missing_parent="$work_dir/overlay-missing-parent"
{
    printf 'frogfs-overlay 1\n'
    printf 'file /overlay-missing/payload overlay.bin %s %s\n' \
        "$overlay_size" "$overlay_hash"
} >"$overlay_missing_parent"
expect_rejected_overlay "$overlay_missing_parent" \
    "$work_dir/overlay-missing-parent.img"

overlay_volume="$work_dir/overlay-volume"
printf 'frogfs-overlay 1\nvolume frog-root\n' >"$overlay_volume"
expect_rejected_overlay "$overlay_volume" "$work_dir/overlay-volume.img"

overlay_manifest_header="$work_dir/overlay-manifest-header"
printf 'frogfs-manifest 2\nvolume frog-root\n' >"$overlay_manifest_header"
expect_rejected_overlay "$overlay_manifest_header" \
    "$work_dir/overlay-manifest-header.img"

if "$tool" --manifest "$manifest" --overlay "$overlay_manifest" \
        --overlay "$overlay_manifest" --output "$work_dir/double-overlay.img" \
        >/dev/null 2>&1; then
    echo 'duplicate --overlay unexpectedly accepted' >&2
    exit 1
fi
test ! -e "$work_dir/double-overlay.img"
test "$(stat -c %Y "$image")" = "$first_mtime"

printf '\001' | dd of="$image" bs=1 seek=4096 conv=notrunc status=none
if "$tool" --manifest "$manifest" --output "$image" --verify \
        >/dev/null 2>&1; then
    echo 'corrupt image unexpectedly verified' >&2
    exit 1
fi
"$tool" --manifest "$manifest" --output "$image"
test "$(sha256sum "$image" | awk '{print $1}')" = "$first_hash"

bad_manifest="$work_dir/bad-hash.manifest"
bad_image="$work_dir/bad-hash.img"
{
    printf 'frogfs-manifest 2\nvolume frog-root\ndir /test\n'
    printf 'file /test/payload payload.bin %s %064d\n' "$source_size" 0
} >"$bad_manifest"
if "$tool" --manifest "$bad_manifest" --output "$bad_image" \
        >/dev/null 2>&1; then
    echo 'bad source hash unexpectedly built' >&2
    exit 1
fi
test ! -e "$bad_image"
if "$tool" --manifest "$bad_manifest" --output "$image" \
        >/dev/null 2>&1; then
    echo 'bad source hash unexpectedly replaced existing image' >&2
    exit 1
fi
test "$(sha256sum "$image" | awk '{print $1}')" = "$first_hash"

oversized="$work_dir/oversized.bin"
oversized_manifest="$work_dir/oversized.manifest"
oversized_image="$work_dir/oversized.img"
truncate -s 1059841 "$oversized"
{
    printf 'frogfs-manifest 2\nvolume frog-root\ndir /test\n'
    printf 'file /test/oversized oversized.bin 1059841 %064d\n' 0
} >"$oversized_manifest"
if "$tool" --manifest "$oversized_manifest" --output "$oversized_image" \
        >/dev/null 2>&1; then
    echo 'oversized source unexpectedly built' >&2
    exit 1
fi
test ! -e "$oversized_image"

absolute_manifest="$work_dir/absolute.manifest"
absolute_image="$work_dir/absolute.img"
{
    printf 'frogfs-manifest 2\nvolume frog-root\n'
    printf 'file /absolute %s %s %s\n' \
        "$source_file" "$source_size" "$source_hash"
} >"$absolute_manifest"
if "$tool" --manifest "$absolute_manifest" --output "$absolute_image" \
        >/dev/null 2>&1; then
    echo 'absolute manifest source unexpectedly accepted' >&2
    exit 1
fi
test ! -e "$absolute_image"

valid_elf="$work_dir/valid.elf"
elf_manifest="$work_dir/elf.manifest"
elf_image="$work_dir/elf.img"
python3 - "$valid_elf" <<'PY'
import struct
import sys

image = bytearray(8192)
image[:16] = b"\x7fELF\x01\x01\x01" + bytes(9)
struct.pack_into("<HHIIIIIHHHHHH", image, 16,
                 2, 3, 1, 0x08048100, 52, 0, 0,
                 52, 32, 1, 0, 0, 0)
struct.pack_into("<IIIIIIII", image, 52,
                 1, 0, 0x08048000, 0x08048000,
                 len(image), len(image), 5, 4096)
image[0x100] = 0xC3
with open(sys.argv[1], "wb") as stream:
    stream.write(image)
PY
elf_hash=$(sha256sum "$valid_elf" | awk '{print $1}')
{
    printf 'frogfs-manifest 2\nvolume frog-root\n'
    printf 'elf /valid valid.elf 8192 %s\n' "$elf_hash"
} >"$elf_manifest"
"$tool" --manifest "$elf_manifest" --output "$elf_image"
"$tool" --manifest "$elf_manifest" --output "$elf_image" --verify

python3 - "$valid_elf" "$work_dir" <<'PY'
import struct
import sys

source, output = sys.argv[1:]
with open(source, "rb") as stream:
    valid = stream.read()

mutations = {
    "machine": lambda image: struct.pack_into("<H", image, 18, 62),
    "entry": lambda image: struct.pack_into("<I", image, 24, 0x08047000),
    "file-range": lambda image: struct.pack_into("<I", image, 52 + 16, 9000),
    "user-range": lambda image: struct.pack_into("<I", image, 52 + 8,
                                                   0x07000000),
    "page-limit": lambda image: struct.pack_into("<I", image, 52 + 20,
                                                   4097 * 4096),
    "dynamic": lambda image: struct.pack_into("<I", image, 52, 2),
}
for name, mutate in mutations.items():
    image = bytearray(valid)
    mutate(image)
    with open(f"{output}/invalid-{name}.elf", "wb") as stream:
        stream.write(image)
PY
for invalid_elf in "$work_dir"/invalid-*.elf; do
    invalid_name=${invalid_elf##*/}
    invalid_manifest="$work_dir/$invalid_name.manifest"
    invalid_image="$work_dir/$invalid_name.img"
    invalid_hash=$(sha256sum "$invalid_elf" | awk '{print $1}')
    {
        printf 'frogfs-manifest 2\nvolume frog-root\n'
        printf 'elf /invalid %s 8192 %s\n' "$invalid_name" "$invalid_hash"
    } >"$invalid_manifest"
    if "$tool" --manifest "$invalid_manifest" --output "$invalid_image" \
            >/dev/null 2>&1; then
        echo "$invalid_name unexpectedly accepted" >&2
        exit 1
    fi
    test ! -e "$invalid_image"
done

max_file="$work_dir/max.bin"
max_manifest="$work_dir/max.manifest"
max_image="$work_dir/max.img"
truncate -s 1059840 "$max_file"
max_hash=$(sha256sum "$max_file" | awk '{print $1}')
{
    printf 'frogfs-manifest 2\nvolume frog-root\n'
    printf 'file /max max.bin 1059840 %s\n' "$max_hash"
} >"$max_manifest"
"$tool" --manifest "$max_manifest" --output "$max_image"
"$tool" --manifest "$max_manifest" --output "$max_image" --verify

python3 - "$max_image" "$max_file" <<'PY'
import hashlib
import struct
import sys

image_path, source_path = sys.argv[1:]
with open(image_path, "rb") as stream:
    stream.seek(2048 * 512 + 20)
    fields = struct.unpack("<15I", stream.read(60))
    zone_size = fields[3]
    inode_table_block = fields[8]
    data_start_block = fields[10]
    zone_count = fields[2]
    stream.seek(inode_table_block * zone_size + 100)
    inode = stream.read(100)
    number, _mode, _padding, size = struct.unpack_from("<IHHI", inode)
    zones = struct.unpack_from("<15I", inode, 16)
    blocks = struct.unpack_from("<I", inode, 76)[0]
    if number != 1 or size != 1059840 or blocks != 1039:
        raise SystemExit("invalid maximum-size inode")
    data_blocks = list(zones[:11])
    for table_block in zones[11:15]:
        if not data_start_block <= table_block < data_start_block + zone_count:
            raise SystemExit("invalid indirect table block")
        stream.seek(table_block * zone_size)
        data_blocks.extend(struct.unpack("<256I", stream.read(zone_size)))
    if len(data_blocks) != 1035 or len(set(data_blocks)) != len(data_blocks):
        raise SystemExit("invalid maximum-size block map")
    digest = hashlib.sha256()
    for block in data_blocks:
        if not data_start_block <= block < data_start_block + zone_count:
            raise SystemExit("invalid indirect data block")
        stream.seek(block * zone_size)
        digest.update(stream.read(zone_size))
with open(source_path, "rb") as stream:
    expected = hashlib.sha256(stream.read()).digest()
if digest.digest() != expected:
    raise SystemExit("maximum-size file contents differ")
PY

many_manifest="$work_dir/many.manifest"
many_image="$work_dir/many.img"
{
    printf 'frogfs-manifest 2\nvolume frog-root\n'
    for index in $(seq -w 0 83); do
        printf 'file /many/file%s payload.bin %s %s\n' \
            "$index" "$source_size" "$source_hash"
    done
    printf 'dir /many\n'
} >"$many_manifest"
"$tool" --manifest "$many_manifest" --output "$many_image"

python3 - "$many_image" <<'PY'
import struct
import sys

with open(sys.argv[1], "rb") as stream:
    stream.seek(2048 * 512 + 20)
    fields = struct.unpack("<15I", stream.read(60))
    zone_size = fields[3]
    inode_table_block = fields[8]
    stream.seek(inode_table_block * zone_size + 100)
    inode = stream.read(100)
    size = struct.unpack_from("<I", inode, 8)[0]
    zones = struct.unpack_from("<15I", inode, 16)
    if size != 3 * 42 * 24 or not all(zones[:3]):
        raise SystemExit("directory boundary did not allocate three blocks")
    stream.seek(zones[2] * zone_size)
    entry = stream.read(24)
    name = entry[:16].split(b"\0", 1)[0]
    inode_number, file_type = struct.unpack_from("<II", entry, 16)
    if (name, inode_number, file_type) != (b"file82", 84, 5):
        raise SystemExit("directory entry did not restart at block boundary")
PY

python3 - "$image" "$source_file" <<'PY'
import hashlib
import struct
import sys

path = sys.argv[1]
source_path = sys.argv[2]
with open(path, "rb") as stream:
    mbr = stream.read(512)
    stream.seek(2048 * 512)
    superblock = stream.read(512)

if len(mbr) != 512 or mbr[510:512] != b"\x55\xaa":
    raise SystemExit("invalid MBR signature")
partition = struct.unpack_from("<B3sB3sII", mbr, 446)
if partition[0] != 0 or partition[2] != 0x83:
    raise SystemExit("invalid primary partition type")
if partition[4:] != (2048, 161792):
    raise SystemExit("invalid primary partition geometry")
if len(superblock) != 512 or struct.unpack_from("<I", superblock)[0] != 0xF206:
    raise SystemExit("invalid FrogFS superblock")

fields = struct.unpack_from("<15I", superblock, 20)
inode_count, inode_size, zone_count, zone_size = fields[:4]
imap_block, imap_blocks, zmap_block, zmap_blocks = fields[4:8]
inode_table_block, inode_table_blocks, data_start_block = fields[8:11]
root_inode, dir_entry_size, log_zone_size, max_file_size = fields[11:15]
if (inode_count, inode_size, zone_size, root_inode, dir_entry_size,
        log_zone_size, max_file_size) != (4096, 100, 1024, 0, 24, 1, 1059840):
    raise SystemExit("invalid FrogFS ABI fields")
partition_start_block = 2048 // 2
partition_end_block = partition_start_block + 161792 // 2
if (imap_block != partition_start_block + 1 or
        zmap_block != imap_block + imap_blocks or
        inode_table_block != zmap_block + zmap_blocks or
        data_start_block != inode_table_block + inode_table_blocks or
        data_start_block + zone_count != partition_end_block):
    raise SystemExit("invalid FrogFS absolute block layout")

def read_inode(stream, inode_number):
    stream.seek(inode_table_block * zone_size + inode_number * inode_size)
    raw = stream.read(inode_size)
    if len(raw) != inode_size:
        raise SystemExit("short FrogFS inode")
    number, mode, _padding, size = struct.unpack_from("<IHHI", raw)
    links = raw[12]
    zones = struct.unpack_from("<15I", raw, 16)
    blocks = struct.unpack_from("<I", raw, 76)[0]
    return number, mode, size, links, zones, blocks

def read_directory(stream, inode):
    _number, _mode, size, _links, zones, _blocks = inode
    stream.seek(zones[0] * zone_size)
    raw = stream.read(size)
    entries = []
    for offset in range(0, size, dir_entry_size):
        name = raw[offset:offset + 16].split(b"\0", 1)[0]
        number, file_type = struct.unpack_from("<II", raw, offset + 16)
        entries.append((name, number, file_type))
    return entries

with open(path, "rb") as stream:
    root = read_inode(stream, 0)
    test_dir = read_inode(stream, 1)
    payload = read_inode(stream, 2)
    if root[:4] != (0, (3 << 11) | 0o755, 72, 3):
        raise SystemExit("invalid root inode")
    if test_dir[:4] != (1, (3 << 11) | 0o755, 72, 2):
        raise SystemExit("invalid /test inode")
    if payload[0] != 2 or payload[1] != (5 << 11) | 0o644 or payload[3] != 1:
        raise SystemExit("invalid payload inode")
    if read_directory(stream, root) != [
            (b".", 0, 3), (b"..", 0, 3), (b"test", 1, 3)]:
        raise SystemExit("invalid root directory")
    if read_directory(stream, test_dir) != [
            (b".", 1, 3), (b"..", 0, 3), (b"payload", 2, 5)]:
        raise SystemExit("invalid /test directory")
    remaining = payload[2]
    contents = bytearray()
    for block in payload[4][:11]:
        if remaining == 0:
            break
        if block == 0:
            raise SystemExit("missing direct payload block")
        stream.seek(block * zone_size)
        chunk = stream.read(min(zone_size, remaining))
        if len(chunk) != min(zone_size, remaining):
            raise SystemExit("short payload block")
        contents.extend(chunk)
        remaining -= len(chunk)
    if remaining != 0:
        raise SystemExit("test payload unexpectedly needs indirect blocks")
with open(source_path, "rb") as stream:
    source_contents = stream.read()
if contents != source_contents or hashlib.sha256(contents).digest() != hashlib.sha256(source_contents).digest():
    raise SystemExit("payload contents differ")
PY
