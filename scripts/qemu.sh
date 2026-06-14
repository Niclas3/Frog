#!/bin/bash
DEBUG="./scripts/debug"
pkill -x qemu-system-x86_64 2>/dev/null; pkill -x qemu-system-i386 2>/dev/null; true
make clean-all &&
make newimg &&
make newhd80img && #FIX me free lfb <-- this is a bug 
make debug_run &
gdb -x $DEBUG/qemu_debug_script.gdb
