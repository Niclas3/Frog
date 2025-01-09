include ../../Makefile.os_rules
subdir += printk/
subdir += irq/
subdir += thread/
subdir += time/

obj-y += softirq.o
obj-y += panic.o
# obj-y += syscall.o

all: $(subdir) $(obj-y)

$(subdir): %:
	$(MAKE) -C $@

$(obj-y) : %.o : %.c
	$(CC) $(C_FLAG) $(DEPDIR)/$(@:.o=.d) -o $(BUILD_DIR)/$@  $<

.PHONY: $(subdir) clean
clean:
	rm -rf ./*.o

