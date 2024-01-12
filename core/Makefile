 # use ELF format
 # Real OS code ###########################
include ../Makefile.os_rules
C_FLAG += -g
BUILD_DIR = ./build

C_OBJS = $(BUILD_DIR)/bootpack.o $(BUILD_DIR)/protect.o \
	 $(BUILD_DIR)/graphic.o $(BUILD_DIR)/2d_graphics.o $(BUILD_DIR)/fsk_mouse.o \
	 $(BUILD_DIR)/ide.o $(BUILD_DIR)/ps2hid.o $(BUILD_DIR)/console.o $(BUILD_DIR)/cmos.o \
         $(BUILD_DIR)/int.o $(BUILD_DIR)/pic.o \
	 $(BUILD_DIR)/tss.o $(BUILD_DIR)/destab.o  \
	 $(BUILD_DIR)/usr.o $(BUILD_DIR)/systask.o $(BUILD_DIR)/syscall-init.o \
	 $(BUILD_DIR)/lib.o $(BUILD_DIR)/libdebug.o $(BUILD_DIR)/list.o $(BUILD_DIR)/bitmap.o $(BUILD_DIR)/ioqueue.o $(BUILD_DIR)/stdio.o  $(BUILD_DIR)/poudland.o\
	 $(BUILD_DIR)/string.o \
	 $(BUILD_DIR)/ipc.o \
	 $(BUILD_DIR)/fs.o $(BUILD_DIR)/inode.o $(BUILD_DIR)/fs_c.o $(BUILD_DIR)/file.o $(BUILD_DIR)/dir.o $(BUILD_DIR)/pipe.o $(BUILD_DIR)/char_file.o \
	 $(BUILD_DIR)/memory.o \
         $(BUILD_DIR)/semaphore.o $(BUILD_DIR)/process.o $(BUILD_DIR)/exec.o $(BUILD_DIR)/exit.o \
         $(BUILD_DIR)/switch.o $(BUILD_DIR)/threads.o $(BUILD_DIR)/sched.o $(BUILD_DIR)/fork.o \
         $(BUILD_DIR)/panic.o $(BUILD_DIR)/kernel_print.o \
	 $(BUILD_DIR)/core.o

$(BUILD_DIR)/switch.o:
	$(AS) $(A_FLAG) -o $@ $(shell find thread -type f -name "switch.s")

$(BUILD_DIR)/semaphore.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find thread -type f -name "semaphore.c")

$(BUILD_DIR)/sched.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find thread -type f -name "sched.c")

$(BUILD_DIR)/threads.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find thread -type f -name "threads.c")

$(BUILD_DIR)/exec.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find thread -type f -name "exec.c")

$(BUILD_DIR)/process.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find thread -type f -name "process.c")

$(BUILD_DIR)/fork.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find thread -type f -name "fork.c")

$(BUILD_DIR)/exit.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find thread -type f -name "exit.c")

$(BUILD_DIR)/panic.o: panic.c
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $^
$(BUILD_DIR)/kernel_print.o: kernel_print.c
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $^

$(BUILD_DIR)/memory.o: mm/memory.c
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $^

$(BUILD_DIR)/protect.o: protect.c
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $^

$(BUILD_DIR)/lib.o:
	cd ./lib && $(MAKE) lib.o

$(BUILD_DIR)/poudland.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find lib -type f -name "poudland.c")

$(BUILD_DIR)/graphic.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find lib -type f -name "graphic.c")
$(BUILD_DIR)/2d_graphics.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find lib -type f -name "2d_graphics.c")
$(BUILD_DIR)/fsk_mouse.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find lib -type f -name "fsk_mouse.c")

$(BUILD_DIR)/libdebug.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) -g $(shell find lib -type f -name "debug.c")

$(BUILD_DIR)/list.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) -g $(shell find lib -type f -name "list.c")

