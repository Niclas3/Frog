#include "../include/bootpack.h"
#include "../include/graphic.h"

void draw_cursor8(char *mouse, char bc)
{
    char cursor[16][16] = {
        "**************..", 
        "*OOOOOOOOOOO*...", 
        "*OOOOOOOOOO*....",
        "*OOOOOOOOO*.....", 
        "*OOOOOOOO*......", 
        "*OOOOOOO*.......",
        "*OOOOOOO*.......", 
        "*OOOOOOOO*......", 
        "*OOOO**OOO*.....",
        "*OOO*..*OOO*....", 
        "*OO*....*OOO*...", 
        "*O*......*OOO*..",
        "**........*OOO*.", 
        "*..........*OOO*", 
        "............*OO*",
        ".............***"
    };
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            if (cursor[i][j] == '*') {
                mouse[i * 16 + j] = COL8_000000;
            } else if (cursor[i][j] == 'O') {
                mouse[i * 16 + j] = COL8_FFFFFF;
            } else if (cursor[i][j] == '.') {
                mouse[i * 16 + j] = bc;
            }
        }
    }
    return;
}

void putblock8_8(char *vram,
                 int vxsize,
                 int pxsize,
                 int pysize,
                 int px0,
                 int py0,
                 char *buf,
                 int bxsize)
{
    for (int y = 0; y < pysize; y++) {
        for (int x = 0; x < pxsize; x++) {
            vram[(py0 + y) * vxsize + (px0 + x)] = buf[y * bxsize + x];
        }
    }
}



void draw_backgrond(unsigned char *vram, int xsize, int ysize)
{
    /* boxfill8(vram, xsize, c.color_id, 0, 0, xsize - 1, ysize - 29); */

    boxfill8(vram, xsize, COL8_008484, 0, 0, xsize - 1, ysize - 29);
    boxfill8(vram, xsize, COL8_C6C6C6, 0, ysize - 28, xsize - 1, ysize - 28);
    boxfill8(vram, xsize, COL8_FFFFFF, 0, ysize - 27, xsize - 1, ysize - 27);
    boxfill8(vram, xsize, COL8_C6C6C6, 0, ysize - 26, xsize - 1, ysize - 1);

    boxfill8(vram, xsize, COL8_FFFFFF, 3, ysize - 24, 59, ysize - 24);
    boxfill8(vram, xsize, COL8_FFFFFF, 2, ysize - 24, 2, ysize - 4);
    boxfill8(vram, xsize, COL8_848484, 3, ysize - 4, 59, ysize - 4);
    boxfill8(vram, xsize, COL8_848484, 59, ysize - 23, 59, ysize - 5);
    boxfill8(vram, xsize, COL8_000000, 2, ysize - 3, 59, ysize - 3);
    boxfill8(vram, xsize, COL8_000000, 60, ysize - 24, 60, ysize - 3);

    boxfill8(vram, xsize, COL8_848484, xsize - 47, ysize - 24, xsize - 4,
             ysize - 24);
    boxfill8(vram, xsize, COL8_848484, xsize - 47, ysize - 23, xsize - 47,
             ysize - 4);
    boxfill8(vram, xsize, COL8_FFFFFF, xsize - 47, ysize - 3, xsize - 4,
             ysize - 3);
    boxfill8(vram, xsize, COL8_FFFFFF, xsize - 3, ysize - 24, xsize - 3,
             ysize - 3);
}


// char c : color
void putfont8(unsigned char *vram, int xsize, int x, int y, char c, char *font)
{
    char d;
    for (int i = 0; i < 16; i++) {
        d = font[i];
        if ((d & 0x80) != 0) {
            vram[(y + i) * xsize + x + 0] = c;
        }
        if ((d & 0x40) != 0) {
            vram[(y + i) * xsize + x + 1] = c;
        }
        if ((d & 0x20) != 0) {
            vram[(y + i) * xsize + x + 2] = c;
        }
        if ((d & 0x10) != 0) {
            vram[(y + i) * xsize + x + 3] = c;
        }
        if ((d & 0x08) != 0) {
            vram[(y + i) * xsize + x + 4] = c;
        }
        if ((d & 0x04) != 0) {
            vram[(y + i) * xsize + x + 5] = c;
        }
        if ((d & 0x02) != 0) {
            vram[(y + i) * xsize + x + 6] = c;
        }
        if ((d & 0x01) != 0) {
            vram[(y + i) * xsize + x + 7] = c;
        }
    }
    return;
}

void putfonts8_asc(char *vram, int xsize, int x, int y, char color,unsigned char *s){
    char *hankaku = (char*)FONT_HANKAKU;
    for(;*s != 0x00 ;s++){
        putfont8((unsigned char *)vram, xsize, x, y, color,hankaku + *s *16);
        x+=8;
    }
}


