#!/usr/bin/env bash
set -u
set -o pipefail

repo_dir=$(cd "$(dirname "$0")/.." && pwd)
profile=${1:-boot-smoke}
timeout_seconds=${FROG_QEMU_TIMEOUT:-30}
keep=${FROG_QEMU_KEEP:-0}
sector_size=512
loader_sector_count=11
kernel_start_sector=13
kernel_sector_count=512

case "$profile" in
    boot-smoke) stages=(boot) ;;
    process-smoke) stages=(boot) ;;
    user-smoke) stages=(boot) ;;
    framebuffer-smoke) stages=(boot) ;;
    disk-smoke) stages=(prepare verify corrupt) ;;
    *) echo "usage: $0 {boot-smoke|process-smoke|user-smoke|framebuffer-smoke|disk-smoke}" >&2; exit 2 ;;
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
framebuffer_width=
framebuffer_height=
started_at=$(date -u +%Y-%m-%dT%H:%M:%SZ)

mkdir -p "$result_root"

write_result()
{
    RESULT_JSON="$result_json" PROFILE="$profile" \
    CLASSIFICATION="$classification" FAILED_STAGE="$failed_stage" \
    QEMU_STATUS="$qemu_status" STARTED_AT="$started_at" \
    DISK_SHA_BEFORE="$disk_sha_before" DISK_SHA_AFTER="$disk_sha_after" \
    FRAMEBUFFER_WIDTH="$framebuffer_width" \
    FRAMEBUFFER_HEIGHT="$framebuffer_height" \
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
    "framebuffer_width": (int(os.environ["FRAMEBUFFER_WIDTH"])
                          if os.environ["FRAMEBUFFER_WIDTH"] else None),
    "framebuffer_height": (int(os.environ["FRAMEBUFFER_HEIGHT"])
                           if os.environ["FRAMEBUFFER_HEIGHT"] else None),
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
    local core_image_size
    core_image_size=$(wc -c <"$repo_dir/core/build/core.img") || return 1
    local core_image_limit=$((kernel_sector_count * sector_size))
    if [ "$core_image_size" -gt "$core_image_limit" ]; then
        echo "core.img is $core_image_size bytes; boot image limit is $core_image_limit" \
             >>"$build_log"
        return 1
    fi
    (cd "$repo_dir/booter" &&
        nasm -p boot.inc -f bin MBR.s -o "$stage_dir/MBR.bin") \
        >>"$build_log" 2>&1 || return 1
    local -a loader_args
    loader_args=(-p boot.inc -f bin loader.s -o "$stage_dir/loader.img")
    if [ "$profile" = framebuffer-smoke ]; then
        loader_args=(-DFRAMEBUFFER_TEST "${loader_args[@]}")
    else
        loader_args=(-DVGA_ENABLE "${loader_args[@]}")
    fi
    (cd "$repo_dir/booter" && nasm "${loader_args[@]}") \
        >>"$build_log" 2>&1 || return 1
    local loader_image_size
    local loader_image_limit=$((loader_sector_count * sector_size))
    loader_image_size=$(wc -c <"$stage_dir/loader.img") || return 1
    if [ "$loader_image_size" -gt "$loader_image_limit" ]; then
        echo "loader.img is $loader_image_size bytes; MBR loader limit is $loader_image_limit" \
             >>"$build_log"
        return 1
    fi
    gcc -g -o "$stage_dir/hankaku.bin" "$repo_dir/tools/create_fonts.c" -lm \
        >>"$build_log" 2>&1 || return 1
    cp "$repo_dir/tools/hankaku.txt" "$stage_dir/hankaku.txt" || return 1
    (cd "$stage_dir" && ./hankaku.bin) >>"$build_log" 2>&1 || return 1

    dd if="$stage_dir/MBR.bin" of="$stage_dir/hd.img" \
       bs=512 count=360 conv=notrunc status=none || return 1
    dd if="$stage_dir/loader.img" of="$stage_dir/hd.img" \
       bs="$sector_size" seek=2 count="$loader_sector_count" \
       conv=notrunc status=none || return 1
    dd if="$repo_dir/core/build/core.img" of="$stage_dir/hd.img" \
       bs="$sector_size" seek="$kernel_start_sector" \
       count="$kernel_sector_count" conv=notrunc status=none || return 1
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
        -display none -monitor none -serial none -no-reboot -vga std \
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

qmp_screendump()
{
    local socket_path=$1
    local output_path=$2
    QMP_SOCKET="$socket_path" SCREENSHOT="$output_path" python3 - <<'PY'
import json
import os
import socket

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.settimeout(5)
sock.connect(os.environ["QMP_SOCKET"])
stream = sock.makefile("rwb", buffering=0)

def receive():
    while True:
        message = json.loads(stream.readline().decode("ascii"))
        if "event" not in message:
            return message

def execute(command, arguments=None):
    payload = {"execute": command}
    if arguments is not None:
        payload["arguments"] = arguments
    stream.write((json.dumps(payload) + "\n").encode("ascii"))
    response = receive()
    if "error" in response:
        raise RuntimeError(response["error"])

receive()
execute("qmp_capabilities")
execute("screendump", {"filename": os.environ["SCREENSHOT"]})
execute("quit")
sock.close()
PY
}

validate_framebuffer_ppm()
{
    local ppm=$1
    FRAMEBUFFER_META="$work_dir/framebuffer-meta" SCREENSHOT="$ppm" python3 - <<'PY'
import os
import sys

def fail(message):
    print(message, file=sys.stderr)
    raise SystemExit(1)

path = os.environ["SCREENSHOT"]
with open(path, "rb") as stream:
    def token():
        while True:
            value = stream.readline()
            if not value:
                fail("truncated PPM header")
            value = value.strip()
            if value and not value.startswith(b"#"):
                return value

    if token() != b"P6":
        fail("QEMU screendump is not P6 PPM")
    dimensions = token().split()
    if len(dimensions) != 2:
        fail("invalid PPM dimensions")
    width, height = map(int, dimensions)
    if int(token()) != 255:
        fail("unsupported PPM max value")
    pixels = stream.read()

if (width, height) != (1024, 768):
    fail(f"unexpected framebuffer dimensions {width}x{height}")
if len(pixels) != width * height * 3:
    fail("truncated PPM pixels")

for y in range(height):
    for x in range(width):
        expected = ((255, 0, 0) if x < width // 3 else
                    (0, 255, 0) if x < 2 * width // 3 else
                    (0, 0, 255))
        if 480 <= x < 544 and 352 <= y < 416:
            expected = (255, 255, 255)
        offset = (y * width + x) * 3
        actual = tuple(pixels[offset:offset + 3])
        if actual != expected:
            fail(f"pixel ({x}, {y}) is {actual}, expected {expected}")

with open(os.environ["FRAMEBUFFER_META"], "w", encoding="ascii") as stream:
    stream.write(f"{width} {height}\n")
PY
}

run_framebuffer_stage()
{
    local stage=$1
    local stage_dir="$work_dir/$stage"
    local debug_log="$stage_dir/debugcon.log"
    local qemu_log="$stage_dir/qemu.log"
    local qmp_socket="$stage_dir/qmp.sock"
    local screenshot="$stage_dir/framebuffer.ppm"
    local begin_seen=0
    local guest_failure_seen=0
    local qmp_failed=0

    timeout --signal=TERM --kill-after=2s "${timeout_seconds}s" \
        qemu-system-i386 \
        -display none -monitor none -serial none -no-reboot -vga std \
        -m 1G \
        -drive "format=raw,file=$stage_dir/hd.img,if=ide,index=0,media=disk" \
        -drive "format=raw,file=$data_disk,if=ide,index=1,media=disk" \
        -chardev "file,id=frogdebug,path=$debug_log" \
        -device isa-debugcon,iobase=0xe9,chardev=frogdebug \
        -device isa-debug-exit,iobase=0xf4,iosize=0x01 \
        -qmp "unix:$qmp_socket,server=on,wait=off" \
        -d int,guest_errors,cpu_reset -D "$qemu_log" \
        >"$stage_dir/qemu.stdout" 2>"$stage_dir/qemu.stderr" &
    local runner_pid=$!
    local ready=0
    for _ in $(seq 1 $((timeout_seconds * 20))); do
        if grep -q '^FROGTEST v=1 BEGIN profile=framebuffer-smoke$' \
                  "$debug_log" 2>/dev/null; then
            begin_seen=1
        fi
        if grep -Eq '^FROGTEST (CASE .* FAIL|MILESTONE .* FAIL|ABORT reason=.*|END FAIL)$' \
                   "$debug_log" 2>/dev/null; then
            guest_failure_seen=1
            break
        fi
        if [ "$begin_seen" -eq 1 ] &&
           grep -q '^FROGTEST SYNC framebuffer-ready$' \
                "$debug_log" 2>/dev/null; then
            ready=1
            break
        fi
        if ! kill -0 "$runner_pid" 2>/dev/null; then
            break
        fi
        sleep 0.05
    done

    if [ "$guest_failure_seen" -eq 1 ]; then
        kill "$runner_pid" 2>/dev/null || true
        wait "$runner_pid" 2>/dev/null || true
        classification=GUEST_TEST_FAILED
        return
    fi

    if [ "$ready" -eq 1 ] &&
       qmp_screendump "$qmp_socket" "$screenshot" \
           2>"$stage_dir/qmp-error.log"; then
        wait "$runner_pid"
        qemu_status=$?
        if [ "$qemu_status" -eq 0 ] &&
           validate_framebuffer_ppm "$screenshot" \
               2>"$stage_dir/framebuffer-validator.log"; then
            read -r framebuffer_width framebuffer_height \
                <"$work_dir/framebuffer-meta"
            classification=PASS
        elif [ "$qemu_status" -ne 0 ]; then
            classification=EARLY_QEMU_EXIT
        else
            classification=FRAMEBUFFER_MISMATCH
        fi
    else
        if [ "$ready" -eq 1 ]; then
            qmp_failed=1
            kill "$runner_pid" 2>/dev/null || true
        fi
        wait "$runner_pid"
        qemu_status=$?
        if grep -q '\[PANIC\]' "$debug_log" 2>/dev/null; then
            classification=PANIC
        elif grep -q 'ASSERT_FAILED' "$debug_log" 2>/dev/null; then
            classification=ASSERT_FAILED
        elif grep -qi 'triple fault' "$qemu_log" 2>/dev/null; then
            classification=TRIPLE_FAULT
        elif grep -Eqi 'qmp.*(bind|listen)|Failed to bind socket' \
                     "$stage_dir/qemu.stderr" 2>/dev/null; then
            classification=QMP_FAILED
        elif [ "$qmp_failed" -eq 1 ]; then
            classification=QMP_FAILED
        elif [ "$qemu_status" -eq 124 ] || [ "$qemu_status" -eq 137 ]; then
            classification=BOOT_TIMEOUT
        else
            classification=EXPECTED_MARKER_MISSING
        fi
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

    if [ "$profile" = framebuffer-smoke ]; then
        run_framebuffer_stage "$stage"
    else
        run_stage "$stage"
    fi

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
