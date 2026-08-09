include ../../Makefile.os_rules

APP_BUILD := build
COMPOSITOR := $(APP_BUILD)/compositor
DESKTOP := $(APP_BUILD)/desktop
APP_CFLAGS := -c -fno-pie -ffreestanding -fno-stack-protector \
	-fno-builtin -fno-asynchronous-unwind-tables -fno-unwind-tables \
	-ffunction-sections -fdata-sections -march=i386 -m32 -Os -Wall -Wextra \
	-Iinclude $(foreach D,$(INCLUDES),-I$(D))

.PHONY: all production compositor desktop poudland-p0 test clean clean-production FORCE

all: production
production: $(COMPOSITOR) $(DESKTOP)
compositor poudland-p0: $(COMPOSITOR)
desktop: $(DESKTOP)

FORCE:

$(APP_BUILD):
	mkdir -p $@

$(COMPOSITOR): FORCE | $(APP_BUILD)
	$(MAKE) -C poudland_p0 all

$(APP_BUILD)/app_start.o: start.s FORCE | $(APP_BUILD)
	$(CC) $(APP_CFLAGS) -o $@ $<

$(APP_BUILD)/desktop.o: desktop.c FORCE | $(APP_BUILD)
	$(CC) $(APP_CFLAGS) -MMD -MP -MF $(APP_BUILD)/desktop.d -o $@ $<

$(DESKTOP): $(APP_BUILD)/app_start.o $(APP_BUILD)/desktop.o \
		poudland_p0/poudland_p0.ld
	$(LD) --gc-sections -m elf_i386 -T poudland_p0/poudland_p0.ld \
		-o $@ $(APP_BUILD)/app_start.o $(APP_BUILD)/desktop.o

test: production
	../../tools/test-production-apps.sh $(COMPOSITOR) $(DESKTOP)

clean-production:
	$(MAKE) -C poudland_p0 clean
	rm -f $(APP_BUILD)/app_start.o $(APP_BUILD)/desktop.o \
		$(APP_BUILD)/desktop.d $(DESKTOP)

clean: clean-production

-include $(APP_BUILD)/desktop.d
