 # use ELF format
 # Real OS code ###########################
include ../../Makefile.os_rules
INCLUDE = ../include

# ipc_base.o: ipc.c
# 	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $<
#
# ipc.o: ipc_base.o
# 	ld -m elf_i386 -r -o ../$(BUILD_DIR)/$@ $^

ipc.o: ipc.c
	$(CC) -I $(INCLUDE) -o ../$(BUILD_DIR)/$@ $(C_FLAG) -g $<


clean:
	rm -rf *.o
