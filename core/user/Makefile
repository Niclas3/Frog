include ../../Makefile.os_rules

OBJCOPY ?= objcopy

USER_PROFILES := basic process disk_prepare framebuffer_mmap input
USER_BLOBS := $(foreach P,$(USER_PROFILES),$(BUILD_DIR)/user_smoke_$(P)_blob.o)
EXEC_TARGET_BLOB := $(BUILD_DIR)/exec_target_blob.o
USER_CFLAGS := -c -fno-pie -ffreestanding -fno-stack-protector \
	-fno-builtin -fno-asynchronous-unwind-tables -fno-unwind-tables \
	-ffunction-sections -fdata-sections -march=i386 -m32 -Os -Wall -Wextra \
	$(foreach D, $(INCLUDES), -I$(D))

.PHONY: all clean
all: $(USER_BLOBS) $(EXEC_TARGET_BLOB)

$(BUILD_DIR)/user_smoke_basic_user.o: user_smoke.c
	$(CC) $(USER_CFLAGS) -DUSER_SMOKE_BASIC \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_basic_user.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_process_user.o: user_smoke.c
	$(CC) $(USER_CFLAGS) -DUSER_SMOKE_PROCESS \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_process_user.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_disk_prepare_user.o: user_smoke.c
	$(CC) $(USER_CFLAGS) -DUSER_SMOKE_DISK_PREPARE \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_disk_prepare_user.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_framebuffer_mmap_user.o: framebuffer_mmap_smoke.c
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_framebuffer_mmap_user.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_input_user.o: input_smoke.c
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_input_user.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_framebuffer_mmap_runtime.o: ../kernel/syscall.c
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_framebuffer_mmap_runtime.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_process_runtime.o \
$(BUILD_DIR)/user_smoke_disk_prepare_runtime.o: ../kernel/syscall.c
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/$(@F:.o=.d) \
		-o $@ $<

$(BUILD_DIR)/user_smoke_process.elf: \
	$(BUILD_DIR)/user_smoke_process_user.o \
	$(BUILD_DIR)/user_smoke_process_runtime.o user_smoke.ld
	$(LD) --gc-sections -m elf_i386 -T user_smoke.ld -o $@ \
		$(BUILD_DIR)/user_smoke_process_user.o \
		$(BUILD_DIR)/user_smoke_process_runtime.o

$(BUILD_DIR)/user_smoke_disk_prepare.elf: \
	$(BUILD_DIR)/user_smoke_disk_prepare_user.o \
	$(BUILD_DIR)/user_smoke_disk_prepare_runtime.o user_smoke.ld
	$(LD) --gc-sections -m elf_i386 -T user_smoke.ld -o $@ \
		$(BUILD_DIR)/user_smoke_disk_prepare_user.o \
		$(BUILD_DIR)/user_smoke_disk_prepare_runtime.o

$(BUILD_DIR)/user_smoke_framebuffer_mmap.elf: \
	$(BUILD_DIR)/user_smoke_framebuffer_mmap_user.o \
	$(BUILD_DIR)/user_smoke_framebuffer_mmap_runtime.o user_smoke.ld
	$(LD) -m elf_i386 -T user_smoke.ld -o $@ \
		$(BUILD_DIR)/user_smoke_framebuffer_mmap_user.o \
		$(BUILD_DIR)/user_smoke_framebuffer_mmap_runtime.o

$(BUILD_DIR)/user_smoke_%.elf: $(BUILD_DIR)/user_smoke_%_user.o user_smoke.ld
	$(LD) --gc-sections -m elf_i386 -T user_smoke.ld -o $@ $<

$(BUILD_DIR)/user_smoke_%.bin: $(BUILD_DIR)/user_smoke_%.elf
	$(OBJCOPY) -O binary $< $@
	@test "$$(wc -c < $@)" -le 4096 || \
		{ echo "$@ exceeds one page" >&2; exit 1; }

$(BUILD_DIR)/user_smoke_%_blob.o: $(BUILD_DIR)/user_smoke_%.bin
	cd $(BUILD_DIR) && \
		$(LD) -m elf_i386 -r -b binary user_smoke_$*.bin \
		-o user_smoke_$*_blob.o
	$(OBJCOPY) \
		--rename-section .data=.rodata.user_image,alloc,load,readonly,data,contents \
		$@

$(BUILD_DIR)/exec_target_start.o: exec_target_start.S
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/exec_target_start.d \
		-o $@ $<

$(BUILD_DIR)/exec_target_user.o: exec_target.c
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/exec_target_user.d \
		-o $@ $<

$(BUILD_DIR)/exec_target.elf: $(BUILD_DIR)/exec_target_start.o \
	$(BUILD_DIR)/exec_target_user.o exec_target.ld
	$(LD) -N --gc-sections -m elf_i386 -T exec_target.ld -o $@ \
		$(BUILD_DIR)/exec_target_start.o $(BUILD_DIR)/exec_target_user.o

$(EXEC_TARGET_BLOB): $(BUILD_DIR)/exec_target.elf
	cd $(BUILD_DIR) && \
		$(LD) -m elf_i386 -r -b binary exec_target.elf \
		-o exec_target_blob.o
	$(OBJCOPY) \
		--rename-section .data=.rodata.exec_fixture,alloc,load,readonly,data,contents \
		$@

clean:
	@:
