include ../../Makefile.os_rules
subdir += frogfs/
# subdir += packagefs/
subdir += tmpfs/
subdir += vfs/
subdir += devfs/


obj-y += select.o
obj-y += fd.o
obj-y += syscall_fs.o
obj-y += fs_regression.o
# obj-y += pipe.o

all: $(subdir) $(build-obj-y)

$(subdir): %:
	$(MAKE) -C $@

.PHONY: all $(subdir) clean
clean:
	rm -rf ./*.o
