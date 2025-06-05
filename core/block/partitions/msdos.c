/**
 * MBR
 *
 *****************************************************************************/
#include <frog/blk_types.h>
#include <frog/block.h>
#include <frog/list.h>
#include <frog/memory.h>
#include <frog/string.h>
#include <kernel/assert.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <stdio.h>
#include "mbr.h"


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
static inline void move_4_entries(struct partition_table_entry *des,
                                  struct partition_table_entry *src)
{
        copy_4_entries(des, src);
        memset(src, 0, sizeof(struct partition_table_entry) * 4);
}
/**
 * get disk partition table
 *
 * @param hd disk pointer for read it
 * @param start_lba offset in lba
 * @param entries entries for return
 * @return 0 success
 *****************************************************************************/
static inline uint_32 get_dpt(struct block_device *hd,
                              uint_32 start_lba,
                              struct partition_table_entry *entries)
{
        struct boot_sector sector;
        /*
         * Read the first sector of given hard disk, it contains MBR(master boot
         * record)layout is
         * here[https://en.wikipedia.org/wiki/Master_boot_record] The first
         * partition entry at 0x01be, size 16 bytes. There are 4 same partition
         * entries
         * * */
        bio_read(hd, start_lba, &sector, 1);
        copy_4_entries(entries,
                       (struct partition_table_entry *) &sector.tables);
        ASSERT(sector.signature == 0xaa55);
        return 0;
}

/**
 * Calculate next logic partition table from given entries
 *
 * @param entries from extension partition (only 2)
 * @param next return next partition table to this pointer
 * @return return 0 represent the last entry
 *****************************************************************************/

static uint_32 g_ext_base_offset = 0;  // in lba type
static uint_32 next_dpt(struct block_device *hd,
                        struct partition_table_entry *entries,
                        struct partition_table_entry *next)
{
        for (uint_32 i = 0; i < 4; i++) {
                struct partition_table_entry entry = entries[i];
                if (IS_NULL_ENTRY(entry)) {
                        continue;
                }
                // 1. test (entries->fs_type) if it is 0x5 which means next dpt
                if (entry.fs_type == DPT_FILE_SYSTEM_TYPE_EXT) {
                        get_dpt(hd, g_ext_base_offset + entry.start_lba, next);
                        return 1;
                } else {
                        // ignore other type disk partition table
                }
        }
        return 0;
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
        struct partition_table_entry entries[4] = {0};
        struct partition_table_entry next[4] = {0};
        struct partition_table_entry *p_entries = entries;
        struct partition_table_entry *p_next = next;

        get_dpt(hd, 0, p_entries);
        for (int i = 0; i < 4; i++) {
                uint_32 sec_cnt = entries[i].sec_cnt;
                uint_32 start_lba = entries[i].start_lba;
                uint_8 type = entries[i].fs_type;
                bool is_end_of_logic_partation = false;
                // if this entry is empty
                if (!sec_cnt && !start_lba) {
                        continue;
                }

                if (type == DPT_FILE_SYSTEM_TYPE_EXT ||
                    type == DPT_FILE_SYSTEM_TYPE_LBA) {
                        // logic partations
                        uint_32 ext_base = start_lba;
                        uint_32 logic_next_lba = 0;
                        uint_32 count = 1;

                        struct block_device *extbdev = alloc_partation_bdev(
                            hd, ext_base, sec_cnt, i + 1);
                        add_partations_bdev(hd, extbdev);

                        while (!is_end_of_logic_partation) {
                                memset(
                                    next, 0,
                                    sizeof(struct partition_table_entry) * 2);
                                get_dpt(hd, ext_base + logic_next_lba, p_next);
                                uint_32 logic_start_lba = ext_base +
                                                          logic_next_lba +
                                                          next[0].start_lba;
                                uint_32 logic_sec_cnt = next[0].sec_cnt;
                                struct block_device *pbdev =
                                    alloc_partation_bdev(hd, logic_start_lba,
                                                         logic_sec_cnt,
                                                         i + count + 1);
                                add_partations_bdev(hd, pbdev);
                                bool is_empty_next = IS_NULL_ENTRY(next[1]);
                                if (!is_empty_next) {
                                        logic_next_lba = next[1].start_lba;
                                        count++;
                                        if (count >= MAX_LOGICAL_PARTATIONS) {
                                                is_end_of_logic_partation =
                                                    true;
                                                INFO(
                                                    "[block]: MAX of logic "
                                                    "partations number");
                                                break;
                                        }
                                } else {
                                        is_end_of_logic_partation = true;
                                }
                        }
                } else {
                        // primary partations
                        struct block_device *part_bdev =
                            alloc_partation_bdev(hd, start_lba, sec_cnt, i + 1);
                        add_partations_bdev(hd, part_bdev);
                }
        }
}
