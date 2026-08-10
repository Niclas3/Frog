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

./tools/test-qemu-runtime-drives.sh

make -C core clean
make core.img
make -C core debug
if nm core/build/core_symbol.img |
        grep -Eq 'desktop_production_test_|desktop_production_start_disk_init'; then
        echo "normal core contains desktop production test symbols" >&2
        exit 1
fi
if strings -a core/build/core.img core/build/core_symbol.img |
        grep -Eq 'desktop\.root-located|desktop-production-root-ready|desktop\.liveness\.idle|desktop-soak\.liveness|root-namespace\.writable-root-rejected|production-root-negative\.'; then
        echo "normal core contains production-startup test strings" >&2
        exit 1
fi
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

make -C core clean
make -C core QEMU_TEST=1 FROG_TEST_PROFILE=graphical-init-production-smoke core
core_image_size=$(wc -c <core/build/core.img)
if [ "$core_image_size" -gt $((512 * 512)) ]; then
        echo "graphical-init-production core.img is $core_image_size bytes; limit is 262144" >&2
        exit 1
fi

for test_profile in desktop-smoke desktop-soak-10m; do
        make -C core clean
        make -C core QEMU_TEST=1 FROG_TEST_PROFILE="$test_profile" core
        core_image_size=$(wc -c <core/build/core.img)
        if [ "$core_image_size" -gt $((512 * 512)) ]; then
                echo "$test_profile core.img is $core_image_size bytes; limit is 262144" >&2
                exit 1
        fi
done

make -C core clean
make -C core QEMU_TEST=1 FROG_TEST_PROFILE=root-namespace-smoke \
        FROG_TEST_STAGE=writable-root core
core_image_size=$(wc -c <core/build/core.img)
if [ "$core_image_size" -gt $((512 * 512)) ]; then
        echo "writable-root core.img is $core_image_size bytes; limit is 262144" >&2
        exit 1
fi

for negative_stage in missing corrupt duplicate; do
        make -C core clean
        make -C core QEMU_TEST=1 \
                FROG_TEST_PROFILE=production-root-negative-smoke \
                FROG_TEST_STAGE="$negative_stage" core
        core_image_size=$(wc -c <core/build/core.img)
        if [ "$core_image_size" -gt $((512 * 512)) ]; then
                echo "production-root-negative $negative_stage core.img is $core_image_size bytes; limit is 262144" >&2
                exit 1
        fi
done
