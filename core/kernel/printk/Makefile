include ../../../Makefile.os_rules

obj-y += printk.o vga.o debugcon.o

.PHONY: all clean
all: $(build-obj-y) $(BUILD_DIR)/print.o

$(BUILD_DIR)/print.o: print.s
	$(AS) $(A_FLAG) -o $@ $<

clean:
	rm -rf ./*.o
