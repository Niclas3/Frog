# Lib
include ../../Makefile.os_rules

obj-y += string.o
obj-y += list.o
obj-y += bitmap.o
obj-y += stdio.o
# obj-y += ioqueue.o

# fifo.o just be used by fsk_mouse.c
# obj-y += fifo.o
# hashmap.o just used by apps/compositor
# obj-y += hashmap.o
# stdio.o


.PHONY = all
all: $(obj-y)

$(obj-y) : %.o : %.c
	$(CC) $(C_FLAG) $(DEPDIR)/$(@:.o=.d) -o $(BUILD_DIR)/$@  $<

clean:
	rm -rf ./*.o
