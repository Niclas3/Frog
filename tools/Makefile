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
