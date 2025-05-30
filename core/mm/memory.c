#include <asm/page.h>
#include <frog/irqflags.h>
#include <frog/memory.h>
#include <frog/semaphore.h>
#include <frog/string.h>
#include <frog/threads.h>

#include <frog/math.h>  // for DIV_ROUND_UP
#include <kernel/assert.h>
#include <kernel/debug.h>
#include <kernel/panic.h>

#include <frog/linker.h>  // fetch runtime elf section bounds
#include "ARDS.h"  // for Address Range Descriptor Structure at mem_init()

// for kernel test
#include <frog/printk.h>

// init per local cpu interrupt stack
#include <kernel/cpu.h>


#include "./mem_egg.h"    // structure of small memory

#define MEM_BITMAP_BASE 0xc0060000UL

// 1 page dir table
#define PDT_COUNT 1UL
// no.254 is upper 1G memory start at 0xc000_0000
//
// [PDE no.768       map-> pg0 address ] represents size 4MB
// 0xc000_0000
//
// [PDE no.769       ~ no.1023 pde -> pg1 2nd page address] represent size 1GB
// 0xc040_0000       ~ 0xFFC0_0000 : virtual address range

#define KPT_COUNT 255

#define PG0_COUNT 1
// In real world Frog don't need all Upper vaddress I will give it 4MB
#define PGT_COUNT (KPT_COUNT / 255 + PG0_COUNT)

#define PG_OCCUPIED 1
#define PG_VACANT 0

/*
 * kernel block descriptions.
 * */
struct mem_block_desc k_block_descs[DESC_CNT];

struct pool kernel_pool;
struct pool user_pool;
struct _virtual_addr kernel_viraddr;


// upper 10 bits pde
#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)
// mid   10 bits pte
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)

static void flush_cr3(uint_32 *pgdir)
{
        uint_32 pagedir_phy_addr =
            0x100000;  // default pagedir address is 0x100000
        if (pgdir != NULL) {
                pagedir_phy_addr = addr_v2p((uint_32) pgdir);
        }
        __asm__ volatile("movl %0, %%cr3;"
                         :
                         : "r"(pagedir_phy_addr)
                         : "memory");
}

static void invalidate(void)
{
        TCB_t *thread = running_thread();
        ASSERT(thread);
        uint_32 pagedir_phy_addr =
            0x100000;  // default pagedir address is 0x100000
        if (thread->pgdir != NULL) {
                pagedir_phy_addr = addr_v2p((uint_32) thread->pgdir);
        }
        __asm__ volatile("movl %0, %%cr3;"
                         :
                         : "r"(pagedir_phy_addr)
                         : "memory");
}

// Get a free 4k phy memory aka 1 page in pool
// -> pte
static void *get_free_page(struct pool *mpool)
{
        int_32 start_pos = -1;
        start_pos = find_block_bitmap(&mpool->pool_bitmap, 1);
        if (start_pos == -1) {
                return NULL;
        }
        set_value_bitmap(&mpool->pool_bitmap, start_pos, 1);
        return (void *) (start_pos * PAGE_SIZE + mpool->phy_addr_start);
}


// Picking a pool return a free v_address
// clang-format off
//  Kernel virtual address space
//                          +-------------------------------------+
// 0xC000_0000              |                                     |
//                          |                                     |
//                          |                                     |
//                          |                                     |
// 0xC007_0000              +-------------------------------------+ kernel code
//                          |                                     |
//                          |                                     |
//                          |                                     |
// 0xC007_CFCC              +-------------------------------------+ _end
//                          |                                     |
// (_end & ~0xfff) + 0x1000 +-------------------------------------+ kernel heap
// bottom
//                          |                                     |
//                          |                                     |
                                        /* ... */
