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
