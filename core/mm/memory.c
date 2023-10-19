#include <const.h>  // for PG_SIZE
#include <math.h>   // for DIV_ROUND_UP
#include <debug.h>
#include <panic.h>
#include <string.h>
#include <sys/int.h>
#include <sys/memory.h>
#include <sys/threads.h>

// Assume that core.o is 70kb aka 0x11800
// and start at 0x80000
// so the end is 0x80000 + 0x11800 = 0x91800
/* #define K_HEAP_START 0x00092000 */
#define K_HEAP_START 0xc0100000 + (PG_SIZE * (PDT_COUNT + PGT_COUNT))
// from 0x92000 to 0xa0000 aka 0xe000 => 12 threads
// 14 pages aka 14 * 4kb = 0xe000
/* #define K_HEAP_START 0x00100000 */

#define MEM_BITMAP_BASE 0xc0009a00
/* #define MEM_BITMAP_BASE 0x00009a00 */

// 1 page dir table and 8 page table
#define PDT_COUNT 1
#define PGT_COUNT 254 + 1 + 1

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

struct mem_block_desc k_block_descs[DESC_CNT];

struct pool kernel_pool;
struct pool user_pool;
struct _virtual_addr kernel_viraddr;

// upper 10 bits pde
#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)
// mid   10 bits pte
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)

void free_addr_bitmap(struct bitmap *map,
                             uint_32 addr_start,
                             uint_32 addr,
                             uint_32 pos,
                             uint_32 pg_cnt);
uint_32 virtual_addr_to_phycial_addr(void *v_addr);
void free_vaddress(pool_type poolt, uint_32 vaddress, uint_32 pg_cnt);

void *malloc_page(enum mem_pool_type poolt, uint_32 pg_cnt);
void mfree_page(enum mem_pool_type poolt, void *_vaddr, uint_32 pg_cnt);

/*
 * Init description of memory block from 16B to 1024B
 * kernel description block array is `k_block_descs[DESC_CNT]`
 * */
void block_desc_init(struct mem_block_desc *desc_array)
{
    uint_16 desc_idx = 0;
    uint_16 block_size = 16;
    for (desc_idx = 0; desc_idx < DESC_CNT; desc_idx++) {
        desc_array[desc_idx].block_size = block_size;
        desc_array[desc_idx].blocks_per_arena =
            (PG_SIZE - sizeof(struct arena)) / block_size;
        init_list_head(&desc_array[desc_idx].free_list);
        block_size *= 2;
    }
}

static void mem_pool_init(uint_32 all_mem)
{
    // 1 page dir table and 254 page table
    //                      769 ~ 1022 pde
    //                      768 and 0 pg
    uint_32 page_table_size = PG_SIZE * (PDT_COUNT + PGT_COUNT);
    /*
     *  Page table start at 0x100000
     *  used_mem = page_table_size + 1MB
     *  1MB includes 0xa000 and 0xb800
     **/
    /* uint_32 used_mem = page_table_size + 0x100000;
     * I put page table at 0x0
     * 0x80000 ~ 0x92000    kernel code
     * 0xa0000              vga
     * 0xb8000              text view
     * 0x100000             end of used memory
     * 0x100000 ~ 4kb * (PDT_COUNT + PGT_COUNT)   page table
     * physical memory usage
     * */
    uint_32 used_mem = page_table_size + 0x100000;
    /*
     *  all_mem for now is 32MB
     *  It must be calculate by loader.s before entering protected mode
     * */
    uint_32 free_mem = all_mem - used_mem;
    uint_16 all_free_pages = free_mem / PG_SIZE;

    /* kernel used memory vs user used memory
     * */
    uint_16 kernel_free_page = all_free_pages / 2;
    uint_16 user_free_page = all_free_pages - kernel_free_page;


    // Kernel bitmap length
    // In bitmap 1 bit represents a free page.
    // div 8 for how many one byte
    uint_32 kbm_length = kernel_free_page / 8;

    // user space bitmap length
    uint_32 ubm_length = user_free_page / 8;

    // Kernel pool start
    // First address of free memory
    // kp_start = 0x0100000;  //1M
    uint_32 kp_start = used_mem;
    // User pool start
    // up_start  = 0x1080000;  // 15.5Mb kernel physical memory
    // free_page = 0xf80; // 3968
    uint_32 up_start = kp_start + kernel_free_page * PG_SIZE;

    kernel_pool.phy_addr_start = kp_start;

    user_pool.phy_addr_start = up_start;

    kernel_pool.pool_size = kernel_free_page * PG_SIZE;
    user_pool.pool_size = user_free_page * PG_SIZE;

    kernel_pool.pool_bitmap.map_bytes_length = kbm_length;
    user_pool.pool_bitmap.map_bytes_length = ubm_length;

    // kernel pool bit map fix at MEM_BITMAP_BASE 0x9a00
    kernel_pool.pool_bitmap.bits = (void *) MEM_BITMAP_BASE;
    user_pool.pool_bitmap.bits = (void *) (MEM_BITMAP_BASE + kbm_length);

    // init kernel bitmap
    init_bitmap(&kernel_pool.pool_bitmap);
    // init user bitmap
    init_bitmap(&user_pool.pool_bitmap);
    lock_init(&kernel_pool.lock);
    lock_init(&user_pool.lock);

    // vaddr for kernel used
    kernel_viraddr.vaddr_bitmap.map_bytes_length = kbm_length;
    kernel_viraddr.vaddr_bitmap.bits =
        (void *) (MEM_BITMAP_BASE + kbm_length + ubm_length);
    kernel_viraddr.vaddr_start = K_HEAP_START;
    init_bitmap(&kernel_viraddr.vaddr_bitmap);
}

