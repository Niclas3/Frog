make clean-all && 
make newimg && 
make mount_debug && 
~/bin/bin/bochs -q -f ./bochsrc.debug & 
gdb -x debug_script.gdb
