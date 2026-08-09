SKIP_DEPENDENCIES := 1
include ../Makefile.os_rules
CORE = core
CORE_IMAGE := $(BUILD_DIR)/core.img
CORE_SYMBOL_IMAGE := $(BUILD_DIR)/core_symbol.img

# $(BUILD_DIR)/exit.o
# $(BUILD_DIR)/fork.o

obj-y  = $(BUILD_DIR)/head32.o  \
	 $(BUILD_DIR)/main.o $(BUILD_DIR)/init.o \
	 $(BUILD_DIR)/user_smoke_basic_blob.o \
	 $(BUILD_DIR)/user_smoke_process_blob.o \
	 $(BUILD_DIR)/user_smoke_disk_prepare_blob.o \
	 $(BUILD_DIR)/user_smoke_framebuffer_mmap_blob.o \
	 $(BUILD_DIR)/user_smoke_anonymous_mmap_blob.o \
	 $(BUILD_DIR)/user_smoke_user_allocator_blob.o \
	 $(BUILD_DIR)/user_smoke_packagefs_blob.o \
	 $(BUILD_DIR)/user_smoke_packagefs_lifecycle_blob.o \
	 $(BUILD_DIR)/user_smoke_packagefs_userlib_blob.o \
	 $(BUILD_DIR)/user_smoke_poudland_v1_connect_blob.o \
	 $(BUILD_DIR)/user_smoke_poudland_v1_lifecycle_blob.o \
	 $(BUILD_DIR)/user_smoke_poudland_v1_version_blob.o \
	 $(BUILD_DIR)/user_smoke_poudland_v1_errno_blob.o \
	 $(BUILD_DIR)/user_smoke_poudland_v1_id_blob.o \
	 $(BUILD_DIR)/user_smoke_poudland_v1_routing_blob.o \
	 $(BUILD_DIR)/user_smoke_poudland_v1_retry_blob.o \
	 $(BUILD_DIR)/user_smoke_poudland_v1_create_blob.o \
	 $(BUILD_DIR)/user_smoke_poudland_v1_close_blob.o \
	 $(BUILD_DIR)/user_smoke_poudland_v1_error_blob.o \
	 $(BUILD_DIR)/user_smoke_poudland_v1_protocol_blob.o \
	 $(BUILD_DIR)/user_smoke_poudland_v1_fatal_blob.o \
	 $(BUILD_DIR)/user_smoke_poudland_v1_overflow_blob.o \
	 $(BUILD_DIR)/user_smoke_poudland_v1_hup_blob.o \
	 $(BUILD_DIR)/user_smoke_input_blob.o \
	 $(BUILD_DIR)/user_smoke_time_blob.o \
	 $(BUILD_DIR)/user_smoke_wait2_blob.o \
	 $(BUILD_DIR)/exec_target_blob.o \
	 $(BUILD_DIR)/exec.o $(BUILD_DIR)/process.o $(BUILD_DIR)/process_regression.o $(BUILD_DIR)/sched.o $(BUILD_DIR)/semaphore.o \
	 $(BUILD_DIR)/switch.o $(BUILD_DIR)/threads.o \
	 $(BUILD_DIR)/timer.o $(BUILD_DIR)/timekeeping.o \
	 $(BUILD_DIR)/controller.o $(BUILD_DIR)/irq.o $(BUILD_DIR)/softirq.o \
	 $(BUILD_DIR)/syscall-init.o $(BUILD_DIR)/descriptor.o \
	 $(BUILD_DIR)/page_fault.o \
	 $(BUILD_DIR)/exceptions.o $(BUILD_DIR)/idt.o \
	 $(BUILD_DIR)/i8253.o $(BUILD_DIR)/i8259a.o $(BUILD_DIR)/segment.o \
	 $(BUILD_DIR)/tss.o $(BUILD_DIR)/pc_plt_init.o $(BUILD_DIR)/framebuffer.o $(BUILD_DIR)/framebuffer_smoke.o \
	 $(BUILD_DIR)/msdos.o \
	 $(BUILD_DIR)/blk_core.o \
	 $(BUILD_DIR)/chardev.o $(BUILD_DIR)/mount.o $(BUILD_DIR)/vfs.o $(BUILD_DIR)/devfs.o $(BUILD_DIR)/tmpfs.o $(BUILD_DIR)/packagefs.o $(BUILD_DIR)/fd.o $(BUILD_DIR)/syscall_fs.o $(BUILD_DIR)/fs_regression.o \
	 $(BUILD_DIR)/frogfs.o $(BUILD_DIR)/inode.o $(BUILD_DIR)/ffs_utils.o $(BUILD_DIR)/ffs_super_block.o \
	 $(BUILD_DIR)/kasan.o $(BUILD_DIR)/memory.o $(BUILD_DIR)/bootmem.o $(BUILD_DIR)/phys_resource.o $(BUILD_DIR)/mm_test.o $(BUILD_DIR)/uaccess.o $(BUILD_DIR)/vm.o \
	 $(BUILD_DIR)/panic.o $(BUILD_DIR)/qemu_test.o \
	 $(BUILD_DIR)/select.o $(BUILD_DIR)/fork.o $(BUILD_DIR)/exit.o $(BUILD_DIR)/syscall.o \
	 $(BUILD_DIR)/isa_bus_core.o $(BUILD_DIR)/plt_bus_core.o $(BUILD_DIR)/bus.o $(BUILD_DIR)/device.o \
	 $(BUILD_DIR)/driver.o \
	 $(BUILD_DIR)/ps2_mouse_driver.o $(BUILD_DIR)/ps2_kbd_driver.o $(BUILD_DIR)/ide.o \
	 $(BUILD_DIR)/printk.o $(BUILD_DIR)/print.o $(BUILD_DIR)/vga.o $(BUILD_DIR)/debugcon.o \
	 $(BUILD_DIR)/bitmap.o $(BUILD_DIR)/list.o $(BUILD_DIR)/stdio.o $(BUILD_DIR)/refcount.o $(BUILD_DIR)/fifo.o $(BUILD_DIR)/string.o $(BUILD_DIR)/ioqueue.o \
	 $(BUILD_DIR)/cpu.o $(BUILD_DIR)/boot.o \

ifneq ($(QEMU_TEST),1)
obj-y += $(BUILD_DIR)/user_smoke_graphical_init_blob.o
else ifeq ($(FROG_TEST_PROFILE),desktop-smoke)
obj-y += $(BUILD_DIR)/user_smoke_graphical_init_blob.o
else ifeq ($(FROG_TEST_PROFILE),desktop-soak-10m)
obj-y += $(BUILD_DIR)/user_smoke_graphical_init_blob.o
endif

ifeq ($(FROG_TEST_PROFILE),poudland-builtin-smoke)
obj-y += $(BUILD_DIR)/user_smoke_poudland_builtin_blob.o \
	 $(BUILD_DIR)/poudland_p0_elf_blob.o \
	 $(BUILD_DIR)/poudland_p0_cursor_blob.o \
	 $(BUILD_DIR)/poudland_builtin_test.o
endif

ifeq ($(FROG_TEST_PROFILE),poudland-e2e-smoke)
obj-y += $(BUILD_DIR)/user_smoke_poudland_e2e_bootstrap_blob.o
endif
ifeq ($(FROG_TEST_PROFILE),frogfs-image-smoke)
obj-y += $(BUILD_DIR)/poudland_p0_cursor_blob.o \
	 $(BUILD_DIR)/frogfs_image_test.o
endif
ifeq ($(FROG_TEST_PROFILE),frogfs-exec-smoke)
obj-y += $(BUILD_DIR)/user_smoke_frogfs_exec_blob.o
endif


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
