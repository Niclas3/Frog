#ifndef __MM_MEM_EGG_H
#define __MM_MEM_EGG_H
#include <frog/types.h>
#include <frog/bitmap.h>
#include <frog/semaphore.h>
/*  If struct arena's attribute large is true cnt stand for page_frame cnt,
 *  if not for mem_block count.
 *  there are 7 different description.
 *  1. 1024 B
 *  2. 512  B
 *  3. 256  B
 *  4. 128  B
 *  5. 64   B
 *  6. 32   B
 *  7. 16   B
 * Why we use 16B?
 * int -> 32bits -> 8B
 * So the smallest arena can hold 2 int numbers.
 */
struct arena {
        struct mem_block_desc *desc;
        uint_32 cnt;
        bool large;  // flag about this arena is over 1024b or not
};

struct mem_block {
        struct list_head free_elem;
};

struct pool {
        struct bitmap pool_bitmap;
        struct lock lock;
        uint_32 phy_addr_start;  // pool must at a phy address
        uint_32 pool_size;
};
#endif
