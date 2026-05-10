#ifndef __FROG_BLOCK_H
#define __FROG_BLOCK_H
#include <frog/blk_types.h>
#include <frog/blkdevice.h>

struct gendisk;

extern void block_init(void);

struct block_device *get_block_device(dev_t dev_no);
extern int bio_write(struct block_device *hd,
              unsigned int lba,
              void *buf,
              unsigned int sec_cnt);
extern int bio_read(struct block_device *hd,
              unsigned int lba,
              void *buf,
              unsigned int sec_cnt);

#endif
