include ../../../Makefile.os_rules
obj-y += bus.o
obj-y += device.o
obj-y += driver.o
obj-y += chardev.o

all: $(subdir) $(build-obj-y)

$(subdir): %:
	$(MAKE) -C $@

.PHONY: all $(subdir) clean
clean:
	rm -rf ./*.o
