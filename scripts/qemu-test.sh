#!/usr/bin/env bash
set -u

repo_dir=$(cd "$(dirname "$0")/.." && pwd)
profile=${1:-boot-smoke}
timeout_seconds=${FROG_QEMU_TIMEOUT:-30}
keep=${FROG_QEMU_KEEP:-0}

case "$profile" in
    boot-smoke|disk-smoke) ;;
    *) echo "usage: $0 {boot-smoke|disk-smoke}" >&2; exit 2 ;;
esac

work_dir=$(mktemp -d "${TMPDIR:-/tmp}/frog-qemu-${profile}.XXXXXX")
result_root="$repo_dir/build/qemu-test"
mkdir -p "$result_root"
debug_log="$work_dir/debugcon.log"
qemu_log="$work_dir/qemu.log"
result_json="$work_dir/result.json"
classification=BUILD_FAILED
qemu_status=-1
started_at=$(date -u +%Y-%m-%dT%H:%M:%SZ)

write_result()
{
    RESULT_JSON="$result_json" PROFILE="$profile" CLASSIFICATION="$classification" \
    QEMU_STATUS="$qemu_status" STARTED_AT="$started_at" DEBUG_LOG="$debug_log" \
    python3 - <<'PY'
import json
import os

data = {
    "schema_version": 1,
    "profile": os.environ["PROFILE"],
    "classification": os.environ["CLASSIFICATION"],
    "passed": os.environ["CLASSIFICATION"] == "PASS",
    "qemu_exit_status": int(os.environ["QEMU_STATUS"]),
    "started_at": os.environ["STARTED_AT"],
    "debug_log": (None if os.environ["CLASSIFICATION"] == "PASS"
                  else os.path.basename(os.environ["DEBUG_LOG"])),
}
with open(os.environ["RESULT_JSON"], "w", encoding="ascii") as stream:
    json.dump(data, stream, indent=2)
    stream.write("\n")
PY
}

preserve_result()
{
    write_result
    if [ "$classification" = PASS ] && [ "$keep" != 1 ]; then
        cp "$result_json" "$result_root/${profile}-result.json"
        rm -rf "$work_dir"
    else
        artifact_dir="$result_root/$(date -u +%Y%m%dT%H%M%SZ)-${profile}-${classification}"
        mv "$work_dir" "$artifact_dir"
        echo "$classification: $artifact_dir/result.json" >&2
    fi
}

build_failed()
{
    preserve_result
    exit 1
}

cd "$repo_dir"
cp ../hd.img "$work_dir/hd.img" || build_failed
cp ../hd80M.img "$work_dir/hd80M.img" || build_failed

make -C core clean >"$work_dir/build.log" 2>&1 || build_failed
make -C core QEMU_TEST=1 FROG_TEST_PROFILE="$profile" core >>"$work_dir/build.log" 2>&1 || build_failed
(cd booter && nasm -p boot.inc -f bin MBR.s -o "$work_dir/MBR.bin") >>"$work_dir/build.log" 2>&1 || build_failed
(cd booter && nasm -DVGA_ENABLE -p boot.inc -f bin loader.s -o "$work_dir/loader.img") >>"$work_dir/build.log" 2>&1 || build_failed
gcc -g -o "$work_dir/hankaku.bin" tools/create_fonts.c -lm >>"$work_dir/build.log" 2>&1 || build_failed
cp tools/hankaku.txt "$work_dir/hankaku.txt" || build_failed
(cd "$work_dir" && ./hankaku.bin) >>"$work_dir/build.log" 2>&1 || build_failed

dd if="$work_dir/MBR.bin" of="$work_dir/hd.img" bs=512 count=360 conv=notrunc status=none || build_failed
dd if="$work_dir/loader.img" of="$work_dir/hd.img" bs=512 seek=2 count=300 conv=notrunc status=none || build_failed
dd if=core/build/core.img of="$work_dir/hd.img" bs=512 seek=13 count=300 conv=notrunc status=none || build_failed
dd if="$work_dir/hankaku_font.img" of="$work_dir/hd.img" bs=512 seek=2048 conv=notrunc status=none || build_failed

set +e
timeout --signal=TERM --kill-after=2s "${timeout_seconds}s" \
    qemu-system-i386 \
    -display none -monitor none -serial none -no-reboot \
    -m 1G \
    -drive "format=raw,file=$work_dir/hd.img,if=ide,index=0,media=disk" \
    -drive "format=raw,file=$work_dir/hd80M.img,if=ide,index=1,media=disk" \
    -chardev "file,id=frogdebug,path=$debug_log" \
    -device isa-debugcon,iobase=0xe9,chardev=frogdebug \
    -device isa-debug-exit,iobase=0xf4,iosize=0x01 \
    -d guest_errors,cpu_reset -D "$qemu_log" \
    >"$work_dir/qemu.stdout" 2>"$work_dir/qemu.stderr"
qemu_status=$?
set -e

if [ "$qemu_status" -eq 124 ] || [ "$qemu_status" -eq 137 ]; then
    classification=BOOT_TIMEOUT
elif grep -q '\[PANIC\]' "$debug_log" 2>/dev/null; then
    classification=PANIC
elif grep -q 'ASSERT_FAILED' "$debug_log" 2>/dev/null; then
    classification=ASSERT_FAILED
elif grep -qi 'triple fault' "$qemu_log" 2>/dev/null; then
    classification=TRIPLE_FAULT
elif grep -q '^FROGTEST END FAIL$' "$debug_log" 2>/dev/null; then
    classification=TEST_FAILED
elif [ "$qemu_status" -eq 1 ] && \
     grep -q "^FROGTEST v=1 BEGIN profile=${profile}$" "$debug_log" 2>/dev/null && \
     grep -q '^FROGTEST MILESTONE ring3 PASS$' "$debug_log" 2>/dev/null && \
     grep -q '^FROGTEST END PASS$' "$debug_log" 2>/dev/null; then
    classification=PASS
elif [ "$qemu_status" -eq 0 ] || [ "$qemu_status" -eq 1 ] || [ "$qemu_status" -eq 3 ]; then
    classification=EXPECTED_MARKER_MISSING
else
    classification=EARLY_QEMU_EXIT
fi

preserve_result
[ "$classification" = PASS ]
