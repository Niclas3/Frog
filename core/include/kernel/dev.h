#ifndef __FROG_KERNEL_DEV_H
#define __FROG_KERNEL_DEV_H

#include <frog/types.h>
#define DEV_TYPE_CHAR  1
#define DEV_TYPE_BLOCK 2

#define MK_DEV(major, minor) (((major) << 16) | (minor))

#define DEV_MAJOR(dev_nr) ((dev_nr) & (0xffff << 16)) >> 16
// second 16bits is minor number
#define DEV_MINOR(dev_nr) (dev_nr) & 0xffff
#define DEV_NR(major, minor) (major << 16) | (minor & 0xffff)

int devfs_create_node(const char *pathname, int type, int major, int minor);
int devfs_remove_node(const char *pathname, int type, int major, int minor);

#endif
