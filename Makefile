include ./Makefile.os_rules
BOCHS := bochs -q
DISK = hd.img
BOOTER = MBR.bin
LOADER = loader.img
CORE   = core.img
CORESYM   = core_symbol.img
FONT   = hankaku_font.img

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
	dd if=$(CORE) of=$(DISK) bs=512 count=300 seek=13 conv=notrunc #core 23k (blank is 64k)
	dd if=$(FONT) of=$(DISK) bs=512 count=300 seek=150 conv=notrunc #font.img for now size 4k
	# sudo mount -o loop $(DISK) /mnt/floppy 
	# sudo cp loader.img /mnt/floppy -v
	# sudo cp core.img /mnt/floppy -v
	# sudo cp name.txt /mnt/floppy -v
	# sudo umount /mnt/floppy

mount_debug: bootloader loader.img core_symbol.img font
	dd if=$(LOADER) of=$(DISK) bs=512 count=300 seek=2 conv=notrunc #loader
	dd if=$(CORE) of=$(DISK) bs=512 count=300 seek=13 conv=notrunc #core 23k (blank is 64k)
	dd if=$(FONT) of=$(DISK) bs=512 count=300 seek=150 conv=notrunc #font.img for now size 4k
umount:
	sudo umount /mnt/floppy

load_core: core.img
	sudo cp core.img /mnt/floppy -v
	ls /mnt/floppy

bootloader: $(BOOTER)
	dd if=$< of=$(DISK) bs=512 count=360 conv=notrunc

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
	# cd ./tools  && $(MAKE) clean
