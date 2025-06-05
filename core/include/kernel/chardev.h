#ifndef __FROG_KERNEL_CHARDEV_H
#define __FROG_KERNEL_CHARDEV_H
#include <frog/types.h>
struct file_operations;

#define MAX_CHARDEV 255 // size is 1 byte

int register_chrdev(uint_32 major, const struct file_operations *fops);
int unregister_chrdev(uint_32 major);
struct list_head *get_chrdev_list(void);
int chrdev_init(void);
const struct file_operations *get_chardev_fop(int major);

#endif
