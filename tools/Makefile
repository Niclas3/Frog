CC ?= gcc
CFLAGS ?= -O2 -g -Wall -Wextra -Werror -std=c11

.PHONY: all test clean font

all: mkfrogfs_image

mkfrogfs_image: mkfrogfs_image.c
	$(CC) $(CFLAGS) -o $@ $<

test: mkfrogfs_image test-mkfrogfs-image.sh \
	test-poudland-p0-protocol.sh test-poudland-p0-server.sh \
		test-poudland-p0-render.sh test-desktop-lifecycle.sh \
		test-graphical-init.sh test-installed-graphical-init.sh test-system-init.sh
	@./test-mkfrogfs-image.sh ./mkfrogfs_image
	@./test-poudland-p0-protocol.sh
	@./test-poudland-p0-server.sh
	@./test-poudland-p0-render.sh
	@./test-desktop-lifecycle.sh
	@./test-graphical-init.sh
	@./test-installed-graphical-init.sh
	@./test-system-init.sh

# build font library###############################
# -lm library <math.h>
font: hankaku.bin
	./$<
	mv hankaku_font.img ..
hankaku.bin: create_fonts.c
	$(CC) -g -o hankaku.bin $< -lm
###################################################

clean:
	rm -rf hankaku.bin
	rm -rf hankaku_font.img
	rm -f mkfrogfs_image
