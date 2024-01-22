 # use ELF format
 # Real OS code ###########################
include ../../Makefile.os_rules
INCLUDE = ../include

# OUT_FILE= main.o inode.o
# OUT_FILE= main.o
OUT_FILE=

# main.o: main.c
# 	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $<

# inode.o: inode.c
# 	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $<

fs.o: $(OUT_FILE)
	ld -m elf_i386 -r -o ../$(BUILD_DIR)/$@ $^

clean:
	rm -rf *.o
