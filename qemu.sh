make clean-all && 
make newimg && 
make newhd80img && #FIX me free lfb <-- this is a bug 
make mount_debug && 
make run &
gdb -x qemu_debug_script.gdb
