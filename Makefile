include ./Makefile.os_rules
BOCHS := bochs -q
DISK = hd.img
BOOTER = MBR.bin
LOADER = loader.img
CORE   = core.img
CORESYM   = core_symbol.img
FONT   = hankaku_font.img
TEST_PROC = core/apps/build/compositor
# TEST_PROC = core/apps/build/ls
TEST_IMG= core/apps/test/b.bmp

# Use ELF format
# Real OS code ###########################
core.img:
	cd ./core && $(MAKE) all
core_symbol.img:
	cd ./core && $(MAKE) debug
##########################################

# To protected mode ###############################
loader.img:                           #
	cd ./booter && $(MAKE) $@
###################################################

# Build bootloader from floppy ###########################################
ipl10.bin:
	cd ./booter && $(MAKE) $@
##############################################################
# Build bootloader from hard disk ###########################################
MBR.bin:
	cd ./booter && $(MAKE) $@
##############################################################

.PHONY:clean clean-all font start reset newimg mount umount load_core bootloader

start: clean-all newimg mount
	$(BOCHS)

reset:  clean umount newimg mount
	$(BOCHS)

newimg:
	cp ../hd.img ./$(DISK)

newhd80img:
	cp ../hd80M.img ./hd80M.img

#C:10 H:2 S:18
mount: bootloader loader.img core.img font
	dd if=$(LOADER) of=$(DISK) bs=512 count=300 seek=2 conv=notrunc #loader
	dd if=$(CORE) of=$(DISK) bs=512 count=300 seek=13 conv=notrunc  #core 122k (blank is 1M)
	dd if=$(FONT) of=$(DISK) bs=512 count=300 seek=2048 conv=notrunc #font.img for now size 4k place to offset 1M
	dd if=$(TEST_PROC) of=$(DISK) bs=512 count=300 seek=3000 conv=notrunc
	dd if=$(TEST_IMG) of=$(DISK) bs=512 count=300 seek=6144 conv=notrunc # place to 3M img size < 150k

mount_debug: bootloader loader.img core_symbol.img font
	dd if=$(LOADER) of=$(DISK) bs=512 count=300 seek=2 conv=notrunc #loader
	dd if=$(CORE) of=$(DISK) bs=512 count=300 seek=13 conv=notrunc  #core 122k (blank is 1M)
	dd if=$(FONT) of=$(DISK) bs=512 count=300 seek=2048 conv=notrunc #font.img for now size 4k
	dd if=$(TEST_PROC) of=$(DISK) bs=512 count=300 seek=3000 conv=notrunc
	dd if=$(TEST_IMG) of=$(DISK) bs=512 count=300 seek=6144 conv=notrunc # place at 3M, img size < 150k
umount:
	sudo umount /mnt/floppy

load_core: core.img
	sudo cp core.img /mnt/floppy -v
	ls /mnt/floppy

bootloader: $(BOOTER)
	dd if=$< of=$(DISK) bs=512 count=360 conv=notrunc

# Launch OS through qemu 
# NOTE: Remove driftfix=slew if not needed
# -rtc base=localtime,clock=host,driftfix=slew \
# NOTE: -enable-kvm makes RTC and disk accesses slow for me, but can be better accuracy
#
	# qemu-system-i386 \
	# -S -s \
	# -monitor stdio \
	# -m 128m \
	# -drive format=raw,file=$(DISK),if=ide,index=0,media=disk \
	# -drive format=raw,file=hd80M.img,if=ide,index=1,media=disk \
	# -rtc base=localtime,clock=host \
	# -audiodev id=alsa,driver=alsa \
	# -machine pcspk-audiodev=alsa
	
	# qemu-system-i386 \
	# -S -s \
	# -monitor stdio \
	# -m 128m \
	# -enable-kvm \
	# -drive format=raw,file=$(DISK),if=ide,index=0,media=disk \
	# -drive format=raw,file=hd80M.img,if=ide,index=1,media=disk \
	# -rtc base=localtime,clock=host \
	# -audiodev id=alsa,driver=alsa \
	# -machine pcspk-audiodev=alsa
run:
	qemu-system-i386 \
	-S -s \
	-monitor stdio \
	-m 1G \
	-enable-kvm \
	-drive format=raw,file=$(DISK),if=ide,index=0,media=disk \
	-drive format=raw,file=hd80M.img,if=ide,index=1,media=disk \
	-rtc base=localtime,clock=host \
	-audiodev id=alsa,driver=alsa \
	-machine pcspk-audiodev=alsa

debug_run:
	qemu-system-i386 \
	-S -s \
	-monitor stdio \
	-m 1G \
	-drive format=raw,file=$(DISK),if=ide,index=0,media=disk \
	-drive format=raw,file=hd80M.img,if=ide,index=1,media=disk \
	-rtc base=localtime,clock=host \
	-audiodev id=alsa,driver=alsa \
	-machine pcspk-audiodev=alsa \


# Tools ###################################################### 
# Generate font
font :
	cd ./tools/ && $(MAKE) font
############################################################## 

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
