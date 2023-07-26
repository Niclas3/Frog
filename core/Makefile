 # use ELF format
 # Real OS code ###########################
include ../Makefile.os_rules
core: core.o graphic.o bootpack.o destab.o int.o
	ld $(LD_FLAG) -o $(BUILD)/$@.img $^

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

clean:
	rm -rf *.o
