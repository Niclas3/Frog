CFLAGS= -g -Wall -c -fno-builtin -W -Wstrict-prototypes -Wmissing-prototypes -Wsystem-headers -m32
CINCLUDE= -I../include
OBJS= ../build/stdio.o ../lib/oslib.o  ../libc/string.o ../build/usr.o
# LD_FLAG := -m elf_i386 -Ttext 0x05048000
LD_FLAG := -m elf_i386
DISK = ../../hd.img
# -Ttext 0xc0080000
ls.o: ls.c
	gcc $(CFLAGS) $(CINCLUDE) $^ -o $@

ls:ls.o
	ld $(LD_FLAG) -e main $^ $(OBJS) -o $@

load:ls
	dd if=ls of=$(DISK) bs=512 count=300 seek=384 conv=notrunc

clean: 
	rm -rf *.o
