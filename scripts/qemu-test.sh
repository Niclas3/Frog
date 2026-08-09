#!/usr/bin/env bash
set -u
set -o pipefail

repo_dir=$(cd "$(dirname "$0")/.." && pwd)
profile=${1:-boot-smoke}
timeout_seconds=${FROG_QEMU_TIMEOUT:-}
keep=${FROG_QEMU_KEEP:-0}
qemu_memory=${FROG_QEMU_MEMORY:-1G}
desktop_drag_x=${FROG_QEMU_DESKTOP_DRAG_X:-40}
desktop_drag_y=${FROG_QEMU_DESKTOP_DRAG_Y:-25}
desktop_drop_case=${FROG_QEMU_DESKTOP_DROP_CASE:-}
desktop_wrong_pixel=${FROG_QEMU_DESKTOP_WRONG_PIXEL:-0}
desktop_stale_image=${FROG_QEMU_DESKTOP_STALE_IMAGE:-0}
desktop_soak_watchdog=${FROG_QEMU_SOAK_WATCHDOG_TEST:-0}
sector_size=512
loader_sector_count=11
kernel_start_sector=13
kernel_sector_count=512

if [[ ! "$qemu_memory" =~ ^[1-9][0-9]*[MG]$ ]]; then
    echo "FROG_QEMU_MEMORY must be a positive integer followed by M or G" >&2
    exit 2
fi
if [[ ! "$desktop_drag_x" =~ ^-?[0-9]+$ ]] ||
   [[ ! "$desktop_drag_y" =~ ^-?[0-9]+$ ]]; then
    echo "FROG_QEMU_DESKTOP_DRAG_X/Y must be integers" >&2
    exit 2
fi
if [[ ! "$desktop_wrong_pixel" =~ ^[01]$ ]] ||
   [[ ! "$desktop_stale_image" =~ ^[01]$ ]] ||
   [[ ! "$desktop_soak_watchdog" =~ ^[01]$ ]]; then
    echo "desktop fault-injection controls must be 0 or 1" >&2
    exit 2
fi

case "$profile" in
    boot-smoke) stages=(boot) ;;
    process-smoke) stages=(boot) ;;
    user-smoke) stages=(boot) ;;
    framebuffer-smoke) stages=(boot) ;;
    framebuffer-mmap-smoke) stages=(boot) ;;
    anonymous-mmap-smoke) stages=(boot) ;;
    user-allocator-smoke) stages=(boot) ;;
    packagefs-smoke) stages=(boot) ;;
    packagefs-lifecycle-smoke) stages=(boot) ;;
    packagefs-userlib-smoke) stages=(boot) ;;
    poudland-v1-connect-smoke) stages=(boot) ;;
    poudland-v1-lifecycle-smoke) stages=(boot) ;;
    poudland-v1-version-smoke) stages=(boot) ;;
    poudland-v1-errno-smoke) stages=(boot) ;;
    poudland-v1-id-smoke) stages=(boot) ;;
    poudland-v1-routing-smoke) stages=(boot) ;;
    poudland-v1-retry-smoke) stages=(boot) ;;
    poudland-v1-create-smoke) stages=(boot) ;;
    poudland-v1-close-smoke) stages=(boot) ;;
    poudland-v1-error-smoke) stages=(boot) ;;
    poudland-v1-protocol-smoke) stages=(boot) ;;
    poudland-v1-fatal-smoke) stages=(boot) ;;
    poudland-v1-overflow-smoke) stages=(boot) ;;
    poudland-v1-hup-smoke) stages=(boot) ;;
    poudland-builtin-smoke) stages=(boot) ;;
    poudland-e2e-smoke) stages=(boot) ;;
    desktop-smoke) stages=(boot) ;;
    desktop-soak-10m) stages=(boot) ;;
    frogfs-image-smoke) stages=(boot) ;;
    frogfs-exec-smoke) stages=(boot) ;;
    input-smoke) stages=(boot) ;;
    time-smoke) stages=(boot) ;;
    wait2-smoke) stages=(boot) ;;
    disk-smoke) stages=(prepare verify corrupt) ;;
    *) echo "usage: $0 {boot-smoke|process-smoke|user-smoke|framebuffer-smoke|framebuffer-mmap-smoke|user-allocator-smoke|packagefs-smoke|packagefs-lifecycle-smoke|packagefs-userlib-smoke|poudland-v1-connect-smoke|poudland-v1-lifecycle-smoke|poudland-v1-version-smoke|poudland-v1-errno-smoke|poudland-v1-id-smoke|poudland-v1-routing-smoke|poudland-v1-retry-smoke|poudland-v1-create-smoke|poudland-v1-close-smoke|poudland-v1-error-smoke|poudland-v1-protocol-smoke|poudland-v1-fatal-smoke|poudland-v1-overflow-smoke|poudland-v1-hup-smoke|poudland-builtin-smoke|poudland-e2e-smoke|desktop-smoke|desktop-soak-10m|frogfs-image-smoke|frogfs-exec-smoke|anonymous-mmap-smoke|input-smoke|time-smoke|wait2-smoke|disk-smoke}" >&2; exit 2 ;;
esac

if [ -z "$timeout_seconds" ]; then
    if [ "$profile" = desktop-soak-10m ]; then
        timeout_seconds=660
    else
        timeout_seconds=30
    fi
fi
if [[ ! "$timeout_seconds" =~ ^[1-9][0-9]*$ ]]; then
    echo "FROG_QEMU_TIMEOUT must be a positive integer" >&2
    exit 2
fi

if { [ "$profile" = poudland-builtin-smoke ] ||
     [ "$profile" = poudland-e2e-smoke ] ||
     [ "$profile" = desktop-smoke ] ||
     [ "$profile" = desktop-soak-10m ] ||
     [ "$profile" = frogfs-image-smoke ] ||
     [ "$profile" = frogfs-exec-smoke ]; } &&
   [ -z "${FROG_QEMU_MEMORY+x}" ]; then
    qemu_memory=16M
fi

work_dir=$(mktemp -d "${TMPDIR:-/tmp}/frog-qemu-${profile}.XXXXXX")
result_root="$repo_dir/build/qemu-test"
result_json="$work_dir/result.json"
data_disk="$work_dir/hd80M.img"
desktop_base_disk="$repo_dir/build/desktop-smoke-root.img"
classification=BUILD_FAILED
failed_stage=none
qemu_status=-1
disk_sha_before=
disk_sha_after=
base_disk_sha_before=
base_disk_sha_after=
framebuffer_width=
framebuffer_height=
soak_heartbeat_count=
soak_resources_stable=
started_at=$(date -u +%Y-%m-%dT%H:%M:%SZ)

mkdir -p "$result_root"

