include ../../Makefile.os_rules
obj-y += memory.o
obj-y += kasan.o
obj-y += page_fault.o
obj-y += mm_test.o
obj-y += uaccess.o

.PHONY: all clean
all: $(build-obj-y)

clean:
	rm -rf ./*.o
