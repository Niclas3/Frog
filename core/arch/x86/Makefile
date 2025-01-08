subdir += boot/
subdir += entry/
# subdir += video/
subdir += irq/
subdir += kernel/

all: $(subdir)

$(subdir): %:
	$(MAKE) -C $@

.PHONY: $(subdir) clean
clean:
	rm -rf ./*.o

