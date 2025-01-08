#ifndef _FS_PART_MBR_H_
#define _FS_PART_MBR_H_
#include <frog/types.h>

/*
 * partition_table_entry
 *-----------------------------------------------------------------------------
 *| offset |  data width |               description                           |
 *------------------------------------------------------------------------------
 *|   0    |     1       | active partition mark. value 0x80 or 0x0.if 0x80 presents os-loadable.
 *------------------------------------------------------------------------------
 *|   1    |     1       | partition start header number
 *------------------------------------------------------------------------------
 *|   2    |     1       | partition start sector number
 *------------------------------------------------------------------------------
 *|   3    |     1       | partition start cylinder number
 *------------------------------------------------------------------------------
 *|   4    |     1       | file system type id, 0x0 represents unknow file system, 1 for FAT32
 *------------------------------------------------------------------------------
 *|   5    |     1       | partition end head number
 *------------------------------------------------------------------------------
 *|   6    |     1       | partition end sector number
 *------------------------------------------------------------------------------
 *|   7    |     1       | partition end cylinder number
 *------------------------------------------------------------------------------
 *|   8    |     4       | start of offset partition
 *------------------------------------------------------------------------------
 *|   12   |     4       | capacity of sectors
 *-----------------------------------------------------------------------------
 **/
struct partition_table_entry {
        uint_8 bootable;     // 0x80 is bootable
        uint_8 start_head;
        uint_8 start_sec;    // sector
        uint_8 start_chs;    // cylinder
        uint_8 fs_type;
        uint_8 end_head;
        uint_8 end_sec;      // sector
        uint_8 end_chs;      // cylinder
        uint_32 offset_lba;  // this offset in sector lba so you need to times
                             // 512 to covert to xxx bytes
        uint_32 sec_cnt;     // all sector counts
} __attribute__((packed));

struct boot_sector {
        uint_8 code_area[446];                   // Bootstrap code area
        struct partition_table_entry tables[4];  // primary partition table
        uint_16 signature;                       // 0xaa55
} __attribute__((packed));

// System file type ID fs_type
enum dpt_fs_t {
        DPT_FILE_SYSTEM_TYPE_UNKNOW = 0x0,
        DPT_FILE_SYSTEM_TYPE_EXT = 0x5,
        DPT_FILE_SYSTEM_TYPE_LINUX = 0x83
};

#define IS_NULL_ENTRY(entry)                                 \
        ((entry).end_head == 0) && ((entry).end_sec == 0) && \
            ((entry).end_chs == 0)

#endif
