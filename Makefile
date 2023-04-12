AS := nasm
OUTBIN := out.bin

# all: build load hanbote.sys
start: build load
	bochs

#C:10 H:2 S:18
load: build
	dd if=$(OUTBIN) of=a.img bs=512 count=360 conv=notrunc

build: $(OUTBIN) hanbote.sys

$(OUTBIN):ipl10.s
	$(AS) $< -o $(OUTBIN)

hanbote.sys: hanbote.s
	$(AS) $< -o $@

clean:
	rm -rf *.bin
	rm -rf *.sys
