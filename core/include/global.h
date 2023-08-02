#ifndef GLOBAL_H
#define GLOBAL_H

///Keyboard buffer
struct KEYBUF{
    unsigned char data[32];
    unsigned char flag;
};

extern struct KEYBUF keybuf;

#endif
