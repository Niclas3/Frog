include ../../Makefile.os_rules
obj-y += memory.o
obj-y += kasan.o
obj-y += page_fault.o
obj-y += mm_test.o

.PHONY = all
all: $(obj-y)

$(obj-y) : %.o : %.c
	$(CC) $(C_FLAG) $(DEPDIR)/$(@:.o=.d) -o $(BUILD_DIR)/$@  $<

clean:
	rm -rf ./*.o
