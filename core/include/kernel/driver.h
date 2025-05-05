#ifndef __FROG_KERNEL_DRIVER_H
#define __FROG_KERNEL_DRIVER_H
#include <frog/list.h>

struct bus_type;
struct device;

struct driver{
        const char* name;
        struct bus_type *bus;
        int (*probe)(struct device *dev);
        struct list_head node;              // target to driver_list in bus
};

int register_driver(struct driver *drv);

#endif
