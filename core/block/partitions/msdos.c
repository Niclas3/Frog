/**
 * MBR
 *
 *****************************************************************************/
#include "mbr.h"
#include <frog/blk_types.h>
#include <frog/string.h>

static void copy_entry(struct partition_table_entry *des,
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
        des->offset_lba = src->offset_lba;
}

static void copy_4_entries(struct partition_table_entry *des,
                           struct partition_table_entry *src)
{
        for (int i = 0; i < 4; i++) {
                copy_entry(&des[i], &src[i]);
        }
}
static void move_4_entries(struct partition_table_entry *des,
                           struct partition_table_entry *src)
{
        copy_4_entries(des, src);
        memset(src, 0, sizeof(struct partition_table_entry) * 4);
}
/**
 * get disk partition table
 *
 * @param hd disk pointer for read it
 * @param offset_lba offset in lba
 * @param entries entries for return
 * @return 0 success
 *****************************************************************************/
static uint_32 get_dpt(struct disk *hd,
                uint_32 offset_lba,
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
        ide_read(hd, offset_lba, &sector, 1);
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
static uint_32 next_dpt(struct disk *hd,
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
                        get_dpt(hd, g_ext_base_offset + entry.offset_lba, next);
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
void scan_partitions(struct disk *hd)
{
        // TODO: maybe should use mm/sys_malloc()
        struct partition_table_entry entries[4] = {0};
        struct partition_table_entry next[4] = {0};
        struct partition_table_entry *p_entries = entries;
        struct partition_table_entry *p_next = next;

        uint_8 lp_idx = 0;  // logic_partition idex max is 8
        get_dpt(hd, 0, p_entries);
        // 1. hd->prim_partition
        for (int i = 0; i < 4; i++) {
                hd->prim_partition[i].sec_cnt = entries[i].sec_cnt;
                hd->prim_partition[i].start_lba = entries[i].offset_lba;
                hd->prim_partition[i].my_disk = hd;
                // main partition number start at no.1
                sprintf(hd->prim_partition[i].name, "%s%d", hd->name, i + 1);
                list_add_tail(&hd->prim_partition[i].part_tag, &partition_list);
        }

        // set ext partition offset
        uint_32 ext_base = entries[3].offset_lba;
        uint_32 pre_offset = 0;  // previous offset
        // 2. hd->logic_partition
        while (next_dpt(hd, p_entries, p_next)) {
                if (g_ext_base_offset == 0) {
                        g_ext_base_offset = ext_base;
                }
                hd->logic_partition[lp_idx].my_disk = hd;
                hd->logic_partition[lp_idx].sec_cnt = next[0].sec_cnt;
                // logic partition index starts at no.5
                sprintf(hd->logic_partition[lp_idx].name, "%s%d", hd->name,
                        lp_idx + 5);
                // g_ext_partition_offset is set at next_dpt() when the get
                // first logic partition
                hd->logic_partition[lp_idx].start_lba =
                    next[0].offset_lba + g_ext_base_offset + pre_offset;
                list_add_tail(&hd->logic_partition[lp_idx].part_tag,
                              &partition_list);

                // save offset to previous offset
                pre_offset = next[1].offset_lba;
                lp_idx++;
                copy_4_entries(p_entries, p_next);
        }
}
