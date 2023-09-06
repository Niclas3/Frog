#ifndef GLOBAL_H
#define GLOBAL_H

typedef struct B_info {
    char cyls;
    char leds;
    char vmode;
    char reserve;
    short scrnx, scrny;
    unsigned char *vram;
} BOOTINFO;

typedef struct Color {
    unsigned char color_id;
} COLOR;

extern BOOTINFO info;

#endif