void mem_init()
{
    uint_32 mem_bytes_total = 0x2000000;  // 32M //(*(uint_32 *) (0xb00));
    mem_pool_init(mem_bytes_total);
    block_desc_init(k_block_descs);
}

// Return a mem_block from a arena[idx]
static struct mem_block *arena2block(struct arena *a, uint_32 idx)
{
    return (struct mem_block *) ((uint_32) a + sizeof(struct arena) +
                                 idx * a->desc->block_size);
}

// Return a given mem_block
// the block size is 4kb aka 0xfff;
static struct arena *block2arena(struct mem_block *b)
{
    return (struct arena *) ((uint_32) b & 0xfffff000);
}

// Alloc memory
// alloc 'size' memory from memory
// First we need know who want to alloc memory from mm, so test current process
// if is kernel use kernel pool of memory if not use user pool of memory.
void *sys_malloc(uint_32 size)
{
    pool_type pool_t;
    struct pool *mem_pool;
    uint_32 pool_size;
    struct mem_block_desc *descs;
    TCB_t *cur = running_thread();
    // Kernel
    if (cur->pgdir == NULL) {
        pool_t = MP_KERNEL;
        mem_pool = &kernel_pool;
        pool_size = kernel_pool.pool_size;
        descs = k_block_descs;
    } else {
        // User
        pool_t = MP_USER;
        mem_pool = &user_pool;
        pool_size = user_pool.pool_size;
        descs = cur->u_block_descs;
    }

    if (!(size > 0 && size < pool_size)) {
        return NULL;
    }

    struct arena *area;
    struct mem_block *block;
    lock_fetch(&mem_pool->lock);
    // If be allocated size is over 1024B return a whole arena
    if (size > 1024) {
        uint_32 page_cnt = DIV_ROUND_UP(size + sizeof(struct arena), PG_SIZE);
        area = malloc_page(pool_t, page_cnt);

        if (area != NULL) {
            memset(area, 0, PG_SIZE * page_cnt);
            area->desc = NULL;
            area->cnt = page_cnt;
            area->large = true;
            lock_release(&mem_pool->lock);
            return (void *) (area + 1);
        } else {
            // maybe not enough memory
            lock_release(&mem_pool->lock);
            return NULL;
        }
    } else {  // require memory less than 1024B
        uint_8 desc_idx;
        for (desc_idx = 0; desc_idx < DESC_CNT; desc_idx++) {
            if (size <= descs[desc_idx].block_size) {
                // from small to large
                break;
            }
        }
        // Alloc 1 page for arena if descriptor free list is empty
        if (list_is_empty(&descs[desc_idx].free_list)) {
            area = malloc_page(pool_t, 1);
            if (area == NULL) {
                lock_release(&mem_pool->lock);
                return NULL;
            }
            memset(area, 0, PG_SIZE);

            area->desc = &descs[desc_idx];
            area->large = false;
            area->cnt = descs[desc_idx].blocks_per_arena;
            uint_32 block_idx;
            enum intr_status old_status = intr_disable();
            for (block_idx = 0; block_idx < descs[desc_idx].blocks_per_arena;
                 block_idx++) {
                block = arena2block(area, block_idx);
                list_add_tail(&block->free_elem, &area->desc->free_list);
            }
            intr_set_status(old_status);
        } else {
        }

        // alloc block
        block = container_of(list_pop(&(descs[desc_idx].free_list)),
                             struct mem_block, free_elem);
        memset(block, 0, descs[desc_idx].block_size);
        area = block2arena(block);
        area->cnt--;
        lock_release(&mem_pool->lock);
        return (void *) block;
    }
}

