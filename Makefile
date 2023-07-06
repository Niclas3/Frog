AS := nasm
CC := gcc

# -c   do not link
# -m32 outfile format is elf_i386
# -lc  link with libc
C_FLAG := -c -m32

# -f elf   outfile format is elf_i386
A_FLAG := -f elf

AS_INCLUDE := boot.inc

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
# -lc link libc
LD_FLAG := -s -m elf_i386

start: newimg mount
	bochs

reset:  umount newimg mount 
	bochs

newimg:
	cp ../a.img .

#C:10 H:2 S:18
mount: bootloader haribote.img core.o
	sudo mount -o loop a.img /mnt/floppy 
	sudo cp haribote.img /mnt/floppy -v
	sudo cp core.o /mnt/floppy -v
	# sudo cp name.txt /mnt/floppy -v
	# sudo umount /mnt/floppy
umount:
	sudo umount /mnt/floppy

load_core:
	sudo cp core.img /mnt/floppy -v
	ls /mnt/floppy

# Use ELF format
# Real OS code ###########################
core.o:
	cd ./core && $(MAKE) core
##########################################


# To protected mode ###############################
haribote.img:                           #
	cd ./booter && $(MAKE) $@
###################################################

# Build bootloader ###########################################
bootloader: ipl10.bin
	dd if=$< of=a.img bs=512 count=360 conv=notrunc

ipl10.bin:
	cd ./booter && $(MAKE) $@
##############################################################

# Tools ###################################################### 
# Generate font
font :
	cd ./tools/ && $(MAKE) font
############################################################## 

clean:
	rm -rf *.bin
	rm -rf *.sys
	rm -rf *.o
	rm -rf a.out
	find . -type f -name "core.*" ! -name "core.s" -delete
	find . -type f -name "*.img" ! -name "a.img" -delete
	rm -rf haribote.img
	rm -rf bochsout.txt
