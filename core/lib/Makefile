 # use ELF format
 # Real OS code ###########################
include ../../Makefile.os_rules
INCLUDE = ../include
# OUT_FILE= bitmap.o debug.o ioqueue.o fifo.o list.o oslib.o stdio.o string.o print.o
OUT_FILE= oslib.o print.o fifo.o hashmap.o

# bitmap.o: bitmap.c
# 	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $<

# debug.o: debug.c
# 	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $<

# ioqueue.o: ioqueue.c
# 	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $<

# list.o: list.c
# 	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $<

oslib.o: oslib.c
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $<

fifo.o: fifo.c
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $<

hashmap.o: hashmap.c
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $<

# stdio.o: stdio.c
# 	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $<

print.o: print.s
	$(AS) $(A_FLAG) -o $@ $<

lib.o: $(OUT_FILE)
	ld -m elf_i386 -r -o ../$(BUILD_DIR)/$@ $^

clean:
	rm -rf *.o
