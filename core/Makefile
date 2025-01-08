 # use ELF format
 # Real OS code ###########################
include ../Makefile.os_rules
CORE = core

obj-y  = $(BUILD_DIR)/main.o \
	 $(BUILD_DIR)/debug.o $(BUILD_DIR)/printk.o\
	 $(BUILD_DIR)/bitmap.o $(BUILD_DIR)/list.o $(BUILD_DIR)/stdio.o $(BUILD_DIR)/string.o \
	 $(BUILD_DIR)/controller.o $(BUILD_DIR)/irq.o \
	 $(BUILD_DIR)/boot.o $(BUILD_DIR)/syscall-init.o $(BUILD_DIR)/descriptor.o \
	 $(BUILD_DIR)/exceptions.o $(BUILD_DIR)/head32.o $(BUILD_DIR)/idt.o \
	 $(BUILD_DIR)/i8253.o $(BUILD_DIR)/i8259a.o $(BUILD_DIR)/segment.o \
	 $(BUILD_DIR)/tss.o \

# obj-y = $(BUILD_DIR)/*.o


# link object files, the final file of kernel is named `core.img`
core: core.img

debug: core_symbol.img


core_symbol.img: $(obj-y)
	ld $(LD_DEBUG_FLAG) -o $(BUILD_DIR)/$@ $^

core.img: $(obj-y)
	ld $(LD_FLAG) -o $(BUILD_DIR)/$@ $^

$(obj-y):
	$(MAKE) -C ./arch/x86/
	$(MAKE) -C ./lib/
	$(MAKE) -C ./init/
	$(MAKE) -C ./kernel/

sb:
	@ls build

clean:
	rm -rf ./.deps/*.d
	cd ./build && rm -rf ./*