write_result()
{
    RESULT_JSON="$result_json" PROFILE="$profile" \
    CLASSIFICATION="$classification" FAILED_STAGE="$failed_stage" \
    QEMU_STATUS="$qemu_status" STARTED_AT="$started_at" \
    QEMU_MEMORY="$qemu_memory" \
    DISK_SHA_BEFORE="$disk_sha_before" DISK_SHA_AFTER="$disk_sha_after" \
    BASE_DISK_SHA_BEFORE="$base_disk_sha_before" \
    BASE_DISK_SHA_AFTER="$base_disk_sha_after" \
    FRAMEBUFFER_WIDTH="$framebuffer_width" \
    FRAMEBUFFER_HEIGHT="$framebuffer_height" \
    SOAK_HEARTBEAT_COUNT="$soak_heartbeat_count" \
    SOAK_RESOURCES_STABLE="$soak_resources_stable" \
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
    "qemu_memory": os.environ["QEMU_MEMORY"],
    "corrupt_disk_sha256_before": os.environ["DISK_SHA_BEFORE"] or None,
    "corrupt_disk_sha256_after": os.environ["DISK_SHA_AFTER"] or None,
    "base_disk_sha256_before": os.environ["BASE_DISK_SHA_BEFORE"] or None,
    "base_disk_sha256_after": os.environ["BASE_DISK_SHA_AFTER"] or None,
    "framebuffer_width": (int(os.environ["FRAMEBUFFER_WIDTH"])
                          if os.environ["FRAMEBUFFER_WIDTH"] else None),
    "framebuffer_height": (int(os.environ["FRAMEBUFFER_HEIGHT"])
                           if os.environ["FRAMEBUFFER_HEIGHT"] else None),
    "soak_heartbeat_count": (
        int(os.environ["SOAK_HEARTBEAT_COUNT"])
        if os.environ["SOAK_HEARTBEAT_COUNT"] else None),
    "soak_duration_seconds": (
        int(os.environ["SOAK_HEARTBEAT_COUNT"]) * 60
        if os.environ["SOAK_HEARTBEAT_COUNT"] else None),
    "soak_resources_stable": (
        os.environ["SOAK_RESOURCES_STABLE"] == "true"
        if os.environ["SOAK_RESOURCES_STABLE"] else None),
}
with open(os.environ["RESULT_JSON"], "w", encoding="ascii") as stream:
    json.dump(data, stream, indent=2)
    stream.write("\n")
PY
}

preserve_result()
{
    write_result
    if [ "$classification" = PASS ]; then
        cp "$result_json" "$result_root/${profile}-result.json"
        if [ "$keep" != 1 ]; then
            rm -rf "$work_dir"
            return
        fi
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
    if [ "$profile" = anonymous-mmap-smoke ] &&
       [ "$qemu_memory" = 16M ]; then
        make_args+=(FROG_TEST_ALLOW_MAX_OOM=1)
    fi
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
    if [ "$profile" = framebuffer-smoke ] ||
       [ "$profile" = framebuffer-mmap-smoke ] ||
       [ "$profile" = poudland-builtin-smoke ] ||
       [ "$profile" = poudland-e2e-smoke ] ||
       [ "$profile" = desktop-smoke ] ||
       [ "$profile" = desktop-soak-10m ]; then
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
        -m "$qemu_memory" \
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
    local transcript_path=$3
    QMP_SOCKET="$socket_path" SCREENSHOT="$output_path" \
    QMP_TRANSCRIPT="$transcript_path" python3 - <<'PY'
import json
import os
import socket

transcript = open(os.environ["QMP_TRANSCRIPT"], "w", encoding="ascii")
sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.settimeout(5)
sock.connect(os.environ["QMP_SOCKET"])
stream = sock.makefile("rwb", buffering=0)

def record(direction, payload):
    transcript.write(f"{direction} {payload}\n")
    transcript.flush()

def receive():
    while True:
        line = stream.readline().decode("ascii").rstrip("\n")
        record("<", line)
        message = json.loads(line)
        if "event" not in message:
            return message

def execute(command, arguments=None):
    payload = {"execute": command}
    if arguments is not None:
        payload["arguments"] = arguments
    line = json.dumps(payload)
    record(">", line)
    stream.write((line + "\n").encode("ascii"))
    response = receive()
    if "error" in response:
        raise RuntimeError(response["error"])

receive()
execute("qmp_capabilities")
execute("screendump", {"filename": os.environ["SCREENSHOT"]})
execute("quit")
transcript.close()
sock.close()
PY
}

qmp_inject_input()
{
    local socket_path=$1
    local debug_log=$2
    local transcript_path=$3
    QMP_SOCKET="$socket_path" DEBUG_LOG="$debug_log" \
    QMP_TRANSCRIPT="$transcript_path" QMP_TIMEOUT="$timeout_seconds" \
    python3 - <<'PY'
import json
import os
import socket
import time

deadline = time.monotonic() + int(os.environ["QMP_TIMEOUT"])
transcript = open(os.environ["QMP_TRANSCRIPT"], "w", encoding="ascii")

def record(direction, payload):
    transcript.write(f"{direction} {payload}\n")
    transcript.flush()

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.settimeout(1)
while True:
    try:
        sock.connect(os.environ["QMP_SOCKET"])
        break
    except (FileNotFoundError, ConnectionRefusedError):
        if time.monotonic() >= deadline:
            raise RuntimeError("QMP socket was not ready")
        time.sleep(0.05)
stream = sock.makefile("rwb", buffering=0)

def receive():
    while True:
        line = stream.readline().decode("ascii").rstrip("\n")
        if not line:
            raise RuntimeError("QMP connection closed")
        record("<", line)
        message = json.loads(line)
        if "event" not in message:
            return message

def execute(command, arguments=None):
    payload = {"execute": command}
    if arguments is not None:
        payload["arguments"] = arguments
    line = json.dumps(payload, separators=(",", ":"))
    record(">", line)
    stream.write((line + "\n").encode("ascii"))
    response = receive()
    if "error" in response:
        raise RuntimeError(response["error"])

def guest_log():
    try:
        with open(os.environ["DEBUG_LOG"], "r", encoding="ascii",
                  errors="replace") as source:
            return source.read()
    except FileNotFoundError:
        return ""

def wait_marker(marker):
    expected = f"FROGTEST SYNC {marker}\n"
    while time.monotonic() < deadline:
        content = guest_log()
        if expected in content:
            return
        if ("FROGTEST END " in content or "FROGTEST ABORT " in content or
                "[PANIC]" in content or "ASSERT_FAILED" in content):
            raise RuntimeError(f"guest stopped before {marker}")
        time.sleep(0.05)
    raise RuntimeError(f"timed out waiting for {marker}")

def inject_after_block(marker, next_marker, events):
    wait_marker(marker)
    time.sleep(0.1)
    content = guest_log()
    if (f"FROGTEST SYNC {next_marker}\n" in content or
            "FROGTEST END " in content):
        raise RuntimeError(f"guest advanced past {marker} before host input")
    execute("input-send-event", {"events": events})

receive()
execute("qmp_capabilities")

inject_after_block("input-keyboard-ready", "input-mouse-move-ready", [
    {"type": "key", "data": {"down": True,
     "key": {"type": "qcode", "data": "a"}}},
    {"type": "key", "data": {"down": False,
     "key": {"type": "qcode", "data": "a"}}},
])

inject_after_block("input-mouse-move-ready", "input-mouse-button-ready", [
    {"type": "rel", "data": {"axis": "x", "value": 7}},
])

inject_after_block("input-mouse-button-ready", "input-both-ready", [
    {"type": "btn", "data": {"down": True, "button": "left"}},
])

inject_after_block("input-both-ready", "input-never", [
    {"type": "key", "data": {"down": True,
     "key": {"type": "qcode", "data": "a"}}},
    {"type": "key", "data": {"down": False,
     "key": {"type": "qcode", "data": "a"}}},
    {"type": "rel", "data": {"axis": "x", "value": 3}},
])

transcript.close()
sock.close()
PY
}

