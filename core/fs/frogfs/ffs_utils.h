#include <frog/block.h>
#include <frog/memory.h>
#include <frog/types.h>
#include <kernel/vfs.h>
#include "inode.h"
#include "frogfs.h"

int frogfs_validate_bdev(struct block_device *bdev);
int frogfs_read_super_sector(struct block_device *bdev,
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
