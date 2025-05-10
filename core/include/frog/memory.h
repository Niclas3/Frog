#ifndef __SYS_MEMORY_H
#define __SYS_MEMORY_H
#include <frog/bitmap.h>
#include <frog/list.h>
#include <frog/types.h>
#include <frog/bug.h>

typedef struct _virtual_addr {
    struct bitmap vaddr_bitmap;
    uint_32 vaddr_start;
} virtual_addr;

typedef enum mem_pool_type { MP_KERNEL = 1, MP_USER } pool_type;

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

/*
 * Tell the user there is some problem. Beep too, so we can
 * see^H^H^Hhear bugs in early bootup as well!
 * The offending file and line are encoded after the "officially
 * undefined" opcode for parsing in the trap handler.
 */
#define PAGE_BUG(page) do { \
	BUG(); \
} while (0)

#define DESC_CNT 7  // type counts of memory blocks

/* P bit shows if or not this entry in memory
 * R/W W bit shows read / execute
 * R/W R bit shows read / execute
 */
#define PG_P_SET 1
#define PG_P_CLI 0

#define PG_RW_W 2
#define PG_RW_R 0
#define PG_US_S 0  // supervisor
#define PG_US_U 4  // user


void mem_init(void);
// alloc any size memory
void *sys_malloc(uint_32 size);
// Free pointed memory
void sys_free(void *ptr);

void mfree_page(enum mem_pool_type poolt, void *_vaddr, uint_32 pg_cnt);
void free_phy_page(uint_32 phy_addr_page);

// Alloc a page aka (4kb) link to vaddr_start
void *malloc_page_with_vaddr(enum mem_pool_type poolt, uint_32 vaddr_start);

void *malloc_page_with_vaddr_test(enum mem_pool_type poolt,
                                  uint_32 vaddr_start);

// alloc a phyaddr to given virtual address
// void *get_phy_free_page_with_vaddr(enum mem_pool_type poolt, uint_32 vaddr);
void *get_phy_free_page_with_vaddr(enum mem_pool_type poolt,
                                   uint_32 vaddr,
                                   uint_32 *child_pgdir);

// init block descriptors
void block_desc_init(struct mem_block_desc *desc_array);

// // get or free 4k phy memory aka 1 page -> pte
// void* get_free_page(struct pool *mpool);
// void free_page(struct pool *mpool, uint_32 phy_addr_page);

// Copy or free 4m phy memory -> dte
// TODO:
// void copy_page_tables();
// void free_page_tables();

// combine v address -> phy address
// v_addr from `get_free_vaddress`
// phy_addr from `get_free_page()`
void put_page(void *v_addr, void *phy_addr);

uint_32 addr_v2p(uint_32 vaddr);
// Get kernel page from memory
void *get_kernel_page(uint_32 pg_cnt);

void *get_user_page(uint_32 pg_cnt);

uint_32 *pde_ptr(uint_32 vaddr);

uint_32 *pte_ptr(uint_32 vaddr);

void *kmalloc(uint_32 size);
void kfree(void *ptr);
void *umalloc(uint_32 size);
void ufree(void *ptr);


#endif
