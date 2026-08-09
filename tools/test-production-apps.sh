#!/usr/bin/env bash
set -eu

python3 - "$@" <<'PY'
import os
import struct
import sys

if len(sys.argv) != 3:
    raise SystemExit("usage: test-production-apps.sh COMPOSITOR DESKTOP")

for path in sys.argv[1:]:
    with open(path, "rb") as stream:
        image = stream.read()
    if len(image) <= 4096:
        raise SystemExit(f"{path}: production ELF does not cross 4 KiB")
    if image[:7] != b"\x7fELF\x01\x01\x01":
        raise SystemExit(f"{path}: not ELF32 little-endian")
    fields = struct.unpack_from("<HHIIIIIHHHHHH", image, 16)
    elf_type, machine, version, entry, phoff = fields[:5]
    ehsize, phentsize, phnum = fields[7:10]
    if (elf_type, machine, version, ehsize, phentsize) != (2, 3, 1, 52, 32):
        raise SystemExit(f"{path}: unsupported ELF header")
    if phnum < 2 or phnum > 32 or phoff + phnum * phentsize > len(image):
        raise SystemExit(f"{path}: invalid program header table")
    load_segments = []
    for index in range(phnum):
        program = struct.unpack_from("<IIIIIIII", image,
                                     phoff + index * phentsize)
        p_type, offset, vaddr, _paddr, filesz, memsz, flags, _align = program
        if p_type in (2, 3, 7):
            raise SystemExit(f"{path}: dynamic ELF dependency")
        if p_type != 1:
            continue
        if offset + filesz > len(image) or filesz > memsz:
            raise SystemExit(f"{path}: invalid load segment")
        load_segments.append((vaddr, vaddr + memsz, flags))
    if len(load_segments) < 2:
        raise SystemExit(f"{path}: expected separate text/data loads")
    if not any((flags & 5) == 5 and start <= entry < end
               for start, end, flags in load_segments):
        raise SystemExit(f"{path}: entry is not executable")
PY

for elf in "$@"; do
    if nm -u "$elf" | grep -q .; then
        echo "$elf: unresolved symbols" >&2
        nm -u "$elf" >&2
        exit 1
    fi
done
