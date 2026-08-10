include ./Makefile.os_rules
BOCHS := bochs -q
BOOT_DISK_TEMPLATE = ../hd.img
FROG_BOOT_IMAGE = build/frog-boot.img
DISK = $(FROG_BOOT_IMAGE)
INIT_BOOT_CODE = MBR.bin
LOADER = loader.img
CORE   = core/build/core.img
CORESYM   = core/build/core_symbol.img
FONT   = hankaku_font.img
TEST_PROC = core/apps/build/compositor
# TEST_PROC = core/apps/build/ls
TEST_IMG= core/apps/test/b.bmp
FROG_ROOT_IMAGE = build/frog-root.img
FROG_ROOT_MANIFEST = config/frog-root.manifest
FROG_TEST_ROOT_IMAGE = build/frog-test-root.img
FROG_TEST_OVERLAY = config/frog-test.overlay
SECTOR_SIZE := 512
LOADER_SECTOR_COUNT := 11
CORE_START_SECTOR := 13
CORE_SECTOR_COUNT := 512

ifeq ($(QEMU_TEST),1)
LOADER_AD_FLAG := -DVGA_ENABLE
NORMAL_RUNTIME_DEP :=
RUNTIME_DATA_IMAGE := hd80M.img
RUNTIME_DATA_SNAPSHOT :=
else
LOADER_AD_FLAG := -DFRAMEBUFFER_TEST
NORMAL_RUNTIME_DEP := frog-root.img
RUNTIME_DATA_IMAGE := $(FROG_ROOT_IMAGE)
RUNTIME_DATA_SNAPSHOT := ,snapshot=on
endif
RUNTIME_DATA_DRIVE := format=raw,file=$(RUNTIME_DATA_IMAGE),if=ide,index=1,media=disk$(RUNTIME_DATA_SNAPSHOT)
BOOT_DISK_DEP := frog-boot.img

# Use ELF format
#kernel code ###########################
core.img: FORCE
	cd ./core && $(MAKE) clean
	cd ./core && $(MAKE) core

# OS code with symbol for debug
core_symbol.img: core.img FORCE
	cd ./core && $(MAKE) debug
##########################################

#bootloader
loader.img: FORCE                    #
	@mkdir -p $(dir $(FROG_ROOT_IMAGE))
	cd ./booter && $(AS) $(LOADER_AD_FLAG) -p $(AS_INCLUDE) -f bin \
		-o ../build/loader.img.tmp loader.s
	mv -f build/loader.img.tmp $@
#Initial Boot code for floppy
ipl10.bin:
	cd ./booter && $(MAKE) $@
#Initial Boot code for hard disk
MBR.bin: FORCE
	@mkdir -p $(dir $(FROG_ROOT_IMAGE))
	cd ./booter && $(AS) -p $(AS_INCLUDE) -f bin \
		-o ../build/MBR.bin.tmp MBR.s
	mv -f build/MBR.bin.tmp $@
##############################################################

.PHONY: clean clean-all font start reset newimg mount umount load_core \
	init_boot_code frog-boot.img frog-root.img frog-root-verify \
	frog-root-inventory frog-test-root.img frog-test-root-verify frog-root-test FORCE

FORCE:

define run_quiet
	@log_file=$$(mktemp); \
	if ! $(1) >"$$log_file" 2>&1; then \
		cat "$$log_file" >&2; rm -f "$$log_file"; exit 1; \
	fi; \
	rm -f "$$log_file"
endef

frog-boot.img: FORCE
	@mkdir -p $(dir $(FROG_BOOT_IMAGE))
	cp $(BOOT_DISK_TEMPLATE) $(FROG_BOOT_IMAGE).tmp
	mv -f $(FROG_BOOT_IMAGE).tmp $(FROG_BOOT_IMAGE)

# Tools ###################################################### 
# Generate homemade font
font :
	cd ./tools/ && $(MAKE) font

tools/mkfrogfs_image: tools/mkfrogfs_image.c
	$(call run_quiet,$(MAKE) -C tools mkfrogfs_image)

