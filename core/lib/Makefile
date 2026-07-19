# Lib
include ../../Makefile.os_rules

obj-y += string.o
obj-y += list.o
obj-y += bitmap.o
obj-y += stdio.o
obj-y += refcount.o

# fifo.o just be used by fsk_mouse.c
obj-y += fifo.o
# hashmap.o just used by apps/compositor
# obj-y += hashmap.o


.PHONY: all clean
all: $(build-obj-y)

clean:
	rm -rf ./*.o
