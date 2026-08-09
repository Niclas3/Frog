#!/usr/bin/env bash
set -eu

repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT

common_flags="-std=c11 -Wall -Wextra -Werror -fno-builtin"
include_flags="-I$repo_dir/core/apps/include -I$repo_dir/core/include -I$repo_dir/core/arch/x86/include"

"${CC:-cc}" $common_flags $include_flags -Dmain=desktop_entry \
    -c "$repo_dir/core/apps/desktop.c" -o "$work_dir/desktop.o"
"${CC:-cc}" $common_flags $include_flags \
    -c "$repo_dir/tools/test-desktop-lifecycle.c" \
    -o "$work_dir/test.o"
"${CC:-cc}" "$work_dir/desktop.o" "$work_dir/test.o" \
    -o "$work_dir/test-desktop-lifecycle"
"$work_dir/test-desktop-lifecycle"
