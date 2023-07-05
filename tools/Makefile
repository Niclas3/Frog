# build font library###############################
# -lm library <math.h>
font: hankaku.bin
	./$<
hankaku.bin: create_fonts.c
	$(CC) -g -o hankaku.bin $< -lm
###################################################

clean:
	rm -rf hankaku.bin
	rm -rf hankaku_font.img
