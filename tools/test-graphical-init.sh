#!/usr/bin/env bash
set -eu

repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT

common_flags="-std=c11 -Wall -Wextra -Werror -fno-builtin"
include_flags="-I$repo_dir/core/include -I$repo_dir/core/arch/x86/include"

"${CC:-cc}" $common_flags $include_flags -DGRAPHICAL_INIT_HOST_TEST \
    "$repo_dir/core/user/graphical_init.c" \
    "$repo_dir/tools/test-graphical-init.c" \
    -o "$work_dir/test-graphical-init"
"$work_dir/test-graphical-init"
