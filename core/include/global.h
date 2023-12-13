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

#define VBE_MODE_INFO_POINTER   0x0ffc
#define VBE_INFO_POINTER        0x1000

#define MMAP_INFO_POINTER       0x0ff8
#define MMAP_INFO_COUNT_POINTER 0x0ff4 //; 2 bytes

#endif
