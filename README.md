# 🐸 Frog
Frog is a project that aims to build an operating system for self-teaching OS technology

Currently Frog only support X86 PC.

The Frog includes a kernel, bootloader and a compositor for desktop.

<need image there>

## Goals
- support network
- more GUI
- support X86-64 and more arch

## Build&Run at unix-like system
### requirments
- `bximage`
- ```sudo apt-get -y install qemu-system-x86```

We need two *.img files hd.img and hd80M.img at home directory. You can create it by
`bximage`.
> hd.img size should over 10m. and hd80M should be 80M.
run ./build.sh

## Project Layout
