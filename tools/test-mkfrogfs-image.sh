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
    printf 'frogfs-manifest 1\n'
    printf 'file /test/payload payload.bin %s %s\n' \
        "$source_size" "$source_hash"
} >"$manifest"

"$tool" --manifest "$manifest" --output "$image"
"$tool" --manifest "$manifest" --output "$image" --verify
first_hash=$(sha256sum "$image" | awk '{print $1}')
first_mtime=$(stat -c %Y "$image")
sleep 1
"$tool" --manifest "$manifest" --output "$image"
test "$(sha256sum "$image" | awk '{print $1}')" = "$first_hash"
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
    printf 'frogfs-manifest 1\n'
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
    printf 'frogfs-manifest 1\n'
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
    printf 'frogfs-manifest 1\n'
    printf 'file /absolute %s %s %s\n' \
        "$source_file" "$source_size" "$source_hash"
} >"$absolute_manifest"
if "$tool" --manifest "$absolute_manifest" --output "$absolute_image" \
        >/dev/null 2>&1; then
    echo 'absolute manifest source unexpectedly accepted' >&2
    exit 1
fi
test ! -e "$absolute_image"

max_file="$work_dir/max.bin"
max_manifest="$work_dir/max.manifest"
max_image="$work_dir/max.img"
truncate -s 1059840 "$max_file"
max_hash=$(sha256sum "$max_file" | awk '{print $1}')
{
    printf 'frogfs-manifest 1\n'
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
    printf 'frogfs-manifest 1\n'
    for index in $(seq -w 0 83); do
        printf 'file /many/file%s payload.bin %s %s\n' \
            "$index" "$source_size" "$source_hash"
    done
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
