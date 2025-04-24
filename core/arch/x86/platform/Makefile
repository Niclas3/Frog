include ../../../../Makefile.os_rules
subdir += pc/

all: $(subdir)

$(subdir): %:
	$(MAKE) -C $@

.PHONY: $(subdir) clean all

clean:
	rm -rf ./*.o
