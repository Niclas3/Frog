include ../../Makefile.os_rules

OBJCOPY ?= objcopy

USER_PROFILES := basic process disk_prepare
USER_BLOBS := $(foreach P,$(USER_PROFILES),$(BUILD_DIR)/user_smoke_$(P)_blob.o)
USER_CFLAGS := -c -fno-pie -ffreestanding -fno-stack-protector \
	-fno-builtin -fno-asynchronous-unwind-tables -fno-unwind-tables \
	-march=i386 -m32 -Os -Wall -Wextra \
	$(foreach D, $(INCLUDES), -I$(D))

.PHONY: all clean
all: $(USER_BLOBS)

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

$(BUILD_DIR)/user_smoke_%.elf: $(BUILD_DIR)/user_smoke_%_user.o user_smoke.ld
	$(LD) -m elf_i386 -T user_smoke.ld -o $@ $<

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

clean:
	@:
