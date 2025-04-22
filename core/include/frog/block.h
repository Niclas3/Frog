#ifndef __FROG_BLOCK_H
#define __FROG_BLOCK_H
#include <frog/bio.h>
#include <frog/blk_types.h>

struct gendisk;

extern void block_init(void);
extern void submit_bio(int rwflag, struct bio *bio);
// bio_* functions will read/write sectors
extern void bio_write(struct block_device *hd,
                      unsigned int lba,
                      void *buf,
                      unsigned int sec_cnt);
extern void bio_read(struct block_device *hd,
                     unsigned int lba,
                     void *buf,
                     unsigned int sec_cnt);

#endif
