include ../../../Makefile.os_rules
obj-y += frogfs.o
obj-y += inode.o
obj-y += ffs_utils.o
obj-y += ffs_super_block.o

all: $(subdir) $(build-obj-y)

$(subdir): %:
	$(MAKE) -C $@

.PHONY: all $(subdir) clean
clean:
	rm -rf ./*.o