qmp_inject_poudland_builtin()
{
    local socket_path=$1
    local debug_log=$2
    local transcript_path=$3
    QMP_SOCKET="$socket_path" DEBUG_LOG="$debug_log" \
    QMP_TRANSCRIPT="$transcript_path" QMP_TIMEOUT="$timeout_seconds" \
    python3 - <<'PY'
import json
import os
import socket
import time

deadline = time.monotonic() + int(os.environ["QMP_TIMEOUT"])
transcript = open(os.environ["QMP_TRANSCRIPT"], "w", encoding="ascii")

def record(direction, payload):
    transcript.write(f"{direction} {payload}\n")
    transcript.flush()

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.settimeout(1)
while True:
    try:
        sock.connect(os.environ["QMP_SOCKET"])
        break
    except (FileNotFoundError, ConnectionRefusedError):
        if time.monotonic() >= deadline:
            raise RuntimeError("QMP socket was not ready")
        time.sleep(0.05)
stream = sock.makefile("rwb", buffering=0)

def receive():
    while True:
        line = stream.readline().decode("ascii").rstrip("\n")
        if not line:
            raise RuntimeError("QMP connection closed")
        record("<", line)
        message = json.loads(line)
        if "event" not in message:
            return message

def execute(command, arguments=None):
    payload = {"execute": command}
    if arguments is not None:
        payload["arguments"] = arguments
    line = json.dumps(payload, separators=(",", ":"))
    record(">", line)
    stream.write((line + "\n").encode("ascii"))
    response = receive()
    if "error" in response:
        raise RuntimeError(response["error"])

def guest_log():
    try:
        with open(os.environ["DEBUG_LOG"], "r", encoding="ascii",
                  errors="replace") as source:
            return source.read()
    except FileNotFoundError:
        return ""

def wait_marker(marker):
    expected = f"FROGTEST SYNC {marker}\n"
    while time.monotonic() < deadline:
        content = guest_log()
        if expected in content:
            return
        if ("FROGTEST END " in content or "FROGTEST ABORT " in content or
                "[PANIC]" in content or "ASSERT_FAILED" in content):
            raise RuntimeError(f"guest stopped before {marker}")
        time.sleep(0.05)
    raise RuntimeError(f"timed out waiting for {marker}")

def inject_after_block(marker, next_marker, events):
    wait_marker(marker)
    time.sleep(0.1)
    content = guest_log()
    if (f"FROGTEST SYNC {next_marker}\n" in content or
            "FROGTEST END " in content):
        raise RuntimeError(f"guest advanced past {marker} before host input")
    execute("input-send-event", {"events": events})

receive()
execute("qmp_capabilities")

inject_after_block("poudland-builtin-initial-ready",
                   "poudland-builtin-focus-ready", [
    {"type": "btn", "data": {"down": True, "button": "left"}},
])

inject_after_block("poudland-builtin-focus-ready",
                   "poudland-builtin-keyboard-ready", [
    {"type": "key", "data": {"down": True,
     "key": {"type": "qcode", "data": "a"}}},
    {"type": "key", "data": {"down": False,
     "key": {"type": "qcode", "data": "a"}}},
])

inject_after_block("poudland-builtin-keyboard-ready",
                   "poudland-builtin-drag-ready", [
    {"type": "rel", "data": {"axis": "x", "value": 40}},
    {"type": "rel", "data": {"axis": "y", "value": 25}},
])

inject_after_block("poudland-builtin-drag-ready",
                   "poudland-builtin-final-ready", [
    {"type": "btn", "data": {"down": False, "button": "left"}},
])

wait_marker("poudland-builtin-final-ready")
transcript.close()
sock.close()
PY
}

