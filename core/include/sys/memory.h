#ifndef __SYS_MEMORY_H
#define __SYS_MEMORY_H
#include <bitmap.h>
#include <ostype.h>
#include <sys/semaphore.h>
#include <list.h>

typedef struct _virtual_addr {
    struct bitmap vaddr_bitmap;
    uint_32 vaddr_start;
} virtual_addr;

typedef enum mem_pool_type{
    MP_KERNEL = 1,
    MP_USER
} pool_type;

struct mem_block {
    struct list_head free_elem;
};

// The largest size is 4KB
// There are seven different descriptions 
//
// @Attr block_size :
struct mem_block_desc {
    uint_32 block_size;
    uint_32 blocks_per_arena;
    struct list_head free_list;
};

#define DESC_CNT 7    // type counts of memory blocks

/* P bit shows if or not this entry in memory 
 * R/W W bit shows read / execute
 * R/W R bit shows read / execute
 */
#define PG_P_SET 1            
#define PG_P_CLI 0

#define PG_RW_W  2
#define PG_RW_R  0
#define PG_US_S  0  // supervisor
#define PG_US_U  4  // user
                    
struct pool {
    struct bitmap pool_bitmap;
    struct lock lock;
    uint_32 phy_addr_start;  // pool must at a phy address
    uint_32 pool_size;
};

// alloc any size memory
void *sys_malloc(uint_32 size);

void mem_init(void);

//Picking a pool return a free v_address
static void* get_free_vaddress(pool_type poolt, uint_32 pg_cnt);
//Alloc a page aka (4kb) link to vaddr_start
void *malloc_page_with_vaddr(enum mem_pool_type poolt, uint_32 vaddr_start);

// get or free 4k phy memory aka 1 page -> pte
void* get_free_page(struct pool *mpool);

void free_page(struct pool *mpool, uint_32 phy_addr_page);

// Copy or free 4m phy memory -> dte
// TODO: 
// void copy_page_tables();
// void free_page_tables();

// combine v address -> phy address
// v_addr from `get_free_vaddress`
// phy_addr from `get_free_page()`
void put_page(void *v_addr, void* phy_addr);

uint_32 addr_v2p(uint_32 vaddr);
// Get kernel page from memory
void* get_kernel_page(uint_32 pg_cnt);
void* get_user_page(uint_32 pg_cnt);

#endif
