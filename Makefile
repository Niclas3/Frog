include ./Makefile.os_rules
BOCHS := bochs -q
DISK = hd.img
INIT_BOOT_CODE = MBR.bin
LOADER = loader.img
CORE   = core.img
CORESYM   = $(BUILD_DIR)/core_symbol.img
FONT   = hankaku_font.img
TEST_PROC = core/apps/build/compositor
# TEST_PROC = core/apps/build/ls
TEST_IMG= core/apps/test/b.bmp

# Use ELF format
#kernel code ###########################
core.img:
	cd ./core && $(MAKE) core

# OS code with symbol for debug
core_symbol.img:
	cd ./core && $(MAKE) debug
##########################################

#bootloader
loader.img:                           #
	cd ./booter && $(MAKE) $@
#Initial Boot code for floppy
ipl10.bin:
	cd ./booter && $(MAKE) $@
#Initial Boot code for hard disk
MBR.bin:
	cd ./booter && $(MAKE) $@
##############################################################

.PHONY:clean clean-all font start reset newimg mount umount load_core init_boot_code 

# Tools ###################################################### 
# Generate homemade font
font :
	cd ./tools/ && $(MAKE) font

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
newimg:
	cp ../hd.img ./$(DISK)

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
init_boot_code: $(INIT_BOOT_CODE)
	dd if=$< of=$(DISK) bs=512 count=360 conv=notrunc

# mount all things
# mount_debug ONLY ONE difference is core_symbol.img were used.
#
# 1. Burn `bootloader code` 
# 2. Burn `kernel code`
# 3. Burn `font`
# 4. Burn test things (programs)
# 5. Burn test things (images)
mount: init_boot_code loader.img core.img font
	dd if=$(LOADER) of=$(DISK) bs=512 count=300 seek=2 conv=notrunc #loader
	dd if=$(CORE) of=$(DISK) bs=512 count=300 seek=13 conv=notrunc  #core 122k (blank is 1M)
	dd if=$(FONT) of=$(DISK) bs=512 count=300 seek=2048 conv=notrunc #font.img for now size 4k place to offset 1M
	dd if=$(TEST_PROC) of=$(DISK) bs=512 count=300 seek=3000 conv=notrunc
	dd if=$(TEST_IMG) of=$(DISK) bs=512 count=300 seek=6144 conv=notrunc # place to 3M img size < 150k

mount_debug: init_boot_code loader.img core_symbol.img font
	dd if=$(LOADER) of=$(DISK) bs=512 count=300 seek=2 conv=notrunc #loader
	dd if=$(CORESYM) of=$(DISK) bs=512 count=300 seek=13 conv=notrunc  #core 122k (blank is 1M)
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
run:
	qemu-system-i386 \
	-monitor stdio \
	-m 1G \
	-drive format=raw,file=$(DISK),if=ide,index=0,media=disk \
	-drive format=raw,file=hd80M.img,if=ide,index=1,media=disk \
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
	-m 1G \
	-drive format=raw,file=$(DISK),if=ide,index=0,media=disk \
	-drive format=raw,file=hd80M.img,if=ide,index=1,media=disk \
	-netdev tap,id=net0,ifname=tap0,script=no,downscript=no \
	-device e1000,netdev=net0 \
	-rtc base=localtime,clock=host \
	-audiodev id=alsa,driver=alsa \
	-machine pcspk-audiodev=alsa \
