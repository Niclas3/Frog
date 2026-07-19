#include <frog/errno.h>
#include <frog/refcount.h>
#include <frog/semaphore.h>
#include <kernel/bus.h>
#include <kernel/assert.h>
#include <kernel/device.h>
#include <kernel/driver.h>

void device_init(struct device *dev, device_release_t release)
{
        ASSERT(dev != NULL);

        dev->name = NULL;
        dev->bus = NULL;
        INIT_LIST_HEAD(&dev->node);
        dev->io_base = 0;
        dev->irq_nr = 0;
        dev->driver = NULL;
        dev->driver_data = NULL;
        dev->state = DEVICE_NEW;
        refcount_init(&dev->refs, 0);
        lock_init(&dev->lock);
        dev->release = release;
}

bool device_get_live(struct device *dev)
{
        bool acquired = false;

        if (dev == NULL)
                return false;

        lock_fetch(&dev->lock);
        if (dev->state == DEVICE_LIVE)
                acquired = refcount_get_live(&dev->refs);
        lock_release(&dev->lock);
        return acquired;
}

void device_put(struct device *dev)
{
        device_release_t release;

        ASSERT(dev != NULL);
        if (!refcount_put(&dev->refs))
                return;

        ASSERT(dev->state == DEVICE_DEAD);
        release = dev->release;
        if (release != NULL)
                release(dev);
}

int device_begin_unregister(struct device *dev)
{
        int ret = 0;

        if (dev == NULL)
                return -EINVAL;

        lock_fetch(&dev->lock);
        if (dev->state == DEVICE_DYING) {
                ret = -EALREADY;
                goto out;
        }
        if (dev->state != DEVICE_LIVE) {
                ret = -ENODEV;
                goto out;
        }

        dev->state = DEVICE_DYING;
        if (refcount_read(&dev->refs) != 1) {
                dev->state = DEVICE_LIVE;
                ret = -EBUSY;
        }
out:
        lock_release(&dev->lock);
        return ret;
}

int device_cancel_unregister(struct device *dev)
{
        int ret = 0;

        if (dev == NULL)
                return -EINVAL;

        lock_fetch(&dev->lock);
        if (dev->state != DEVICE_DYING) {
                ret = -EINVAL;
        } else {
                dev->state = DEVICE_LIVE;
        }
        lock_release(&dev->lock);
        return ret;
}

int device_finish_unregister(struct device *dev)
{
        int ret = 0;

        if (dev == NULL)
                return -EINVAL;

        lock_fetch(&dev->lock);
        if (dev->state != DEVICE_DYING) {
                ret = -EINVAL;
                goto out;
        }
        if (refcount_read(&dev->refs) != 1) {
                ret = -EBUSY;
                goto out;
        }

        list_del_init(&dev->node);
        dev->driver = NULL;
        dev->driver_data = NULL;
        dev->state = DEVICE_DEAD;
out:
        lock_release(&dev->lock);
        if (ret == 0)
                device_put(dev);
        return ret;
}

int register_device(struct device *dev)
{
        struct bus_type *bus;
        struct list_head *pos;
        int ret = 0;

        if (dev == NULL || dev->bus == NULL)
                return -EINVAL;
        bus = dev->bus;
        if (bus->match == NULL || bus->probe == NULL)
                return -EINVAL;

        lock_fetch(&dev->lock);
        if (dev->state == DEVICE_LIVE) {
                ret = -EALREADY;
                goto out;
        }
        if (dev->state != DEVICE_NEW) {
                ret = -ENODEV;
                goto out;
        }

        list_for_each (pos, &bus->driver_list) {
                struct driver *drv = container_of(pos, struct driver, node);

                if (!bus->match(dev, drv))
                        continue;

                ret = bus->probe(dev, drv);
                if (ret != 0) {
                        dev->driver = NULL;
                        dev->driver_data = NULL;
                        goto out;
                }
                dev->driver = drv;
                break;
        }

        refcount_init(&dev->refs, 1);
        dev->state = DEVICE_LIVE;
        list_add_tail(&dev->node, &bus->device_list);
out:
        lock_release(&dev->lock);
        return ret;
}

#ifdef CONFIG_QEMU_TEST
#include <kernel/qemu_test.h>

static int device_test_probe_result;
static int device_test_release_count;

static int device_test_match(struct device *dev, struct driver *drv)
{
        return dev->name == drv->name;
}

