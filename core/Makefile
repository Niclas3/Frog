SKIP_DEPENDENCIES := 1
include ../Makefile.os_rules
CORE = core
CORE_IMAGE := $(BUILD_DIR)/core.img
CORE_SYMBOL_IMAGE := $(BUILD_DIR)/core_symbol.img

# $(BUILD_DIR)/exec.o
# $(BUILD_DIR)/exit.o
# $(BUILD_DIR)/fork.o

obj-y  = $(BUILD_DIR)/head32.o  \
	 $(BUILD_DIR)/main.o $(BUILD_DIR)/init.o $(BUILD_DIR)/user_smoke_blob.o \
	 $(BUILD_DIR)/process.o $(BUILD_DIR)/process_regression.o $(BUILD_DIR)/sched.o $(BUILD_DIR)/semaphore.o \
	 $(BUILD_DIR)/switch.o $(BUILD_DIR)/threads.o \
	 $(BUILD_DIR)/timer.o \
	 $(BUILD_DIR)/controller.o $(BUILD_DIR)/irq.o $(BUILD_DIR)/softirq.o \
	 $(BUILD_DIR)/syscall-init.o $(BUILD_DIR)/descriptor.o \
	 $(BUILD_DIR)/page_fault.o \
	 $(BUILD_DIR)/exceptions.o $(BUILD_DIR)/idt.o \
	 $(BUILD_DIR)/i8253.o $(BUILD_DIR)/i8259a.o $(BUILD_DIR)/segment.o \
	 $(BUILD_DIR)/tss.o $(BUILD_DIR)/pc_plt_init.o $(BUILD_DIR)/framebuffer_smoke.o \
	 $(BUILD_DIR)/msdos.o \
	 $(BUILD_DIR)/blk_core.o \
	 $(BUILD_DIR)/chardev.o $(BUILD_DIR)/mount.o $(BUILD_DIR)/vfs.o $(BUILD_DIR)/devfs.o $(BUILD_DIR)/tmpfs.o $(BUILD_DIR)/fd.o $(BUILD_DIR)/syscall_fs.o $(BUILD_DIR)/fs_regression.o \
	 $(BUILD_DIR)/frogfs.o $(BUILD_DIR)/inode.o $(BUILD_DIR)/ffs_utils.o $(BUILD_DIR)/ffs_super_block.o \
	 $(BUILD_DIR)/kasan.o $(BUILD_DIR)/memory.o $(BUILD_DIR)/mm_test.o $(BUILD_DIR)/uaccess.o \
	 $(BUILD_DIR)/panic.o $(BUILD_DIR)/qemu_test.o \
	 $(BUILD_DIR)/select.o $(BUILD_DIR)/fork.o $(BUILD_DIR)/exit.o $(BUILD_DIR)/syscall.o \
	 $(BUILD_DIR)/isa_bus_core.o $(BUILD_DIR)/plt_bus_core.o $(BUILD_DIR)/bus.o $(BUILD_DIR)/device.o \
	 $(BUILD_DIR)/driver.o \
	 $(BUILD_DIR)/ps2_mouse_driver.o $(BUILD_DIR)/ps2_kbd_driver.o $(BUILD_DIR)/ide.o \
	 $(BUILD_DIR)/printk.o $(BUILD_DIR)/print.o $(BUILD_DIR)/vga.o $(BUILD_DIR)/debugcon.o \
	 $(BUILD_DIR)/bitmap.o $(BUILD_DIR)/list.o $(BUILD_DIR)/stdio.o $(BUILD_DIR)/fifo.o $(BUILD_DIR)/string.o $(BUILD_DIR)/ioqueue.o \
	 $(BUILD_DIR)/cpu.o $(BUILD_DIR)/boot.o \


# obj-y = $(BUILD_DIR)/*.o


# Build objects through the existing directory ownership hierarchy.  The
# second make invocation starts after that traversal, so link freshness is
# evaluated against object timestamps updated by the recursive builds.
prepare_build:
	@mkdir -p $(DEPDIR) $(BUILD_DIR)

build_objects: prepare_build
	$(MAKE) -C ./user/
	$(MAKE) -C ./arch/x86/
	$(MAKE) -C ./lib/
	$(MAKE) -C ./init/
	$(MAKE) -C ./mm/
	$(MAKE) -C ./kernel/
	$(MAKE) -C ./drivers/
	$(MAKE) -C ./fs/
	$(MAKE) -C ./block/

core: build_objects
	$(MAKE) $(CORE_IMAGE)

debug: build_objects
	$(MAKE) $(CORE_SYMBOL_IMAGE)

$(CORE_SYMBOL_IMAGE): $(obj-y)
	$(LD) $(LD_DEBUG_FLAG) -o $@ $^

$(CORE_IMAGE): $(obj-y)
	$(LD) $(LD_FLAG) -o $@ $^

.PHONY: prepare_build build_objects core debug clean

sb:
	@ls build

clean:
	rm -f $(DEPDIR)/*.d
	rm -f $(BUILD_DIR)/*
