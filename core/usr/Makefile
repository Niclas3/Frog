 # use ELF format
 # Real OS code ###########################
include ../../Makefile.os_rules
INCLUDE = ../include

syscall.o: syscall.c
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $<

syscall-init.o: syscall-init.c
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $<


usr.o: syscall.o syscall-init.o
	ld -m elf_i386 -r -o ../$(BUILD_DIR)/$@ $^

clean:
	rm -rf *.o