qmp_inject_desktop()
{
    local socket_path=$1
    local debug_log=$2
    local transcript_path=$3
    local screenshot=$4
    QMP_SOCKET="$socket_path" DEBUG_LOG="$debug_log" \
    QMP_TRANSCRIPT="$transcript_path" QMP_TIMEOUT="$timeout_seconds" \
    DESKTOP_DRAG_X="$desktop_drag_x" DESKTOP_DRAG_Y="$desktop_drag_y" \
    SCREENSHOT="$screenshot" \
    DESKTOP_SOAK=$([ "$profile" = desktop-soak-10m ] && echo 1 || echo 0) \
    DESKTOP_SOAK_WATCHDOG="$desktop_soak_watchdog" \
    python3 - <<'PY'
import json
import os
import socket
import time

deadline = time.monotonic() + int(os.environ["QMP_TIMEOUT"])
transcript = open(os.environ["QMP_TRANSCRIPT"], "w", encoding="ascii")

def record(direction, payload):
    transcript.write(f"{direction} {payload}\n")
    transcript.flush()

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.settimeout(1)
while True:
    try:
        sock.connect(os.environ["QMP_SOCKET"])
        break
    except (FileNotFoundError, ConnectionRefusedError):
        if time.monotonic() >= deadline:
            raise RuntimeError("QMP socket was not ready")
        time.sleep(0.05)
stream = sock.makefile("rwb", buffering=0)

def receive():
    while True:
        line = stream.readline().decode("ascii").rstrip("\n")
        if not line:
            raise RuntimeError("QMP connection closed")
        record("<", line)
        message = json.loads(line)
        if "event" not in message:
            return message

def execute(command, arguments=None):
    payload = {"execute": command}
    if arguments is not None:
        payload["arguments"] = arguments
    line = json.dumps(payload, separators=(",", ":"))
    record(">", line)
    stream.write((line + "\n").encode("ascii"))
    response = receive()
    if "error" in response:
        raise RuntimeError(response["error"])

def guest_log():
    try:
        with open(os.environ["DEBUG_LOG"], "r", encoding="ascii",
                  errors="replace") as source:
            return source.read()
    except FileNotFoundError:
        return ""

def guest_failed(content):
    return ("FROGTEST CASE " in content and " FAIL\n" in content or
            "FROGTEST MILESTONE " in content and " FAIL\n" in content or
            "FROGTEST ABORT " in content or
            "FROGTEST END FAIL\n" in content or
            "[PANIC]" in content or "ASSERT_FAILED" in content)

def wait_marker(marker):
    expected = f"FROGTEST SYNC {marker}\n"
    while time.monotonic() < deadline:
        content = guest_log()
        if guest_failed(content):
            raise RuntimeError(f"guest stopped before {marker}")
        if expected in content:
            return
        time.sleep(0.05)
    raise RuntimeError(f"timed out waiting for {marker}")

def wait_record(record):
    while time.monotonic() < deadline:
        content = guest_log()
        if guest_failed(content):
            raise RuntimeError(f"guest stopped before {record}")
        if content.splitlines().count(record) == 1:
            return
        time.sleep(0.05)
    raise RuntimeError(f"timed out waiting for {record}")

def inject_after_block(marker, next_marker, events):
    wait_marker(marker)
    time.sleep(0.1)
    content = guest_log()
    if f"FROGTEST SYNC {next_marker}\n" in content:
        raise RuntimeError(f"guest advanced past {marker} before host input")
    execute("input-send-event", {"events": events})

receive()
execute("qmp_capabilities")

inject_after_block("desktop-initial-ready", "desktop-focus-ready", [
    {"type": "btn", "data": {"down": True, "button": "left"}},
])

inject_after_block("desktop-focus-ready", "desktop-keyboard-ready", [
    {"type": "key", "data": {"down": True,
     "key": {"type": "qcode", "data": "a"}}},
    {"type": "key", "data": {"down": False,
     "key": {"type": "qcode", "data": "a"}}},
])

inject_after_block("desktop-keyboard-ready", "desktop-drag-ready", [
    {"type": "rel", "data": {
        "axis": "x", "value": int(os.environ["DESKTOP_DRAG_X"])}},
    {"type": "rel", "data": {
        "axis": "y", "value": int(os.environ["DESKTOP_DRAG_Y"])}},
])

inject_after_block("desktop-drag-ready", "desktop-final-frame", [
    {"type": "btn", "data": {"down": False, "button": "left"}},
])

wait_marker("desktop-final-frame")
wait_marker("desktop-client-observed")
wait_marker("desktop-idle-stable")
if os.environ["DESKTOP_SOAK"] == "1":
    wait_marker("desktop-soak-start")
    if os.environ["DESKTOP_SOAK_WATCHDOG"] == "1":
        time.sleep(2)
        raise RuntimeError(
            "timed out waiting for desktop-soak-watchdog-never")
    for minute in range(1, 11):
        wait_record(f"FROGTEST HEARTBEAT desktop-soak minute={minute}")
        print(f"desktop-soak heartbeat {minute}/10", flush=True)
    wait_marker("desktop-soak-complete")
    execute("screendump", {"filename": os.environ["SCREENSHOT"]})
    execute("quit")
transcript.close()
sock.close()
PY
}

qmp_capture_poudland_e2e()
{
    local socket_path=$1
    local debug_log=$2
    local screenshot=$3
    local transcript_path=$4
    QMP_SOCKET="$socket_path" DEBUG_LOG="$debug_log" \
    SCREENSHOT="$screenshot" QMP_TRANSCRIPT="$transcript_path" \
    QMP_TIMEOUT="$timeout_seconds" python3 - <<'PY'
import json
import os
import socket
import time

deadline = time.monotonic() + int(os.environ["QMP_TIMEOUT"])
transcript = open(os.environ["QMP_TRANSCRIPT"], "w", encoding="ascii")

def record(direction, payload):
    transcript.write(f"{direction} {payload}\n")
    transcript.flush()

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.settimeout(1)
while True:
    try:
        sock.connect(os.environ["QMP_SOCKET"])
        break
    except (FileNotFoundError, ConnectionRefusedError):
        if time.monotonic() >= deadline:
            raise RuntimeError("QMP socket was not ready")
        time.sleep(0.05)
stream = sock.makefile("rwb", buffering=0)

def receive():
    while True:
        line = stream.readline().decode("ascii").rstrip("\n")
        if not line:
            raise RuntimeError("QMP connection closed")
        record("<", line)
        message = json.loads(line)
        if "event" not in message:
            return message

def execute(command, arguments=None):
    payload = {"execute": command}
    if arguments is not None:
        payload["arguments"] = arguments
    line = json.dumps(payload, separators=(",", ":"))
    record(">", line)
    stream.write((line + "\n").encode("ascii"))
    response = receive()
    if "error" in response:
        raise RuntimeError(response["error"])

def guest_log():
    try:
        with open(os.environ["DEBUG_LOG"], "r", encoding="ascii",
                  errors="replace") as source:
            return source.read()
    except FileNotFoundError:
        return ""

def guest_failed(content):
    return ("FROGTEST CASE " in content and " FAIL\n" in content or
            "FROGTEST MILESTONE " in content and " FAIL\n" in content or
            "FROGTEST ABORT " in content or
            "FROGTEST END FAIL\n" in content or
            "[PANIC]" in content or "ASSERT_FAILED" in content)

def wait_marker(marker):
    expected = f"FROGTEST SYNC {marker}\n"
    while time.monotonic() < deadline:
        content = guest_log()
        if guest_failed(content):
            raise RuntimeError(f"guest stopped before {marker}")
        if expected in content:
            return
        time.sleep(0.05)
    raise RuntimeError(f"timed out waiting for {marker}")

def ppm_pixels(path):
    with open(path, "rb") as source:
        data = source.read()
    offset = 0

    def token():
        nonlocal offset
        while offset < len(data):
            if data[offset] == ord("#"):
                while offset < len(data) and data[offset] != ord("\n"):
                    offset += 1
            elif data[offset] in b" \t\r\n":
                offset += 1
            else:
                break
        start = offset
        while offset < len(data) and data[offset] not in b" \t\r\n":
            offset += 1
        if start == offset:
            raise RuntimeError("truncated PPM header")
        return data[start:offset]

    if token() != b"P6":
        raise RuntimeError("QEMU screendump is not P6 PPM")
    width = int(token())
    height = int(token())
    if int(token()) != 255:
        raise RuntimeError("unsupported PPM max value")
    if data[offset:offset + 2] == b"\r\n":
        offset += 2
    elif offset < len(data) and data[offset] in b" \t\r\n":
        offset += 1
    else:
        raise RuntimeError("missing PPM pixel separator")
    pixels = data[offset:]
    if (width, height) != (1024, 768) or len(pixels) != width * height * 3:
        raise RuntimeError("unexpected or truncated PPM")
    return width, pixels

def pixel_is(width, pixels, x, y, expected):
    offset = (y * width + x) * 3
    return tuple(pixels[offset:offset + 3]) == expected

def final_scene_ready(path):
    width, pixels = ppm_pixels(path)
    samples = (
        (0, 0, (32, 64, 96)),
        (110, 110, (204, 85, 51)),
        (210, 210, (204, 85, 51)),
        (230, 205, (51, 153, 102)),
        (530, 430, (51, 153, 102)),
        (540, 430, (32, 64, 96)),
        (310, 110, (32, 64, 96)),
        (430, 500, (32, 64, 96)),
        (600, 300, (32, 64, 96)),
    )
    return all(pixel_is(width, pixels, x, y, expected)
               for x, y, expected in samples)

receive()
execute("qmp_capabilities")
wait_marker("poudland-e2e-desktop-launched")

while time.monotonic() < deadline:
    content = guest_log()
    if guest_failed(content):
        raise RuntimeError("guest failed before the final frame")
    execute("screendump", {"filename": os.environ["SCREENSHOT"]})
    if final_scene_ready(os.environ["SCREENSHOT"]):
        execute("quit")
        transcript.close()
        sock.close()
        raise SystemExit(0)
    time.sleep(0.05)
raise RuntimeError("timed out waiting for exact Poudland frame")
PY
}

