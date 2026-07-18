#!/usr/bin/env bash
set -u
set -o pipefail

repo_dir=$(cd "$(dirname "$0")/.." && pwd)
profile=${1:-boot-smoke}
timeout_seconds=${FROG_QEMU_TIMEOUT:-30}
keep=${FROG_QEMU_KEEP:-0}

case "$profile" in
    boot-smoke) stages=(boot) ;;
    disk-smoke) stages=(prepare verify corrupt) ;;
    *) echo "usage: $0 {boot-smoke|disk-smoke}" >&2; exit 2 ;;
esac

work_dir=$(mktemp -d "${TMPDIR:-/tmp}/frog-qemu-${profile}.XXXXXX")
result_root="$repo_dir/build/qemu-test"
result_json="$work_dir/result.json"
data_disk="$work_dir/hd80M.img"
classification=BUILD_FAILED
failed_stage=none
qemu_status=-1
disk_sha_before=
disk_sha_after=
started_at=$(date -u +%Y-%m-%dT%H:%M:%SZ)

mkdir -p "$result_root"

write_result()
{
    RESULT_JSON="$result_json" PROFILE="$profile" \
    CLASSIFICATION="$classification" FAILED_STAGE="$failed_stage" \
    QEMU_STATUS="$qemu_status" STARTED_AT="$started_at" \
    DISK_SHA_BEFORE="$disk_sha_before" DISK_SHA_AFTER="$disk_sha_after" \
    python3 - <<'PY'
import json
import os

data = {
    "schema_version": 2,
    "profile": os.environ["PROFILE"],
    "classification": os.environ["CLASSIFICATION"],
    "passed": os.environ["CLASSIFICATION"] == "PASS",
    "failed_stage": (None if os.environ["FAILED_STAGE"] == "none"
                     else os.environ["FAILED_STAGE"]),
    "qemu_exit_status": int(os.environ["QEMU_STATUS"]),
    "started_at": os.environ["STARTED_AT"],
    "corrupt_disk_sha256_before": os.environ["DISK_SHA_BEFORE"] or None,
    "corrupt_disk_sha256_after": os.environ["DISK_SHA_AFTER"] or None,
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
        return
    fi

    artifact_dir="$result_root/$(date -u +%Y%m%dT%H%M%SZ)-${profile}-${classification}-$$"
    mv "$work_dir" "$artifact_dir"
    echo "$classification: $artifact_dir/result.json" >&2
}

build_stage()
{
    local stage=$1
    local stage_dir="$work_dir/$stage"
    local build_log="$stage_dir/build.log"
    local -a make_args

    mkdir -p "$stage_dir"
    cp "$repo_dir/../hd.img" "$stage_dir/hd.img" || return 1

    make -C "$repo_dir/core" clean >"$build_log" 2>&1 || return 1
    make_args=(QEMU_TEST=1 FROG_TEST_PROFILE="$profile")
    if [ "$profile" = disk-smoke ]; then
        make_args+=(FROG_TEST_STAGE="$stage")
    fi
    make -C "$repo_dir/core" "${make_args[@]}" core \
        >>"$build_log" 2>&1 || return 1
    (cd "$repo_dir/booter" &&
        nasm -p boot.inc -f bin MBR.s -o "$stage_dir/MBR.bin") \
        >>"$build_log" 2>&1 || return 1
    (cd "$repo_dir/booter" &&
        nasm -DVGA_ENABLE -p boot.inc -f bin loader.s \
             -o "$stage_dir/loader.img") >>"$build_log" 2>&1 || return 1
    gcc -g -o "$stage_dir/hankaku.bin" "$repo_dir/tools/create_fonts.c" -lm \
        >>"$build_log" 2>&1 || return 1
    cp "$repo_dir/tools/hankaku.txt" "$stage_dir/hankaku.txt" || return 1
    (cd "$stage_dir" && ./hankaku.bin) >>"$build_log" 2>&1 || return 1

    dd if="$stage_dir/MBR.bin" of="$stage_dir/hd.img" \
       bs=512 count=360 conv=notrunc status=none || return 1
    dd if="$stage_dir/loader.img" of="$stage_dir/hd.img" \
       bs=512 seek=2 count=300 conv=notrunc status=none || return 1
    dd if="$repo_dir/core/build/core.img" of="$stage_dir/hd.img" \
       bs=512 seek=13 count=300 conv=notrunc status=none || return 1
    dd if="$stage_dir/hankaku_font.img" of="$stage_dir/hd.img" \
       bs=512 seek=2048 conv=notrunc status=none || return 1
}

run_stage()
{
    local stage=$1
    local stage_dir="$work_dir/$stage"
    local debug_log="$stage_dir/debugcon.log"
    local qemu_log="$stage_dir/qemu.log"
    local expected_profile=$profile

    if [ "$profile" = disk-smoke ]; then
        expected_profile="disk-smoke.$stage"
    fi

    timeout --signal=TERM --kill-after=2s "${timeout_seconds}s" \
        qemu-system-i386 \
        -display none -monitor none -serial none -no-reboot \
        -m 1G \
        -drive "format=raw,file=$stage_dir/hd.img,if=ide,index=0,media=disk" \
        -drive "format=raw,file=$data_disk,if=ide,index=1,media=disk" \
        -chardev "file,id=frogdebug,path=$debug_log" \
        -device isa-debugcon,iobase=0xe9,chardev=frogdebug \
        -device isa-debug-exit,iobase=0xf4,iosize=0x01 \
        -d int,guest_errors,cpu_reset -D "$qemu_log" \
        >"$stage_dir/qemu.stdout" 2>"$stage_dir/qemu.stderr"
    qemu_status=$?

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
    elif [ "$qemu_status" -eq 1 ] &&
         grep -q "^FROGTEST v=1 BEGIN profile=${expected_profile}$" \
              "$debug_log" 2>/dev/null &&
         grep -q '^FROGTEST END PASS$' "$debug_log" 2>/dev/null; then
        classification=PASS
    elif [ "$qemu_status" -eq 0 ] || [ "$qemu_status" -eq 1 ] ||
         [ "$qemu_status" -eq 3 ]; then
        classification=EXPECTED_MARKER_MISSING
    else
        classification=EARLY_QEMU_EXIT
    fi
}

cp "$repo_dir/../hd80M.img" "$data_disk" || {
    preserve_result
    exit 1
}

for stage in "${stages[@]}"; do
    failed_stage=$stage
    classification=BUILD_FAILED
    if ! build_stage "$stage"; then
        preserve_result
        exit 1
    fi

    if [ "$profile" = disk-smoke ] && [ "$stage" = corrupt ]; then
        disk_sha_before=$(sha256sum "$data_disk" | awk '{print $1}')
    fi

    run_stage "$stage"

    if [ "$profile" = disk-smoke ] && [ "$stage" = corrupt ]; then
        disk_sha_after=$(sha256sum "$data_disk" | awk '{print $1}')
        if [ "$classification" = PASS ] &&
           [ "$disk_sha_before" != "$disk_sha_after" ]; then
            classification=DISK_MUTATED
        fi
    fi

    if [ "$classification" != PASS ]; then
        preserve_result
        exit 1
    fi
done

failed_stage=none
classification=PASS
preserve_result
