/**
 * MBR
 *
 *****************************************************************************/
#include <frog/blk_types.h>
#include <frog/block.h>
#include <frog/errno.h>
#include <frog/memory.h>
#include <frog/string.h>
#include <kernel/debug.h>
#include "mbr.h"

#define MSDOS_PRIMARY_PARTITIONS 4
#define MSDOS_MAX_PARTITIONS \
        (MSDOS_PRIMARY_PARTITIONS + MAX_LOGICAL_PARTATIONS)

struct parsed_partition {
        uint_32 start_lba;
        uint_32 sec_cnt;
        uint_32 minor;
};

struct msdos_parse_state {
        struct parsed_partition partitions[MSDOS_MAX_PARTITIONS];
        uint_32 partition_count;
        uint_32 primary_start[MSDOS_PRIMARY_PARTITIONS];
        uint_32 primary_count[MSDOS_PRIMARY_PARTITIONS];
        uint_32 primary_range_count;
        uint_32 logical_start[MAX_LOGICAL_PARTATIONS];
        uint_32 logical_count[MAX_LOGICAL_PARTATIONS];
        uint_32 logical_range_count;
        uint_32 visited_ebrs[MAX_LOGICAL_PARTATIONS + 1];
        uint_32 visited_ebr_count;
};


static inline void copy_entry(struct partition_table_entry *des,
                              struct partition_table_entry *src)
{
        des->start_head = src->start_head;
        des->start_sec = src->start_sec;
        des->start_chs = src->start_chs;
        des->end_head = src->end_head;
        des->end_sec = src->end_sec;
        des->end_chs = src->end_chs;
        des->fs_type = src->fs_type;
        des->bootable = src->bootable;
        des->sec_cnt = src->sec_cnt;
        des->start_lba = src->start_lba;
}
static inline void copy_4_entries(struct partition_table_entry *des,
                                  struct partition_table_entry *src)
{
        for (int i = 0; i < 4; i++) {
                copy_entry(&des[i], &src[i]);
        }
}

static bool is_extended_type(uint_8 type)
{
        return type == DPT_FILE_SYSTEM_TYPE_EXT ||
               type == DPT_FILE_SYSTEM_TYPE_LBA;
}

static bool entry_is_valid(const struct partition_table_entry *entry)
{
        if (IS_NULL_ENTRY(*entry))
                return true;
        return entry->fs_type != DPT_FILE_SYSTEM_TYPE_UNKNOW &&
               entry->start_lba != 0 && entry->sec_cnt != 0 &&
               (entry->bootable == 0 || entry->bootable == 0x80);
}

static bool ranges_overlap(uint_32 first_start,
                           uint_32 first_count,
                           uint_32 second_start,
                           uint_32 second_count)
{
        unsigned long long first_end =
            (unsigned long long) first_start + first_count;
        unsigned long long second_end =
            (unsigned long long) second_start + second_count;
        return first_start < second_end && second_start < first_end;
}

static bool range_contains_lba(uint_32 start, uint_32 count, uint_32 lba)
{
        unsigned long long end = (unsigned long long) start + count;
        return lba >= start && (unsigned long long) lba < end;
}

static bool partition_range_is_valid(struct block_device *hd,
                                     unsigned long long start_lba,
                                     unsigned long long sector_count)
{
        if (!hd || !hd->bd_disk || !sector_count)
                return false;
        unsigned long long device_start = hd->bd_start_lba;
        unsigned long long device_end =
            device_start + (unsigned long long) hd->bd_sec_cnt;
        unsigned long long disk_end = hd->bd_disk->lba_sectors;
        unsigned long long end_lba = start_lba + sector_count;
        return device_end >= device_start && device_end <= disk_end &&
               end_lba >= start_lba && start_lba >= device_start &&
               end_lba <= device_end && end_lba <= disk_end &&
               start_lba <= 0xffffffffULL;
}

static bool add_parsed_partition(struct msdos_parse_state *state,
                                 uint_32 start_lba,
                                 uint_32 sec_cnt,
                                 uint_32 minor)
{
        if (state->partition_count >= MSDOS_MAX_PARTITIONS)
                return false;
        struct parsed_partition *partition =
            &state->partitions[state->partition_count++];
        partition->start_lba = start_lba;
        partition->sec_cnt = sec_cnt;
        partition->minor = minor;
        return true;
}

static bool primary_range_is_unique(struct msdos_parse_state *state,
                                    uint_32 start_lba,
                                    uint_32 sec_cnt)
{
        for (uint_32 index = 0; index < state->primary_range_count; index++) {
                if (ranges_overlap(start_lba, sec_cnt,
                                   state->primary_start[index],
                                   state->primary_count[index]))
                        return false;
        }
        if (state->primary_range_count >= MSDOS_PRIMARY_PARTITIONS)
                return false;
        uint_32 index = state->primary_range_count++;
        state->primary_start[index] = start_lba;
        state->primary_count[index] = sec_cnt;
        return true;
}

