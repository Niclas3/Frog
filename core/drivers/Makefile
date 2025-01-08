subdir += ide/
subdir += input/
subdir += rtc/
subdir += tty/

all: $(subdir)

$(subdir): %:
	$(MAKE) -C $@

.PHONY: $(subdir) clean
clean:
	rm -rf ./*.o
