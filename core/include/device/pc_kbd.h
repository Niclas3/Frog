#ifndef __DEVICE_PC_KBD_H
#define __DEVICE_PC_KBD_H
#include <ostype.h>

#define KBD_BUF_SIZE 2048
struct file;
struct partition;
struct dir;

uint_32 pc_kbd_init(void);
int_32 kbd_create(struct partition *part,
                  struct dir *parent_d,
                  char *name,
                  void *target);
uint_32 read_kbd(struct file *file, void *buf, uint_32 count);

#endif