//                          |                                     |
//                          |                                     |
// 0xFF80_0000              +-------------------------------------+ kernel stack pool start
//                          |                                     |
//                          |                                     | reserver by kernel stack  / 1 pagesize 5page
// 0xFFBF_FFFF              +-------------------------------------+
// 0xFFC0_0000              +-------------------------------------+ Page table
// Directory start
//                          |                                     |
//                          |                                     |
// 0xFFFF_FFFF              +-------------------------------------+
// clang-format on
// Over all 80kb kernel stack
// 2 page per thread
// FrogOS support 10 kernel threads for now
static void alloc_kstack_pool(int page_count)
{
        ASSERT(page_count <= (K_THREAD_MAX * K_STACKSZ_IN_PAGE) &&
               page_count > 0);
        uintptr_t alloc_start = K_STACK_START & ~0xFFFUL;
        uintptr_t paddress;

        while (page_count--) {
                paddress = get_free_page(&kernel_pool);
                PANIC_IF(!paddress, "[mm]: not enough physical memory");
                put_page(alloc_start, paddress);
                alloc_start -= 0x1000UL;
        }
}


static void *get_free_vaddress(pool_type poolt, uint_32 pg_cnt)
{
        uint_32 v_start_addr = 0;
        uint_32 start_pos = -1;
        TCB_t *cur = running_thread();
        if (poolt == MP_KERNEL) {
                start_pos =
                    find_block_bitmap(&kernel_viraddr.vaddr_bitmap, pg_cnt);
                if (start_pos == -1) {
                        return NULL;
                }
                for (int i = 0; i < pg_cnt; i++) {
                        set_value_bitmap(&kernel_viraddr.vaddr_bitmap,
                                         start_pos + i, 1);
                }
                v_start_addr =
                    start_pos * PAGE_SIZE + kernel_viraddr.vaddr_start;
                return (void *) v_start_addr;
        } else if (poolt == MP_USER) {
                start_pos = find_block_bitmap(&cur->progress_vaddr.vaddr_bitmap,
                                              pg_cnt);
                if (start_pos == -1) {
                        return NULL;
                }
                for (int i = 0; i < pg_cnt; i++) {
                        set_value_bitmap(&cur->progress_vaddr.vaddr_bitmap,
                                         start_pos + i, 1);
                }
                v_start_addr =
                    start_pos * PAGE_SIZE + cur->progress_vaddr.vaddr_start;
                return (void *) v_start_addr;
        } else {
                PANIC(
                    "get_free_vaddress: not allow kernel alloc userspace or "
                    "user alloc "
                    "kernel space.");
                return NULL;
        }
}

// Get free vaddress and paddress and put them together
// 1. get free vaddress from vpool according to kernel or user
// 2. get free phy address from pool using get_free_page(pool)
// 3. put vaddress and paddress together using put_page(vaddr, paddr)
static void *malloc_page(enum mem_pool_type poolt, uint_32 pg_cnt)
{
        ASSERT(pg_cnt > 0);
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
                vaddr += PAGE_SIZE;
        }
        return vaddr_start;
}

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
                    (PAGE_SIZE - sizeof(struct arena)) / block_size;
                INIT_LIST_HEAD(&desc_array[desc_idx].free_list);
                block_size *= 2;
        }
}

// Covert paddress to position
static int_32 paddress2position(uintptr_t paddress, struct pool *pool)
{
        return (paddress - pool->phy_addr_start) / PAGE_SIZE;
}

static bool is_in_pool_range(uintptr_t paddress, struct pool *pool)
{
        return (pool->phy_addr_start >= paddress) ||
               (paddress < pool->pool_size * PAGE_SIZE + pool->phy_addr_start);
}

