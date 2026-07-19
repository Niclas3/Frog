#ifndef __FROG_KERNEL_DEVICE_H
#define __FROG_KERNEL_DEVICE_H
#include <frog/list.h>
#include <frog/refcount.h>
#include <frog/semaphore.h>

struct bus_type;
struct driver;
struct device;

enum device_state {
        DEVICE_NEW = 0,
        DEVICE_LIVE,
        DEVICE_DYING,
        DEVICE_DEAD,
};

typedef void (*device_release_t)(struct device *dev);

struct device {
        char *name;
        struct bus_type *bus;
        struct list_head node;            // target attach to bus list
        uint_32 io_base;
        uint_32 irq_nr;
        struct driver *driver;            // when match a driver will set dirver
        void *driver_data;
        enum device_state state;
        refcount_t refs;
        struct lock lock;
        device_release_t release;
};

void device_init(struct device *dev, device_release_t release);
bool device_get_live(struct device *dev);
void device_put(struct device *dev);
int device_begin_unregister(struct device *dev);
int device_cancel_unregister(struct device *dev);
int device_finish_unregister(struct device *dev);
int register_device(struct device *dev);

#ifdef CONFIG_QEMU_TEST
void device_lifecycle_regression_test(void);
#endif

#endif
