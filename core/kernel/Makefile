include ../../Makefile.os_rules
subdir += printk/
subdir += irq/
subdir += thread/
subdir += time/

obj-y += softirq.o
obj-y += panic.o
obj-y += qemu_test.o
obj-y += ioqueue.o
obj-y += cpu.o

obj-y += syscall.o

all: $(subdir) $(build-obj-y)

$(subdir): %:
	$(MAKE) -C $@

.PHONY: all $(subdir) clean
clean:
	rm -rf ./*.o
