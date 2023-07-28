#ifndef KEYBOARD_H
#define KEYBOARD_H

#define PORT_KEYDATE         0x0060
#define PORT_KEYSTATE        0x0064
#define PORT_KEYCOMMD        0x0064
#define KEYSTA_SEND_NOTREADY 0x02
#define KEYCMD_WRITE_MODE    0x60
#define KBC_MODE             0x47

void wait_KBC_sendready();

void init_keyboard();

#endif
