#include <kernel/bus.h>
#include <kernel/driver.h>
#include <kernel/assert.h>
#include <kernel/device.h>
#include <frog/string.h>

struct bus_type isa_bus={};

static int isa_bus_match(struct device *dev, struct driver *drv)
{
        ASSERT(dev && drv);
        return strcmp(dev->name, drv->name) == 0;
}

static int isa_bus_probe(struct device *dev, struct driver *drv)
{
        ASSERT(dev && drv);
        return drv->probe(dev);
}

struct bus_type *isa_bus_init(void)
{
        isa_bus.name = "isa_bus";
        isa_bus.match = isa_bus_match;
        isa_bus.probe = isa_bus_probe;
        return &isa_bus;
}
