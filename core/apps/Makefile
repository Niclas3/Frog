include ../../Makefile.os_rules

APP_BUILD := build
COMPOSITOR := $(APP_BUILD)/compositor
DESKTOP := $(APP_BUILD)/desktop
POUDLAND_E2E_SMOKE := $(APP_BUILD)/poudland-e2e-smoke
APP_CFLAGS := -c -fno-pie -ffreestanding -fno-stack-protector \
	-fno-builtin -fno-asynchronous-unwind-tables -fno-unwind-tables \
	-ffunction-sections -fdata-sections -march=i386 -m32 -Os -Wall -Wextra \
	-Iinclude $(foreach D,$(INCLUDES),-I$(D))

.PHONY: all production compositor desktop poudland-p0 poudland-e2e-smoke \
	test clean clean-production clean-test FORCE

all: production
production: $(COMPOSITOR) $(DESKTOP)
compositor poudland-p0: $(COMPOSITOR)
desktop: $(DESKTOP)
poudland-e2e-smoke: $(POUDLAND_E2E_SMOKE)

FORCE:

$(APP_BUILD):
	mkdir -p $@

$(COMPOSITOR): FORCE | $(APP_BUILD)
	$(MAKE) -C poudland_p0 all

$(APP_BUILD)/app_start.o: start.s FORCE | $(APP_BUILD)
	$(CC) $(APP_CFLAGS) -o $@ $<

$(APP_BUILD)/desktop.o: desktop.c FORCE | $(APP_BUILD)
	$(CC) $(APP_CFLAGS) -MMD -MP -MF $(APP_BUILD)/desktop.d -o $@ $<

$(APP_BUILD)/desktop_packagefs.o: lib/packagefs.c FORCE | $(APP_BUILD)
	$(CC) $(APP_CFLAGS) -MMD -MP \
		-MF $(APP_BUILD)/desktop_packagefs.d -o $@ $<

$(APP_BUILD)/desktop_poudland_v1.o: lib/gua/poudland_v1.c FORCE | $(APP_BUILD)
	$(CC) $(APP_CFLAGS) -MMD -MP \
		-MF $(APP_BUILD)/desktop_poudland_v1.d -o $@ $<

$(APP_BUILD)/desktop_syscall.o: ../kernel/syscall.c FORCE | $(APP_BUILD)
	$(CC) $(APP_CFLAGS) -MMD -MP \
		-MF $(APP_BUILD)/desktop_syscall.d -o $@ $<

$(DESKTOP): $(APP_BUILD)/app_start.o $(APP_BUILD)/desktop.o \
		$(APP_BUILD)/desktop_packagefs.o \
		$(APP_BUILD)/desktop_poudland_v1.o \
		$(APP_BUILD)/desktop_syscall.o \
		poudland_p0/poudland_p0.ld
	$(LD) --gc-sections -m elf_i386 -T poudland_p0/poudland_p0.ld \
		-o $@ $(APP_BUILD)/app_start.o $(APP_BUILD)/desktop.o \
		$(APP_BUILD)/desktop_packagefs.o \
			$(APP_BUILD)/desktop_poudland_v1.o \
			$(APP_BUILD)/desktop_syscall.o

$(APP_BUILD)/poudland_e2e_smoke.o: poudland_e2e_smoke.c FORCE | $(APP_BUILD)
	$(CC) $(APP_CFLAGS) -MMD -MP \
		-MF $(APP_BUILD)/poudland_e2e_smoke.d -o $@ $<

$(APP_BUILD)/poudland_e2e_packagefs.o: lib/packagefs.c FORCE | $(APP_BUILD)
	$(CC) $(APP_CFLAGS) -MMD -MP \
		-MF $(APP_BUILD)/poudland_e2e_packagefs.d -o $@ $<

$(APP_BUILD)/poudland_e2e_client.o: lib/gua/poudland_v1.c FORCE | $(APP_BUILD)
	$(CC) $(APP_CFLAGS) -MMD -MP \
		-MF $(APP_BUILD)/poudland_e2e_client.d -o $@ $<

$(APP_BUILD)/poudland_e2e_syscall.o: ../kernel/syscall.c FORCE | $(APP_BUILD)
	$(CC) $(APP_CFLAGS) -MMD -MP \
		-MF $(APP_BUILD)/poudland_e2e_syscall.d -o $@ $<

$(POUDLAND_E2E_SMOKE): $(APP_BUILD)/app_start.o \
		$(APP_BUILD)/poudland_e2e_smoke.o \
		$(APP_BUILD)/poudland_e2e_packagefs.o \
		$(APP_BUILD)/poudland_e2e_client.o \
		$(APP_BUILD)/poudland_e2e_syscall.o \
		poudland_p0/poudland_p0.ld
	$(LD) --gc-sections -m elf_i386 -T poudland_p0/poudland_p0.ld \
		-o $@ $(APP_BUILD)/app_start.o \
		$(APP_BUILD)/poudland_e2e_smoke.o \
		$(APP_BUILD)/poudland_e2e_packagefs.o \
		$(APP_BUILD)/poudland_e2e_client.o \
		$(APP_BUILD)/poudland_e2e_syscall.o

test: production $(POUDLAND_E2E_SMOKE)
	../../tools/test-production-apps.sh $(COMPOSITOR) $(DESKTOP) \
		$(POUDLAND_E2E_SMOKE)

clean-production:
	$(MAKE) -C poudland_p0 clean
	rm -f $(APP_BUILD)/app_start.o $(APP_BUILD)/desktop*.o \
			$(APP_BUILD)/desktop*.d $(DESKTOP)

clean-test:
	rm -f $(APP_BUILD)/poudland_e2e_*.o \
		$(APP_BUILD)/poudland_e2e_*.d $(POUDLAND_E2E_SMOKE)

clean: clean-production clean-test

-include $(APP_BUILD)/desktop*.d
-include $(APP_BUILD)/poudland_e2e_*.d
