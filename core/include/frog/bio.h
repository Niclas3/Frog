#ifndef __FROG_BIO_H
#define __FROG_BIO_H

struct block_device;

struct bio_vec {
        char *bv_page;         // memory start pointer
        unsigned int bv_size;  // size of buffer
        unsigned int offset;   // offset;
};

#define BIO_OPERATOR_READ 1
#define BIO_OPERATOR_WRITE 2
#define BIO_OPERATOR_SYNC 3

struct bio {
        unsigned int bi_start_sector;   // read lba start
        unsigned int bi_sector_number;  // read number of sector
        unsigned int
            bi_opf;  // operator flag (READ/WRITE/SYNC) bio_operator_flag
        void (*bi_completion_callback)(struct bio *bio);  // completion callback
        struct block_device *bi_bdev;                     // target block device
        struct bio_vec *bi_vec;                           // target of memory
        struct bio *bi_next;                              // next bio
};

#endif
