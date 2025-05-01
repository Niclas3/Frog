#ifndef __FROG_KERNEL_BUS_H
#define __FROG_KERNEL_BUS_H
#include <frog/list.h>

struct device;
struct driver;

struct bus_type {
        const char *name;
        int (*match)(struct device *dev, struct driver *drv);
        int (*probe)(struct device *dev, struct driver *drv);
        struct list_head node;
        struct list_head device_list;
        struct list_head driver_list;
};

int register_bus(struct bus_type *bus);

#endif
