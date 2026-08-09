#!/usr/bin/env bash
set -eu

repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT

"${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
    -I"$repo_dir/core/apps/include" -I"$repo_dir/core/include" \
    -I"$repo_dir/core/apps/poudland_p0" \
    "$repo_dir/tools/test-poudland-p0-render.c" \
    "$repo_dir/core/apps/poudland_p0/protocol.c" \
    "$repo_dir/core/apps/poudland_p0/render.c" \
    -o "$work_dir/test-poudland-p0-render"
"$work_dir/test-poudland-p0-render"