// mark a range of physical address used
static int __mark_physical_page_reserved(pool_type type, uintptr_t paddress)
{
        struct pool *pool;
        int_32 pos;
        if (type == MP_KERNEL) {
                pool = &kernel_pool;
        } else if (type == MP_USER) {
                pool = &user_pool;
        } else {
                PANIC("[mm]: Error memory pool type");
                return -1;
        }

        if (!is_in_pool_range(paddress, pool)) {
                PANIC("[mm]: paddress is not in available range");
                return -1;
        }

        pos = paddress2position(paddress, pool);
        set_value_bitmap(&pool->pool_bitmap, pos, PG_OCCUPIED);

        return 0;
}

static int mark_kpage_reserved(pool_type type, uintptr_t paddress)
{
        return __mark_physical_page_reserved(MP_KERNEL, paddress);
}


/* Kernel pool and user pool manage physical memory.
 *
 * */
static void mem_pool_init(uint_32 all_mem)
{
        // 1 page dir table and 255 page table
        //                      no.769 ~ no.1022 pde
        //                      no.768 and no.0 pg
        uint_32 page_table_size = PAGE_SIZE * (PDT_COUNT + PGT_COUNT);
        /*
         *  Page table start at 0x0010_0000
         **/
        // clang-format off
        /* uint_32 used_mem = page_table_size + 0x0010_0000;
         * I put page table at    0x0010_0000
         * MEM_BITMAP_BASE at 0xc0060000
         *                    0x0006_0000
         * +-----------------------+------------------+----------------+
         * |      address          |   name           |     size       |
         * +-----------------------+------------------+----------------+
         *       0x0000_c508       |   GDT            |   100bytes     |
         *       0x0000_c588       |   IDT            |   255 * 8bytes |
         *       0x0006_0000       | MEM_BITMAP_BASE  |
         *[0x0007_0000,0x0007cfcc] |   kernel code    |   20 pages     |
         *       0x000a_0000       |   vga            |   x pages      |
         *       0x000b_8000       |   text view      |   x pages      |
         *--------------------------------------------+----------------+
         *[0x0010_0000, PG_SZ *    |                  |
         * (PDT_COUNT+PGT_COUNT)]  |                  |
         *                         |   page table     |   3 pages
         *                         |                  |
         * ------------------------+------------------+-----------------
         * physical memory available usage
         *
         * */
        // clang-format on
        uint_32 used_mem = page_table_size + KPAGE_TABLE_START;
        /*
         *  all_mem for now is loading at loader.s use BIOS int.
         *  It must be calculate by loader.s before entering protected mode
         * */
        uint_32 free_mem = all_mem - used_mem;
        uint_16 all_free_pages = free_mem / PAGE_SIZE;

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
        uint_32 kp_start = used_mem;
        // User pool start
        uint_32 up_start = kp_start + kernel_free_page * PAGE_SIZE;

        kernel_pool.phy_addr_start = kp_start;

        user_pool.phy_addr_start = up_start;

        kernel_pool.pool_size = kernel_free_page * PAGE_SIZE;
        user_pool.pool_size = user_free_page * PAGE_SIZE;

        kernel_pool.pool_bitmap.map_bytes_length = kbm_length;
        user_pool.pool_bitmap.map_bytes_length = ubm_length;

        // kernel pool bit map fix at MEM_BITMAP_BASE 0x9a000
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

// Return a mem_block from a arena[idx]
static inline struct mem_block *arena2block(struct arena *a, uint_32 idx)
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

static void free_addr_bitmap(struct bitmap *map,
                             uint_32 addr_start,
                             uint_32 addr,
                             uint_32 pos,
                             uint_32 pg_cnt)
{
        if (addr < addr_start)
                PANIC("[mm]:free bad phy address");
        if (pos > map->map_bytes_length * 8)
                PANIC("[mm]:free bad address over length");
        for (int i = 0; i < pg_cnt; i++) {
                set_value_bitmap(map, pos + i, 0);
        }
}

// Remove assign vaddress at `pos`
static void free_vaddress(pool_type poolt, uint_32 vaddress, uint_32 pg_cnt)
{
        if (poolt == MP_KERNEL) {
                uint_32 offset = (vaddress - kernel_viraddr.vaddr_start);
                uint_32 pos = offset / PAGE_SIZE;
                free_addr_bitmap(&kernel_viraddr.vaddr_bitmap,
                                 kernel_viraddr.vaddr_start, vaddress, pos,
                                 pg_cnt);
        } else {
                TCB_t *cur = running_thread();
                uint_32 offset = (vaddress - cur->progress_vaddr.vaddr_start);
                uint_32 pos = offset / PAGE_SIZE;
                free_addr_bitmap(&cur->progress_vaddr.vaddr_bitmap,
                                 cur->progress_vaddr.vaddr_start, vaddress, pos,
                                 pg_cnt);
        }
}

// Release target address at mpool
static void free_page(struct pool *mpool, uint_32 phy_addr_page)
{
        if (phy_addr_page < mpool->phy_addr_start)
                PANIC("free bad phy address");
        int pos = (phy_addr_page - mpool->phy_addr_start) / PAGE_SIZE;
        if (pos > mpool->pool_bitmap.map_bytes_length * 8)
                PANIC("free bad address over length");
        set_value_bitmap(&mpool->pool_bitmap, pos, 0);
}

void free_phy_page(uint_32 phy_addr_page)
{
        if (phy_addr_page >= user_pool.phy_addr_start) {
                free_page(&user_pool, phy_addr_page);
        } else {
                free_page(&kernel_pool, phy_addr_page);
        }
}

uint_32 *pte_ptr(uint_32 vaddr)
{
        // I set last PDE as PDT table address
        // So the No.1023 pde to hex is 0x3ff
        // when vaddress is 0xffc00000
        // It will get pdt table address
        // top    10 bits 0xffc    <--- this is the last PDE point to page aka
        // PDT middle 10 bits (vaddr's top 10 bits which is original pde index)
        uint_32 target_pte =
            (0xffc00000 + ((vaddr & 0xffc00000) >> 10) + PTE_IDX(vaddr) * 4);
        return (uint_32 *) target_pte;
}

uint_32 *pde_ptr(uint_32 vaddr)
{
        // the last page table address of main process
        // The last page table address in PDE is
        // 0xfffffxxx
        // top    10 bits 0x3ff   <-- the last entry of PDT
        // middle 10 bits 0x3ff   <-- pointer to self (same PDT) again
        // bottom 12 pde_idx      <-- target entry
        uint_32 *target_pde = (uint_32 *) (0xfffff000 + PDE_IDX(vaddr) * 4);
        return target_pde;
}

// Only get page frame address.
static uint_32 virtual_addr_to_physical_addr(void *v_addr)
{
        uint_32 *pde = pde_ptr((uint_32) v_addr);
        uint_32 *pte = pte_ptr((uint_32) v_addr);
        if (*pde & 0x00000001) {            // pde is exist
                if ((*pte & 0x00000001)) {  // pte is exist
                        uint_32 p_addr = *pte & 0xfffff000;
                        return (uint_32) p_addr;
                } else {
                        PANIC("Some thing wrong.");
                        return 0;
                }
        }
        PANIC("Some thing wrong.");
        return 0;
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
                        /* PANIC("pte exists"); */
                        *pte = (phyaddress | PG_US_U | PG_RW_W | PG_P_SET);
                }
                invalidate();
        } else {
                // if there is no pde , let's create it.
                // Create phyaddr at kernel pool
                uint_32 pde_phyaddr = (uint_32) get_free_page(&kernel_pool);
                *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_SET);
                // Clear pte target address 1 page 4kb
                // top 10 ->
                memset((void *) ((int) pte & 0xfffff000), 0, PAGE_SIZE);
                ASSERT(!(*pte & 0x00000001));
                *pte = (phyaddress | PG_US_U | PG_RW_W | PG_P_SET);
        }
}

