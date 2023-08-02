 # use ELF format
 # Real OS code ###########################
include ../Makefile.os_rules

oslib.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find lib -type f -name "oslib.c")

protect.o: protect.c
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $^
hid.o:
	cd ./hid && $(MAKE) hid.o
pic.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find pic -type f -name "*.c")
int.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find interrupt -type f -name "*.c")

destab.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find destab -type f -name "*.c")

graphic.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find gui -type f -name "*.c")

bootpack.o: bootpack.c
	$(CC) $(C_FLAG) -I $(INCLUDE) -o $@ $<

core.o: core.s
	$(AS) $(A_FLAG) -o $@ $<

core: core.o graphic.o bootpack.o destab.o int.o pic.o hid.o protect.o oslib.o
	ld $(LD_FLAG) -o $(BUILD)/$@.img $^
clean:
	rm -rf *.o
	cd ./hid && $(MAKE) clean

