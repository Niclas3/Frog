#!/bin/bash
DEBUG="./scripts/debug"
SESSION_NAME="gdb_session"
GDB_CMD="gdb -x $DEBUG/qemu_debug_script.gdb"
QEMU_CMD="make debug_run"

make clean-all &&
make newimg && 
make newhd80img && #FIX me free lfb <-- this is a bug 
# make mount_debug &&
make debug_run &
gdb -x $DEBUG/qemu_debug_script.gdb

