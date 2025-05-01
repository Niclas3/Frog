#include <kernel/bus.h>

LIST_HEAD(frog_bus_list);

int register_bus(struct bus_type *bus)
{
        INIT_LIST_HEAD(&bus->driver_list);
        INIT_LIST_HEAD(&bus->device_list);
        list_append_tail(&frog_bus_list, &bus->node);
        return 0;
}
