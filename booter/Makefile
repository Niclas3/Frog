AS := nasm
CC := gcc

# -c   do not link
# -m32 outfile format is elf_i386
# -lc  link with libc
C_FLAG := -c -m32

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

# To protected mode ###############################
pmloader.img: naskfunc.s                          #
	$(AS) -p $(AS_INCLUDE) -o $(BUILD)/$@ $<
### For test  #####################################                                                  
naskfunc.o: naskfunc.s                            #
	$(AS) $(A_FLAG) -p $(AS_INCLUDE) -o $@ $<
###################################################


# Build bootloader ###########################################
ipl10.bin:ipl10.s
	$(AS) -p $(AS_INCLUDE) $< -o $(BUILD)/$@
##############################################################

clean:
	rm -rf *.img
	rm -rf *.bin