frog-root-inventory: FORCE tools/mkfrogfs_image $(FROG_ROOT_MANIFEST) config/init.conf
	$(call run_quiet,$(MAKE) -C core/apps QEMU_TEST=0 FROG_TEST_PROFILE= clean-production)
	$(call run_quiet,$(MAKE) -C core/apps QEMU_TEST=0 FROG_TEST_PROFILE= production)
	$(call run_quiet,$(MAKE) -B -C core/user QEMU_TEST=0 FROG_TEST_PROFILE= installed-init)
	@./tools/test-production-graphical-paths.sh \
		core/apps/build/compositor core/apps/build/desktop \
		core/build/init-graphical.elf
	@./tools/test-frog-root-inventory.sh $(FROG_ROOT_MANIFEST)

frog-root.img: FORCE frog-root-inventory
	@mkdir -p $(dir $(FROG_ROOT_IMAGE))
	@./tools/mkfrogfs_image --manifest $(FROG_ROOT_MANIFEST) \
		--output $(FROG_ROOT_IMAGE)

frog-root-verify: frog-root.img frog-root-inventory
	@./tools/mkfrogfs_image --manifest $(FROG_ROOT_MANIFEST) \
		--output $(FROG_ROOT_IMAGE) --verify

frog-test-root.img: FORCE frog-root.img tools/mkfrogfs_image $(FROG_ROOT_MANIFEST) $(FROG_TEST_OVERLAY)
	$(call run_quiet,$(MAKE) -C core/apps QEMU_TEST=0 FROG_TEST_PROFILE= clean-test)
	$(call run_quiet,$(MAKE) -C core/apps legacy-compositor)
	$(call run_quiet,$(MAKE) -C core/apps QEMU_TEST=0 FROG_TEST_PROFILE= poudland-e2e-smoke)
	@mkdir -p $(dir $(FROG_TEST_ROOT_IMAGE))
	@./tools/mkfrogfs_image --manifest $(FROG_ROOT_MANIFEST) \
		--overlay $(FROG_TEST_OVERLAY) --output $(FROG_TEST_ROOT_IMAGE)

frog-test-root-verify: frog-test-root.img frog-root.img
	@./tools/mkfrogfs_image --manifest $(FROG_ROOT_MANIFEST) \
		--overlay $(FROG_TEST_OVERLAY) --output $(FROG_TEST_ROOT_IMAGE) --verify

frog-root-test:
	$(call run_quiet,$(MAKE) -C tools test)
	$(call run_quiet,$(MAKE) frog-root.img)
	$(call run_quiet,$(MAKE) frog-root-verify)
	@set -e; \
	base_hash=$$(sha256sum $(FROG_ROOT_IMAGE) | awk '{print $$1}'); \
	base_mtime=$$(stat -c %Y $(FROG_ROOT_IMAGE)); \
	sleep 1; \
	log_file=$$(mktemp); \
	if ! $(MAKE) frog-root.img >"$$log_file" 2>&1; then \
		cat "$$log_file" >&2; rm -f "$$log_file"; exit 1; \
	fi; \
	rm -f "$$log_file"; \
	test "$$base_hash" = "$$(sha256sum $(FROG_ROOT_IMAGE) | awk '{print $$1}')"; \
	test "$$base_mtime" = "$$(stat -c %Y $(FROG_ROOT_IMAGE))"
	@set -e; \
	base_hash=$$(sha256sum $(FROG_ROOT_IMAGE) | awk '{print $$1}'); \
	base_mtime=$$(stat -c %Y $(FROG_ROOT_IMAGE)); \
	sleep 1; \
	log_file=$$(mktemp); \
	if ! $(MAKE) frog-test-root.img >"$$log_file" 2>&1; then \
		cat "$$log_file" >&2; rm -f "$$log_file"; exit 1; \
	fi; \
	rm -f "$$log_file"; \
	test "$$base_hash" = "$$(sha256sum $(FROG_ROOT_IMAGE) | awk '{print $$1}')"; \
	test "$$base_mtime" = "$$(stat -c %Y $(FROG_ROOT_IMAGE))"; \
	log_file=$$(mktemp); \
	if ! $(MAKE) frog-test-root-verify >"$$log_file" 2>&1; then \
		cat "$$log_file" >&2; rm -f "$$log_file"; exit 1; \
	fi; \
	rm -f "$$log_file"; \
	test "$$base_hash" = "$$(sha256sum $(FROG_ROOT_IMAGE) | awk '{print $$1}')"; \
	test "$$base_mtime" = "$$(stat -c %Y $(FROG_ROOT_IMAGE))"

