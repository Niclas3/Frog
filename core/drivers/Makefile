# subdir += block/
# subdir += input/
# subdir += rtc/
# subdir += tty/

subdir += base/
subdir += isa/
subdir += platform/


all: $(subdir)

$(subdir): %:
	$(MAKE) -C $@

.PHONY: $(subdir) clean
clean:
	rm -rf ./*.o
