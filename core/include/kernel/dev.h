#ifndef __FROG_KERNEL_DEV_H
#define __FROG_KERNEL_DEV_H

typedef unsigned int dev_t;
#define DEV_TYPE_CHAR  1
#define DEV_TYPE_BLOCK 2

#define MK_DEV(major, minor) (((major) << 16) | (minor))
#define DEV_MAJOR(dev) ((dev) >> 16) & 0xffff
#define DEV_MINOR(dev) ((dev) & 0xffff)

int devfs_create_node(char *pathname,int type,  int major, int minor);

#endif
