AS := nasm
CC := gcc

OUTBIN := out.bin

# -c   do not link
# -m32 outfile format is elf_i386
C_FLAG := -c -m32

# -f elf   outfile format is elf_i386
A_FLAG := -f elf

# -s strip-all Omit all symbol information from the output file.
# -m Emulate the mulation linker. You can list the available emulations
#    with the --verbose or -V options.
#         GNU ld (GNU Binutils for Ubuntu) 2.38
#             Supported emulations:
#              elf_x86_64
#              elf32_x86_64
#              elf_i386
#              elf_iamcu
#              elf_l1om
#              elf_k1om
#              i386pep
#              i386pe
LD_FLAG := -s -m elf_i386

# all: build load hanbote.sys
start: build install
	bochs

newimg:
	cp ../a.img .
#C:10 H:2 S:18

mount: install umount
	sudo mount -o loop a.img /mnt/floppy 
	sudo cp haribote.sys /mnt/floppy -v
	# sudo cp name.txt /mnt/floppy -v
	# sudo umount /mnt/floppy
umount:
	sudo umount /mnt/floppy

core : bootpack.o naskfunc.o
	ld $(LD_FLAG) -o out.img $^

bootpack.o: bootpack.c
	$(CC) $(C_FLAG) -o $@ $<

naskfunc.o: naskfunc.s
	$(AS) $(A_FLAG) -o $@ $<

install: build
	dd if=$(OUTBIN) of=a.img bs=512 count=360 conv=notrunc

build: $(OUTBIN) haribote.sys

$(OUTBIN):ipl10.s
	$(AS) $< -o $(OUTBIN)

haribote.sys: asmhead.s
	$(AS) $< -o $@

clean:
	rm -rf *.bin
	rm -rf *.sys
	rm -rf *.o
	rm -rf a.out
	rm -rf bochsout.txt
