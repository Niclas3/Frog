include ../../../../Makefile.os_rules

obj-y += i8253.o
obj-y += i8259a.o


.PHONY = all
all: $(obj-y)

$(obj-y) : %.o : %.c
	$(CC) $(C_FLAG) $(DEPDIR)/$(@:.o=.d) -o $(BUILD_DIR)/$@  $<
clean:
	rm -rf ./*.o
