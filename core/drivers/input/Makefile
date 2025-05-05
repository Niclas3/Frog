include ../../../Makefile.os_rules
subdir += keyboard/
subdir += mouse/

all: $(subdir) $(obj-y)

$(subdir): %:
	$(MAKE) -C $@

$(obj-y) : %.o : %.c
	$(CC) $(C_FLAG) $(DEPDIR)/$(@:.o=.d) -o $(BUILD_DIR)/$@  $<

.PHONY: all $(subdir) clean
clean:
	rm -rf ./*.o
