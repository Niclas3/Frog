#include <frog/block.h>
#include <frog/memory.h>
#include <frog/types.h>
#include <kernel/vfs.h>
#include "inode.h"
#include "frogfs.h"

int read_blocks(struct super_block *sb,
                uint_32 start,
                uint_32 count,
                uint_8 *buf);

int write_blocks(struct super_block *sb,
                 uint_32 start,
                 uint_32 count,
                 uint_8 *buf);

int_32 next_inode_zone_blk(struct inode *inode, int_32 last_zone_idx);
