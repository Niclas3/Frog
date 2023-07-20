include ./Makefile.os_rules
BOCHS := bochs -q
FLOPPY = b.img

start: newimg mount
	$(BOCHS)

reset:  clean umount newimg mount
	$(BOCHS)

newimg:
	cp ../a.img ./$(FLOPPY)

#C:10 H:2 S:18
mount: bootloader loader.img core.bin
	sudo mount -o loop $(FLOPPY) /mnt/floppy 
	# sudo cp loader.img /mnt/floppy -v
	# sudo cp core.bin /mnt/floppy -v
	sudo cp name.txt /mnt/floppy -v
	# sudo umount /mnt/floppy
umount:
	sudo umount /mnt/floppy

load_core: core.bin
	sudo cp core.bin /mnt/floppy -v
	ls /mnt/floppy

bootloader: ipl10.bin
	dd if=$< of=$(FLOPPY) bs=512 count=360 conv=notrunc

# Use ELF format
# Real OS code ###########################
core.bin:
	cd ./core && $(MAKE) core
##########################################

# To protected mode ###############################
loader.img:                           #
	cd ./booter && $(MAKE) $@
###################################################

# Build bootloader ###########################################
ipl10.bin:
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
	find . -type f -name "core.*" ! -name "core.s" -delete
	find . -type f -name "*.img" ! -name "a.img" -delete
clean-all: clean
	cd ./booter && $(MAKE) clean
	cd ./core   && $(MAKE) clean
	cd ./tools  && $(MAKE) clean
