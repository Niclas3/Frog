# pkill bochs &&
make clean-all && 
make newimg && 
make mount_debug && 
make run &
# ~/bin/bin/bochs -q -f ./bochsrc.debug & 
gdb -x qemu_debug_script.gdb
