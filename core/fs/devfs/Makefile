include ../../../Makefile.os_rules
obj-y += devfs.o

all: $(subdir) $(obj-y)

$(subdir): %:
	$(MAKE) -C $@

$(obj-y) : %.o : %.c
	$(CC) $(C_FLAG) $(DEPDIR)/$(@:.o=.d) -o $(BUILD_DIR)/$@  $<

.PHONY: $(subdir) clean
clean:
	rm -rf ./*.o

