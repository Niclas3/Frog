include ../../../../Makefile.os_rules

all: $(BUILD_DIR)/boot.o

$(BUILD_DIR)/boot.o: core.s
	$(AS) $(A_FLAG) -o $@ $<

.PHONY: all clean

clean:
	rm -rf ./*.o
