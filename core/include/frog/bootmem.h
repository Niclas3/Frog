#ifndef _FROG_BOOTMEM_H
#define _FROG_BOOTMEM_H

#include <frog/types.h>

#define BOOTMEM_MAX_ENTRIES 12U

#define BOOTMEM_TYPE_USABLE 1U

struct bootmem_entry {
        uint_32 base_low;
        uint_32 base_high;
        uint_32 length_low;
        uint_32 length_high;
        uint_32 type;
} __attribute__((packed));

bool bootmem_range_valid(const struct bootmem_entry *entry);
int bootmem_find_usable_end(const struct bootmem_entry *entries,
                            uint_32 count,
                            uint_32 start,
                            uint_32 *end_out);
int bootmem_init_from_handoff(void);
uint_32 bootmem_count(void);
const struct bootmem_entry *bootmem_get(uint_32 index);

#endif
