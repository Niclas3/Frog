subdir += boot/
subdir += entry/
subdir += irq/
subdir += kernel/
subdir += platform/

all: $(subdir)

$(subdir): %:
	$(MAKE) -C $@

.PHONY: $(subdir) clean
clean:
	rm -rf ./*.o

