#include <kernel/bus.h>
#include <kernel/device.h>
#include <kernel/driver.h>

int register_driver(struct driver *drv)
{
        struct list_head *frog_driver_list = &drv->bus->driver_list;
        list_add_tail(&drv->bus->driver_list, frog_driver_list);

        // for-each driver_list in this bus
        struct bus_type *bus = drv->bus;
        struct list_head *pos;
        struct list_head *device_list = &drv->bus->device_list;

        list_for_each (pos, device_list) {
                struct device *dev = container_of(pos, struct device, node);
                if (bus->match(dev, drv)) {
                        bus->probe(dev, drv);
                        break;
                }
        }
        return 0;
}
