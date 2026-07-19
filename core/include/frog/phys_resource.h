#ifndef _FROG_PHYS_RESOURCE_H
#define _FROG_PHYS_RESOURCE_H

#include <frog/bootmem.h>
#include <frog/list.h>
#include <frog/refcount.h>
#include <frog/semaphore.h>
#include <frog/types.h>

enum phys_resource_type {
        PHYS_RESOURCE_RAM = 1,
        PHYS_RESOURCE_MMIO = 2,
};

enum vm_cache_mode {
        VM_CACHE_WRITE_BACK = 0,
        VM_CACHE_UNCACHED,
};

enum phys_resource_state {
        PHYS_RESOURCE_NEW = 0,
        PHYS_RESOURCE_REGISTERED,
        PHYS_RESOURCE_DEAD,
};

struct phys_resource_registry;

struct phys_resource {
        unsigned long long start;
        unsigned long long end;
        enum phys_resource_type type;
        enum vm_cache_mode cache_mode;
        enum phys_resource_state state;
        refcount_t refs;
        struct phys_resource_registry *registry;
        struct list_head node;
};

struct phys_resource_registry {
        struct list_head resources;
        struct lock lock;
        bool initialized;
};

void phys_resource_init(struct phys_resource *resource);
void phys_resource_registry_init(struct phys_resource_registry *registry);

/*
 * Build an unpublished registry from a complete validated E820 snapshot.
 * This early-boot operation does not take registry->lock; the caller must not
 * publish the registry until it returns. Storage must hold every usable entry.
 */
int phys_resource_registry_init_from_bootmem(
    struct phys_resource_registry *registry,
    struct phys_resource *storage,
    uint_32 storage_count,
    const struct bootmem_entry *entries,
    uint_32 count);

int phys_resource_registry_register(struct phys_resource_registry *registry,
                                    struct phys_resource *resource,
                                    unsigned long long start,
                                    unsigned long long length,
                                    enum phys_resource_type type,
                                    enum vm_cache_mode cache_mode);
int phys_resource_registry_unregister(struct phys_resource_registry *registry,
                                      struct phys_resource *resource);

/*
 * The resource owner's lock must keep the object alive and serialize a new
 * get with unregister. The registry lock alone cannot protect caller-owned
 * storage after successful unregister.
 */
bool phys_resource_get_live(struct phys_resource *resource);
void phys_resource_put(struct phys_resource *resource);
bool phys_resource_contains(const struct phys_resource *resource,
                            unsigned long long start,
                            unsigned long long length);

/* Global registry wrappers used by physical device owners. */
int phys_resources_init_from_bootmem(const struct bootmem_entry *entries,
                                     uint_32 count);
int phys_resource_register(struct phys_resource *resource,
                           unsigned long long start,
                           unsigned long long length,
                           enum phys_resource_type type,
                           enum vm_cache_mode cache_mode);
int phys_resource_unregister(struct phys_resource *resource);

#endif
