include ../../Makefile.os_rules
obj-y += memory.o
obj-y += bootmem.o
obj-y += phys_resource.o
obj-y += kasan.o
obj-y += page_fault.o
obj-y += mm_test.o
obj-y += uaccess.o
obj-y += vm.o

.PHONY: all clean
all: $(build-obj-y)

clean:
	rm -rf ./*.o
