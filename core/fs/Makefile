include ../../Makefile.os_rules
subdir += frogfs/
# subdir += packagefs/
subdir += tmpfs/
subdir += vfs/
subdir += devfs/


obj-y += select.o
obj-y += fd.o
# obj-y += pipe.o

all: $(subdir) $(obj-y)

$(subdir): %:
	$(MAKE) -C $@

$(obj-y) : %.o : %.c
	$(CC) $(C_FLAG) $(DEPDIR)/$(@:.o=.d) -o $(BUILD_DIR)/$@  $<

.PHONY: $(subdir) clean
clean:
	rm -rf ./*.o

