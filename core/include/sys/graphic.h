#ifndef GRAPHIC
#define GRAPHIC
#include "ostype.h"
#include <global.h>

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

void boxfill8(unsigned char *vram,
              int xsize,
              unsigned char c,
              int x0,
              int y0,
              int x1,
              int y1);
void set_palette(int start, int end, char *rgb);
void init_palette(void);

void putfont8(unsigned char *vram, int xsize, int x, int y, char c, char *font);

void putfonts8_asc(char *vram, int xsize, int x, int y, char color,unsigned char *s);

void putfonts8_asc_error(unsigned char *s, int x, int y);

void put_asc_char(char *vram, int xsize, int x, int y, char color,int num);

void draw_info(char* vram, int scrnx, char color, int px, int py, char* str);

void draw_hex(char* vram, int scrnx, char color, int px, int py, int num);

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
#endif
