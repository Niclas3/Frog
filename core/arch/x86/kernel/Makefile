include ../../../../Makefile.os_rules
obj-y += head32.o
obj-y += exceptions.o
obj-y += descriptor.o
obj-y += idt.o
obj-y += segment.o
obj-y += tss.o


.PHONY: all clean
all: $(build-obj-y)

clean:
	rm -rf ./*.o
