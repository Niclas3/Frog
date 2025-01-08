subdir += printk/
subdir += irq/

all: $(subdir)

$(subdir): %:
	$(MAKE) -C $@

.PHONY: $(subdir) clean
clean:
	rm -rf ./*.o

