include ../../../../Makefile.os_rules

obj-y += lfbvideo.o


.PHONY = all
all: $(obj-y)

$(obj-y) : %.o : %.c
	$(CC) $(ILOCINCLUDE) $(IPUBINCLUDE) -o $@ $(C_FLAG) $(DEBUG) $<

clean:
	rm -rf ./*.o
