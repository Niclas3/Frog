 # use ELF format
 # Real OS code ###########################
include ../../Makefile.os_rules
INCLUDE = ../include

keyboard.o: keyboard.c
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $<

ps2mouse.o: ps2mouse.c
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $<

hid.o: keyboard.o ps2mouse.o
	ld -m elf_i386 -r -o ../$@ $^

clean:
	rm -rf *.o
