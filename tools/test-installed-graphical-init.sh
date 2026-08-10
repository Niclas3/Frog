#!/usr/bin/env bash
set -eu

repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT

"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -fno-builtin \
    -I"$repo_dir/core/include" -I"$repo_dir/core/arch/x86/include" \
    -DGRAPHICAL_INIT_HOST_TEST \
    -DEXPECTED_COMPOSITOR_PATH=\"/bin/compositor\" \
    -DEXPECTED_DESKTOP_PATH=\"/bin/desktop\" \
    "$repo_dir/core/user/graphical_init.c" \
    "$repo_dir/tools/test-graphical-init.c" \
    -o "$work_dir/test-installed-graphical-init"
"$work_dir/test-installed-graphical-init"