void sys_free(void *ptr)
{
    ASSERT(ptr != NULL);
    struct pool *mem_pool;
    enum mem_pool_type pool_t;
    if (ptr != NULL) {
        TCB_t *cur = running_thread();
        // Is thread
        if (cur->pgdir == NULL) {
            ASSERT((uint_32) ptr >= K_HEAP_START);
            pool_t = MP_KERNEL;
            mem_pool = &kernel_pool;
        } else {  // is process
            pool_t = MP_USER;
            mem_pool = &user_pool;
        }
        lock_fetch(&mem_pool->lock);
        // Get target pointer arena get metadate
        struct mem_block *block = ptr;
        struct arena *a = block2arena(block);
        ASSERT(a->large == 0 || a->large == 1);
        if (a->desc == NULL &&
            a->large == true) {  // arena is equal or over 1024B
            mfree_page(pool_t, a, a->cnt);
        } else {
            /* If less than 1024B, first free memory to desc->free_list
             * */
            list_add_tail(&block->free_elem, &a->desc->free_list);
            // Test all arena free_list are free, if true release arena
            if (++a->cnt == a->desc->blocks_per_arena) {
                uint_32 block_idx;
                for (block_idx = 0; block_idx < a->desc->blocks_per_arena;
                     block_idx++) {
                    struct mem_block *b = arena2block(a, block_idx);
                    list_del_init(&b->free_elem);
                }
                mfree_page(pool_t, a, 1);
            }
        }
        lock_release(&mem_pool->lock);
    }
}

void free_addr_bitmap(struct bitmap *map,
                             uint_32 addr_start,
                             uint_32 addr,
                             uint_32 pos,
                             uint_32 pg_cnt)
{
    if (addr < addr_start)
        panic("free bad phy address");
    if (pos > map->map_bytes_length)
        panic("free bad address over length");
    for (int i = 0; i < pg_cnt; i++) {
        set_value_bitmap(map, pos + i, 0);
    }
}

// Picking a pool return a free v_address
static void *get_free_vaddress(pool_type poolt, uint_32 pg_cnt)
{
    int_32 v_start_addr = 0;
    int_32 start_pos = -1;
    TCB_t *cur = running_thread();
    if (cur->pgdir == NULL && poolt == MP_KERNEL) {
        start_pos = find_block_bitmap(&kernel_viraddr.vaddr_bitmap, pg_cnt);
        if (start_pos == -1) {
            return NULL;
        }
        for (int i = 0; i < pg_cnt; i++) {
            set_value_bitmap(&kernel_viraddr.vaddr_bitmap, start_pos + i, 1);
        }
        v_start_addr = start_pos * PG_SIZE + kernel_viraddr.vaddr_start;
        return (void *) v_start_addr;
    } else if (cur->pgdir != NULL && poolt == MP_USER) {
        // TODO: user vaddress pools
        start_pos =
            find_block_bitmap(&cur->progress_vaddr.vaddr_bitmap, pg_cnt);
        if (start_pos == -1) {
            return NULL;
        }
        for (int i = 0; i < pg_cnt; i++) {
            set_value_bitmap(&cur->progress_vaddr.vaddr_bitmap, start_pos + i,
                             1);
        }
        v_start_addr = start_pos * PG_SIZE + cur->progress_vaddr.vaddr_start;
        return (void *) v_start_addr;
    } else {
        PAINC(
            "get_free_vaddress: not allow kernel alloc userspace or user alloc "
            "kernel space.");
        return NULL;
    }
}

// Remove assign vaddress at `pos`
void free_vaddress(pool_type poolt, uint_32 vaddress, uint_32 pg_cnt)
{
    if (poolt == MP_KERNEL) {
        uint_32 pos = (vaddress - kernel_viraddr.vaddr_start) / PG_SIZE;
        free_addr_bitmap(&kernel_viraddr.vaddr_bitmap,
                         kernel_viraddr.vaddr_start, vaddress, pos, pg_cnt);
    } else {
        TCB_t *cur = running_thread();
        uint_32 pos = (vaddress - cur->progress_vaddr.vaddr_start) / PG_SIZE;
        free_addr_bitmap(&cur->progress_vaddr.vaddr_bitmap,
                         cur->progress_vaddr.vaddr_start, vaddress, pos,
                         pg_cnt);
    }
}

