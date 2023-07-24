#include "include/graphic.h"
#include "include/bootpack.h"

//-------------------------------------
typedef struct B_info {
    char cyls;
    char leds;
    char vmode;
    char reserve;
    short scrnx, scrny;
    char *vram;
} BOOTINFO;

typedef struct Color {
    unsigned char color_id;
} COLOR;
// HariMain must at top of file

void HariMain(void)
{
    char font_A[16] = {0x00, 0x18, 0x18, 0x18, 0x18, 0x24, 0x24, 0x24,
                       0x24, 0x7e, 0x42, 0x42, 0x42, 0xe7, 0x00, 0x00};

    char *hankaku = (char *) FONT_HANKAKU; // size 4096

    init_palette();
    unsigned char *vram;
    int xsize, ysize;
    vram = (unsigned char *) 0xa0000;
    xsize = 320;
    ysize = 200;

    char *mcursor =(char*) 0x7c00; 
    char *mcursor1 =(char*) 0x7c01;

    COLOR c = {.color_id = COL8_008484};

    draw_backgrond(vram, xsize, ysize);

    int pysize = 16;
    int pxsize = 16;
    int bxsize = 16;
    int vxsize = xsize;
    int py0 = 50;
    int px0 = 50;

    draw_cursor8(mcursor, COL8_848484);
    draw_cursor8(mcursor1, COL8_008484);
    int mx = 70;
    int my = 50;
    putblock8_8((char *)vram, xsize, 16, 16, mx, my, mcursor, 16);
    putblock8_8((char *)vram, xsize, 16, 16, mx+40, my+20, mcursor1, 16);


    /* char s[16] = {0}; */
    /* sprintf(s, "A"); */

    /* putfont8(vram, xsize, 8, 8, COL8_00FF00, font_A); */
    /* putfont8(vram, xsize, 8-4, 8-4, COL8_00FF00, hankaku + 'A' * 16); */
    /* putfont8(vram, xsize, 16, 8, COL8_00FF00, hankaku + 'B' * 16); */
    /* putfont8(vram, xsize, 24, 8, COL8_00FF00, hankaku + '1' * 16); */
    putfonts8_asc(vram, xsize, 8, 8, COL8_0000FF, "Niclas 123");


    for (;;) {
        _io_hlt();  // execute _io_hlt in naskfunc.s
    }
}