static void put_page_and_flush(void *v_addr, void *phy_addr, uint_32 *pgdir)
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
                        /* PANIC("pte exists"); */
                        *pte = (phyaddress | PG_US_U | PG_RW_W | PG_P_SET);
                }
                flush_cr3(pgdir);
        } else {
                // if there is no pde , let's create it.
                // Create phyaddr at kernel pool
                uint_32 pde_phyaddr = (uint_32) get_free_page(&kernel_pool);
                *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_SET);
                // Clear pte target address 1 page 4kb
                // top 10 ->
                memset((void *) ((int) pte & 0xfffff000), 0, PAGE_SIZE);
                ASSERT(!(*pte & 0x00000001));
                *pte = (phyaddress | PG_US_U | PG_RW_W | PG_P_SET);
        }
}

// Remove virtual address from page table
static void remove_page(void *v_addr)
{
        uint_32 vaddress = (uint_32) v_addr;
        /* uint_32 *pde = pde_ptr(vaddress); */
        uint_32 *pte = pte_ptr(vaddress);
        *pte &= ~PG_P_SET;
        __asm__ volatile("invlpg %0" ::"m"(v_addr) : "memory");
        /* if (*pde & 0x00000001) {      // test pde if exist */
        /*     if (*pte & 0x00000001) {  // test pde if exist or not */
        /*         *pte &= 0x11111110;   // PG_P_CLI; */
        /*     } else {                  // pte is not exists */
        /*         PANIC("Free twice"); */
        /*         // Still make pde is unexist */
        /*         *pte &= 0x11111110;  // PG_P_CLI; */
        /*     } */
        /*     __asm__ volatile("invlpg %0" ::"m"(v_addr) : "memory"); */
        /* } */
}

