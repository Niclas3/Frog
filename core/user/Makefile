include ../../Makefile.os_rules

OBJCOPY ?= objcopy

USER_SMOKE_OBJ := $(BUILD_DIR)/user_smoke_user.o
USER_SMOKE_ELF := $(BUILD_DIR)/user_smoke.elf
USER_SMOKE_BIN := $(BUILD_DIR)/user_smoke.bin
USER_SMOKE_BLOB := $(BUILD_DIR)/user_smoke_blob.o
USER_SMOKE_DEP := $(DEPDIR)/user_smoke_user.d

USER_CFLAGS := -c -fno-pie -ffreestanding -fno-stack-protector \
	-fno-builtin -fno-asynchronous-unwind-tables -fno-unwind-tables \
	-march=i386 -m32 -Os -Wall -Wextra \
	$(foreach D, $(INCLUDES), -I$(D))

.PHONY: all clean
all: $(USER_SMOKE_BLOB)

$(USER_SMOKE_OBJ): user_smoke.c
	$(CC) $(USER_CFLAGS) -MMD -MP -MT $@ -MF $(USER_SMOKE_DEP) -o $@ $<

$(USER_SMOKE_ELF): $(USER_SMOKE_OBJ) user_smoke.ld
	$(LD) -m elf_i386 -T user_smoke.ld -o $@ $(USER_SMOKE_OBJ)

$(USER_SMOKE_BIN): $(USER_SMOKE_ELF)
	$(OBJCOPY) -O binary $< $@
	@test "$$(wc -c < $@)" -le 4096 || \
		{ echo "user-smoke image exceeds one page" >&2; exit 1; }

$(USER_SMOKE_BLOB): $(USER_SMOKE_BIN)
	cd $(BUILD_DIR) && \
		$(LD) -m elf_i386 -r -b binary user_smoke.bin \
		-o user_smoke_blob.o
	$(OBJCOPY) \
		--rename-section .data=.rodata.user_image,alloc,load,readonly,data,contents \
		$@

clean:
	@:
