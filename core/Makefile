 # use ELF format
 # Real OS code ###########################
include ../Makefile.os_rules
CORE = core

# $(BUILD_DIR)/exec.o
# $(BUILD_DIR)/exit.o
# $(BUILD_DIR)/fork.o

obj-y  = $(BUILD_DIR)/head32.o  \
	 $(BUILD_DIR)/main.o $(BUILD_DIR)/init.o \
	 $(BUILD_DIR)/process.o $(BUILD_DIR)/sched.o $(BUILD_DIR)/semaphore.o \
	 $(BUILD_DIR)/switch.o $(BUILD_DIR)/threads.o \
	 $(BUILD_DIR)/timer.o \
	 $(BUILD_DIR)/controller.o $(BUILD_DIR)/irq.o $(BUILD_DIR)/softirq.o \
	 $(BUILD_DIR)/syscall-init.o $(BUILD_DIR)/descriptor.o \
	 $(BUILD_DIR)/exceptions.o $(BUILD_DIR)/idt.o \
	 $(BUILD_DIR)/i8253.o $(BUILD_DIR)/i8259a.o $(BUILD_DIR)/segment.o \
	 $(BUILD_DIR)/tss.o $(BUILD_DIR)/pc_plt_init.o \
	 $(BUILD_DIR)/memory.o \
	 $(BUILD_DIR)/panic.o \
	 $(BUILD_DIR)/isa_bus_core.o $(BUILD_DIR)/plt_bus_core.o $(BUILD_DIR)/bus.o $(BUILD_DIR)/device.o \
	 $(BUILD_DIR)/driver.o \
	 $(BUILD_DIR)/ps2_kbd_driver.o \
	 $(BUILD_DIR)/printk.o $(BUILD_DIR)/print.o \
	 $(BUILD_DIR)/bitmap.o $(BUILD_DIR)/list.o $(BUILD_DIR)/stdio.o $(BUILD_DIR)/fifo.o $(BUILD_DIR)/string.o $(BUILD_DIR)/ioqueue.o \
	 $(BUILD_DIR)/cpu.o $(BUILD_DIR)/boot.o \


# obj-y = $(BUILD_DIR)/*.o


# link object files, the final file of kernel is named `core.img`
prepare_build:
	@mkdir -p $(DEPDIR)

core:prepare_build core.img

debug: core_symbol.img


core_symbol.img: $(obj-y)
	ld $(LD_DEBUG_FLAG) -o $(BUILD_DIR)/$@ $^

core.img: $(obj-y)
	ld $(LD_FLAG) -o $(BUILD_DIR)/$@  $^

# $(MAKE) -C ./fs/
# $(MAKE) -C ./block/

# $(MAKE) -C ./ipc/
# $(MAKE) -C ./net/
$(obj-y):
	$(MAKE) -C ./arch/x86/
	$(MAKE) -C ./lib/
	$(MAKE) -C ./init/
	$(MAKE) -C ./mm/
	$(MAKE) -C ./kernel/
	$(MAKE) -C ./drivers/

sb:
	@ls build

clean:
	rm -rf ./.deps/*.d
	cd ./build && rm -rf ./*