// get accurate physical from vaddress
uint_32 addr_v2p(uint_32 vaddr)
{
        uint_32 *pte = pte_ptr(vaddr);
        return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
}

// for create process
// get free vaddress and paddress and put them together
void *malloc_page_with_vaddr(enum mem_pool_type poolt, uint_32 vaddr_start)
{
        struct pool *mem_pool = poolt & MP_KERNEL ? &kernel_pool : &user_pool;
        int_32 bit_idx = -1;
        TCB_t *cur = running_thread();
        uint_32 offset;
        if (cur->pgdir == NULL && poolt == MP_KERNEL) {
                offset = (vaddr_start - kernel_viraddr.vaddr_start);
                bit_idx = offset / PAGE_SIZE;
                ASSERT(bit_idx >= 0);
                set_value_bitmap(&kernel_viraddr.vaddr_bitmap, bit_idx, 1);
        } else if (cur->pgdir != NULL && poolt == MP_USER) {
                offset = (vaddr_start - cur->progress_vaddr.vaddr_start);
                bit_idx = offset / PAGE_SIZE;
                ASSERT(bit_idx >= 0);
                set_value_bitmap(&cur->progress_vaddr.vaddr_bitmap, bit_idx, 1);
        } else {
                PANIC(
                    "get_free_vaddress: "
                    "not allow kernel alloc userspace or "
                    "user alloc "
                    "kernel space.");
        }
        void *phyaddrs = get_free_page(mem_pool);
        if (phyaddrs == NULL) {
                return NULL;
        }
        put_page((void *) vaddr_start, phyaddrs);
        return (void *) vaddr_start;
}


// invoke by fork() fork.c
void *get_phy_free_page_with_vaddr(enum mem_pool_type poolt,
                                   uint_32 vaddr,
                                   uint_32 *child_pgdir)
{
        struct pool *mem_pool = poolt == MP_KERNEL ? &kernel_pool : &user_pool;
        lock_fetch(&mem_pool->lock);
        void *page_phyaddr = get_free_page(mem_pool);
        if (page_phyaddr == NULL) {
                lock_fetch(&mem_pool->lock);
                return NULL;
        }
        put_page_and_flush((void *) vaddr, page_phyaddr, child_pgdir);
        lock_release(&mem_pool->lock);
        return (void *) vaddr;
}


