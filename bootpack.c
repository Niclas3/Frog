// define
#define COL8_000000 0
#define COL8_FF0000 1
#define COL8_00FF00 2
#define COL8_FFFF00 3
#define COL8_0000FF 4
#define COL8_FF00FF 5
#define COL8_00FFFF 6
#define COL8_FFFFFF 7
#define COL8_C6C6C6 8
#define COL8_840000 9
#define COL8_008400 10
#define COL8_848400 11
#define COL8_000084 12
#define COL8_840084 13
#define COL8_008484 14
#define COL8_848484 15

// Function from core.s
void _io_hlt(void);
/* void _print(char* msg, int len); */
/* void _print(void); */
void _write_mem8(int addr, int data);

void _io_cli(void);
void _io_out8(int port, int data);
int _io_load_eflags(void);
void _io_store_eflags(int eflags);
//-------------------------------------
// dec
void boxfill8(unsigned char *vram,
              int xsize,
              unsigned char c,
              int x0,
              int y0,
              int x1,
              int y1);
void set_palette(int start, int end, unsigned char *rgb);
void init_palette(void);
//-------------------------------------

// HariMain must at top of file
void HariMain(void)
{
    /* init_palette(); */

    // 0xa0000-0x0ffff
    /* for (int i = 0x00000; i <= 0x0ffff; i++) { */
    /*     *(p + i) = i & 0x0f; */
    /*     #<{(| _write_mem8(i, i & 0x0f); |)}># */
    /* } */

    char *vram;
    int xsize, ysize;

    /* init_palette(); */
    vram = (char *) 0xa0000;
    xsize = 320;
    ysize = 200;

    boxfill8(vram, xsize, COL8_008484,  0,         0,          xsize -  1, ysize - 29);
    boxfill8(vram, xsize, COL8_C6C6C6,  0,         ysize - 28, xsize -  1, ysize - 28);
    boxfill8(vram, xsize, COL8_FFFFFF,  0,         ysize - 27, xsize -  1, ysize - 27);
    boxfill8(vram, xsize, COL8_C6C6C6,  0,         ysize - 26, xsize -  1, ysize -  1);

    boxfill8(vram, xsize, COL8_FFFFFF,  3,         ysize - 24, 59,         ysize - 24);
    boxfill8(vram, xsize, COL8_FFFFFF,  2,         ysize - 24,  2,         ysize -  4);
    boxfill8(vram, xsize, COL8_848484,  3,         ysize -  4, 59,         ysize -  4);
    boxfill8(vram, xsize, COL8_848484, 59,         ysize - 23, 59,         ysize -  5);
    boxfill8(vram, xsize, COL8_000000,  2,         ysize -  3, 59,         ysize -  3);
    boxfill8(vram, xsize, COL8_000000, 60,         ysize - 24, 60,         ysize -  3);

    boxfill8(vram, xsize, COL8_848484, xsize - 47, ysize - 24, xsize -  4, ysize - 24);
    boxfill8(vram, xsize, COL8_848484, xsize - 47, ysize - 23, xsize - 47, ysize -  4);
    boxfill8(vram, xsize, COL8_FFFFFF, xsize - 47, ysize -  3, xsize -  4, ysize -  3);
    boxfill8(vram, xsize, COL8_FFFFFF, xsize -  3, ysize - 24, xsize -  3, ysize -  3);


    for (;;) {
        _io_hlt();  // execute _io_hlt in naskfunc.s
    }
}

void set_palette(int start, int end, unsigned char *rgb)
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
    static unsigned char table_rgb[16 * 3] = {
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