static int device_test_bus_probe(struct device *dev, struct driver *drv)
{
        return drv->probe(dev);
}

static int device_test_probe(struct device *dev)
{
        dev->driver_data = (void *) 1;
        return device_test_probe_result;
}

static void device_test_release(struct device *dev)
{
        (void) dev;
        device_test_release_count++;
}

static void device_test_bus_init(struct bus_type *bus)
{
        bus->name = "device-test";
        bus->match = device_test_match;
        bus->probe = device_test_bus_probe;
        INIT_LIST_HEAD(&bus->node);
        INIT_LIST_HEAD(&bus->device_list);
        INIT_LIST_HEAD(&bus->driver_list);
}

void device_lifecycle_regression_test(void)
{
        struct bus_type bus;
        struct device dev;
        struct driver drv;
        bool acquired;
        int released_before;
        int ret;

        device_test_bus_init(&bus);
        device_init(&dev, device_test_release);
        dev.name = "lifecycle";
        dev.bus = &bus;

        ret = register_device(&dev);
        acquired = ret == 0 && device_get_live(&dev);
        frog_test_case("device.live-get",
                       acquired && refcount_read(&dev.refs) == 2);

        ret = device_begin_unregister(&dev);
        frog_test_case("device.unregister-busy",
                       ret == -EBUSY && dev.state == DEVICE_LIVE &&
                           refcount_read(&dev.refs) == 2);
        if (acquired)
                device_put(&dev);

        ret = device_begin_unregister(&dev);
        frog_test_case("device.unregister-dying",
                       ret == 0 && dev.state == DEVICE_DYING &&
                           !device_get_live(&dev));
        ret = device_cancel_unregister(&dev);
        frog_test_case("device.unregister-cancel",
                       ret == 0 && dev.state == DEVICE_LIVE);

        released_before = device_test_release_count;
        ret = device_begin_unregister(&dev);
        if (ret == 0)
                ret = device_finish_unregister(&dev);
        frog_test_case("device.unregister-dead",
                       ret == 0 && dev.state == DEVICE_DEAD &&
                           refcount_read(&dev.refs) == 0 &&
                           list_is_empty(&bus.device_list) &&
                           device_test_release_count == released_before + 1);
        frog_test_case("device.dead-no-revive", !device_get_live(&dev));
        frog_test_case("device.dead-no-reregister",
                       register_device(&dev) == -ENODEV &&
                           dev.state == DEVICE_DEAD &&
                           refcount_read(&dev.refs) == 0);

        device_test_bus_init(&bus);
        drv.name = "probe-error";
        drv.bus = &bus;
        drv.probe = device_test_probe;
        INIT_LIST_HEAD(&drv.node);
        device_test_probe_result = -EIO;
        ret = register_driver(&drv);
        device_init(&dev, device_test_release);
        dev.name = drv.name;
        dev.bus = &bus;
        if (ret == 0)
                ret = register_device(&dev);
        frog_test_case("device.register-probe-error",
                       ret == -EIO && dev.state == DEVICE_NEW &&
                           refcount_read(&dev.refs) == 0 &&
                           dev.driver == NULL &&
                           dev.driver_data == NULL &&
                           list_is_empty(&bus.device_list));
        device_test_probe_result = EIO;
        ret = register_device(&dev);
        frog_test_case("device.register-positive-probe-error",
                       ret == EIO && dev.state == DEVICE_NEW &&
                           refcount_read(&dev.refs) == 0 &&
                           dev.driver == NULL &&
                           dev.driver_data == NULL &&
                           list_is_empty(&bus.device_list));

        device_test_bus_init(&bus);
        device_init(&dev, device_test_release);
        dev.name = "driver-probe-error";
        dev.bus = &bus;
        ret = register_device(&dev);
        drv.name = dev.name;
        drv.bus = &bus;
        drv.probe = device_test_probe;
        INIT_LIST_HEAD(&drv.node);
        device_test_probe_result = EIO;
        if (ret == 0)
                ret = register_driver(&drv);
        frog_test_case("driver.register-probe-error",
                       ret == EIO && dev.state == DEVICE_LIVE &&
                           dev.driver == NULL &&
                           dev.driver_data == NULL &&
                           list_is_empty(&bus.driver_list));
        if (dev.state == DEVICE_LIVE && device_begin_unregister(&dev) == 0)
                device_finish_unregister(&dev);
}
#endif
