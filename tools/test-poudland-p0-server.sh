#!/usr/bin/env bash
set -eu

repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT

"${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
    -DPOUDLAND_P0_SERVER_HOST_TEST \
    -I"$repo_dir/core/apps/include" -I"$repo_dir/core/include" \
    -I"$repo_dir/core/apps/poudland_p0" \
    "$repo_dir/tools/test-poudland-p0-server.c" \
    "$repo_dir/core/apps/poudland_p0/protocol.c" \
    "$repo_dir/core/apps/poudland_p0/server.c" \
    -o "$work_dir/test-poudland-p0-server"
"$work_dir/test-poudland-p0-server"
