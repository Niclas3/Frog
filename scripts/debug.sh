#!/bin/bash
DEBUG="./scripts/debug"

if command -v tmux > /dev/null 2>&1; then
        ./scripts/debug/tmux_auto.sh
else
        make clean-all &&
        make newimg && 
        make newhd80img && #FIX me free lfb <-- this is a bug
        make debug_run &
        gdb -x $DEBUG/qemu_debug_script.gdb
fi


