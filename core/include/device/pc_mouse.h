#ifndef __DEVICE_PC_MOUSE_H
#define __DEVICE_PC_MOUSE_H
#include <hid/mouse.h>
#include <ostype.h>

#define MOUSE_DEFAULT 0
#define MOUSE_SCROLLWHEEL 1
#define MOUSE_BUTTONS 2
#ifndef MOUSE_V_BIT
#define MOUSE_V_BIT 0x08
#endif
#define MOUSE_PKG_BUF_SIZE sizeof(mouse_device_packet_t) * 1024

struct file;
struct partition;
struct dir;

void handle_ps2_mouse_scancode(uint_8 scancode);
uint_32 pc_mouse_init(void);

int_32 pcmouse_create(struct partition *part,
                      struct dir *parent_d,
                      char *name,
                      void *target);
uint_32 read_pcmouse(struct file *file, void *buf, uint_32 count);



#endif