clean:
	rm -rf *.bin
	rm -rf *.o
	rm -rf *.lock
	find . -type f -name "core.*" ! -name "core.s" -delete
	find . -type f -name "*.img" ! -name "hd80M.img" -delete

clean-all: clean
	cd ./booter && $(MAKE) clean
	cd ./core   && $(MAKE) clean
	cd ./core/apps && $(MAKE) clean
	# cd ./tools  && $(MAKE) clean

# a new disk image for kernel install
newimg: frog-boot.img

# a new file 80M disk image
newhd80img:
	cp ../hd80M.img ./hd80M.img

umount:
	sudo umount /mnt/floppy

# load core for check kernel code
load_core: core.img
	sudo cp core.img /mnt/floppy -v
	ls /mnt/floppy

## BUILD commands

# Burn `initial boot code` into first kernel disk.
init_boot_code: $(BOOT_DISK_DEP) $(INIT_BOOT_CODE)
	dd if=$(INIT_BOOT_CODE) of=$(DISK) bs=512 count=360 conv=notrunc

# mount all things
# mount_debug keeps core_symbol.img for GDB, but boots the stripped core.img.
#
# 1. Burn `bootloader code` 
# 2. Burn `kernel code`
# 3. Burn `font`
# 4. Burn test things (programs)
# 5. Burn test things (images)
mount: $(NORMAL_RUNTIME_DEP) init_boot_code loader.img core.img font
	@loader_size=$$(wc -c < "$(LOADER)"); \
	loader_limit=$$(($(LOADER_SECTOR_COUNT) * $(SECTOR_SIZE))); \
	if [ "$$loader_size" -gt "$$loader_limit" ]; then \
		printf '%s is %s bytes; MBR loader limit is %s\n' \
			"$(LOADER)" "$$loader_size" "$$loader_limit" >&2; exit 1; \
	fi
	@core_size=$$(wc -c < "$(CORE)"); \
	core_limit=$$(($(CORE_SECTOR_COUNT) * $(SECTOR_SIZE))); \
	if [ "$$core_size" -gt "$$core_limit" ]; then \
		printf '%s is %s bytes; boot image limit is %s\n' \
			"$(CORE)" "$$core_size" "$$core_limit" >&2; exit 1; \
	fi
	dd if=$(LOADER) of=$(DISK) bs=$(SECTOR_SIZE) count=$(LOADER_SECTOR_COUNT) seek=2 conv=notrunc #loader
	dd if=$(CORE) of=$(DISK) bs=$(SECTOR_SIZE) count=$(CORE_SECTOR_COUNT) seek=$(CORE_START_SECTOR) conv=notrunc #core, reserved through sector 524
	dd if=$(FONT) of=$(DISK) bs=512 count=300 seek=2048 conv=notrunc #font.img for now size 4k place to offset 1M
ifeq ($(QEMU_TEST),1)
	dd if=$(TEST_PROC) of=$(DISK) bs=512 count=300 seek=3000 conv=notrunc
	dd if=$(TEST_IMG) of=$(DISK) bs=512 count=300 seek=6144 conv=notrunc # place to 3M img size < 150k
endif

