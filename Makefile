AS := nasm
OUTBIN := out.bin

# all: build load hanbote.sys
start: build load
	bochs

newimg:
	cp ../a.img .
#C:10 H:2 S:18
mount: load
	sudo mount -o loop a.img /mnt/floppy
	sudo cp haribote.sys /mnt/floppy -v
	sudo umount /mnt/floppy

install: build
	dd if=$(OUTBIN) of=a.img bs=512 count=360 conv=notrunc

build: $(OUTBIN) haribote.sys

$(OUTBIN):ipl10.s
	$(AS) $< -o $(OUTBIN)

haribote.sys: asmhead.s
	$(AS) $< -o $@

clean:
	rm -rf *.bin
	rm -rf *.sys
