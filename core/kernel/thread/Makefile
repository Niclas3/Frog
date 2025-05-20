include ../../../Makefile.os_rules

# obj-y += exec.o
# obj-y += exit.o
obj-y += fork.o
obj-y += process.o
obj-y += sched.o
obj-y += semaphore.o
obj-y += threads.o

.PHONY = all
all: $(obj-y) switch.o

$(obj-y) : %.o : %.c
	$(CC) $(C_FLAG) $(DEPDIR)/$(@:.o=.d) -o $(BUILD_DIR)/$@  $<

switch.o : switch.s
	$(AS) $(A_FLAG) -o $(BUILD_DIR)/$@ $^
clean:
	rm -rf ./*.o
