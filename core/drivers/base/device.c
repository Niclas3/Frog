#include <kernel/bus.h>
#include <kernel/device.h>
#include <kernel/driver.h>

int register_device(struct device *dev)
{
        struct list_head *frog_dev_list = &dev->bus->device_list;
        struct bus_type *bus = dev->bus;

        list_add_tail(&dev->node, frog_dev_list);

        // for-each driver_list in this bus
        struct list_head *pos;
        struct list_head *frog_driver_list = &dev->bus->driver_list;

        list_for_each (pos, frog_driver_list) {
                struct driver *drv = container_of(pos, struct driver, node);
                if (bus->match(dev, drv)) {
                        if (bus->probe(dev, drv) == 0)
                                dev->driver = drv;
                        break;
                }
        }

        return 0;
}
