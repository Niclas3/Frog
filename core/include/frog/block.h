#ifndef __FROG_BLOCK_H
#define __FROG_BLOCK_H
#include <frog/blk_types.h>
#include <frog/blkdevice.h>

struct gendisk;

typedef int (*block_partition_callback_t)(struct block_device *bdev,
                                          void *data);

int block_init(void);

struct block_device *get_block_device(dev_t dev_no);

/*
 * Visit the partitions published before this call.  The block registry is
 * startup-only today: publication is serialized, no block_device removal
 * exists, and callbacks must not publish disks recursively.  Consequently a
 * callback may retain a returned pointer for the rest of the boot.  The walk
 * is bounded by registry counters captured on entry and never exposes list
 * nodes or whole-disk block_device objects.
 *
 * Returns zero after a complete walk, the callback's non-zero stop value, or
 * a negative error if the registry changes or is internally inconsistent.
 */
int block_for_each_partition(block_partition_callback_t callback, void *data);
void msdos_scan_partitions(struct block_device *bdev);
extern int bio_write(struct block_device *hd,
              unsigned int lba,
              void *buf,
              unsigned int sec_cnt);
extern int bio_read(struct block_device *hd,
              unsigned int lba,
              void *buf,
              unsigned int sec_cnt);

#endif
