# 🐸 Frog
Frog is a project that aims to build an operating system for self-teaching OS technology

Currently Frog only support x86 PC.

The Frog includes a kernel, bootloader and a compositor for desktop.

<need image there>

## Goals
- support network
- more GUI
- support POSIX.1-2017
- support X86-64 and more arch

## Build and run on a Unix-like system

Required host tools include GNU Make, GCC/binutils with i386 support, NASM,
Python 3, and `qemu-system-i386`.

From `src/`:

```sh
make frog-root.img
make frog-root-verify
make run
```

The host builder creates the deterministic read-only `frog-root` System Image;
there is no guest prepare boot. Normal QEMU attaches that reusable image
through a disposable snapshot, then the kernel switches it to `/` and starts
disk-loaded `/sbin/init`.

Run the compile/host gate with `./scripts/CI.sh`. Run focused QEMU profiles
serially; the complete fast graphical check is
`./scripts/qemu-test.sh desktop-smoke`, while the ten-minute soak remains a
separate explicit command. See
`doc/root-filesystem-implementation-handoff.md` for architecture, exact
commands, retained evidence, and known limits.

Generated images, binaries, screenshots, and QEMU result artifacts are build
outputs and are not committed.

## Project Layout

## feature support
[] support PCI
[] support TCP/IP stack
