include ../../../Makefile.os_rules
obj-y += frogfs.o
obj-y += inode.o
obj-y += ffs_utils.o
obj-y += ffs_super_block.o
obj-y += root_locator.o
obj-y += root_policy.o

ifeq ($(FROG_TEST_PROFILE),root-locator-smoke)
obj-y += root_locator_regression.o
endif

ifeq ($(FROG_TEST_PROFILE),root-namespace-smoke)
obj-y += root_namespace_regression.o
endif

all: $(subdir) $(build-obj-y)

$(subdir): %:
	$(MAKE) -C $@

.PHONY: all $(subdir) clean
clean:
	rm -rf ./*.o
