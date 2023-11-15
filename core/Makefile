 # use ELF format
 # Real OS code ###########################
include ../Makefile.os_rules
C_FLAG += -g
BUILD_DIR = ./build

C_OBJS = $(BUILD_DIR)/bootpack.o $(BUILD_DIR)/protect.o $(BUILD_DIR)/graphic.o \
	 $(BUILD_DIR)/device.o $(BUILD_DIR)/ide.o $(BUILD_DIR)/ps2mouse.o \
         $(BUILD_DIR)/int.o $(BUILD_DIR)/pic.o \
	 $(BUILD_DIR)/tss.o $(BUILD_DIR)/destab.o  \
	 $(BUILD_DIR)/usr.o $(BUILD_DIR)/systask.o $(BUILD_DIR)/syscall-init.o \
	 $(BUILD_DIR)/lib.o $(BUILD_DIR)/libdebug.o $(BUILD_DIR)/list.o\
	 $(BUILD_DIR)/ipc.o \
	 $(BUILD_DIR)/fs.o $(BUILD_DIR)/inode.o $(BUILD_DIR)/fs_c.o $(BUILD_DIR)/file.o\
	 $(BUILD_DIR)/memory.o $(BUILD_DIR)/semaphore.o $(BUILD_DIR)/process.o \
         $(BUILD_DIR)/switch.o $(BUILD_DIR)/threads.o $(BUILD_DIR)/sched.o \
         $(BUILD_DIR)/panic.o \
	 $(BUILD_DIR)/core.o

$(BUILD_DIR)/switch.o:
	$(AS) $(A_FLAG) -o $@ $(shell find thread -type f -name "switch.s")

$(BUILD_DIR)/semaphore.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find thread -type f -name "semaphore.c")

$(BUILD_DIR)/sched.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find thread -type f -name "sched.c")

$(BUILD_DIR)/threads.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find thread -type f -name "threads.c")

$(BUILD_DIR)/process.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find thread -type f -name "process.c")

$(BUILD_DIR)/panic.o: panic.c
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $^
$(BUILD_DIR)/memory.o: mm/memory.c
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $^

$(BUILD_DIR)/protect.o: protect.c
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $^

$(BUILD_DIR)/lib.o:
	cd ./lib && $(MAKE) lib.o

$(BUILD_DIR)/libdebug.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) -g $(shell find lib -type f -name "debug.c")

$(BUILD_DIR)/list.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) -g $(shell find lib -type f -name "list.c")

$(BUILD_DIR)/device.o:
	cd ./device && $(MAKE) device.o

$(BUILD_DIR)/ide.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) -g $(shell find device -type f -name "ide.c")

$(BUILD_DIR)/ps2mouse.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) -g $(shell find device -type f -name "ps2mouse.c")

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

$(BUILD_DIR)/fs_c.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) -g $(shell find fs -type f -name "fs.c")

$(BUILD_DIR)/file.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) -g $(shell find fs -type f -name "file.c")

$(BUILD_DIR)/pic.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find pic -type f -name "*.c")

$(BUILD_DIR)/int.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find interrupt -type f -name "*.c")

$(BUILD_DIR)/tss.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find destab -type f -name "tss.c")
	
$(BUILD_DIR)/destab.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find destab -type f -name "descriptor.c")
$(BUILD_DIR)/graphic.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find gui -type f -name "*.c")
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

move:
	cp $(BUILD_DIR)/core.img ../

move_symbol:
	cp $(BUILD_DIR)/core_symbol.img ../

all: mk_dir build move

debug: mk_dir build_symbol move_symbol

clean:
	rm -rf *.o
	cd ./build && rm -rf ./*
	cd ./device && $(MAKE) clean

