include ../../../../Makefile.os_rules
subdir += pc/

all: $(subdir)

$(subdir): %:
	$(MAKE) -C $@

.PHONY: all $(subdir) clean

clean:
	rm -rf ./*.o
