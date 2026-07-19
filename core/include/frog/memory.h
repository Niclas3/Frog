#ifndef __SYS_MEMORY_H
#define __SYS_MEMORY_H
#include <frog/bitmap.h>
#include <frog/bug.h>
#include <frog/list.h>
#include <frog/types.h>
#include <frog/linker.h>  // fetch runtime elf section bounds
#include <asm/page.h>


// Kernel heap start after linker symbol `char _end[]`
// _end is defined at ld scripts which at./core/scripts/kernel_dbg.ld
#define K_HEAP_START ((uintptr_t) _end & ~0xfffUL) + 0x1000UL

//      KHEAP_SHA_MEM_START 0xFF400000
//      + 96 pages
#define K_STACK_POOL_BOTTOM 0xFF800000UL

// Top of the PDE[1023] (4MB) virtual address space
#define K_STACK_START 0xFFBFFFFFUL
#define K_THREAD_MAX 5
#define K_STACKSZ_IN_PAGE 2

#define INTR_STACKSZ_PAGE 2  // 2 times page size 8kb

typedef struct _virtual_addr {
        struct bitmap vaddr_bitmap;
        uint_32 vaddr_start;
} virtual_addr;

typedef enum mem_pool_type { MP_KERNEL = 1, MP_USER } pool_type;

struct mm_struct;
struct phys_resource;

// The largest size is 4KB
// There are seven different descriptions
//
// @Attr block_size :
struct mem_block_desc {
        uint_32 block_size;
        uint_32 blocks_per_arena;
        uint_32 redzone_size;
        struct list_head free_list;
};

/*
 * Tell the user there is some problem. Beep too, so we can
 * see^H^H^Hhear bugs in early bootup as well!
 * The offending file and line are encoded after the "officially
 * undefined" opcode for parsing in the trap handler.
 */
#define PAGE_BUG(page) \
        do {           \
                BUG(); \
        } while (0)

#define DESC_CNT 7  // type counts of memory blocks

void mem_init(void);
uint_32 mem_pool_fit_page_count(uint_32 page_count,
                                uint_32 bitmap_window_bytes);
// alloc any size memory
void *sys_malloc(uint_32 size);
// Free pointed memory
void sys_free(void *ptr);

void free_page(enum mem_pool_type poolt, void *_vaddr, uint_32 pg_cnt);
void free_phy_page(uint_32 phy_addr_page);

/* Raw page-table frames. These helpers do not create virtual mappings. */
uint_32 alloc_kernel_page_frame(void);
void free_kernel_page_frame(uint_32 physical);

// Alloc a page aka (4kb) link to vaddr_start
void *malloc_page_with_vaddr(enum mem_pool_type poolt, uint_32 vaddr_start);

// alloc a phyaddr to given virtual address
// void *get_phy_free_page_with_vaddr(enum mem_pool_type poolt, uint_32 vaddr);
void *get_phy_free_page_with_vaddr(enum mem_pool_type poolt,
                                   uint_32 vaddr,
                                   struct mm_struct *mm);

// init block descriptors
void block_desc_init(struct mem_block_desc *desc_array);

// // get or free 4k phy memory aka 1 page -> pte
// void* get_physical_page(struct pool *mpool);
// void free_physical_page(struct pool *mpool, uint_32 phy_addr_page);

// Copy or free 4m phy memory -> dte
// TODO:
// void copy_page_tables();
// void free_physical_page_tables();

// combine v address -> phy address
// v_addr from `get_virtual_pages`
// phy_addr from `get_physical_page()`
void put_page(void *v_addr, void *phy_addr);

/* The caller transfers a live resource pin to the permanent kernel alias. */
#define KERNEL_FRAMEBUFFER_VADDR 0xf0000000UL
int map_kernel_framebuffer_pinned(const struct phys_resource *resource,
                                  uint_32 size);

uint_32 addr_v2p(uint_32 vaddr);
// Get kernel page from memory
void *get_kernel_page(uint_32 pg_cnt);

void *get_user_page(uint_32 pg_cnt);

void *umalloc(uint_32 size);
void ufree(void *ptr);
void *kmalloc(uint_32 size);
void kfree(void *ptr);

#ifdef CONFIG_POSION_MEMORY
#else
#endif

#endif
