#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "$0")/.." && pwd)
makefile="$repo_dir/Makefile"
work_dir=$(mktemp -d "${TMPDIR:-/tmp}/frog-runtime-drive-test.XXXXXX")
trap 'rm -rf "$work_dir"' EXIT

check_expanded_drive()
{
    local qemu_test=$1
    local expected=$2
    local database="$work_dir/make-${qemu_test}.txt"
    local actual

    make --no-print-directory -pn -f "$makefile" QEMU_TEST="$qemu_test" FORCE \
        >"$database"
    actual=$(awk '/^RUNTIME_DATA_DRIVE := / {
                      sub(/^RUNTIME_DATA_DRIVE := /, ""); print; exit
                  }' "$database")
    if [ "$actual" != "$expected" ]; then
        printf 'QEMU_TEST=%s runtime drive is %s, expected %s\n' \
            "$qemu_test" "$actual" "$expected" >&2
        return 1
    fi
}

check_expanded_drive 0 \
    'format=raw,file=build/frog-root.img,if=ide,index=1,media=disk,snapshot=on'
check_expanded_drive 1 \
    'format=raw,file=hd80M.img,if=ide,index=1,media=disk'

python3 - "$makefile" <<'PY'
import re
import sys

path = sys.argv[1]
with open(path, "r", encoding="utf-8") as stream:
    lines = stream.readlines()

target_pattern = re.compile(r"^[A-Za-z0-9_.%/-]+\s*:")
expected_recipe = '\t-drive "$(RUNTIME_DATA_DRIVE)" \\\n'
for target in ("run", "debug_run", "debug_runv1"):
    start = next((index for index, line in enumerate(lines)
                  if line.startswith(f"{target}:")), None)
    if start is None:
        raise SystemExit(f"missing QEMU target {target}")
    end = len(lines)
    for index in range(start + 1, len(lines)):
        if target_pattern.match(lines[index]):
            end = index
            break
    recipe = "".join(lines[start:end])
    if recipe.count(expected_recipe) != 1:
        raise SystemExit(
            f"{target} must use exactly one unified runtime-data drive")
    if "RUNTIME_DATA_IMAGE" in recipe:
        raise SystemExit(f"{target} bypasses the unified runtime-data drive")
PY
