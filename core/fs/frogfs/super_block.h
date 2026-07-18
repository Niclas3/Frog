#ifndef __FS_SUPER_BLOCK
#define __FS_SUPER_BLOCK

#include <frog/bitmap.h>
#include <frog/semaphore.h>
#include <frog/types.h>
#include <frog/compiler.h>

#define FROGFS_MAGIC 0xF206UL

// a super block for frogfs
struct __frogfs_super_block {
        uint_32 s_magic;     // magic number of this file system 0x2023B07A
        char vol_name[16];    // volume name
        uint_32 s_ninodes;   // inodes number
        uint_32 s_inode_sz;  // inodes size in byte

        uint_32 s_nzones;   // logic blocks number
        uint_32 s_zone_sz;  // logic block size in byte 512B

        uint_32 s_imap_blk;  // inode bitmap start block
        uint_32 s_imap_sz;   // inode bitmap size count in blocks count in
                              // block ()

        uint_32 s_zmap_blk;  // zone bitmap start block
        uint_32 s_zmap_sz;   // zone (logic blocks) bitmap size count in blocks
                              // count in sector (aka 512B)

        uint_32 s_inode_table_blk;  // inode table start sector
        uint_32 s_inode_table_sz;   // inode table size in sector

        uint_32 s_data_start_blk;  // first data block
        uint_32 root_inode_no;
        uint_32 dir_entry_size;

        uint_32 s_log_zone_sz;  // log2(disk blocks / logic blocks)
        uint_32 s_max_file_sz;  // max length for one file

        uint_32 s_mtime;   // modified date
        uint_8 s_rd_only;  // read only mark

        uint_8 pad[427];  // for up to 512 bytes
} __attribute__((packed));


STATIC_ASSERT(sizeof(struct __frogfs_super_block) == 512, superblock_must_be_512_bytes);

// a super block for frogfs
struct frogfs_super_block {
        struct __frogfs_super_block disk_sb;
        struct bitmap *z_bmap;
        struct bitmap *i_bmap;
        struct lock fs_lock;
        bool needs_fsck;
};

struct super_block;
enum frogfs_bmap_t;

int_32 alloc_inode_bitmap(struct super_block *sb);

int_32 free_inode_bitmap(struct super_block *sb, int_32 index);

int_32 alloc_zone_bitmap(struct super_block *sb);

int_32 free_znode_bitmap(struct super_block *sb, int_32 index);


int flush_bitmap_block(struct super_block *sb,
                       enum frogfs_bmap_t b_type,
                       int_32 bit_idx);

int read_bitmap(struct super_block *sb,
                uint_32 blk_start,
                uint_32 blk_size,
                struct bitmap *bmap);
int write_bitmap(struct super_block *sb,
                 uint_32 blk_start,
                 uint_32 blk_count,
                 struct bitmap *bmap);

#endif
