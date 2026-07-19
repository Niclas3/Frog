include ../../../Makefile.os_rules

# obj-y += exec.o
obj-y += exit.o
obj-y += fork.o
obj-y += process.o
obj-y += process_regression.o
obj-y += sched.o
obj-y += semaphore.o
obj-y += threads.o

.PHONY: all clean
all: $(build-obj-y) $(BUILD_DIR)/switch.o

$(BUILD_DIR)/switch.o: switch.s
	$(AS) $(A_FLAG) -o $@ $<
clean:
	rm -rf ./*.o