static bool logical_range_is_unique(struct msdos_parse_state *state,
                                    uint_32 start_lba,
                                    uint_32 sec_cnt)
{
        for (uint_32 index = 0; index < state->logical_range_count; index++) {
                if (ranges_overlap(start_lba, sec_cnt,
                                   state->logical_start[index],
                                   state->logical_count[index]))
                        return false;
        }
        for (uint_32 index = 0; index < state->visited_ebr_count; index++) {
                if (range_contains_lba(start_lba, sec_cnt,
                                       state->visited_ebrs[index]))
                        return false;
        }
        if (state->logical_range_count >= MAX_LOGICAL_PARTATIONS)
                return false;
        uint_32 index = state->logical_range_count++;
        state->logical_start[index] = start_lba;
        state->logical_count[index] = sec_cnt;
        return true;
}

static bool add_visited_ebr(struct msdos_parse_state *state, uint_32 ebr_lba)
{
        for (uint_32 index = 0; index < state->visited_ebr_count; index++) {
                if (state->visited_ebrs[index] == ebr_lba)
                        return false;
        }
        for (uint_32 index = 0; index < state->logical_range_count; index++) {
                if (range_contains_lba(state->logical_start[index],
                                       state->logical_count[index], ebr_lba))
                        return false;
        }
        if (state->visited_ebr_count >= MAX_LOGICAL_PARTATIONS + 1)
                return false;
        state->visited_ebrs[state->visited_ebr_count++] = ebr_lba;
        return true;
}
/**
 * get disk partition table
 *
 * @param hd disk pointer for read it
 * @param start_lba offset in lba
 * @param entries entries for return
 * @return 0 success
 *****************************************************************************/
static int get_dpt(struct block_device *hd,
                   uint_32 start_lba,
                   struct partition_table_entry *entries)
{
        if (!entries || !partition_range_is_valid(hd, start_lba, 1))
                return -EINVAL;
        struct boot_sector sector = {0};
        /*
         * Read the first sector of given hard disk, it contains MBR(master boot
         * record)layout is
         * here[https://en.wikipedia.org/wiki/Master_boot_record] The first
         * partition entry at 0x01be, size 16 bytes. There are 4 same partition
         * entries
         * * */
        int ret = bio_read(hd, start_lba, &sector, 1);
        if (ret < 0)
                return ret;
        if (sector.signature != 0xaa55)
                return -EINVAL;
        copy_4_entries(entries,
                       (struct partition_table_entry *) &sector.tables);
        return 0;
}

static bool parse_logical_partitions(struct block_device *hd,
                                     struct msdos_parse_state *state,
                                     uint_32 ext_base,
                                     uint_32 ext_sec_cnt)
{
        struct partition_table_entry entries[4] = {0};
        unsigned long long ext_end =
            (unsigned long long) ext_base + ext_sec_cnt;
        uint_32 next_relative_lba = 0;
        uint_32 logical_minor = 5;

        while (true) {
                unsigned long long ebr_lba_64 =
                    (unsigned long long) ext_base + next_relative_lba;
                if (ebr_lba_64 >= ext_end || ebr_lba_64 > 0xffffffffULL ||
                    !partition_range_is_valid(hd, ebr_lba_64, 1))
                        return false;
                uint_32 ebr_lba = (uint_32) ebr_lba_64;
                if (!add_visited_ebr(state, ebr_lba) ||
                    get_dpt(hd, ebr_lba, entries) < 0)
                        return false;
                if (!IS_NULL_ENTRY(entries[2]) ||
                    !IS_NULL_ENTRY(entries[3]))
                        return false;

                struct partition_table_entry *data = &entries[0];
                struct partition_table_entry *link = &entries[1];
                if (IS_NULL_ENTRY(*data))
                        return IS_NULL_ENTRY(*link);
                if (!entry_is_valid(data) || is_extended_type(data->fs_type) ||
                    data->fs_type == DPT_FILE_SYSTEM_TYPE_GPT_PROTECTED)
                        return false;

                unsigned long long data_start_64 =
                    ebr_lba_64 + data->start_lba;
                unsigned long long data_end = data_start_64 + data->sec_cnt;
                if (data_start_64 <= ebr_lba_64 || data_end < data_start_64 ||
                    data_end > ext_end || data_start_64 > 0xffffffffULL ||
                    !partition_range_is_valid(hd, data_start_64,
                                              data->sec_cnt))
                        return false;
                uint_32 data_start = (uint_32) data_start_64;
                if (!logical_range_is_unique(state, data_start, data->sec_cnt) ||
                    !add_parsed_partition(state, data_start, data->sec_cnt,
                                          logical_minor++))
                        return false;

                if (IS_NULL_ENTRY(*link))
                        return true;
                if (!entry_is_valid(link) || !is_extended_type(link->fs_type) ||
                    link->bootable != 0)
                        return false;
                unsigned long long next_ebr_64 =
                    (unsigned long long) ext_base + link->start_lba;
                unsigned long long link_end = next_ebr_64 + link->sec_cnt;
                if (next_ebr_64 >= ext_end || next_ebr_64 > 0xffffffffULL ||
                    link_end < next_ebr_64 || link_end > ext_end ||
                    !partition_range_is_valid(hd, next_ebr_64,
                                              link->sec_cnt))
                        return false;
                if (state->logical_range_count >= MAX_LOGICAL_PARTATIONS)
                        return false;
                next_relative_lba = link->start_lba;
        }
}

