include ../../Makefile.os_rules

OBJCOPY ?= objcopy

USER_PROFILES := basic graphical_init process disk_prepare framebuffer_mmap anonymous_mmap user_allocator packagefs packagefs_lifecycle packagefs_userlib poudland_v1_connect poudland_v1_lifecycle poudland_v1_version poudland_v1_errno poudland_v1_id poudland_v1_routing poudland_v1_retry poudland_v1_create poudland_v1_close poudland_v1_error poudland_v1_protocol poudland_v1_fatal poudland_v1_overflow poudland_v1_hup poudland_builtin input time wait2
USER_BLOBS := $(foreach P,$(USER_PROFILES),$(BUILD_DIR)/user_smoke_$(P)_blob.o)
EXEC_TARGET_BLOB := $(BUILD_DIR)/exec_target_blob.o
POUDLAND_P0_ELF_BLOB := $(BUILD_DIR)/poudland_p0_elf_blob.o
POUDLAND_P0_CURSOR_BLOB := $(BUILD_DIR)/poudland_p0_cursor_blob.o
GRAPHICAL_INIT_COMPOSITOR_STUB := \
	$(BUILD_DIR)/graphical-init-compositor-stub.elf
GRAPHICAL_INIT_DESKTOP_STUB := \
	$(BUILD_DIR)/graphical-init-desktop-stub.elf
USER_CFLAGS := -c -fno-pie -ffreestanding -fno-stack-protector \
	-fno-builtin -fno-asynchronous-unwind-tables -fno-unwind-tables \
	-ffunction-sections -fdata-sections -march=i386 -m32 -Os -Wall -Wextra \
	$(foreach D, $(INCLUDES), -I$(D))
ANONYMOUS_MMAP_TEST_CFLAGS :=
ifeq ($(FROG_TEST_ALLOW_MAX_OOM),1)
ANONYMOUS_MMAP_TEST_CFLAGS += -DFROG_TEST_ALLOW_MAX_OOM
endif

.PHONY: all clean FORCE installed-init system-init-selection-artifacts \
	graphical-init-production-artifacts
all: $(USER_BLOBS) $(EXEC_TARGET_BLOB)
ifeq ($(FROG_TEST_PROFILE),poudland-builtin-smoke)
all: $(POUDLAND_P0_ELF_BLOB) $(POUDLAND_P0_CURSOR_BLOB)
endif

installed-init: $(BUILD_DIR)/init.elf $(BUILD_DIR)/init-graphical.elf

ifeq ($(FROG_TEST_PROFILE),system-init-selection-smoke)
ifneq ($(QEMU_TEST),1)
$(error system-init-selection-smoke artifacts require QEMU_TEST=1)
endif
all: system-init-selection-artifacts
endif

ifeq ($(FROG_TEST_PROFILE),graphical-init-production-smoke)
ifneq ($(QEMU_TEST),1)
$(error graphical-init-production-smoke artifacts require QEMU_TEST=1)
endif
all: graphical-init-production-artifacts
endif

graphical-init-production-artifacts: $(GRAPHICAL_INIT_COMPOSITOR_STUB) \
	$(GRAPHICAL_INIT_DESKTOP_STUB)

SYSTEM_INIT_SELECTION_CFLAGS :=
ifeq ($(FROG_TEST_STAGE),open-failure)
SYSTEM_INIT_SELECTION_CFLAGS += -DFROG_SYSTEM_INIT_TEST_OPEN_FAILURE
else ifeq ($(FROG_TEST_STAGE),read-failure)
SYSTEM_INIT_SELECTION_CFLAGS += -DFROG_SYSTEM_INIT_TEST_READ_FAILURE
else ifeq ($(FROG_TEST_STAGE),close-failure)
SYSTEM_INIT_SELECTION_CFLAGS += -DFROG_SYSTEM_INIT_TEST_CLOSE_FAILURE
else ifeq ($(FROG_TEST_STAGE),exec-returned)
SYSTEM_INIT_SELECTION_CFLAGS += -DFROG_SYSTEM_INIT_TEST_EXEC_RETURNED
endif

