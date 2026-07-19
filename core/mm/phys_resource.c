#include <frog/errno.h>
#include <frog/kernel.h>
#include <frog/list.h>
#include <frog/phys_resource.h>
#include <kernel/assert.h>
#include <kernel/panic.h>

static struct phys_resource_registry physical_resources;
static struct phys_resource boot_ram_resources[BOOTMEM_MAX_ENTRIES];

static bool phys_resource_type_valid(enum phys_resource_type type)
{
        return type == PHYS_RESOURCE_RAM || type == PHYS_RESOURCE_MMIO;
}

static bool phys_resource_cache_valid(enum phys_resource_type type,
                                      enum vm_cache_mode cache_mode)
{
        return (type == PHYS_RESOURCE_RAM &&
                cache_mode == VM_CACHE_WRITE_BACK) ||
               (type == PHYS_RESOURCE_MMIO &&
                cache_mode == VM_CACHE_UNCACHED);
}

static int phys_resource_range_end(unsigned long long start,
                                   unsigned long long length,
                                   unsigned long long *end)
{
        if (length == 0)
                return -EINVAL;
        if (start > ~0ULL - length)
                return -EOVERFLOW;
        *end = start + length;
        return 0;
}

static bool phys_resource_ranges_overlap(unsigned long long first_start,
                                         unsigned long long first_end,
                                         unsigned long long second_start,
                                         unsigned long long second_end)
{
        return first_start < second_end && second_start < first_end;
}

void phys_resource_init(struct phys_resource *resource)
{
        ASSERT(resource != NULL);
        resource->start = 0;
        resource->end = 0;
        resource->type = 0;
        resource->cache_mode = 0;
        resource->state = PHYS_RESOURCE_NEW;
        refcount_init(&resource->refs, 0);
        resource->registry = NULL;
        INIT_LIST_HEAD(&resource->node);
}

void phys_resource_registry_init(struct phys_resource_registry *registry)
{
        ASSERT(registry != NULL);
        INIT_LIST_HEAD(&registry->resources);
        lock_init(&registry->lock);
        registry->initialized = true;
}

static int phys_resource_register_unlocked(
    struct phys_resource_registry *registry,
    struct phys_resource *resource,
    unsigned long long start,
    unsigned long long length,
    enum phys_resource_type type,
    enum vm_cache_mode cache_mode)
{
        struct list_head *position;
        unsigned long long end;
        int result;

        if (registry == NULL || resource == NULL || !registry->initialized ||
            resource->state != PHYS_RESOURCE_NEW ||
            !phys_resource_type_valid(type) ||
            !phys_resource_cache_valid(type, cache_mode))
                return -EINVAL;

        result = phys_resource_range_end(start, length, &end);
        if (result < 0)
                return result;

        list_for_each(position, &registry->resources) {
                struct phys_resource *existing =
                    list_entry(position, struct phys_resource, node);

                if (phys_resource_ranges_overlap(start, end, existing->start,
                                                 existing->end) &&
                    (type != PHYS_RESOURCE_RAM ||
                     existing->type != PHYS_RESOURCE_RAM))
                        return -EBUSY;
        }

        /* Overlapping RAM entries are legal, so order in a separate pass. */
        list_for_each(position, &registry->resources) {
                struct phys_resource *existing =
                    list_entry(position, struct phys_resource, node);

                if (start < existing->start)
                        break;
        }

        resource->start = start;
        resource->end = end;
        resource->type = type;
        resource->cache_mode = cache_mode;
        resource->registry = registry;
        refcount_init(&resource->refs, 1);
        INIT_LIST_HEAD(&resource->node);
        list_add_tail(&resource->node, position);
        resource->state = PHYS_RESOURCE_REGISTERED;
        return 0;
}

