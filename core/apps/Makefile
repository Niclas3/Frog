# CFLAGS= -g -Wall -c -fno-builtin -W -Wstrict-prototypes -Wmissing-prototypes -Wsystem-headers -m32 -fno-stack-protector
CFLAGS := -g -c -m32 -fno-stack-protector -Wstrict-prototypes -Wmissing-prototypes -fno-builtin

# -s --strip-all
#     Omit all symbol information from the output
#     file.
LD_FLAG := -m elf_i386
CINCLUDE= -I../include
OBJS= ../build/stdio.o ../lib/oslib.o ../build/string.o \
      ../build/usr.o ../build/libdebug.o \
      ../build/list.o
# sys_OBJS = ../build/memory.o
sys_OBJS =
gua_OBJS = ../build/2d_graphics.o ../build/poudland.o

BUILD =./build
DISK = ../../hd.img


ls.o: ls.c
	gcc $(CFLAGS) $(CINCLUDE) $^ -o $@

# home brew C runtime
crt.o: start.s
	nasm -f elf -o $@ $^

objs:
	cd ../ && make all

# list app things
ls:ls.o crt.o
	ld $(LD_FLAG) $^ $(OBJS) -o $(BUILD)/$@

# compositor things
bmp.o: bmp.c
	gcc $(CFLAGS) $(CINCLUDE) -I. $^ -o $@

jpeg.o: jpeg.c
	gcc $(CFLAGS) $^ -o $@

compositor.o: compositor.c
	gcc $(CFLAGS) $(CINCLUDE) -I. $^ -o $@

compositor: compositor.o crt.o bmp.o jpeg.o
	ld $(LD_FLAG) $^ $(OBJS) $(gua_OBJS) $(sys_OBJS) -o $(BUILD)/$@

all: compositor ls

load: compositor
	dd if=$(BUILD)/$^ of=$(DISK) bs=512 count=300 seek=384 conv=notrunc

clean: 
	rm -rf *.o
	rm -rf ./build/*
