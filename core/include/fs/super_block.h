#ifndef __FS_SUPER_BLCOK
#define __FS_SUPER_BLCOK

#include <ostype.h>
#include <bitmap.h>

struct super_block {
    uint_32 s_magic;          // magic number of this file system 0x2023B07A
    uint_32 s_ninodes;        // inodes number
    uint_32 s_nzones;          // logic blocks number

    uint_32 s_imap_lba;       // inode bitmap start sector
    uint_32 s_imap_sz;        // inode bitmap size count in blocks count in sector (aka 512B)

    uint_32 s_zmap_lba;       // zone bitmap start sector
    uint_32 s_zmap_sz;        // zone (logic blocks) bitmap size count in blocks count in sector (aka 512B)

    uint_32 s_inode_table_lba; //inode table start sector
    uint_32 s_inode_table_sz;  // inode table size in sector
                                   //  512B       /    4kB
    uint_32 s_data_start_lba;
    uint_32 root_inode_no;
    uint_32 dir_entry_size;

    uint_32 s_log_zone_sz;    // log2(disk blocks / logic blocks)
    uint_32 s_max_file_sz;      // max length for one file

    uint_8 s_lock;             // locking mark
    uint_16 s_dev;             // device number of super block in
    uint_32 s_time;            // modified date
    uint_8 s_rd_only;          // read only mark
    uint_8 s_dirt;             // dirty mark

    uint_8 pad[11];            // for up to 512 bytes
}__attribute__((packed));

#endif
