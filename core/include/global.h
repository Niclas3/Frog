#ifndef GLOBAL_H
#define GLOBAL_H

///Keyboard buffer
struct KEYBUF{
    unsigned char data,flag;
};

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

extern struct KEYBUF keybuf;
extern BOOTINFO info;

#endif
