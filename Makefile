include ./Makefile.os_rules
BOCHS := bochs -q
DISK = hd.img
BOOTER = MBR.bin
LOADER = loader.img
CORE   = core.img
FONT   = hankaku_font.img

start: newimg  clean-all mount
	$(BOCHS)

reset:  clean umount newimg mount
	$(BOCHS)

newimg:
	cp ../hd.img ./$(DISK)

#C:10 H:2 S:18
mount: bootloader loader.img core.bin font
	dd if=$(LOADER) of=$(DISK) bs=512 count=300 seek=2 conv=notrunc #loader
	dd if=$(CORE) of=$(DISK) bs=512 count=300 seek=13 conv=notrunc #core 26 block
	dd if=$(FONT) of=$(DISK) bs=512 count=300 seek=55 conv=notrunc #font.img for now size 4k
	# sudo mount -o loop $(DISK) /mnt/floppy 
	# sudo cp loader.img /mnt/floppy -v
	# sudo cp core.bin /mnt/floppy -v
	# sudo cp name.txt /mnt/floppy -v
	# sudo umount /mnt/floppy
umount:
	sudo umount /mnt/floppy

load_core: core.bin
	sudo cp core.bin /mnt/floppy -v
	ls /mnt/floppy

bootloader: $(BOOTER)
	dd if=$< of=$(DISK) bs=512 count=360 conv=notrunc

# Use ELF format
# Real OS code ###########################
core.bin:
	cd ./core && $(MAKE) core
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

# Tools ###################################################### 
# Generate font
.PHONY:font
font :
	cd ./tools/ && $(MAKE) font
############################################################## 

.PHONY:clean
clean:
	rm -rf *.bin
	rm -rf *.o
	rm -rf *.lock
	find . -type f -name "core.*" ! -name "core.s" -delete
	find . -type f -name "*.img" ! -name "a.img" -delete
clean-all: clean
	cd ./booter && $(MAKE) clean
	cd ./core   && $(MAKE) clean
	# cd ./tools  && $(MAKE) clean
