#ifndef __FROG_KERNEL_DEVICE_H
#define __FROG_KERNEL_DEVICE_H
#include <frog/list.h>

struct bus_type;

struct device {
        char *name;
        struct bus_type *bus;
        struct list_head node;
};

int register_device(struct device *dev);

#endif