// Get kernel page from memory
void *get_kernel_page(uint_32 pg_cnt)
{
        lock_fetch(&kernel_pool.lock);
        void *vaddr = malloc_page(MP_KERNEL, pg_cnt);
        if (vaddr != NULL) {
                memset(vaddr, 0, pg_cnt * PAGE_SIZE);
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
                memset(vaddr, 0, pg_cnt * PAGE_SIZE);
        }
        lock_release(&user_pool.lock);
        return vaddr;
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

        uint_32 phy_addr;
        for (int i = 0; i < pg_cnt; i++) {
                phy_addr = virtual_addr_to_physical_addr((void *) vaddr);
                free_page(mem_pool, phy_addr);
                remove_page((void *) vaddr);
                vaddr += PAGE_SIZE;
        }
}

static void *malloc_internal(uint_32 size, pool_type pool_t)
{
        struct pool *mem_pool;
        uint_32 pool_size;
        struct mem_block_desc *descs;
        // Kernel
        if (pool_t == MP_KERNEL) {
                mem_pool = &kernel_pool;
                pool_size = kernel_pool.pool_size;
                descs = k_block_descs;
        } else if (pool_t == MP_USER) {
                // User
                TCB_t *cur = running_thread();
                mem_pool = &user_pool;
                pool_size = user_pool.pool_size;
                descs = cur->u_block_descs;
        } else {
                PANIC("[WORNG:mm] pool type at malloc");
        }

        if (!(size > 0 && size < pool_size)) {
                return NULL;
        }

        struct arena *area;
        struct mem_block *block;
        lock_fetch(&mem_pool->lock);
        // If be allocated size is over 1024B return a whole arena
        if (size > 1024) {
                uint_32 page_cnt = CEIL(size + sizeof(struct arena), PAGE_SIZE);
                area = malloc_page(pool_t, page_cnt);

                if (area != NULL) {
                        memset(area, 0, PAGE_SIZE * page_cnt);
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
        } else {  // require memory equal and less than 1024B
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
                        memset(area, 0, PAGE_SIZE);

                        area->desc = &descs[desc_idx];
                        area->large = false;
                        area->cnt = descs[desc_idx].blocks_per_arena;
                        uint_32 block_idx;
                        unsigned long flags;
                        local_irq_save(flags);
                        for (block_idx = 0;
                             block_idx < descs[desc_idx].blocks_per_arena;
                             block_idx++) {
                                block = arena2block(area, block_idx);
                                list_add_tail(&block->free_elem,
                                              &area->desc->free_list);
                        }
                        local_irq_restore(flags);
                } else {
                        // do nothing
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
        return 0;
}

void *kmalloc(uint_32 size)
{
        return malloc_internal(size, MP_KERNEL);
}

void *umalloc(uint_32 size)
{
        return malloc_internal(size, MP_USER);
}

static void free_internal(void *ptr, pool_type p_type)
{
        struct pool *mem_pool;
        if (ptr != NULL) {
                if (p_type == MP_KERNEL) {
                        ASSERT((uint_32) ptr >= K_HEAP_START);
                        mem_pool = &kernel_pool;
                } else if (p_type == MP_USER) {  // is process
                        mem_pool = &user_pool;
                } else {
                        PANIC("[WORNG:mm]: at free pool type");
                }
                lock_fetch(&mem_pool->lock);
                // Get target pointer arena get metadate
                struct mem_block *block = ptr;
                struct arena *a = block2arena(block);
                ASSERT(a->large == 0 || a->large == 1);
                if (a->desc == NULL &&
                    a->large == true) {  // arena is equal or over 1024B
                        mfree_page(p_type, a, a->cnt);
                } else {
                        /* If less than 1024B, first free memory to
                         * desc->free_list
                         * */
                        memset(block, 0, sizeof(struct list_head));
                        INIT_LIST_HEAD(&block->free_elem);

                        list_add_tail(&block->free_elem, &a->desc->free_list);
                        // Test all arena free_list are free, if true release
                        // arena
                        if (++a->cnt == a->desc->blocks_per_arena) {
                                uint_32 block_idx;
                                for (block_idx = 0;
                                     block_idx < a->desc->blocks_per_arena;
                                     block_idx++) {
                                        struct mem_block *b =
                                            arena2block(a, block_idx);
                                        list_del_init(&b->free_elem);
                                }
                                mfree_page(p_type, a, 1);
                        }
                }
                lock_release(&mem_pool->lock);
        }
}

void kfree(void *ptr)
{
        if (ptr == NULL)
                return;
        ASSERT(ptr != NULL);
        free_internal(ptr, MP_KERNEL);
}

void ufree(void *ptr)
{
        if (ptr == NULL)
                return;
        ASSERT(ptr != NULL);
        free_internal(ptr, MP_USER);
}
// Alloc memory
// alloc 'size' memory from memory
// First we need know who want to alloc memory from mm, so test current process
// if is kernel use kernel pool of memory if not use user pool of memory.
void *sys_malloc(uint_32 size)
{
        TCB_t *cur = running_thread();
        // Kernel
        if (cur->pgdir == NULL) {
                return malloc_internal(size, MP_KERNEL);
        } else {
                // User
                return malloc_internal(size, MP_USER);
        }
}

void sys_free(void *ptr)
{
        ASSERT(ptr != NULL);
        if (ptr != NULL) {
                TCB_t *cur = running_thread();
                // Is thread
                if (cur->pgdir == NULL) {
                        ASSERT((uint_32) ptr >= K_HEAP_START);
                        free_internal(ptr, MP_KERNEL);
                } else {  // is process
                        free_internal(ptr, MP_USER);
                }
        } else {
                DEBUG("[mm]: free a NULL pointer");
                return;
        }
}

void mem_init()
{
        struct memory_map_descriptor *mmap_desc =
            *((struct memory_map_descriptor **) MMAP_INFO_POINTER);
        uint_32 *mmap_desc_cnt = *((uint_32 **) MMAP_INFO_COUNT_POINTER);
        uint_32 mem_bytes_total = 0;  // 32M //(*(uint_32 *) (0xb00));
        // Memory map from BISO success
        if (mmap_desc && mmap_desc_cnt) {
                printk("\n");
                printk(
                    "baselow    basehight    lenghtlow    lenhight    type    "
                    "\n");
                for (int i = 0; i < *mmap_desc_cnt; i++) {
                        struct memory_map_descriptor mmp_desc = mmap_desc[i];
                        uint_32 base_low = mmp_desc.base_addr_low;
                        uint_32 base_high = mmp_desc.base_addr_high;
                        uint_32 length_low = mmp_desc.length_low;
                        uint_32 length_hight = mmp_desc.length_high;
                        ARDS_t type = mmp_desc.type;
                        printk(
                            "%x         %x           %x           %x          "
                            "%x\n",
                            base_low, base_high, length_low, length_hight,
                            type);
                        if (type == ARDS_address_range_memory) {
                                mem_bytes_total += mmp_desc.length_low;
                        }
                }
        }

        mem_pool_init(mem_bytes_total);
        block_desc_init(k_block_descs);

        alloc_kstack_pool(K_THREAD_MAX * K_STACKSZ_IN_PAGE);

        // init local_cpu
        struct cpu_local *cpu = NULL;
        each_cpu(cpu)
        {
                char *vaddr = malloc_page(MP_KERNEL, INTR_STACKSZ_PAGE);
                cpu->irq_stack_top = vaddr + (PAGE_SIZE << 1);
                ASSERT(cpu->irq_stack_top != NULL);
        }
}