mount_debug: $(NORMAL_RUNTIME_DEP) init_boot_code loader.img core.img core_symbol.img font
	@loader_size=$$(wc -c < "$(LOADER)"); \
	loader_limit=$$(($(LOADER_SECTOR_COUNT) * $(SECTOR_SIZE))); \
	if [ "$$loader_size" -gt "$$loader_limit" ]; then \
		printf '%s is %s bytes; MBR loader limit is %s\n' \
			"$(LOADER)" "$$loader_size" "$$loader_limit" >&2; exit 1; \
	fi
	@core_size=$$(wc -c < "$(CORE)"); \
	core_limit=$$(($(CORE_SECTOR_COUNT) * $(SECTOR_SIZE))); \
	if [ "$$core_size" -gt "$$core_limit" ]; then \
		printf '%s is %s bytes; boot image limit is %s\n' \
			"$(CORE)" "$$core_size" "$$core_limit" >&2; exit 1; \
	fi
	dd if=$(LOADER) of=$(DISK) bs=$(SECTOR_SIZE) count=$(LOADER_SECTOR_COUNT) seek=2 conv=notrunc #loader
	dd if=$(CORE) of=$(DISK) bs=$(SECTOR_SIZE) count=$(CORE_SECTOR_COUNT) seek=$(CORE_START_SECTOR) conv=notrunc #core, reserved through sector 524
	dd if=$(FONT) of=$(DISK) bs=512 count=300 seek=2048 conv=notrunc #font.img for now size 4k
	# dd if=$(TEST_PROC) of=$(DISK) bs=512 count=300 seek=3000 conv=notrunc
	# dd if=$(TEST_IMG) of=$(DISK) bs=512 count=300 seek=6144 conv=notrunc # place at 3M, img size < 150k


# Launch OS through qemu 
# NOTE: Remove driftfix=slew if not needed
# -rtc base=localtime,clock=host,driftfix=slew \
# HINTS:
# -S -s for debug
# -s shorthand for -gdb tcp::1234
# -enable-kvm
# -enable-kvm \
# NOTE: -enable-kvm makes RTC and disk accesses slow for me, but can be better accuracy
run: mount
	qemu-system-i386 \
	-monitor stdio \
	-chardev file,id=frogdebug,path=build/frog-run-debugcon.log \
	-device isa-debugcon,iobase=0xe9,chardev=frogdebug \
	-m 16M \
	-drive format=raw,file=$(DISK),if=ide,index=0,media=disk \
	-drive "$(RUNTIME_DATA_DRIVE)" \
	-vga std \
	-rtc base=localtime,clock=host \
	-audiodev id=alsa,driver=alsa \
	-machine pcspk-audiodev=alsa \


# NOTE:
#  -netdev the host OS must has a tap type interface named 'tap0'
#  If you don't have it, you can create it use `sudo tuncrl -t tap0 -u `whoami``
#  to create a tap0 interface.
#  I used bridge to exchange network packages.
#  for more infomation please check this (url)[https://niclas3.github.io/2024/12/09/network_bridging_with_qemu.html]
#⚠️ WARNING: This rule disables KVM so GDB can safely insert breakpoints and observe real-mode / protected-mode transitions.
#  Do NOT add -enable-kvm here, or breakpoints may silently fail.
debug_run: mount_debug
	qemu-system-i386 \
	-S \
	-s \
	-monitor stdio \
	-chardev file,id=frogdebug,path=build/frog-debug-debugcon.log \
	-device isa-debugcon,iobase=0xe9,chardev=frogdebug \
	-cpu 486 \
	-m 1G \
	-drive format=raw,file=$(DISK),if=ide,index=0,media=disk \
	-drive "$(RUNTIME_DATA_DRIVE)" \
	-vga std \
	-netdev tap,id=net0,ifname=tap0,script=no,downscript=no \
	-device e1000,netdev=net0 \
	-rtc base=localtime,clock=host \
	-audiodev id=alsa,driver=alsa \
	-machine pcspk-audiodev=alsa \

debug_runv1: mount_debug
	qemu-system-i386 \
	-no-reboot \
	-d int,cpu_reset \
	-monitor stdio \
	-chardev file,id=frogdebug,path=build/frog-debugv1-debugcon.log \
	-device isa-debugcon,iobase=0xe9,chardev=frogdebug \
	-m 1G \
	-drive format=raw,file=$(DISK),if=ide,index=0,media=disk \
	-drive "$(RUNTIME_DATA_DRIVE)" \
	-vga std \
	-netdev tap,id=net0,ifname=tap0,script=no,downscript=no \
	-device e1000,netdev=net0 \
	-rtc base=localtime,clock=host \
	-audiodev id=alsa,driver=alsa \
	-machine pcspk-audiodev=alsa \
