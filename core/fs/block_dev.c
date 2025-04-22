// contain blk_devs[] for global table what register blk_dev
#include <frog/types.h>
void register_blkdev() {}

int fs_blk_read(struct block_device *bdev, char *buf, size_t size) {}

int fs_blk_write(struct block_device *bdev, char *buf, size_t size) {}
