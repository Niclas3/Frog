include ../../../Makefile.os_rules

APP_BUILD := ../build
TARGET := $(APP_BUILD)/compositor
POUDLAND_P0_CFLAGS := -c -fno-pie -ffreestanding -fno-stack-protector \
	-fno-builtin -fno-asynchronous-unwind-tables -fno-unwind-tables \
	-ffunction-sections -fdata-sections -march=i386 -m32 -Os -Wall -Wextra \
	-I../include $(foreach D,$(INCLUDES),-I$(D))

ifeq ($(QEMU_TEST),1)
ifeq ($(FROG_TEST_PROFILE),poudland-builtin-smoke)
POUDLAND_P0_CFLAGS += -DFROG_POUDLAND_P0_TEST
endif
endif

POUDLAND_P0_OBJECTS := \
	$(APP_BUILD)/poudland_p0_start.o \
	$(APP_BUILD)/poudland_p0_main.o \
	$(APP_BUILD)/poudland_p0_display.o \
	$(APP_BUILD)/poudland_p0_bmp.o \
	$(APP_BUILD)/poudland_p0_render.o \
	$(APP_BUILD)/poudland_p0_scene.o \
	$(APP_BUILD)/poudland_p0_protocol.o \
	$(APP_BUILD)/poudland_p0_server.o \
	$(APP_BUILD)/poudland_p0_packagefs.o \
	$(APP_BUILD)/poudland_p0_syscall.o \
	$(APP_BUILD)/poudland_p0_allocator.o \
	$(APP_BUILD)/poudland_p0_test.o

.PHONY: all clean FORCE
all: $(TARGET)

FORCE:

$(APP_BUILD):
	mkdir -p $@

$(APP_BUILD)/poudland_p0_start.o: ../start.s FORCE | $(APP_BUILD)
	$(CC) $(POUDLAND_P0_CFLAGS) -o $@ $<

$(APP_BUILD)/poudland_p0_main.o: main.c test.h FORCE | $(APP_BUILD)
	$(CC) $(POUDLAND_P0_CFLAGS) \
		-MMD -MP -MF $(APP_BUILD)/poudland_p0_main.d -o $@ $<

$(APP_BUILD)/poudland_p0_test.o: test.c test.h FORCE | $(APP_BUILD)
	$(CC) $(POUDLAND_P0_CFLAGS) \
		-MMD -MP -MF $(APP_BUILD)/poudland_p0_test.d -o $@ $<

$(APP_BUILD)/poudland_p0_display.o: display.c poudland_p0.h FORCE | $(APP_BUILD)
	$(CC) $(POUDLAND_P0_CFLAGS) \
		-MMD -MP -MF $(APP_BUILD)/poudland_p0_display.d -o $@ $<

$(APP_BUILD)/poudland_p0_bmp.o: bmp.c poudland_p0.h FORCE | $(APP_BUILD)
	$(CC) $(POUDLAND_P0_CFLAGS) \
		-MMD -MP -MF $(APP_BUILD)/poudland_p0_bmp.d -o $@ $<

$(APP_BUILD)/poudland_p0_render.o: render.c poudland_p0.h FORCE | $(APP_BUILD)
	$(CC) $(POUDLAND_P0_CFLAGS) \
		-MMD -MP -MF $(APP_BUILD)/poudland_p0_render.d -o $@ $<

$(APP_BUILD)/poudland_p0_scene.o: scene.c poudland_p0.h test.h FORCE | $(APP_BUILD)
	$(CC) $(POUDLAND_P0_CFLAGS) \
		-MMD -MP -MF $(APP_BUILD)/poudland_p0_scene.d -o $@ $<

$(APP_BUILD)/poudland_p0_protocol.o: protocol.c protocol.h geometry.h FORCE | $(APP_BUILD)
	$(CC) $(POUDLAND_P0_CFLAGS) \
		-MMD -MP -MF $(APP_BUILD)/poudland_p0_protocol.d -o $@ $<

$(APP_BUILD)/poudland_p0_server.o: server.c server.h protocol.h \
		poudland_p0.h FORCE | $(APP_BUILD)
	$(CC) $(POUDLAND_P0_CFLAGS) \
		-MMD -MP -MF $(APP_BUILD)/poudland_p0_server.d -o $@ $<

$(APP_BUILD)/poudland_p0_packagefs.o: ../lib/packagefs.c FORCE | $(APP_BUILD)
	$(CC) $(POUDLAND_P0_CFLAGS) \
		-MMD -MP -MF $(APP_BUILD)/poudland_p0_packagefs.d -o $@ $<

$(APP_BUILD)/poudland_p0_syscall.o: ../../kernel/syscall.c FORCE | $(APP_BUILD)
	$(CC) $(POUDLAND_P0_CFLAGS) \
		-MMD -MP -MF $(APP_BUILD)/poudland_p0_syscall.d -o $@ $<

$(APP_BUILD)/poudland_p0_allocator.o: ../../lib/user_malloc.c FORCE | $(APP_BUILD)
	$(CC) $(POUDLAND_P0_CFLAGS) \
		-MMD -MP -MF $(APP_BUILD)/poudland_p0_allocator.d -o $@ $<

$(TARGET): $(POUDLAND_P0_OBJECTS) poudland_p0.ld
	$(LD) --gc-sections -m elf_i386 -T poudland_p0.ld -o $@ \
		$(POUDLAND_P0_OBJECTS)

clean:
	rm -f $(APP_BUILD)/poudland_p0_*.o \
		$(APP_BUILD)/poudland_p0_*.d $(TARGET)

-include $(APP_BUILD)/poudland_p0_*.d