system-init-selection-artifacts: \
	$(BUILD_DIR)/system-init-selection-init.elf \
	$(BUILD_DIR)/system-init-selection-target.elf

$(BUILD_DIR)/system_init_selection_start.o: ../apps/start.s
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/system_init_selection_start.d \
		-o $@ $<

$(BUILD_DIR)/system_init_selection_runtime.o: ../kernel/syscall.c
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/system_init_selection_runtime.d \
		-o $@ $<

$(BUILD_DIR)/system_init_selection_init.o: system_init.c
	$(CC) $(USER_CFLAGS) -DCONFIG_QEMU_TEST \
		-DCONFIG_FROG_TEST_SYSTEM_INIT_SELECTION \
		-DFROG_SYSTEM_INIT_SELECTION_TEST_BUILD \
		$(SYSTEM_INIT_SELECTION_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/system_init_selection_init.d \
		-o $@ $<

$(BUILD_DIR)/system_init_selection_target.o: system_init_selection_target.c
	$(CC) $(USER_CFLAGS) -DCONFIG_QEMU_TEST \
		-DCONFIG_FROG_TEST_SYSTEM_INIT_SELECTION \
		-MMD -MP -MT $@ -MF $(DEPDIR)/system_init_selection_target.d \
		-o $@ $<

$(BUILD_DIR)/system-init-selection-init.elf: \
	$(BUILD_DIR)/system_init_selection_start.o \
	$(BUILD_DIR)/system_init_selection_init.o \
	$(BUILD_DIR)/system_init_selection_runtime.o system_init.ld
	$(LD) --gc-sections -m elf_i386 -T system_init.ld -o $@ \
		$(BUILD_DIR)/system_init_selection_start.o \
		$(BUILD_DIR)/system_init_selection_init.o \
		$(BUILD_DIR)/system_init_selection_runtime.o

$(BUILD_DIR)/system-init-selection-target.elf: \
	$(BUILD_DIR)/system_init_selection_start.o \
	$(BUILD_DIR)/system_init_selection_target.o \
	$(BUILD_DIR)/system_init_selection_runtime.o system_init.ld
	$(LD) --gc-sections -m elf_i386 -T system_init.ld -o $@ \
		$(BUILD_DIR)/system_init_selection_start.o \
		$(BUILD_DIR)/system_init_selection_target.o \
		$(BUILD_DIR)/system_init_selection_runtime.o

$(BUILD_DIR)/installed_init_start.o: ../apps/start.s
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/installed_init_start.d \
		-o $@ $<

$(BUILD_DIR)/installed_init_runtime.o: ../kernel/syscall.c
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/installed_init_runtime.d \
		-o $@ $<

$(BUILD_DIR)/installed_system_init.o: system_init.c
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/installed_system_init.d \
		-o $@ $<

$(BUILD_DIR)/installed_graphical_init.o: graphical_init.c
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/installed_graphical_init.d \
		-o $@ $<

$(BUILD_DIR)/graphical_init_compositor_stub.o: \
	graphical_init_production_child.c
	$(CC) $(USER_CFLAGS) -DFROG_GRAPHICAL_CHILD_COMPOSITOR \
		-DCONFIG_QEMU_TEST -DCONFIG_FROG_TEST_GRAPHICAL_INIT_PRODUCTION \
		-MMD -MP -MT $@ \
		-MF $(DEPDIR)/graphical_init_compositor_stub.d -o $@ $<

$(BUILD_DIR)/graphical_init_desktop_stub.o: \
	graphical_init_production_child.c
	$(CC) $(USER_CFLAGS) -DFROG_GRAPHICAL_CHILD_DESKTOP \
		-DCONFIG_QEMU_TEST -DCONFIG_FROG_TEST_GRAPHICAL_INIT_PRODUCTION \
		-MMD -MP -MT $@ \
		-MF $(DEPDIR)/graphical_init_desktop_stub.d -o $@ $<

$(GRAPHICAL_INIT_COMPOSITOR_STUB): $(BUILD_DIR)/installed_init_start.o \
	$(BUILD_DIR)/graphical_init_compositor_stub.o \
	$(BUILD_DIR)/installed_init_runtime.o system_init.ld
	$(LD) --gc-sections -m elf_i386 -T system_init.ld -o $@ \
		$(BUILD_DIR)/installed_init_start.o \
		$(BUILD_DIR)/graphical_init_compositor_stub.o \
		$(BUILD_DIR)/installed_init_runtime.o

$(GRAPHICAL_INIT_DESKTOP_STUB): $(BUILD_DIR)/installed_init_start.o \
	$(BUILD_DIR)/graphical_init_desktop_stub.o \
	$(BUILD_DIR)/installed_init_runtime.o system_init.ld
	$(LD) --gc-sections -m elf_i386 -T system_init.ld -o $@ \
		$(BUILD_DIR)/installed_init_start.o \
		$(BUILD_DIR)/graphical_init_desktop_stub.o \
		$(BUILD_DIR)/installed_init_runtime.o

$(BUILD_DIR)/init.elf: $(BUILD_DIR)/installed_init_start.o \
	$(BUILD_DIR)/installed_system_init.o $(BUILD_DIR)/installed_init_runtime.o \
	system_init.ld
	$(LD) --gc-sections -m elf_i386 -T system_init.ld -o $@ \
		$(BUILD_DIR)/installed_init_start.o \
		$(BUILD_DIR)/installed_system_init.o \
		$(BUILD_DIR)/installed_init_runtime.o

$(BUILD_DIR)/init-graphical.elf: $(BUILD_DIR)/installed_init_start.o \
	$(BUILD_DIR)/installed_graphical_init.o $(BUILD_DIR)/installed_init_runtime.o \
	system_init.ld
	$(LD) --gc-sections -m elf_i386 -T system_init.ld -o $@ \
		$(BUILD_DIR)/installed_init_start.o \
		$(BUILD_DIR)/installed_graphical_init.o \
		$(BUILD_DIR)/installed_init_runtime.o
ifeq ($(FROG_TEST_PROFILE),poudland-e2e-smoke)
all: $(BUILD_DIR)/user_smoke_poudland_e2e_bootstrap_blob.o
endif
ifeq ($(FROG_TEST_PROFILE),frogfs-image-smoke)
all: $(POUDLAND_P0_CURSOR_BLOB)
endif
ifeq ($(FROG_TEST_PROFILE),frogfs-exec-smoke)
all: $(BUILD_DIR)/user_smoke_frogfs_exec_blob.o
endif

FORCE:

$(BUILD_DIR)/user_smoke_basic_user.o: user_smoke.c
	$(CC) $(USER_CFLAGS) -DUSER_SMOKE_BASIC \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_basic_user.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_graphical_init_user.o: graphical_init.c
	$(CC) $(USER_CFLAGS) \
		-DFROG_GRAPHICAL_COMPOSITOR_PATH=\"/test/compositor\" \
		-DFROG_GRAPHICAL_DESKTOP_PATH=\"/test/desktop\" \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_graphical_init_user.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_graphical_init_start.o: ../apps/start.s
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_graphical_init_start.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_graphical_init_runtime.o: ../kernel/syscall.c
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_graphical_init_runtime.d \
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

$(BUILD_DIR)/user_smoke_anonymous_mmap_user.o: anonymous_mmap_smoke.c
	$(CC) $(USER_CFLAGS) $(ANONYMOUS_MMAP_TEST_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_anonymous_mmap_user.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_user_allocator_user.o: user_allocator_smoke.c
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_user_allocator_user.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_user_allocator_allocator.o: ../lib/user_malloc.c
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_user_allocator_allocator.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_packagefs_user.o: packagefs_smoke.c
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_packagefs_user.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_packagefs_lifecycle_user.o: packagefs_lifecycle_smoke.c
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_packagefs_lifecycle_user.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_packagefs_userlib_user.o: packagefs_userlib_smoke.c
	$(CC) -I../apps/include $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_packagefs_userlib_user.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_packagefs_userlib_lib.o: ../apps/lib/packagefs.c
	$(CC) -I../apps/include $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_packagefs_userlib_lib.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_packagefs_userlib_runtime.o: ../kernel/syscall.c
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_packagefs_userlib_runtime.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_poudland_v1_connect_user.o: poudland_v1_connect_smoke.c
	$(CC) -I../apps/include $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_poudland_v1_connect_user.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_poudland_v1_connect_packagefs.o: ../apps/lib/packagefs.c
	$(CC) -I../apps/include $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_poudland_v1_connect_packagefs.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_poudland_v1_connect_client.o: ../apps/lib/gua/poudland_v1.c
	$(CC) -I../apps/include $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_poudland_v1_connect_client.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_poudland_v1_connect_runtime.o: ../kernel/syscall.c
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_poudland_v1_connect_runtime.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_poudland_v1_lifecycle_user.o: poudland_v1_lifecycle_smoke.c
	$(CC) -I../apps/include $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_poudland_v1_lifecycle_user.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_poudland_v1_lifecycle_packagefs.o: ../apps/lib/packagefs.c
	$(CC) -I../apps/include $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_poudland_v1_lifecycle_packagefs.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_poudland_v1_lifecycle_client.o: ../apps/lib/gua/poudland_v1.c
	$(CC) -I../apps/include $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_poudland_v1_lifecycle_client.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_poudland_v1_lifecycle_runtime.o: ../kernel/syscall.c
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_poudland_v1_lifecycle_runtime.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_poudland_v1_create_user.o: poudland_v1_wrapper_smoke.c
	$(CC) -I../apps/include $(USER_CFLAGS) -DPOUDLAND_V1_TEST_CREATE \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_poudland_v1_create_user.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_poudland_v1_close_user.o: poudland_v1_wrapper_smoke.c
	$(CC) -I../apps/include $(USER_CFLAGS) -DPOUDLAND_V1_TEST_CLOSE \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_poudland_v1_close_user.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_poudland_v1_error_user.o: poudland_v1_wrapper_smoke.c
	$(CC) -I../apps/include $(USER_CFLAGS) -DPOUDLAND_V1_TEST_ERROR \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_poudland_v1_error_user.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_poudland_v1_protocol_user.o: poudland_v1_protocol_smoke.c
	$(CC) -I../apps/include $(USER_CFLAGS) -DPOUDLAND_V1_TEST_PROTOCOL \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_poudland_v1_protocol_user.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_poudland_v1_fatal_user.o: poudland_v1_protocol_smoke.c
	$(CC) -I../apps/include $(USER_CFLAGS) -DPOUDLAND_V1_TEST_FATAL \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_poudland_v1_fatal_user.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_poudland_v1_overflow_user.o: poudland_v1_protocol_smoke.c
	$(CC) -I../apps/include $(USER_CFLAGS) -DPOUDLAND_V1_TEST_OVERFLOW \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_poudland_v1_overflow_user.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_poudland_v1_hup_user.o: poudland_v1_protocol_smoke.c
	$(CC) -I../apps/include $(USER_CFLAGS) -DPOUDLAND_V1_TEST_HUP \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_poudland_v1_hup_user.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_poudland_v1_%_user.o: poudland_v1_%_smoke.c
	$(CC) -I../apps/include $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_poudland_v1_$*_user.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_poudland_v1_%_packagefs.o: ../apps/lib/packagefs.c
	$(CC) -I../apps/include $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_poudland_v1_$*_packagefs.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_poudland_v1_%_client.o: ../apps/lib/gua/poudland_v1.c
	$(CC) -I../apps/include $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_poudland_v1_$*_client.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_poudland_v1_%_runtime.o: ../kernel/syscall.c
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_poudland_v1_$*_runtime.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_input_user.o: input_smoke.c
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_input_user.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_poudland_builtin_user.o: poudland_builtin_smoke.c
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_poudland_builtin_user.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_poudland_builtin_runtime.o: ../kernel/syscall.c
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_poudland_builtin_runtime.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_poudland_e2e_bootstrap_user.o: poudland_e2e_bootstrap.c
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_poudland_e2e_bootstrap_user.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_poudland_e2e_bootstrap_runtime.o: ../kernel/syscall.c
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_poudland_e2e_bootstrap_runtime.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_frogfs_exec_user.o: frogfs_exec_smoke.c
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_frogfs_exec_user.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_time_user.o: time_smoke.c
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_time_user.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_wait2_user.o: wait2_smoke.c
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_wait2_user.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_framebuffer_mmap_runtime.o: ../kernel/syscall.c
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/user_smoke_framebuffer_mmap_runtime.d \
		-o $@ $<

$(BUILD_DIR)/user_smoke_process_runtime.o \
$(BUILD_DIR)/user_smoke_disk_prepare_runtime.o \
$(BUILD_DIR)/user_smoke_user_allocator_runtime.o: ../kernel/syscall.c
	$(CC) $(USER_CFLAGS) \
		-MMD -MP -MT $@ -MF $(DEPDIR)/$(@F:.o=.d) \
		-o $@ $<

$(BUILD_DIR)/user_smoke_process.elf: \
	$(BUILD_DIR)/user_smoke_process_user.o \
	$(BUILD_DIR)/user_smoke_process_runtime.o user_smoke.ld
	$(LD) --gc-sections -m elf_i386 -T user_smoke.ld -o $@ \
		$(BUILD_DIR)/user_smoke_process_user.o \
		$(BUILD_DIR)/user_smoke_process_runtime.o

$(BUILD_DIR)/user_smoke_graphical_init.elf: \
	$(BUILD_DIR)/user_smoke_graphical_init_start.o \
	$(BUILD_DIR)/user_smoke_graphical_init_user.o \
	$(BUILD_DIR)/user_smoke_graphical_init_runtime.o user_smoke.ld
	$(LD) --gc-sections -m elf_i386 -T user_smoke.ld -o $@ \
		$(BUILD_DIR)/user_smoke_graphical_init_start.o \
		$(BUILD_DIR)/user_smoke_graphical_init_user.o \
		$(BUILD_DIR)/user_smoke_graphical_init_runtime.o

$(BUILD_DIR)/user_smoke_disk_prepare.elf: \
	$(BUILD_DIR)/user_smoke_disk_prepare_user.o \
	$(BUILD_DIR)/user_smoke_disk_prepare_runtime.o user_smoke.ld
	$(LD) --gc-sections -m elf_i386 -T user_smoke.ld -o $@ \
		$(BUILD_DIR)/user_smoke_disk_prepare_user.o \
		$(BUILD_DIR)/user_smoke_disk_prepare_runtime.o

$(BUILD_DIR)/user_smoke_user_allocator.elf: \
	$(BUILD_DIR)/user_smoke_user_allocator_user.o \
	$(BUILD_DIR)/user_smoke_user_allocator_runtime.o \
	$(BUILD_DIR)/user_smoke_user_allocator_allocator.o user_smoke.ld
	$(LD) --gc-sections -m elf_i386 -T user_smoke.ld -o $@ \
		$(BUILD_DIR)/user_smoke_user_allocator_user.o \
		$(BUILD_DIR)/user_smoke_user_allocator_runtime.o \
		$(BUILD_DIR)/user_smoke_user_allocator_allocator.o

$(BUILD_DIR)/user_smoke_framebuffer_mmap.elf: \
	$(BUILD_DIR)/user_smoke_framebuffer_mmap_user.o \
	$(BUILD_DIR)/user_smoke_framebuffer_mmap_runtime.o user_smoke.ld
	$(LD) -m elf_i386 -T user_smoke.ld -o $@ \
		$(BUILD_DIR)/user_smoke_framebuffer_mmap_user.o \
		$(BUILD_DIR)/user_smoke_framebuffer_mmap_runtime.o

$(BUILD_DIR)/user_smoke_packagefs_userlib.elf: \
	$(BUILD_DIR)/user_smoke_packagefs_userlib_user.o \
	$(BUILD_DIR)/user_smoke_packagefs_userlib_lib.o \
	$(BUILD_DIR)/user_smoke_packagefs_userlib_runtime.o user_smoke.ld
	$(LD) --gc-sections -m elf_i386 -T user_smoke.ld -o $@ \
		$(BUILD_DIR)/user_smoke_packagefs_userlib_user.o \
		$(BUILD_DIR)/user_smoke_packagefs_userlib_lib.o \
		$(BUILD_DIR)/user_smoke_packagefs_userlib_runtime.o

$(BUILD_DIR)/user_smoke_poudland_v1_connect.elf: \
	$(BUILD_DIR)/user_smoke_poudland_v1_connect_user.o \
	$(BUILD_DIR)/user_smoke_poudland_v1_connect_packagefs.o \
	$(BUILD_DIR)/user_smoke_poudland_v1_connect_client.o \
	$(BUILD_DIR)/user_smoke_poudland_v1_connect_runtime.o user_smoke.ld
	$(LD) --gc-sections -m elf_i386 -T user_smoke.ld -o $@ \
		$(BUILD_DIR)/user_smoke_poudland_v1_connect_user.o \
		$(BUILD_DIR)/user_smoke_poudland_v1_connect_packagefs.o \
		$(BUILD_DIR)/user_smoke_poudland_v1_connect_client.o \
		$(BUILD_DIR)/user_smoke_poudland_v1_connect_runtime.o

$(BUILD_DIR)/user_smoke_poudland_v1_lifecycle.elf: \
	$(BUILD_DIR)/user_smoke_poudland_v1_lifecycle_user.o \
	$(BUILD_DIR)/user_smoke_poudland_v1_lifecycle_packagefs.o \
	$(BUILD_DIR)/user_smoke_poudland_v1_lifecycle_client.o \
	$(BUILD_DIR)/user_smoke_poudland_v1_lifecycle_runtime.o user_smoke.ld
	$(LD) --gc-sections -m elf_i386 -T user_smoke.ld -o $@ \
		$(BUILD_DIR)/user_smoke_poudland_v1_lifecycle_user.o \
		$(BUILD_DIR)/user_smoke_poudland_v1_lifecycle_packagefs.o \
		$(BUILD_DIR)/user_smoke_poudland_v1_lifecycle_client.o \
		$(BUILD_DIR)/user_smoke_poudland_v1_lifecycle_runtime.o

POUDLAND_V1_WRAPPER_ELFS := \
	$(BUILD_DIR)/user_smoke_poudland_v1_create.elf \
	$(BUILD_DIR)/user_smoke_poudland_v1_close.elf \
	$(BUILD_DIR)/user_smoke_poudland_v1_error.elf \
	$(BUILD_DIR)/user_smoke_poudland_v1_protocol.elf \
	$(BUILD_DIR)/user_smoke_poudland_v1_fatal.elf \
	$(BUILD_DIR)/user_smoke_poudland_v1_overflow.elf \
	$(BUILD_DIR)/user_smoke_poudland_v1_hup.elf

$(POUDLAND_V1_WRAPPER_ELFS): \
$(BUILD_DIR)/user_smoke_poudland_v1_%.elf: \
	$(BUILD_DIR)/user_smoke_poudland_v1_%_user.o \
	$(BUILD_DIR)/user_smoke_poudland_v1_%_packagefs.o \
	$(BUILD_DIR)/user_smoke_poudland_v1_%_client.o \
	$(BUILD_DIR)/user_smoke_poudland_v1_%_runtime.o user_smoke.ld
	$(LD) --gc-sections -m elf_i386 -T user_smoke.ld -o $@ \
		$(BUILD_DIR)/user_smoke_poudland_v1_$*_user.o \
		$(BUILD_DIR)/user_smoke_poudland_v1_$*_packagefs.o \
		$(BUILD_DIR)/user_smoke_poudland_v1_$*_client.o \
		$(BUILD_DIR)/user_smoke_poudland_v1_$*_runtime.o

$(BUILD_DIR)/user_smoke_poudland_v1_%.elf: \
	$(BUILD_DIR)/user_smoke_poudland_v1_%_user.o \
	$(BUILD_DIR)/user_smoke_poudland_v1_%_packagefs.o \
	$(BUILD_DIR)/user_smoke_poudland_v1_%_client.o \
	$(BUILD_DIR)/user_smoke_poudland_v1_%_runtime.o user_smoke.ld
	$(LD) --gc-sections -m elf_i386 -T user_smoke.ld -o $@ \
		$(BUILD_DIR)/user_smoke_poudland_v1_$*_user.o \
		$(BUILD_DIR)/user_smoke_poudland_v1_$*_packagefs.o \
		$(BUILD_DIR)/user_smoke_poudland_v1_$*_client.o \
		$(BUILD_DIR)/user_smoke_poudland_v1_$*_runtime.o

$(BUILD_DIR)/user_smoke_poudland_builtin.elf: \
	$(BUILD_DIR)/user_smoke_poudland_builtin_user.o \
	$(BUILD_DIR)/user_smoke_poudland_builtin_runtime.o user_smoke.ld
	$(LD) --gc-sections -m elf_i386 -T user_smoke.ld -o $@ \
		$(BUILD_DIR)/user_smoke_poudland_builtin_user.o \
		$(BUILD_DIR)/user_smoke_poudland_builtin_runtime.o

$(BUILD_DIR)/user_smoke_poudland_e2e_bootstrap.elf: \
	$(BUILD_DIR)/user_smoke_poudland_e2e_bootstrap_user.o \
	$(BUILD_DIR)/user_smoke_poudland_e2e_bootstrap_runtime.o user_smoke.ld
	$(LD) --gc-sections -m elf_i386 -T user_smoke.ld -o $@ \
		$(BUILD_DIR)/user_smoke_poudland_e2e_bootstrap_user.o \
		$(BUILD_DIR)/user_smoke_poudland_e2e_bootstrap_runtime.o

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

$(POUDLAND_P0_ELF_BLOB): FORCE
	$(MAKE) -C ../apps/poudland_p0 QEMU_TEST=$(QEMU_TEST) \
		FROG_TEST_PROFILE=$(FROG_TEST_PROFILE) \
		TARGET=../build/compositor-legacy all
	cd ../apps/build && \
		$(LD) -m elf_i386 -r -b binary compositor-legacy \
		-o $(POUDLAND_P0_ELF_BLOB)
	$(OBJCOPY) \
		--redefine-sym _binary_compositor_legacy_start=_binary_compositor_start \
		--redefine-sym _binary_compositor_legacy_end=_binary_compositor_end \
		--redefine-sym _binary_compositor_legacy_size=_binary_compositor_size \
		--rename-section .data=.rodata.poudland_p0_elf,alloc,load,readonly,data,contents \
		$@

$(POUDLAND_P0_CURSOR_BLOB): ../apps/test/b.bmp
	cd ../apps/test && \
		$(LD) -m elf_i386 -r -b binary b.bmp \
		-o $(POUDLAND_P0_CURSOR_BLOB)
	$(OBJCOPY) \
		--rename-section .data=.rodata.poudland_p0_cursor,alloc,load,readonly,data,contents \
		$@

clean:
	@:
