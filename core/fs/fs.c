#include <device/ide.h>
#include <fs/fs.h>
#include <fs/inode.h>
#include <fs/super_block.h>
#include <fs/dir.h>
#include <math.h>

/**
 * In this file, we will create file system at some partition.
 * 1. Creating super block
 *    file system layout
 *  ┌───────────┬───────────┬───────┬──────┬──────┬────┬─────────────┐
 *  │ Operating │ super     │ free  │inode │inode │root│ free zone   │
 *  │ system    │ block     │ zone  │      │      │    │             │
 *  │ boot      │           │ bitmap│bitmap│table │dir │             │
 *  │ block     │           │       │      │      │    │             │
 *  └───────────┴───────────┴───────┴──────┴──────┴────┴─────────────┘
 *                             imap   zmap  inode (data zone)
 *   512B        512B
 *   0 sector    1 sector
 *****************************************************************************/
/**
 * partition_format() init all things
 *
 * @param hd target hard disk
 * @param part target partition
 * @return
 *****************************************************************************/

static void partition_format(struct partition *part)
{
    uint_32 os_boot_record_sz = 1;  // obr size in sector
    uint_32 super_block_sz = 1;     // super block size in sector

    //(files/bits) / (bits/sector) = files/ sector
    uint_32 inode_bitmap_sz =
        DIV_ROUND_UP(MAX_FILES_PER_PARTITION, BITS_PER_SECTOR);
    uint_32 inode_table_sz = DIV_ROUND_UP(
        ((sizeof(struct inode)) * MAX_FILES_PER_PARTITION), SECTOR_SIZE);
    uint_32 used_sector_sz =
        os_boot_record_sz + super_block_sz + inode_bitmap_sz + inode_table_sz;
    uint_32 free_sector_sz = part->sec_cnt - used_sector_sz;

    uint_32 zone_bitmap_sz = DIV_ROUND_UP(free_sector_sz, BITS_PER_SECTOR);
    uint_32 bm_len = free_sector_sz - zone_bitmap_sz;
    zone_bitmap_sz = DIV_ROUND_UP(bm_len, BITS_PER_SECTOR);
    uint_32 zones_sz = free_sector_sz - zone_bitmap_sz;

    // Init super block
    struct super_block sb;
    sb.s_magic = 0x2023B07A;
    sb.s_ninodes =
        DIV_ROUND_UP(inode_table_sz * SECTOR_SIZE, sizeof(struct inode));
    sb.s_nzones = DIV_ROUND_UP(zones_sz, ZONE_SIZE);

    sb.s_imap_lba = part->start_lba + os_boot_record_sz + super_block_sz;
    sb.s_imap_sz = inode_bitmap_sz;

    sb.s_zmap_lba = sb.s_imap_lba + inode_bitmap_sz;
    sb.s_zmap_sz = zone_bitmap_sz;

    sb.s_inode_table_lba = sb.s_zmap_lba + zone_bitmap_sz;
    sb.s_inode_table_sz = inode_table_sz;

    sb.s_data_start_lba = sb.s_inode_table_lba + inode_table_sz;
    sb.root_inode_no = 0;
    sb.dir_entry_size = sizeof(struct dir_entry);
    // Write super blocks to disk
    
}
