#ifndef __FROG_KERNEL_DEVICE_H
#define __FROG_KERNEL_DEVICE_H
#include <frog/list.h>

struct bus_type;
struct driver;

struct device {
        char *name;
        struct bus_type *bus;
        struct list_head node;            // target attach to bus list
        uint_32 io_base;
        uint_32 irq_nr;
        struct driver *driver;            // when match a driver will set dirver
        void *driver_data;
};

int register_device(struct device *dev);

#endif
