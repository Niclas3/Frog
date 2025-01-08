include ../../../../Makefile.os_rules

all: boot.o

boot.o : core.s
	$(AS) $(A_FLAG) -o $(BUILD_DIR)/$@ $^

.PHONY: all

clean:
	rm -rf ./*.o