$(BUILD_DIR)/bitmap.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) -g $(shell find lib -type f -name "bitmap.c")

$(BUILD_DIR)/ioqueue.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) -g $(shell find lib -type f -name "ioqueue.c")

# $(BUILD_DIR)/libc.o:
# 	cd ./libc && $(MAKE) libc.o

$(BUILD_DIR)/string.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) -g $(shell find libc -type f -name "string.c")

$(BUILD_DIR)/stdio.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) -g $(shell find libc -type f -name "stdio.c")

# $(BUILD_DIR)/device.o:
# 	cd ./device && $(MAKE) device.o

$(BUILD_DIR)/ide.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) -g $(shell find device -type f -name "ide.c")

$(BUILD_DIR)/cmos.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) -g $(shell find device -type f -name "cmos.c")

$(BUILD_DIR)/ps2hid.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) -g $(shell find device -type f -name "ps2hid.c")

$(BUILD_DIR)/console.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) -g $(shell find device -type f -name "console.c")

$(BUILD_DIR)/usr.o:
	cd ./usr && $(MAKE) usr.o

$(BUILD_DIR)/systask.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) -g $(shell find usr -type f -name "systask.c")

$(BUILD_DIR)/syscall-init.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) -g $(shell find usr -type f -name "syscall-init.c")

$(BUILD_DIR)/ipc.o:
	cd ./ipc && $(MAKE) ipc.o

$(BUILD_DIR)/fs.o:
	cd ./fs && $(MAKE) fs.o

$(BUILD_DIR)/inode.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) -g $(shell find fs -type f -name "inode.c")

$(BUILD_DIR)/pipe.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) -g $(shell find fs -type f -name "pipe.c")

$(BUILD_DIR)/fs_c.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) -g $(shell find fs -type f -name "fs.c")

$(BUILD_DIR)/file.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) -g $(shell find fs -type f -name "file.c")

$(BUILD_DIR)/dir.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) -g $(shell find fs -type f -name "dir.c")

$(BUILD_DIR)/char_file.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) -g $(shell find fs -type f -name "char_file.c")

$(BUILD_DIR)/pic.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find pic -type f -name "*.c")

$(BUILD_DIR)/int.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find interrupt -type f -name "*.c")

$(BUILD_DIR)/tss.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find destab -type f -name "tss.c")
	
$(BUILD_DIR)/destab.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find destab -type f -name "descriptor.c")


$(BUILD_DIR)/bootpack.o: bootpack.c
	$(CC) $(C_FLAG) -I $(INCLUDE) -o $@ $<
$(BUILD_DIR)/core.o: core.s
	$(AS) $(A_FLAG) -o $@ $<

$(BUILD_DIR)/core: $(C_OBJS)
	ld $(LD_FLAG) -o $@.img $^

$(BUILD_DIR)/core_symbol: $(C_OBJS)
	ld $(LD_DEBUG_FLAG) -M -o $@.img $^ 

.PHONY : clean mk_dir move
mk_dir:
	@mkdir -p $(BUILD_DIR)

build: $(BUILD_DIR)/core

build_symbol: $(BUILD_DIR)/core_symbol

export_symbol: $(BUILD_DIR)/core_symbol
	@nm $(BUILD_DIR)/core_symbol.img > $(BUILD_DIR)/Symbols.map
	@objdump -s -d $(BUILD_DIR)/core_symbol.img > $(BUILD_DIR)/Symbols_details.map

move:
	cp $(BUILD_DIR)/core.img ../

move_symbol:
	cp $(BUILD_DIR)/core_symbol.img ../

all: mk_dir build move

debug: mk_dir build_symbol build move

clean:
	rm -rf *.o
	cd ./build && rm -rf ./*
	cd ./device && $(MAKE) clean
	cd ./usr && $(MAKE) clean
	cd ./fs && $(MAKE) clean
	cd ./lib && $(MAKE) clean
	cd ./libc && $(MAKE) clean

