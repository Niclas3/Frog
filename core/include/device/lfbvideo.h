#ifndef __DEVICE_LFBVIDEO_H
#define __DEVICE_LFBVIDEO_H
#include <ostype.h>

struct file;
struct partition;
struct dir;

int_32 ioctl_vid(struct file *file, unsigned long request, void *argp);
int_32 lfb_init(char *argp);
int_32 lfbvideo_create(struct partition *part,
                       struct dir *parent_d,
                       char *name,
                       void *target);

#endif
