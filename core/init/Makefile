# init
include ../../Makefile.os_rules

obj-y += main.o
obj-y += init.o
ifeq ($(FROG_TEST_PROFILE),poudland-builtin-smoke)
obj-y += poudland_builtin_test.o
endif
ifeq ($(FROG_TEST_PROFILE),frogfs-image-smoke)
obj-y += frogfs_image_test.o
endif
ifeq ($(FROG_TEST_PROFILE),system-init-selection-smoke)
obj-y += system_init_selection_test.o
endif
ifeq ($(FROG_TEST_PROFILE),graphical-init-production-smoke)
obj-y += graphical_init_production_test.o
endif
ifneq ($(filter desktop-smoke desktop-soak-10m,$(FROG_TEST_PROFILE)),)
obj-y += desktop_production_test.o
endif

.PHONY: all clean
all: $(build-obj-y)

clean:
	rm -rf ./*.o
