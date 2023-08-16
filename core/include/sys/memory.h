#ifndef __SYS_MEMORY_H
#define __SYS_MEMORY_H
#include <bitmap.h>
#include <ostype.h>

// #define MEM_BITMAP_BASE 0xc009a00
#define MEM_BITMAP_BASE 0x0009a00
struct virtual_addr {
    struct bitmap vaddr_bitmap;
    uint_32 vaddr_start;
};

typedef enum mem_pool_type{
    MP_KERNEL = 1,
    MP_USER
} pool_type;

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
                    //
struct pool {
    struct bitmap pool_bitmap;
    uint_32 phy_addr_start;  // pool must at a phy address
    uint_32 pool_size;
};

void mem_init(void);

//Picking a pool return a free v_address
static void* get_free_vaddress(pool_type poolt, uint_32 pg_cnt);

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

// Get kernel page from memory
void* get_kernel_page(uint_32 pg_cnt);

#endif