int phys_resource_registry_init_from_bootmem(
    struct phys_resource_registry *registry,
    struct phys_resource *storage,
    uint_32 storage_count,
    const struct bootmem_entry *entries,
    uint_32 count)
{
        uint_32 usable_count = 0;

        if (registry == NULL || storage == NULL || entries == NULL ||
            count == 0 || count > BOOTMEM_MAX_ENTRIES)
                return -EINVAL;

        /* Validate the complete input before publishing any resource. */
        for (uint_32 index = 0; index < count; index++) {
                unsigned long long start;
                unsigned long long length;
                unsigned long long end;

                if (!bootmem_range_valid(&entries[index]))
                        return -EINVAL;
                if (entries[index].type != BOOTMEM_TYPE_USABLE)
                        continue;
                if (usable_count == storage_count)
                        return -ENOSPC;

                start = ((unsigned long long) entries[index].base_high << 32) |
                        entries[index].base_low;
                length =
                    ((unsigned long long) entries[index].length_high << 32) |
                    entries[index].length_low;
                if (phys_resource_range_end(start, length, &end) < 0)
                        return -EOVERFLOW;
                usable_count++;
        }
        if (usable_count == 0)
                return -EINVAL;

        phys_resource_registry_init(registry);
        usable_count = 0;
        for (uint_32 index = 0; index < count; index++) {
                unsigned long long start;
                unsigned long long length;
                int result;

                if (entries[index].type != BOOTMEM_TYPE_USABLE)
                        continue;
                start = ((unsigned long long) entries[index].base_high << 32) |
                        entries[index].base_low;
                length =
                    ((unsigned long long) entries[index].length_high << 32) |
                    entries[index].length_low;
                phys_resource_init(&storage[usable_count]);
                result = phys_resource_register_unlocked(
                    registry, &storage[usable_count], start, length,
                    PHYS_RESOURCE_RAM, VM_CACHE_WRITE_BACK);
                ASSERT(result == 0);
                usable_count++;
        }
        return 0;
}

int phys_resource_registry_register(struct phys_resource_registry *registry,
                                    struct phys_resource *resource,
                                    unsigned long long start,
                                    unsigned long long length,
                                    enum phys_resource_type type,
                                    enum vm_cache_mode cache_mode)
{
        int result;

        if (registry == NULL || !registry->initialized)
                return -EINVAL;
        lock_fetch(&registry->lock);
        result = phys_resource_register_unlocked(registry, resource, start,
                                                 length, type, cache_mode);
        lock_release(&registry->lock);
        return result;
}

int phys_resource_registry_unregister(struct phys_resource_registry *registry,
                                      struct phys_resource *resource)
{
        if (registry == NULL || resource == NULL || !registry->initialized)
                return -EINVAL;

        lock_fetch(&registry->lock);
        if (resource->state != PHYS_RESOURCE_REGISTERED ||
            resource->registry != registry) {
                lock_release(&registry->lock);
                return -EINVAL;
        }
        if (refcount_read(&resource->refs) != 1) {
                lock_release(&registry->lock);
                return -EBUSY;
        }

        list_del_init(&resource->node);
        resource->registry = NULL;
        resource->state = PHYS_RESOURCE_DEAD;
        lock_release(&registry->lock);

        if (!refcount_put(&resource->refs))
                PANIC("physical resource final reference was not released");
        return 0;
}

bool phys_resource_get_live(struct phys_resource *resource)
{
        if (resource == NULL ||
            resource->state != PHYS_RESOURCE_REGISTERED)
                return false;
        return refcount_get_live(&resource->refs);
}

void phys_resource_put(struct phys_resource *resource)
{
        ASSERT(resource != NULL);
        if (refcount_put(&resource->refs))
                PANIC("registered physical resource lost its owner reference");
}

bool phys_resource_contains(const struct phys_resource *resource,
                            unsigned long long start,
                            unsigned long long length)
{
        unsigned long long end;

        if (resource == NULL ||
            resource->state != PHYS_RESOURCE_REGISTERED ||
            phys_resource_range_end(start, length, &end) < 0)
                return false;
        return start >= resource->start && end <= resource->end;
}

int phys_resources_init_from_bootmem(const struct bootmem_entry *entries,
                                     uint_32 count)
{
        if (physical_resources.initialized)
                return -EBUSY;
        return phys_resource_registry_init_from_bootmem(
            &physical_resources, boot_ram_resources, BOOTMEM_MAX_ENTRIES,
            entries, count);
}

int phys_resource_register(struct phys_resource *resource,
                           unsigned long long start,
                           unsigned long long length,
                           enum phys_resource_type type,
                           enum vm_cache_mode cache_mode)
{
        return phys_resource_registry_register(&physical_resources, resource,
                                               start, length, type,
                                               cache_mode);
}

int phys_resource_unregister(struct phys_resource *resource)
{
        return phys_resource_registry_unregister(&physical_resources,
                                                 resource);
}