// Get a free 4k phy memory aka 1 page in pool
// -> pte
void *get_free_page(struct pool *mpool)
{
    int_32 start_pos = -1;
    start_pos = find_block_bitmap(&mpool->pool_bitmap, 1);
    if (start_pos == -1) {
        return NULL;
    }
    set_value_bitmap(&mpool->pool_bitmap, start_pos, 1);
    return (void *) (start_pos * PG_SIZE + mpool->phy_addr_start);
}

// Release target address at mpool
void free_page(struct pool *mpool, uint_32 phy_addr_page)
{
    if (phy_addr_page < mpool->phy_addr_start)
        panic("free bad phy address");
    int pos = (phy_addr_page - mpool->phy_addr_start) / PG_SIZE;
    if (pos > mpool->pool_bitmap.map_bytes_length)
        panic("free bad address over length");
    set_value_bitmap(&mpool->pool_bitmap, pos, 0);
}

uint_32 *pte_ptr(uint_32 vaddr)
{
    // I set last PDE as PDT table address
    // So the No.1023 pde to hex is 0x3ff
    // when vaddress is 0xffc00000
    // It will get pdt table address
    // top    10 bits 0xffc    <--- this is the last PDE point to page aka PDT
    // middle 10 bits (vaddr's top 10 bits which is original pde index)
    uint_32 target_pte =
        (0xffc00000 + ((vaddr & 0xffc00000) >> 10) + PTE_IDX(vaddr) * 4);
    return (uint_32 *) target_pte;
}

uint_32 *pde_ptr(uint_32 vaddr)
{
    // The last page table address in PDE is
    // 0xfffffxxx
    // top    10 bits 0x3ff   <-- the last entry of PDT
    // middle 10 bits 0x3ff   <-- pointer to self (same PDT) again
    // bottom 12 pde_idx      <-- target entry
    uint_32 *target_pde = (uint_32 *) (0xfffff000 + PDE_IDX(vaddr) * 4);
    return target_pde;
}
uint_32 addr_v2p(uint_32 vaddr)
{
    uint_32 *pte = pte_ptr(vaddr);
    return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
}
uint_32 virtual_addr_to_phycial_addr(void *v_addr)
{
    uint_32 *pde = pde_ptr((uint_32) v_addr);
    uint_32 *pte = pte_ptr((uint_32) v_addr);
    if (*pde & 0x00000001) {        // pde is exist
        if ((*pte & 0x00000001)) {  // pte is exist
            uint_32 p_addr = *pte & 0xfffff000;
            return (uint_32) p_addr;
        } else {
            return NULL;
        }
    }
}
// Combine v address -> phy address
void put_page(void *v_addr, void *phy_addr)
{
    uint_32 vaddress = (uint_32) v_addr;
    uint_32 phyaddress = (uint_32) phy_addr;
    uint_32 *pde = pde_ptr(vaddress);
    uint_32 *pte = pte_ptr(vaddress);

    // test P bit of vaddress
    if (*pde & 0x00000001) {
        // TODO:
        // test if pte is exist
        // should re-consider v-address start
        /* ASSERT(!(*pte & 0x00000001)); */
        if ((!(*pte & 0x00000001))) {
            *pte = (phyaddress | PG_US_U | PG_RW_W | PG_P_SET);
        } else {
            /* panic("pte exists"); */
            *pte = (phyaddress | PG_US_U | PG_RW_W | PG_P_SET);
        }
    } else {
        // if there is no pde , let's create it.
        // Create phyaddr at kernel pool
        uint_32 pde_phyaddr = (uint_32) get_free_page(&kernel_pool);
        *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_SET);
        // Clear pte target address 1 page 4kb
        // top 10 ->
        memset((void *) ((int) pte & 0xfffff000), 0, PG_SIZE);
        ASSERT(!(*pte & 0x00000001));
        *pte = (phyaddress | PG_US_U | PG_RW_W | PG_P_SET);
    }
}