validate_framebuffer_ppm()
{
    local ppm=$1
    FRAMEBUFFER_META="$work_dir/framebuffer-meta" SCREENSHOT="$ppm" \
    FROG_PROFILE="$profile" \
    DESKTOP_WRONG_PIXEL="$desktop_wrong_pixel" \
    CURSOR_BMP="$repo_dir/core/apps/test/b.bmp" python3 - <<'PY'
import os
import struct
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

profile = os.environ["FROG_PROFILE"]
if profile in ("desktop-smoke", "desktop-soak-10m") and \
        os.environ["DESKTOP_WRONG_PIXEL"] == "1":
    pixels = bytes((pixels[0] ^ 1,)) + pixels[1:]
cursor = None
if profile in ("poudland-builtin-smoke", "poudland-e2e-smoke",
               "desktop-smoke", "desktop-soak-10m"):
    with open(os.environ["CURSOR_BMP"], "rb") as stream:
        bitmap = stream.read()
    if len(bitmap) != 9338 or bitmap[:2] != b"BM":
        fail("unexpected cursor BMP fixture")
    pixel_offset = struct.unpack_from("<I", bitmap, 10)[0]
    cursor_width, signed_height = struct.unpack_from("<ii", bitmap, 18)
    if (cursor_width, signed_height, pixel_offset) != (48, -48, 122):
        fail("unexpected cursor BMP geometry")
    cursor = []
    for cursor_y in range(48):
        row = []
        for cursor_x in range(48):
            offset = pixel_offset + (cursor_y * 48 + cursor_x) * 4
            blue, green, red, alpha = bitmap[offset:offset + 4]
            row.append((red, green, blue, alpha))
        cursor.append(row)

def blend(foreground, background):
    red, green, blue, alpha = foreground
    if alpha == 0:
        return background
    if alpha == 255:
        return red, green, blue
    return tuple((front * alpha + back * (255 - alpha) + 127) // 255
                 for front, back in zip((red, green, blue), background))

def expected_poudland_pixel(x, y):
    expected = (32, 64, 96)
    if 100 <= x < 300 and 100 <= y < 260:
        expected = (204, 85, 51)
    if 260 <= x < 580 and 225 <= y < 465:
        expected = (51, 153, 102)
        if (x < 263 or y < 228 or x >= 577 or y >= 462):
            expected = (255, 255, 255)
    if 270 <= x < 318 and 235 <= y < 283:
        expected = blend(cursor[y - 235][x - 270], expected)
    return expected

def expected_poudland_e2e_pixel(x, y):
    expected = (32, 64, 96)
    if 100 <= x < 300 and 100 <= y < 260:
        expected = (204, 85, 51)
    if 220 <= x < 540 and 200 <= y < 440:
        expected = (51, 153, 102)
    if 230 <= x < 278 and 210 <= y < 258:
        expected = blend(cursor[y - 210][x - 230], expected)
    return expected

for y in range(height):
    for x in range(width):
        if profile in ("poudland-builtin-smoke", "desktop-smoke",
                       "desktop-soak-10m"):
            expected = expected_poudland_pixel(x, y)
        elif profile == "poudland-e2e-smoke":
            expected = expected_poudland_e2e_pixel(x, y)
        else:
            expected = ((255, 0, 0) if x < width // 3 else
                        (0, 255, 0) if x < 2 * width // 3 else
                        (0, 0, 255))
            if 480 <= x < 544 and 352 <= y < 416:
                expected = (255, 255, 255)
            if profile == "framebuffer-mmap-smoke":
                if 64 <= x < 96 and 64 <= y < 96:
                    expected = (255, 0, 255)
                if 128 <= x < 160 and 64 <= y < 96:
                    expected = (255, 255, 0)
        offset = (y * width + x) * 3
        actual = tuple(pixels[offset:offset + 3])
        if actual != expected:
            fail(f"pixel ({x}, {y}) is {actual}, expected {expected}")

with open(os.environ["FRAMEBUFFER_META"], "w", encoding="ascii") as stream:
    stream.write(f"{width} {height}\n")
PY
}

validate_desktop_records()
{
    local debug_log=$1
    DEBUG_LOG="$debug_log" DESKTOP_DROP_CASE="$desktop_drop_case" \
    python3 - <<'PY'
import os
import sys

with open(os.environ["DEBUG_LOG"], "r", encoding="ascii",
          errors="replace") as source:
    lines = source.read().splitlines()
drop_case = os.environ["DESKTOP_DROP_CASE"]
if drop_case:
    dropped_record = f"FROGTEST CASE {drop_case} PASS"
    lines = [line for line in lines if line != dropped_record]

required_cases = (
    "desktop.init-compositor-fork",
    "desktop.init-desktop-fork",
    "desktop.compositor-exec",
    "desktop.service-bind",
    "desktop.compositor-ready",
    "desktop.client-exec",
    "desktop.handshake",
    "desktop.three-creates",
    "desktop.third-close",
    "desktop.two-live-windows",
    "desktop.focus-second-window",
    "desktop.keyboard-to-focus",
    "desktop.drag-configure",
    "desktop.client-observed-events",
    "desktop.final-scene",
    "desktop.idle-present-stable",
)
positions = {}
for name in required_cases:
    record = f"FROGTEST CASE {name} PASS"
    matches = [index for index, line in enumerate(lines) if line == record]
    if len(matches) != 1:
        print(f"required desktop record {record!r} occurred {len(matches)} times",
              file=sys.stderr)
        raise SystemExit(1)
    positions[name] = matches[0]

ordered_edges = (
    ("desktop.init-compositor-fork", "desktop.init-desktop-fork"),
    ("desktop.compositor-exec", "desktop.service-bind"),
    ("desktop.service-bind", "desktop.compositor-ready"),
    ("desktop.client-exec", "desktop.handshake"),
    ("desktop.service-bind", "desktop.handshake"),
    ("desktop.handshake", "desktop.three-creates"),
    ("desktop.three-creates", "desktop.third-close"),
    ("desktop.third-close", "desktop.two-live-windows"),
    ("desktop.two-live-windows", "desktop.focus-second-window"),
    ("desktop.focus-second-window", "desktop.keyboard-to-focus"),
    ("desktop.keyboard-to-focus", "desktop.drag-configure"),
    ("desktop.drag-configure", "desktop.final-scene"),
    ("desktop.final-scene", "desktop.idle-present-stable"),
)
for before, after in ordered_edges:
    if positions[before] >= positions[after]:
        print(f"desktop records out of order: {before} then {after}",
              file=sys.stderr)
        raise SystemExit(1)

required_syncs = (
    "desktop-initial-ready",
    "desktop-focus-ready",
    "desktop-keyboard-ready",
    "desktop-drag-ready",
    "desktop-final-frame",
    "desktop-client-observed",
    "desktop-idle-stable",
)
sync_positions = []
for name in required_syncs:
    record = f"FROGTEST SYNC {name}"
    matches = [index for index, line in enumerate(lines) if line == record]
    if len(matches) != 1:
        print(f"required desktop sync {record!r} occurred {len(matches)} times",
              file=sys.stderr)
        raise SystemExit(1)
    sync_positions.append(matches[0])
if sync_positions[:5] != sorted(sync_positions[:5]):
    print("desktop interaction sync records are out of order", file=sys.stderr)
    raise SystemExit(1)
if sync_positions[6] <= sync_positions[4]:
    print("desktop idle sync precedes the final frame", file=sys.stderr)
    raise SystemExit(1)
PY
}