void my_palette()
{
    /* set_palette(0, 0x0f, table_rgb); */

    int start = 0;
    int end = 0x0f;
    int i, eflags;
    eflags = _io_load_eflags();
    _io_cli();
    _io_out8(0x03c8, start);

    /*         0x00, 0x00, 0x00,  // 00 black */
    _io_out8(0x03c9, 0x00 / 4);
    _io_out8(0x03c9, 0x00 / 4);
    _io_out8(0x03c9, 0x00 / 4);
    /*         0xff, 0x00, 0x00,  // 01 light red */
    _io_out8(0x03c9, 0xff / 4);
    _io_out8(0x03c9, 0x00 / 4);
    _io_out8(0x03c9, 0x00 / 4);
    /*         0x00, 0xff, 0x00,  // 02 light green */
    _io_out8(0x03c9, 0x00 / 4);
    _io_out8(0x03c9, 0xff / 4);
    _io_out8(0x03c9, 0x00 / 4);
    /*         0xff, 0xff, 0x00,  // 03 light yellow */
    _io_out8(0x03c9, 0xff / 4);
    _io_out8(0x03c9, 0xff / 4);
    _io_out8(0x03c9, 0x00 / 4);
    /*         0x00, 0x00, 0xff,  // 04 light blue */
    _io_out8(0x03c9, 0x00 / 4);
    _io_out8(0x03c9, 0x00 / 4);
    _io_out8(0x03c9, 0xff / 4);
    //--------------------------------------------
    /*         0xff, 0x00, 0xff,  // 05 light purple */
    _io_out8(0x03c9, 0xff / 4);
    _io_out8(0x03c9, 0x00 / 4);
    _io_out8(0x03c9, 0xff / 4);
    /*         0x00, 0xff, 0xff,  // 06 light light blue */
    _io_out8(0x03c9, 0x00 / 4);
    _io_out8(0x03c9, 0xff / 4);
    _io_out8(0x03c9, 0xff / 4);
    /*         0xff, 0xff, 0xff,  // 07 write */
    _io_out8(0x03c9, 0xff / 4);
    _io_out8(0x03c9, 0xff / 4);
    _io_out8(0x03c9, 0xff / 4);
    /*         0xc6, 0xc6, 0xc6,  // 08 light gray */
    _io_out8(0x03c9, 0xc6 / 4);
    _io_out8(0x03c9, 0xc6 / 4);
    _io_out8(0x03c9, 0xc6 / 4);
    /*         0x84, 0x00, 0x00,  // 09 dark red */
    _io_out8(0x03c9, 0x84 / 4);
    _io_out8(0x03c9, 0x00 / 4);
    _io_out8(0x03c9, 0x00 / 4);
    /*         0x00, 0x84, 0x00,  // 10 dark green */
    _io_out8(0x03c9, 0x00 / 4);
    _io_out8(0x03c9, 0x84 / 4);
    _io_out8(0x03c9, 0x00 / 4);
    /*         0x84, 0x84, 0x00,  // 11 dark yellow */
    _io_out8(0x03c9, 0x84 / 4);
    _io_out8(0x03c9, 0x84 / 4);
    _io_out8(0x03c9, 0x00 / 4);
    /*         0x00, 0x00, 0x84,  // 12 dark purple */
    _io_out8(0x03c9, 0x00 / 4);
    _io_out8(0x03c9, 0x00 / 4);
    _io_out8(0x03c9, 0x84 / 4);
    /*         0x00, 0x84, 0x84,  // 13 light dark blue */
    _io_out8(0x03c9, 0x00 / 4);
    _io_out8(0x03c9, 0x84 / 4);
    _io_out8(0x03c9, 0x84 / 4);
    /*         0x84, 0x84, 0x84,  // 14 dark gray */
    _io_out8(0x03c9, 0x84 / 4);
    _io_out8(0x03c9, 0x84 / 4);
    _io_out8(0x03c9, 0x84 / 4);

    _io_store_eflags(eflags);
    return;
}

/* void strip(unsigned char* vram){ */
/*  */
/*     // 0xa0000-0x0ffff */
/*     for (int i = 0x00000; i <= 0x0ffff; i++) { */
/*         *(vram + i) = i & 0x0f; */
/*         #<{(| _write_mem8(i, i & 0x0f); |)}># */
/*     } */
/* } */

void set_palette(int start, int end, char *rgb)
{
    int i, eflags;
    eflags = _io_load_eflags();
    _io_cli();
    _io_out8(0x03c8, start);
    for (i = start; i <= end; i++) {
        _io_out8(0x03c9, rgb[0] / 4);
        _io_out8(0x03c9, rgb[1] / 4);
        _io_out8(0x03c9, rgb[2] / 4);
        rgb += 3;
    }
    _io_store_eflags(eflags);
    return;
}

void init_palette(void)
{
    char table_rgb[15 * 3] = {
        //  L              H
        0x00, 0x00, 0x00,  // 00 black
        0xff, 0x00, 0x00,  // 01 light red
        0x00, 0xff, 0x00,  // 02 light green
        0xff, 0xff, 0x00,  // 03 light yellow
        0x00, 0x00, 0xff,  // 04 light blue
        0xff, 0x00, 0xff,  // 05 light purple
        0x00, 0xff, 0xff,  // 06 light light blue
        0xff, 0xff, 0xff,  // 07 write
        0xc6, 0xc6, 0xc6,  // 08 light gray
        0x84, 0x00, 0x00,  // 09 dark red
        0x00, 0x84, 0x00,  // 10 dark green
        0x84, 0x84, 0x00,  // 11 dark yellow
        0x00, 0x00, 0x84,  // 12 dark purple
        0x00, 0x84, 0x84,  // 13 light dark blue
        0x84, 0x84, 0x84,  // 14 dark gray
    };
    set_palette(0, 0x0f, table_rgb);
    return;
}

void boxfill8(unsigned char *vram,
              int xsize,
              unsigned char c,
              int x0,
              int y0,
              int x1,
              int y1)
{
    int x, y;
    for (y = y0; y <= y1; y++) {
        for (x = x0; x <= x1; x++) {
            vram[y * xsize + x] = c;
        }
    }
    return;
}
