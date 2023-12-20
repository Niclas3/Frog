 # use ELF format
 # Real OS code ###########################
include ../../Makefile.os_rules
INCLUDE = ../include

# ps2hid.o: ps2hid.c
# 	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $<

ide.o: ide.c
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $<

# console.o: console.c
# 	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $<

# device.o: ide.o tty.o ps2hid.o
# 	ld -m elf_i386 -r -o ../$(BUILD_DIR)/$@ $^


device.o: ide.o
	ld -m elf_i386 -r -o ../$(BUILD_DIR)/$@ $^

clean:
	rm -rf *.o
