include ./os_rules

start: newimg mount
	bochs

reset:  umount newimg mount 
	bochs

newimg:
	cp ../a.img .

#C:10 H:2 S:18
mount: bootloader pmloader.img core.bin
	sudo mount -o loop a.img /mnt/floppy 
	sudo cp pmloader.img /mnt/floppy -v
	sudo cp core.bin /mnt/floppy -v
	# sudo cp name.txt /mnt/floppy -v
	# sudo umount /mnt/floppy
umount:
	sudo umount /mnt/floppy

load_core: core.bin
	sudo cp core.bin /mnt/floppy -v
	ls /mnt/floppy

bootloader: ipl10.bin
	dd if=$< of=a.img bs=512 count=360 conv=notrunc

# Use ELF format
# Real OS code ###########################
core.bin:
	cd ./core && $(MAKE) core
##########################################

# To protected mode ###############################
pmloader.img:                           #
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
	rm -rf bochsout.txt
	cd ./booter && $(MAKE) clean
	cd ./core   && $(MAKE) clean
	cd ./tools  && $(MAKE) clean