validate_desktop_soak_records()
{
    local debug_log=$1
    DEBUG_LOG="$debug_log" python3 - <<'PY'
import os
import sys

with open(os.environ["DEBUG_LOG"], "r", encoding="ascii",
          errors="replace") as source:
    lines = source.read().splitlines()

required = ["FROGTEST SYNC desktop-soak-start"]
required.extend(
    f"FROGTEST HEARTBEAT desktop-soak minute={minute}"
    for minute in range(1, 11)
)
required.extend((
    "FROGTEST CASE desktop-soak.state-stable PASS",
    "FROGTEST CASE desktop-soak.resources PASS",
    "FROGTEST SYNC desktop-soak-complete",
))
positions = []
for record in required:
    matches = [index for index, line in enumerate(lines) if line == record]
    if len(matches) != 1:
        print(f"required soak record {record!r} occurred {len(matches)} times",
              file=sys.stderr)
        raise SystemExit(1)
    positions.append(matches[0])
if positions != sorted(positions):
    print("desktop soak records are out of order", file=sys.stderr)
    raise SystemExit(1)
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
    local qmp_transcript="$stage_dir/qmp-transcript.log"
    local qmp_input_transcript="$stage_dir/qmp-input-transcript.log"
    local expected_profile=$profile
    local ready_marker=framebuffer-ready
    local begin_seen=0
    local guest_failure_seen=0
    local qmp_failed=0

    if [ "$profile" = framebuffer-mmap-smoke ]; then
        ready_marker=framebuffer-mmap-ready
    elif [ "$profile" = poudland-builtin-smoke ]; then
        ready_marker=poudland-builtin-final-ready
    elif [ "$profile" = desktop-smoke ]; then
        ready_marker=desktop-idle-stable
    elif [ "$profile" = desktop-soak-10m ]; then
        ready_marker=desktop-soak-complete
    fi
    : >"$qmp_transcript"

    timeout --signal=TERM --kill-after=2s "${timeout_seconds}s" \
        qemu-system-i386 \
        -display none -monitor none -serial none -no-reboot -vga std \
        -m "$qemu_memory" -smp 1 \
        -drive "format=raw,file=$stage_dir/hd.img,if=ide,index=0,media=disk" \
        -drive "format=raw,file=$data_disk,if=ide,index=1,media=disk" \
        -chardev "file,id=frogdebug,path=$debug_log" \
        -device isa-debugcon,iobase=0xe9,chardev=frogdebug \
        -device isa-debug-exit,iobase=0xf4,iosize=0x01 \
        -qmp "unix:$qmp_socket,server=on,wait=off" \
        -d int,guest_errors,cpu_reset -D "$qemu_log" \
        >"$stage_dir/qemu.stdout" 2>"$stage_dir/qemu.stderr" &
    local runner_pid=$!
    if [ "$profile" = poudland-e2e-smoke ]; then
        local e2e_qmp_ok=0

        if qmp_capture_poudland_e2e \
                "$qmp_socket" "$debug_log" "$screenshot" \
                "$qmp_transcript" 2>"$stage_dir/qmp-error.log"; then
            e2e_qmp_ok=1
        else
            kill "$runner_pid" 2>/dev/null || true
        fi
        wait "$runner_pid" 2>/dev/null
        qemu_status=$?

        if grep -q '\[PANIC\]' "$debug_log" 2>/dev/null; then
            classification=PANIC
        elif grep -q 'ASSERT_FAILED' "$debug_log" 2>/dev/null; then
            classification=ASSERT_FAILED
        elif grep -qi 'triple fault' "$qemu_log" 2>/dev/null; then
            classification=TRIPLE_FAULT
        elif grep -Eq '^FROGTEST (CASE .* FAIL|MILESTONE .* FAIL|ABORT reason=.*|END FAIL)$' \
                     "$debug_log" 2>/dev/null; then
            classification=GUEST_TEST_FAILED
        elif grep -Eqi 'qmp.*(bind|listen)|Failed to bind socket' \
                     "$stage_dir/qemu.stderr" 2>/dev/null; then
            classification=QMP_FAILED
        elif [ "$e2e_qmp_ok" -ne 1 ]; then
            if grep -q 'timed out waiting for exact Poudland frame' \
                    "$stage_dir/qmp-error.log" 2>/dev/null; then
                validate_framebuffer_ppm "$screenshot" \
                    2>"$stage_dir/framebuffer-validator.log" || true
                classification=FRAMEBUFFER_MISMATCH
            elif grep -q 'timed out waiting' \
                    "$stage_dir/qmp-error.log" 2>/dev/null; then
                if grep -q '^FROGTEST v=1 BEGIN profile=poudland-e2e-smoke$' \
                        "$debug_log" 2>/dev/null; then
                    classification=EXPECTED_MARKER_MISSING
                else
                    classification=BOOT_TIMEOUT
                fi
            else
                classification=QMP_FAILED
            fi
        elif [ "$qemu_status" -eq 124 ] || [ "$qemu_status" -eq 137 ]; then
            classification=BOOT_TIMEOUT
        elif [ "$qemu_status" -ne 0 ]; then
            classification=EARLY_QEMU_EXIT
        elif ! grep -q '^FROGTEST v=1 BEGIN profile=poudland-e2e-smoke$' \
                    "$debug_log" 2>/dev/null ||
             ! grep -q '^FROGTEST SYNC poudland-e2e-desktop-launched$' \
                    "$debug_log" 2>/dev/null; then
            classification=EXPECTED_MARKER_MISSING
        elif validate_framebuffer_ppm "$screenshot" \
                2>"$stage_dir/framebuffer-validator.log"; then
            read -r framebuffer_width framebuffer_height \
                <"$work_dir/framebuffer-meta"
            classification=PASS
        else
            classification=FRAMEBUFFER_MISMATCH
        fi
        return
    fi
    if [ "$profile" = poudland-builtin-smoke ] &&
       ! qmp_inject_poudland_builtin "$qmp_socket" "$debug_log" \
           "$qmp_input_transcript" 2>"$stage_dir/qmp-input-error.log"; then
        qmp_failed=1
    elif { [ "$profile" = desktop-smoke ] ||
           [ "$profile" = desktop-soak-10m ]; } &&
         ! qmp_inject_desktop "$qmp_socket" "$debug_log" \
             "$qmp_input_transcript" "$screenshot" \
             2>"$stage_dir/qmp-input-error.log"; then
        qmp_failed=1
    fi
    if [ "$qmp_failed" -eq 1 ]; then
        kill "$runner_pid" 2>/dev/null || true
        wait "$runner_pid" 2>/dev/null
        qemu_status=$?
        if grep -q '\[PANIC\]' "$debug_log" 2>/dev/null; then
            classification=PANIC
        elif grep -q 'ASSERT_FAILED' "$debug_log" 2>/dev/null; then
            classification=ASSERT_FAILED
        elif grep -Eq '^FROGTEST (CASE .* FAIL|MILESTONE .* FAIL|ABORT reason=.*|END FAIL)$' \
                     "$debug_log" 2>/dev/null; then
            classification=GUEST_TEST_FAILED
        elif grep -Eqi 'qmp.*(bind|listen)|Failed to bind socket' \
                     "$stage_dir/qemu.stderr" 2>/dev/null; then
            classification=QMP_FAILED
        elif grep -q 'timed out waiting for ' \
                  "$stage_dir/qmp-input-error.log" 2>/dev/null; then
            classification=EXPECTED_MARKER_MISSING
        else
            classification=QMP_FAILED
        fi
        return
    fi
    if [ "$profile" = desktop-soak-10m ]; then
        wait "$runner_pid"
        qemu_status=$?
        local desktop_records_ok=1

        if ! validate_desktop_records "$debug_log" \
                2>"$stage_dir/guest-state-validator.log"; then
            desktop_records_ok=0
        fi
        if ! validate_desktop_soak_records "$debug_log" \
                2>>"$stage_dir/guest-state-validator.log"; then
            desktop_records_ok=0
        fi
        if grep -q '\[PANIC\]' "$debug_log" 2>/dev/null; then
            classification=PANIC
        elif grep -q 'ASSERT_FAILED' "$debug_log" 2>/dev/null; then
            classification=ASSERT_FAILED
        elif grep -qi 'triple fault' "$qemu_log" 2>/dev/null; then
            classification=TRIPLE_FAULT
        elif grep -Eq '^FROGTEST (CASE .* FAIL|MILESTONE .* FAIL|ABORT reason=.*|END FAIL)$' \
                     "$debug_log" 2>/dev/null; then
            classification=GUEST_TEST_FAILED
        elif [ "$qemu_status" -ne 0 ]; then
            classification=EARLY_QEMU_EXIT
        elif ! grep -q '^FROGTEST v=1 BEGIN profile=desktop-soak-10m$' \
                    "$debug_log" 2>/dev/null; then
            classification=EXPECTED_MARKER_MISSING
        elif [ "$desktop_records_ok" -ne 1 ]; then
            classification=GUEST_STATE_MISMATCH
        elif validate_framebuffer_ppm "$screenshot" \
                2>"$stage_dir/framebuffer-validator.log"; then
            read -r framebuffer_width framebuffer_height \
                <"$work_dir/framebuffer-meta"
            soak_heartbeat_count=10
            soak_resources_stable=true
            classification=PASS
        else
            classification=FRAMEBUFFER_MISMATCH
        fi
        return
    fi
    local ready=0
    for _ in $(seq 1 $((timeout_seconds * 20))); do
        if grep -q "^FROGTEST v=1 BEGIN profile=${expected_profile}$" \
                  "$debug_log" 2>/dev/null; then
            begin_seen=1
        fi
        if grep -Eq '^FROGTEST (CASE .* FAIL|MILESTONE .* FAIL|ABORT reason=.*|END FAIL)$' \
                   "$debug_log" 2>/dev/null ||
           grep -Eq '\[PANIC\]|ASSERT_FAILED' "$debug_log" 2>/dev/null; then
            guest_failure_seen=1
            break
        fi
        if [ "$begin_seen" -eq 1 ] &&
           grep -q "^FROGTEST SYNC ${ready_marker}$" \
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

    if [ "$ready" -eq 1 ] && [ "$qmp_failed" -eq 0 ] &&
       qmp_screendump "$qmp_socket" "$screenshot" "$qmp_transcript" \
           2>"$stage_dir/qmp-error.log"; then
        wait "$runner_pid"
        qemu_status=$?
        local desktop_records_ok=1
        if [ "$profile" = desktop-smoke ] &&
           ! validate_desktop_records "$debug_log" \
               2>"$stage_dir/guest-state-validator.log"; then
            desktop_records_ok=0
        fi
        if [ "$qemu_status" -eq 0 ] &&
           [ "$desktop_records_ok" -eq 1 ] &&
           validate_framebuffer_ppm "$screenshot" \
               2>"$stage_dir/framebuffer-validator.log"; then
            read -r framebuffer_width framebuffer_height \
                <"$work_dir/framebuffer-meta"
            classification=PASS
        elif [ "$qemu_status" -ne 0 ]; then
            classification=EARLY_QEMU_EXIT
        elif [ "$desktop_records_ok" -ne 1 ]; then
            classification=GUEST_STATE_MISMATCH
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

run_input_stage()
{
    local stage=$1
    local stage_dir="$work_dir/$stage"
    local debug_log="$stage_dir/debugcon.log"
    local qemu_log="$stage_dir/qemu.log"
    local qmp_socket="$stage_dir/qmp.sock"
    local qmp_transcript="$stage_dir/qmp-transcript.log"

    timeout --signal=TERM --kill-after=2s "${timeout_seconds}s" \
        qemu-system-i386 \
        -display none -monitor none -serial none -no-reboot -vga std \
        -m "$qemu_memory" -smp 1 \
        -drive "format=raw,file=$stage_dir/hd.img,if=ide,index=0,media=disk" \
        -drive "format=raw,file=$data_disk,if=ide,index=1,media=disk" \
        -chardev "file,id=frogdebug,path=$debug_log" \
        -device isa-debugcon,iobase=0xe9,chardev=frogdebug \
        -device isa-debug-exit,iobase=0xf4,iosize=0x01 \
        -qmp "unix:$qmp_socket,server=on,wait=off" \
        -d int,guest_errors,cpu_reset -D "$qemu_log" \
        >"$stage_dir/qemu.stdout" 2>"$stage_dir/qemu.stderr" &
    local runner_pid=$!
    local qmp_ok=0

    if qmp_inject_input "$qmp_socket" "$debug_log" "$qmp_transcript" \
            2>"$stage_dir/qmp-error.log"; then
        qmp_ok=1
    else
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
    elif grep -Eq '^FROGTEST (CASE .* FAIL|MILESTONE .* FAIL|ABORT reason=.*|END FAIL)$' \
                 "$debug_log" 2>/dev/null; then
        classification=GUEST_TEST_FAILED
    elif [ "$qmp_ok" -ne 1 ]; then
        classification=QMP_FAILED
    elif [ "$qemu_status" -eq 1 ] &&
         grep -q '^FROGTEST v=1 BEGIN profile=input-smoke$' \
              "$debug_log" 2>/dev/null &&
         grep -q '^FROGTEST END PASS$' "$debug_log" 2>/dev/null; then
        classification=PASS
    elif [ "$qemu_status" -eq 124 ] || [ "$qemu_status" -eq 137 ]; then
        classification=BOOT_TIMEOUT
    else
        classification=EXPECTED_MARKER_MISSING
    fi
}

build_desktop_test_root()
{
    local image_dir="$repo_dir/build/desktop-smoke-root"
    local log_dir="$work_dir/desktop-root"
    local build_log="$log_dir/build.log"
    local manifest="$image_dir/manifest"
    local compositor_size
    local compositor_hash
    local desktop_size
    local desktop_hash
    local bitmap_size
    local bitmap_hash

    mkdir -p "$image_dir" "$log_dir"
    make -C "$repo_dir/tools" mkfrogfs_image >"$build_log" 2>&1 || return 1
    make -C "$repo_dir/core/apps" QEMU_TEST=1 \
        FROG_TEST_PROFILE="$profile" clean-production \
        >>"$build_log" 2>&1 || return 1
    make -C "$repo_dir/core/apps" QEMU_TEST=1 \
        FROG_TEST_PROFILE="$profile" production \
        >>"$build_log" 2>&1 || return 1
    "$repo_dir/tools/test-production-apps.sh" \
        "$repo_dir/core/apps/build/compositor" \
        "$repo_dir/core/apps/build/desktop" \
        "$repo_dir/core/apps/build/desktop" \
        >>"$build_log" 2>&1 || return 1

    cp "$repo_dir/core/apps/test/b.bmp" "$image_dir/b.bmp" || return 1
    cp "$repo_dir/core/apps/build/compositor" \
        "$image_dir/compositor" || return 1
    cp "$repo_dir/core/apps/build/desktop" "$image_dir/desktop" || return 1

    bitmap_size=$(wc -c <"$image_dir/b.bmp") || return 1
    bitmap_hash=$(sha256sum "$image_dir/b.bmp" | awk '{print $1}') || return 1
    compositor_size=$(wc -c <"$image_dir/compositor") || return 1
    compositor_hash=$(sha256sum "$image_dir/compositor" | awk '{print $1}') || return 1
    desktop_size=$(wc -c <"$image_dir/desktop") || return 1
    desktop_hash=$(sha256sum "$image_dir/desktop" | awk '{print $1}') || return 1

    {
        printf 'frogfs-manifest 1\n'
        printf 'file /b.bmp b.bmp %s %s\n' \
            "$bitmap_size" "$bitmap_hash"
        printf 'elf /compositor compositor %s %s\n' \
            "$compositor_size" "$compositor_hash"
        printf 'elf /desktop desktop %s %s\n' \
            "$desktop_size" "$desktop_hash"
    } >"$manifest"

    "$repo_dir/tools/mkfrogfs_image" --manifest "$manifest" \
        --output "$desktop_base_disk" >>"$build_log" 2>&1 || return 1
    "$repo_dir/tools/mkfrogfs_image" --manifest "$manifest" \
        --output "$desktop_base_disk" --verify \
        >>"$build_log" 2>&1 || return 1

    base_disk_sha_before=$(sha256sum "$desktop_base_disk" |
                           awk '{print $1}') || return 1
    cp "$desktop_base_disk" "$data_disk" || return 1

    make -C "$repo_dir/core/apps" QEMU_TEST=0 FROG_TEST_PROFILE= \
        clean-production >>"$build_log" 2>&1 || return 1
    make -C "$repo_dir/core/apps" QEMU_TEST=0 FROG_TEST_PROFILE= production \
        >>"$build_log" 2>&1 || return 1
    "$repo_dir/tools/test-production-apps.sh" \
        "$repo_dir/core/apps/build/compositor" \
        "$repo_dir/core/apps/build/desktop" \
        "$repo_dir/core/apps/build/desktop" \
        >>"$build_log" 2>&1 || return 1
    if [ "$desktop_stale_image" = 1 ]; then
        make -C "$repo_dir" frog-root.img >>"$build_log" 2>&1 || return 1
        cp "$repo_dir/build/frog-root.img" "$data_disk" || return 1
    fi
}

data_disk_source="$repo_dir/../hd80M.img"
if [ "$profile" = desktop-smoke ] ||
   [ "$profile" = desktop-soak-10m ]; then
    build_desktop_test_root || {
        preserve_result
        exit 1
    }
elif [ "$profile" = frogfs-image-smoke ] ||
   [ "$profile" = frogfs-exec-smoke ] ||
   [ "$profile" = poudland-e2e-smoke ]; then
    make -C "$repo_dir" frog-root.img || {
        preserve_result
        exit 1
    }
    data_disk_source="$repo_dir/build/frog-root.img"
    cp "$data_disk_source" "$data_disk" || {
        preserve_result
        exit 1
    }
else
    cp "$data_disk_source" "$data_disk" || {
        preserve_result
        exit 1
    }
fi

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

    if [ "$profile" = framebuffer-smoke ] ||
       [ "$profile" = framebuffer-mmap-smoke ] ||
       [ "$profile" = poudland-builtin-smoke ] ||
       [ "$profile" = poudland-e2e-smoke ] ||
       [ "$profile" = desktop-smoke ] ||
       [ "$profile" = desktop-soak-10m ]; then
        run_framebuffer_stage "$stage"
    elif [ "$profile" = input-smoke ]; then
        run_input_stage "$stage"
    else
        run_stage "$stage"
    fi

    if [ "$profile" = desktop-smoke ] ||
       [ "$profile" = desktop-soak-10m ]; then
        base_disk_sha_after=$(sha256sum "$desktop_base_disk" |
                              awk '{print $1}') || {
            classification=BASE_DISK_MISSING
        }
        if [ "$classification" = PASS ] &&
           [ "$base_disk_sha_before" != "$base_disk_sha_after" ]; then
            classification=BASE_DISK_MUTATED
        fi
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
