#include <frog/bio.h>
#include <frog/blkdevice.h>
#include <frog/blk_types.h>
// put all bio to request -> add those requests to request queue
// wait io_scheduler execute request
void submit_bio(int flag, struct bio *bio)
{
        // make bio to request
        struct block_device *bdev = bio->bi_bdev;
        struct request_queue *queue = bdev->bd_queue;
        struct request *req;
        // make a request and add it to request queue and call block dirver

        bdev->bd_disk->fops->request(queue);  //
}
