# init
include ../../Makefile.os_rules

obj-y += main.o
obj-y += init.o
ifeq ($(FROG_TEST_PROFILE),poudland-builtin-smoke)
obj-y += poudland_builtin_test.o
endif

.PHONY: all clean
all: $(build-obj-y)

clean:
	rm -rf ./*.o
