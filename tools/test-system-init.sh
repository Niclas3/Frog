#!/usr/bin/env bash
set -eu

repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT

"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -fno-builtin \
    -I"$repo_dir/core/include" -I"$repo_dir/core/arch/x86/include" \
    -DSYSTEM_INIT_HOST_TEST "$repo_dir/core/user/system_init.c" \
    "$repo_dir/tools/test-system-init.c" -o "$work_dir/test-system-init"
"$work_dir/test-system-init"
