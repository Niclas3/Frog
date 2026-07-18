include ../../Makefile.os_rules
subdir += partitions/

obj-y += blk_core.o

all: $(subdir) $(build-obj-y)

$(subdir): %:
	$(MAKE) -C $@

.PHONY: all $(subdir) clean
clean:
	rm -rf ./*.o
