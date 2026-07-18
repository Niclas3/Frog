include ../../../Makefile.os_rules

obj-y += printk.o vga.o debugcon.o

.PHONY = all
all: $(obj-y) print.o

$(obj-y) : %.o : %.c
	$(CC) $(C_FLAG) $(DEPDIR)/$(@:.o=.d) -o $(BUILD_DIR)/$@  $<

print.o : print.s
	$(AS) $(A_FLAG) -o $(BUILD_DIR)/$@ $^

clean:
	rm -rf ./*.o
