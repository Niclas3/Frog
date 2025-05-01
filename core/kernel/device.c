#include <kernel/bus.h>
#include <kernel/device.h>
#include <kernel/driver.h>

int register_device(struct device *dev)
{
        struct list_head *frog_dev_list = &dev->bus->device_list;
        struct bus_type *bus = dev->bus;

        list_append_tail(frog_dev_list, &dev->node);

        // for-each driver_list in this bus
        struct list_head *pos;
        struct list_head *frog_driver_list = &dev->bus->driver_list;

        list_for_each (pos, frog_driver_list) {
                struct driver *drv = container_of(pos, struct driver, node);
                if (bus->match(dev, drv)) {
                        bus->probe(dev, drv);
                        break;
                }
        }

        return 0;
}