// Remove v address from page table
void remove_page(void *v_addr)
{
    uint_32 vaddress = (uint_32) v_addr;
    uint_32 *pde = pde_ptr(vaddress);
    uint_32 *pte = pte_ptr(vaddress);
    if (*pde & 0x00000001) {      // test pde if exist
        if (*pte & 0x00000001) {  // test pde if exist or not
            *pte &= 0x11111110;    // PG_P_CLI;
        } else {  // pte is not exists
            panic("Free twice");
            // Still make pde is unexist
            *pte &= 0x11111110;             //PG_P_CLI;
        }
        __asm__ volatile("invlpg %0" ::"m"(v_addr) : "memory");
    }
}

// Get free vaddress and paddress and put them together
// 1. get free vaddress from vpool according to kernel or user
// 2. get free phy address from pool using get_free_page(pool)
// 3. put vaddress and paddress together using put_page(vaddr, paddr)
void *malloc_page(enum mem_pool_type poolt, uint_32 pg_cnt)
{
    ASSERT(pg_cnt > 0 && pg_cnt < 3840);
    void *vaddr_start = get_free_vaddress(poolt, pg_cnt);
    if (vaddr_start == NULL) {
        return NULL;
    }
    uint_32 vaddr = (uint_32) vaddr_start;
    struct pool *mem_pool = poolt & MP_KERNEL ? &kernel_pool : &user_pool;

    while (pg_cnt--) {
        void *phyaddrs = get_free_page(mem_pool);
        if (phyaddrs == NULL) {
            return NULL;
        }
        put_page((void *) vaddr, phyaddrs);
        vaddr += PG_SIZE;
    }
    return vaddr_start;
}

/* @param poolt  : pool types
 * @param vddr   : start freed virtual address
 * @param pg_cnt : continue page numbers
 * */
void mfree_page(enum mem_pool_type poolt, void *_vaddr, uint_32 pg_cnt)
{
    uint_32 vaddr = (uint_32) _vaddr;
    struct pool *mem_pool = poolt & MP_KERNEL ? &kernel_pool : &user_pool;
    free_vaddress(poolt, vaddr, pg_cnt);
    for (int i = 0; i < pg_cnt; i++) {
        uint_32 phy_addr = virtual_addr_to_phycial_addr(vaddr);
        free_page(mem_pool, phy_addr);
        remove_page(vaddr);
        vaddr += PG_SIZE;
    }
}

// get free vaddress and paddress and put them together
void *malloc_page_with_vaddr(enum mem_pool_type poolt, uint_32 vaddr_start)
{
    struct pool *mem_pool = poolt & MP_KERNEL ? &kernel_pool : &user_pool;
    int_32 bit_idx = -1;
    TCB_t *cur = running_thread();
    if (cur->pgdir == NULL && poolt == MP_KERNEL) {
        bit_idx = (vaddr_start - kernel_viraddr.vaddr_start) / PG_SIZE;
        ASSERT(bit_idx > 0);
        set_value_bitmap(&kernel_viraddr.vaddr_bitmap, bit_idx, 1);
    } else if (cur->pgdir != NULL && poolt == MP_USER) {
        bit_idx = (vaddr_start - cur->progress_vaddr.vaddr_start) / PG_SIZE;
        ASSERT(bit_idx > 0);
        set_value_bitmap(&cur->progress_vaddr.vaddr_bitmap, bit_idx, 1);
    } else {
        PAINC(
            "get_free_vaddress: not allow kernel alloc userspace or user alloc "
            "kernel space.");
    }
    void *phyaddrs = get_free_page(mem_pool);
    if (phyaddrs == NULL) {
        return NULL;
    }
    put_page((void *) vaddr_start, phyaddrs);
    return (void *) vaddr_start;
}

// Get kernel page from memory
void *get_kernel_page(uint_32 pg_cnt)
{
    lock_fetch(&kernel_pool.lock);
    void *vaddr = malloc_page(MP_KERNEL, pg_cnt);
    if (vaddr != NULL) {
        memset(vaddr, 0, pg_cnt * PG_SIZE);
    }
    lock_release(&kernel_pool.lock);
    return vaddr;
}

// Get user mode page from memory
void *get_user_page(uint_32 pg_cnt)
{
    lock_fetch(&user_pool.lock);
    void *vaddr = malloc_page(MP_USER, pg_cnt);
    if (vaddr != NULL) {
        memset(vaddr, 0, pg_cnt * PG_SIZE);
    }
    lock_release(&user_pool.lock);
    return vaddr;
}
