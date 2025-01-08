include ../../../../Makefile.os_rules
obj-y += head32.o
obj-y += exceptions.o
obj-y += descriptor.o
obj-y += idt.o
obj-y += segment.o
obj-y += tss.o


.PHONY = all
all: $(obj-y)

$(obj-y) : %.o : %.c
	$(CC) $(C_FLAG) $(DEPDIR)/$(@:.o=.d) -o $(BUILD_DIR)/$@  $<

clean:
	rm -rf ./*.o
