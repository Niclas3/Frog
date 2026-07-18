include ../../../Makefile.os_rules
obj-y += devfs.o

all: $(subdir) $(build-obj-y)

$(subdir): %:
	$(MAKE) -C $@

.PHONY: all $(subdir) clean
clean:
	rm -rf ./*.o
