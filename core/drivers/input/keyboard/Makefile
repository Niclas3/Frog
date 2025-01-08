include ../../../../Makefile.os_rules
obj-y += pc_kbd.o

.PHONY = all
all: $(obj-y)

$(obj-y) : %.o : %.c
	$(CC) $(C_FLAG) $(DEPDIR)/$(@:.o=.d) -o $(BUILD_DIR)/$@  $<

clean:
	rm -rf ./*.o
