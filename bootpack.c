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
void set_palette(int start, int end, char *rgb);
void init_palette(void);

void my_palette();

void putfont8(unsigned char *vram, int xsize, int x, int y, char c, char *font);

void draw_backgrond(unsigned char *vram, int xsize, int ysize);

/* char 'mouse' contains shapes of mouse
 * char 'bc'  is abbs of background color of mouse
 * */
void draw_cursor8(char *mouse, char bc);

void putblock8_8(char *vram,
                 int vxsize,
                 int pxsize,
                 int pysize,
                 int px0,
                 int py0,
                 char *buf,
                 int bxsize);

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
    my_palette();
    unsigned char *vram;
    int xsize, ysize;
    vram = (unsigned char *) 0xa0000;
    xsize = 320;
    ysize = 200;

    char *mcursor;

    COLOR c = {.color_id = COL8_008484};

    draw_backgrond(vram, xsize, ysize);

    /* draw_cursor8(mcursor, COL8_848484); */
    /* int mx = 70; */
    /* int my = 50; */
    /* putblock8_8((char *)vram, xsize, 16, 16, mx, my, mcursor, 16); */

    char *mouse;
    int pysize = 16;
    int pxsize = 16;
    int bxsize = 16;
    int vxsize = xsize;
    int py0 = 50;
    int px0 = 50;

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
                mouse[i * 16 + j] = COL8_008484;
            }
        }
    }

    for (int y = 0; y < pysize; y++) {
        for (int x = 0; x < pxsize; x++) {
            vram[(py0 + y) * vxsize + (px0 + x)] = mouse[y * bxsize + x];
        }
    }
    putfont8(vram, xsize, 8, 8, COL8_00FF00, font_A);

    for (;;) {
        _io_hlt();  // execute _io_hlt in naskfunc.s
    }
}

void draw_cursor8(char *mouse, char bc)
{
    static char cursor[16][16] = {
        "****************", 
        "****************", 
        "****************",
        "****************", 
        "****************", 
        "****************",
        "****************", 
        "****************", 
        "****************",
        "****************", 
        "****************", 
        "****************",
        "****************", 
        "****************", 
        "****************",
        "****************"
        /* "**************..",  */
        /* "*OOOOOOOOOOO*...",  */
        /* "*OOOOOOOOOO*....", */
        /* "*OOOOOOOOO*.....",  */
        /* "*OOOOOOOO*......",  */
        /* "*OOOOOOO*.......", */
        /* "*OOOOOOO*.......",  */
        /* "*OOOOOOOO*......",  */
        /* "*OOOO**OOO*.....", */
        /* "*OOO*..*OOO*....",  */
        /* "*OO*....*OOO*...",  */
        /* "*O*......*OOO*..", */
        /* "**........*OOO*.",  */
        /* "*..........*OOO*",  */
        /* "............*OO*", */
        /* ".............***" */
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
    char *mouse;
    static char cursor[16][16] = {
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
                mouse[i * 16 + j] = COL8_848484;
            }
        }
    }

    for (int y = 0; y < pysize; y++) {
        for (int x = 0; x < pxsize; x++) {
            vram[(py0 + y) * vxsize + (px0 + x)] = mouse[y * bxsize + x];
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
    static char table_rgb[15 * 3] = {
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
