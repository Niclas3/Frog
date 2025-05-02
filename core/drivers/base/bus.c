#include <kernel/bus.h>
#include <kernel/assert.h>

LIST_HEAD(frog_bus_list);

int register_bus(struct bus_type *bus)
{
        ASSERT(bus != NULL);
        INIT_LIST_HEAD(&bus->driver_list);
        INIT_LIST_HEAD(&bus->device_list);
        list_add_tail(&bus->node, &frog_bus_list);
        return 0;
}
