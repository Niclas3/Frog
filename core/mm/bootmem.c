#include <global.h>

#include <asm/page.h>

#include <frog/bootmem.h>
#include <frog/errno.h>
#include <frog/string.h>

#define BOOTMEM_HANDOFF_START 0x00000500U
#define BOOTMEM_HANDOFF_END   0x00100000U

/* A 32-bit exclusive end cannot represent 4 GiB. Keep it page aligned. */
#define BOOTMEM_MAX_ALLOC_END 0xfffff000ULL

static struct bootmem_entry bootmem_snapshot[BOOTMEM_MAX_ENTRIES];
static uint_32 bootmem_snapshot_count;
static bool bootmem_snapshot_ready;

static bool bootmem_handoff_object_valid(uint_32 address, uint_32 size)
{
        if (address < BOOTMEM_HANDOFF_START || address >= BOOTMEM_HANDOFF_END)
                return false;
        return size <= BOOTMEM_HANDOFF_END - address;
}

bool bootmem_range_valid(const struct bootmem_entry *entry)
{
        unsigned long long base;
        unsigned long long length;

        if (entry == NULL)
                return false;
        base = ((unsigned long long) entry->base_high << 32) |
               entry->base_low;
        length = ((unsigned long long) entry->length_high << 32) |
                 entry->length_low;
        if (length == 0)
                return false;
        return base <= ~0ULL - length;
}

static unsigned long long bootmem_entry_base(
    const struct bootmem_entry *entry)
{
        return ((unsigned long long) entry->base_high << 32) |
               entry->base_low;
}

static unsigned long long bootmem_entry_end(
    const struct bootmem_entry *entry)
{
        unsigned long long length =
            ((unsigned long long) entry->length_high << 32) |
            entry->length_low;

        return bootmem_entry_base(entry) + length;
}

int bootmem_find_usable_end(const struct bootmem_entry *entries,
                            uint_32 count,
                            uint_32 start,
                            uint_32 *end_out)
{
        unsigned long long candidate_end = 0;

        if (entries == NULL || end_out == NULL || count == 0 ||
            count > BOOTMEM_MAX_ENTRIES || (start & (PAGE_SIZE - 1)) != 0)
                return -EINVAL;

        for (uint_32 index = 0; index < count; index++) {
                if (!bootmem_range_valid(&entries[index]))
                        return -EINVAL;
        }

        for (uint_32 index = 0; index < count; index++) {
                const struct bootmem_entry *entry = &entries[index];
                unsigned long long base;
                unsigned long long end;

                if (entry->type != BOOTMEM_TYPE_USABLE)
                        continue;
                base = bootmem_entry_base(entry);
                end = bootmem_entry_end(entry);
                if (base > start || end <= start)
                        continue;
                if (end > BOOTMEM_MAX_ALLOC_END)
                        end = BOOTMEM_MAX_ALLOC_END;
                if (end > candidate_end)
                        candidate_end = end;
        }
        if (candidate_end <= start)
                return -EINVAL;

        /* Any non-usable overlap either invalidates start or caps the run. */
        for (uint_32 index = 0; index < count; index++) {
                const struct bootmem_entry *entry = &entries[index];
                unsigned long long base;
                unsigned long long end;

                if (entry->type == BOOTMEM_TYPE_USABLE)
                        continue;
                base = bootmem_entry_base(entry);
                end = bootmem_entry_end(entry);
                if (end <= start || base >= candidate_end)
                        continue;
                if (base <= start)
                        return -EINVAL;
                candidate_end = base;
        }

        candidate_end &= ~((unsigned long long) PAGE_SIZE - 1);
        if (candidate_end <= start)
                return -EINVAL;
        *end_out = (uint_32) candidate_end;
        return 0;
}

int bootmem_init_from_handoff(void)
{
        const uint_32 count_slot = MMAP_INFO_COUNT_POINTER;
        const uint_32 map_slot = MMAP_INFO_POINTER;
        uint_32 count_address;
        uint_32 map_address;
        uint_32 count;

        if (bootmem_snapshot_ready)
                return 0;
        if (!bootmem_handoff_object_valid(count_slot, sizeof(uint_32)) ||
            !bootmem_handoff_object_valid(map_slot, sizeof(uint_32)))
                return -EINVAL;

        count_address = *(volatile const uint_32 *) count_slot;
        map_address = *(volatile const uint_32 *) map_slot;
        if (!bootmem_handoff_object_valid(count_address, sizeof(uint_32)))
                return -EINVAL;
        count = *(volatile const uint_32 *) count_address;
        if (count == 0 || count > BOOTMEM_MAX_ENTRIES)
                return -EINVAL;
        if (!bootmem_handoff_object_valid(
                map_address, count * sizeof(struct bootmem_entry)))
                return -EINVAL;

        const struct bootmem_entry *source =
            (const struct bootmem_entry *) map_address;
        for (uint_32 index = 0; index < count; index++) {
                if (!bootmem_range_valid(&source[index]))
                        return -EINVAL;
        }

        memcpy(bootmem_snapshot, source,
               count * sizeof(struct bootmem_entry));
        bootmem_snapshot_count = count;
        bootmem_snapshot_ready = true;
        return 0;
}

uint_32 bootmem_count(void)
{
        return bootmem_snapshot_ready ? bootmem_snapshot_count : 0;
}

const struct bootmem_entry *bootmem_get(uint_32 index)
{
        if (!bootmem_snapshot_ready || index >= bootmem_snapshot_count)
                return NULL;
        return &bootmem_snapshot[index];
}
