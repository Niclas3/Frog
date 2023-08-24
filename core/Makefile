 # use ELF format
 # Real OS code ###########################
include ../Makefile.os_rules
C_FLAG += -g
BUILD_DIR = ./build

C_OBJS = $(BUILD_DIR)/bootpack.o $(BUILD_DIR)/protect.o $(BUILD_DIR)/graphic.o \
	 $(BUILD_DIR)/hid.o $(BUILD_DIR)/int.o $(BUILD_DIR)/pic.o \
	 $(BUILD_DIR)/destab.o $(BUILD_DIR)/oslib.o $(BUILD_DIR)/list.o \
	 $(BUILD_DIR)/memory.o $(BUILD_DIR)/threads.o \
         $(BUILD_DIR)/panic.o  $(BUILD_DIR)/string.o \
	 $(BUILD_DIR)/bitmap.o $(BUILD_DIR)/debug.o \
	 $(BUILD_DIR)/core.o
 
$(BUILD_DIR)/list.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find lib -type f -name "list.c")
$(BUILD_DIR)/threads.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find thread -type f -name "threads.c")

$(BUILD_DIR)/panic.o: panic.c
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $^
$(BUILD_DIR)/memory.o: mm/memory.c
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $^

$(BUILD_DIR)/bitmap.o: lib/bitmap.c
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $^

$(BUILD_DIR)/string.o: lib/string.c
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) -fno-builtin $^

$(BUILD_DIR)/debug.o: lib/debug.c
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $^
$(BUILD_DIR)/oslib.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find lib -type f -name "oslib.c")
$(BUILD_DIR)/protect.o: protect.c
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $^
$(BUILD_DIR)/hid.o:
	cd ./hid && $(MAKE) hid.o

$(BUILD_DIR)/pic.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find pic -type f -name "*.c")
$(BUILD_DIR)/int.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find interrupt -type f -name "*.c")
$(BUILD_DIR)/destab.o:
	$(CC) -I $(INCLUDE) -o $@ $(C_FLAG) $(shell find destab -type f -name "*.c")
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

move:
	cp $(BUILD_DIR)/core.img ../

all: mk_dir build move
clean:
	rm -rf *.o
	cd ./build && rm -rf ./*
	cd ./hid && $(MAKE) clean

