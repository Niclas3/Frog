AS := nasm
OUTBIN := out.bin

all: build load start
start:
	bochs

load: $(OUTBIN)
	dd if=$(OUTBIN) of=a.img bs=512 count=1

build: helloos.s
	$(AS) $< -o $(OUTBIN)


clean:
	rm -rf *.bin
