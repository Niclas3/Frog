#ifndef __FROGFS_FFS_UTILS_H
#define __FROGFS_FFS_UTILS_H

#include <frog/block.h>
#include <frog/memory.h>
#include <frog/types.h>
#include <kernel/vfs.h>
#include "inode.h"
#include "frogfs.h"

enum frogfs_probe_status {
        FROGFS_PROBE_VALID = 0,
        FROGFS_PROBE_NOT_FROGFS,
        FROGFS_PROBE_CORRUPT,
        FROGFS_PROBE_UNREADABLE,
};

int frogfs_validate_bdev(struct block_device *bdev);
enum frogfs_probe_status frogfs_probe_superblock(
    struct block_device *bdev,
    struct __frogfs_super_block *disk_sb);
int frogfs_write_super_sector(struct block_device *bdev,
                              const struct __frogfs_super_block *disk_sb);
void frogfs_mark_needs_fsck(struct super_block *sb);

int read_blocks(struct super_block *sb,
                uint_32 start,
                uint_32 count,
                uint_8 *buf);

int write_blocks(struct super_block *sb,
                 uint_32 start,
                 uint_32 count,
                 const uint_8 *buf);

#endif