static bool parse_msdos_partitions(struct block_device *hd,
                                   struct msdos_parse_state *state)
{
        struct partition_table_entry entries[4] = {0};
        if (!hd || !hd->bd_disk || !state ||
            get_dpt(hd, hd->bd_start_lba, entries) < 0)
                return false;

        uint_32 ext_base = 0;
        uint_32 ext_sec_cnt = 0;
        bool found_extended = false;
        for (uint_32 index = 0; index < MSDOS_PRIMARY_PARTITIONS; index++) {
                struct partition_table_entry *entry = &entries[index];
                if (IS_NULL_ENTRY(*entry))
                        continue;
                if (!entry_is_valid(entry) ||
                    entry->fs_type == DPT_FILE_SYSTEM_TYPE_GPT_PROTECTED)
                        return false;

                unsigned long long start_64 =
                    (unsigned long long) hd->bd_start_lba + entry->start_lba;
                if (start_64 == hd->bd_start_lba ||
                    start_64 > 0xffffffffULL ||
                    !partition_range_is_valid(hd, start_64, entry->sec_cnt))
                        return false;
                uint_32 start_lba = (uint_32) start_64;
                if (!primary_range_is_unique(state, start_lba,
                                             entry->sec_cnt) ||
                    !add_parsed_partition(state, start_lba, entry->sec_cnt,
                                          index + 1))
                        return false;

                if (is_extended_type(entry->fs_type)) {
                        if (found_extended)
                                return false;
                        found_extended = true;
                        ext_base = start_lba;
                        ext_sec_cnt = entry->sec_cnt;
                }
        }

        return !found_extended ||
               parse_logical_partitions(hd, state, ext_base, ext_sec_cnt);
}

static void publish_partitions(struct block_device *hd,
                               struct msdos_parse_state *state)
{
        if (!state->partition_count)
                return;
        struct block_device **devices =
            kmalloc(sizeof(*devices) * state->partition_count);
        if (!devices)
                return;
        memset(devices, 0, sizeof(*devices) * state->partition_count);

        uint_32 index = 0;
        for (; index < state->partition_count; index++) {
                struct parsed_partition *partition = &state->partitions[index];
                dev_t dev = DEV_NR(hd->bd_disk->major, partition->minor);
                if (get_block_device(dev))
                        break;
                devices[index] = alloc_partation_bdev(
                    hd, partition->start_lba, partition->sec_cnt,
                    partition->minor);
                if (!devices[index])
                        break;
        }
        if (index != state->partition_count) {
                for (uint_32 free_index = 0; free_index < index; free_index++)
                        kfree(devices[free_index]);
                kfree(devices);
                return;
        }

        for (index = 0; index < state->partition_count; index++) {
                if (add_partations_bdev(hd, devices[index]) < 0) {
                        kfree(devices[index]);
                        for (uint_32 free_index = index + 1;
                             free_index < state->partition_count; free_index++)
                                kfree(devices[free_index]);
                        break;
                }
        }
        kfree(devices);
}

/**
 * sacn all disk partition table to
 *                                   hd->prim_partition
 *                                   hd->logic_partition
 *
 * @param hd disk
 * @param ext_lba first is 0
 * @return
 *****************************************************************************/
void msdos_scan_partitions(struct block_device *hd)
{
        if (!hd || !hd->bd_disk)
                return;
        struct msdos_parse_state *state = kmalloc(sizeof(*state));
        if (!state)
                return;
        memset(state, 0, sizeof(*state));
        if (parse_msdos_partitions(hd, state))
                publish_partitions(hd, state);
        kfree(state);
}
