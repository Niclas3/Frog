#!/usr/bin/env bash
set -eu

if [ "$#" -ne 3 ]; then
    echo "usage: $0 COMPOSITOR DESKTOP INIT_GRAPHICAL" >&2
    exit 2
fi

compositor=$1
desktop=$2
init_graphical=$3
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT

for image in "$compositor" "$desktop" "$init_graphical"; do
    [ -f "$image" ] || {
        echo "$image: production ELF is absent" >&2
        exit 1
    }
    strings -a "$image" >"$work_dir/$(basename "$image").strings"
done

require_exact()
{
    local image=$1
    local value=$2
    local strings_file="$work_dir/$(basename "$image").strings"

    grep -Fqx "$value" "$strings_file" || {
        echo "$image: missing production path $value" >&2
        exit 1
    }
}

forbid_exact()
{
    local image=$1
    local value=$2
    local strings_file="$work_dir/$(basename "$image").strings"

    if grep -Fqx "$value" "$strings_file"; then
        echo "$image: contains legacy production path $value" >&2
        exit 1
    fi
}

require_exact "$init_graphical" /bin/compositor
require_exact "$init_graphical" /bin/desktop
require_exact "$compositor" /share/poudland/cursor.bmp

for image in "$compositor" "$desktop" "$init_graphical"; do
    forbid_exact "$image" /test/compositor
    forbid_exact "$image" /test/desktop
    forbid_exact "$image" /test/b.bmp
    forbid_exact "$image" /b.bmp
done
