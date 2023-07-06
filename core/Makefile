AS := nasm
CC := gcc

OUTBIN := out.bin

# -c   do not link
# -m32 outfile format is elf_i386
# -lc  link with libc
# -fno-stack-protector Do not check stack at gcc stage instead of ld
C_FLAG := -c -m32 -fno-stack-protector

# -f elf   outfile format is elf_i386
A_FLAG := -f elf

AS_INCLUDE := ../header/boot.inc

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

BUILD := ..
INCLUDE := ./include


 # use ELF format
 # Real OS code ###########################
core: core.o graphic.o bootpack.o
	ld $(LD_FLAG) -o $(BUILD)/$@.bin $^

graphic.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find gui -type f -name "*.c")

bootpack.o: bootpack.c
	$(CC) $(C_FLAG) -I $(INCLUDE) -o $@ $<

core.o: core.s
	$(AS) $(A_FLAG) -o $@ $<

clean:
	rm -rf *.o
