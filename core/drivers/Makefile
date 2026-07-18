# subdir += rtc/
# subdir += tty/
subdir += input/
subdir += block/
subdir += base/
subdir += isa/
subdir += platform/


all: $(subdir)

$(subdir): %:
	$(MAKE) -C $@

.PHONY: all $(subdir) clean
clean:
	rm -rf ./*.o
