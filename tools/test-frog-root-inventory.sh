#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "usage: $0 MANIFEST" >&2
    exit 2
fi

manifest=$1
manifest_dir=$(cd "$(dirname "$manifest")" && pwd)
repo_root=$(cd "$manifest_dir/.." && pwd)

fail()
{
    echo "frog-root inventory: $*" >&2
    exit 1
}

declare -A expected_dirs=(
    [/bin]=1
    [/dev]=1
    [/etc]=1
    [/etc/frog]=1
    [/sbin]=1
    [/share]=1
    [/share/poudland]=1
)
declare -A expected_kind=(
    [/bin/compositor]=elf
    [/bin/desktop]=elf
    [/etc/frog/init.conf]=file
    [/sbin/init]=elf
    [/sbin/init-graphical]=elf
    [/share/poudland/cursor.bmp]=file
)
declare -A expected_source=(
    [/bin/compositor]=../core/apps/build/compositor
    [/bin/desktop]=../core/apps/build/desktop
    [/etc/frog/init.conf]=init.conf
    [/sbin/init]=../core/build/init.elf
    [/sbin/init-graphical]=../core/build/init-graphical.elf
    [/share/poudland/cursor.bmp]=../core/apps/test/b.bmp
)
declare -A found_dirs=()
declare -A found_files=()

header_seen=0
volume_seen=0
while read -r kind destination source size hash extra; do
    [[ -z "${kind:-}" || "${kind:0:1}" == "#" ]] && continue

    if [[ $header_seen -eq 0 ]]; then
        [[ "$kind ${destination:-}" == "frogfs-manifest 2" && -z "${source:-}" ]] || \
            fail "first record is not frogfs-manifest 2"
        header_seen=1
        continue
    fi

    case "$kind" in
    volume)
        [[ "$destination" == "frog-root" && -z "${source:-}" ]] || \
            fail "volume is not frog-root"
        [[ $volume_seen -eq 0 ]] || fail "duplicate volume record"
        volume_seen=1
        ;;
    dir)
        [[ -n "${destination:-}" && -z "${source:-}" ]] || \
            fail "malformed dir record"
        [[ -n "${expected_dirs[$destination]+yes}" ]] || \
            fail "unexpected directory $destination"
        [[ -z "${found_dirs[$destination]+yes}" ]] || \
            fail "duplicate directory $destination"
        found_dirs[$destination]=1
        ;;
    file|elf)
        [[ -n "${destination:-}" && -n "${source:-}" && -n "${size:-}" && \
            -n "${hash:-}" && -z "${extra:-}" ]] || fail "malformed $kind record"
        [[ -n "${expected_kind[$destination]+yes}" ]] || \
            fail "unexpected file $destination"
        [[ "${expected_kind[$destination]}" == "$kind" ]] || \
            fail "wrong kind for $destination"
        [[ "${expected_source[$destination]}" == "$source" ]] || \
            fail "wrong source for $destination"
        [[ -z "${found_files[$destination]+yes}" ]] || \
            fail "duplicate file $destination"
        source_path="$manifest_dir/$source"
        [[ -f "$source_path" ]] || fail "source is absent for $destination"
        [[ "$(stat -c %s "$source_path")" == "$size" ]] || \
            fail "wrong size for $destination"
        [[ "$(sha256sum "$source_path" | awk '{print $1}')" == "$hash" ]] || \
            fail "wrong hash for $destination"
        if [[ "$kind" == elf ]]; then
            [[ "$(od -An -tx1 -N4 "$source_path" | tr -d '[:space:]')" == \
                7f454c46 ]] || fail "non-ELF source for $destination"
        fi
        found_files[$destination]=1
        ;;
    *)
        fail "unexpected record kind $kind"
        ;;
    esac
done <"$manifest"

[[ $header_seen -eq 1 ]] || fail "missing manifest header"
[[ $volume_seen -eq 1 ]] || fail "missing frog-root volume"
for destination in "${!expected_dirs[@]}"; do
    [[ -n "${found_dirs[$destination]+yes}" ]] || fail "missing directory $destination"
done
for destination in "${!expected_kind[@]}"; do
    [[ -n "${found_files[$destination]+yes}" ]] || fail "missing file $destination"
done

cmp -s "$manifest_dir/init.conf" <(printf 'mode=graphical\n') || \
    fail "config/init.conf is not exact mode=graphical input"
git -C "$repo_root" ls-files --error-unmatch -- core/apps/test/b.bmp >/dev/null || \
    fail "cursor source is not tracked"
