#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

for tool in make gcc ld nasm; do
        if ! command -v "$tool" >/dev/null 2>&1; then
                echo "missing required tool: $tool" >&2
                exit 127
        fi
done

make -C core clean
make core.img
make frog-root-test
make frog-root.img
make frog-root-verify

make -C core clean
make -C core QEMU_TEST=1 FROG_TEST_PROFILE=poudland-e2e-smoke core
core_image_size=$(wc -c <core/build/core.img)
if [ "$core_image_size" -gt $((512 * 512)) ]; then
        echo "poudland-e2e core.img is $core_image_size bytes; limit is 262144" >&2
        exit 1
fi
