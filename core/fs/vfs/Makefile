include ../../../Makefile.os_rules
obj-y += mount.o
obj-y += vfs.o
ifeq ($(FROG_TEST_PROFILE),root-switch-smoke)
obj-y += root_switch_regression.o
endif

all: $(subdir) $(build-obj-y)

$(subdir): %:
	$(MAKE) -C $@

.PHONY: all $(subdir) clean
clean:
	rm -rf ./*.o
