#include <frog/string.h>
#include <kernel/assert.h>
#include <kernel/bus.h>
#include <kernel/device.h>
#include <kernel/driver.h>

struct bus_type platform_bus = {};

static int platform_bus_match(struct device *dev, struct driver *drv)
{
        ASSERT(dev && drv);
        return strcmp(dev->name, drv->name) == 0;
}

static int platform_bus_probe(struct device *dev, struct driver *drv)
{
        ASSERT(dev && drv);
        drv->probe(dev);
        return 0;
}

struct bus_type *platform_bus_init(void)
{
        platform_bus.name = "platform_bus";
        platform_bus.match = platform_bus_match;
        platform_bus.probe = platform_bus_probe;

        return &platform_bus;
}
