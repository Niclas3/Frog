#include <frog/errno.h>
#include <kernel/bus.h>
#include <kernel/device.h>
#include <kernel/driver.h>

static bool driver_is_registered(struct driver *drv)
{
        struct list_head *pos;

        list_for_each (pos, &drv->bus->driver_list) {
                if (container_of(pos, struct driver, node) == drv)
                        return true;
        }
        return false;
}

int register_driver(struct driver *drv)
{
        struct bus_type *bus;
        struct list_head *pos;
        int ret = 0;

        if (drv == NULL || drv->bus == NULL || drv->probe == NULL)
                return -EINVAL;
        bus = drv->bus;
        if (bus->match == NULL || bus->probe == NULL)
                return -EINVAL;
        if (driver_is_registered(drv))
                return -EALREADY;

        list_for_each (pos, &bus->device_list) {
                struct device *dev = container_of(pos, struct device, node);

                lock_fetch(&dev->lock);
                if (dev->state != DEVICE_LIVE || dev->driver != NULL ||
                    !bus->match(dev, drv)) {
                        lock_release(&dev->lock);
                        continue;
                }

                ret = bus->probe(dev, drv);
                if (ret == 0)
                        dev->driver = drv;
                else {
                        dev->driver = NULL;
                        dev->driver_data = NULL;
                }
                lock_release(&dev->lock);
                if (ret != 0)
                        return ret;
                break;
        }

        list_add_tail(&drv->node, &bus->driver_list);
        return ret;
}
