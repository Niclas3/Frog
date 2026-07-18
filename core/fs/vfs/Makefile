include ../../../Makefile.os_rules
obj-y += mount.o
obj-y += vfs.o

all: $(subdir) $(build-obj-y)

$(subdir): %:
	$(MAKE) -C $@

.PHONY: all $(subdir) clean
clean:
	rm -rf ./*.o
