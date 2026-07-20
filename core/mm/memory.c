#include <asm/page.h>
#include <frog/bootmem.h>
#include <frog/irqflags.h>
#include <frog/memory.h>
#include <frog/phys_resource.h>
#include <frog/semaphore.h>
#include <frog/string.h>
#include <frog/threads.h>
#include <frog/vm.h>

#include <frog/math.h>  // for DIV_ROUND_UP
#include <kernel/assert.h>
#include <kernel/debug.h>
#include <kernel/panic.h>

// for kernel test
#include <frog/printk.h>

// init per local cpu interrupt stack
#include <kernel/cpu.h>

#include <frog/shadowmem.h>

#include "./mem_egg.h"    // structure of small memory
#include "./mm_helper.h"  // helper on PTE/PDE etc

#define MEM_BITMAP_BASE 0xc0060000UL
#define MEM_BITMAP_END  0xc0070000UL

// 1 page dir table
#define PDT_COUNT 1UL
// no.254 is upper 1G memory start at 0xc000_0000
//
// [PDE no.768       map-> pg0 address ] represents size 4MB
// 0xc000_0000
//
// [PDE no.769       ~ no.1023 pde -> pg1 2nd page address] represent size 1GB
// 0xc040_0000       ~ 0xFFC0_0000 : virtual address range

#define MAX_KPT_COUNT 255  // represent real 1GB  memory

#define PG0_COUNT 1
// In real world Frog don't need all Upper vaddress I will give it 4MB
#define PGT_COUNT (MAX_KPT_COUNT / 255 + PG0_COUNT)

#define MEM_POOL_START \
        (KPAGE_TABLE_START + PAGE_SIZE * (PDT_COUNT + PGT_COUNT))

#define PG_OCCUPIED 1
#define PG_VACANT 0

/*
 * kernel block descriptions.
 * */
struct mem_block_desc k_block_descs[DESC_CNT];

struct pool kernel_pool;
struct pool user_pool;
struct _virtual_addr kernel_viraddr;


static int mark_kernel_vaddr_reserved(uintptr_t vaddr);

static void *get_physical_page(struct pool *mpool);
static void free_physical_page(struct pool *mpool, uint_32 phy_addr_page);

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
                        PANIC("[mm]: pte not existed");
                        return 0;
                }
        }
        PANIC("[mm]: pde not existed");
        return 0;
}
// get accurate physical from vaddress
uint_32 addr_v2p(uint_32 vaddr)
{
        uint_32 *pte = pte_ptr(vaddr);
        return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
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
        uint_32 pagedir_phy_addr;

        /* put_page() always edits the active recursive page table. */
        __asm__ volatile("movl %%cr3, %0" : "=r"(pagedir_phy_addr));
        __asm__ volatile("movl %0, %%cr3"
                         :
                         : "r"(pagedir_phy_addr)
                         : "memory");
}

static uint_32 page_entry_flags(uint_32 vaddress)
{
        uint_32 flags = PG_RW_W | PG_P_SET;

        if (vaddress < KERNEL_BASE)
                flags |= PG_US_U;
        return flags;
}

// Combine v address -> phy address
void put_page(void *v_addr, void *phy_addr)
{
        uint_32 vaddress = (uint_32) v_addr;
        uint_32 phyaddress = (uint_32) phy_addr;
        uint_32 entry_flags = page_entry_flags(vaddress);
        uint_32 *pde = pde_ptr(vaddress);
        uint_32 *pte = pte_ptr(vaddress);

        // test P bit of vaddress
        if (*pde & 0x00000001) {
                // TODO:
                // test if pte is exist
                // should re-consider v-address start
                /* ASSERT(!(*pte & 0x00000001)); */
                if ((!(*pte & 0x00000001))) {
                        *pte = phyaddress | entry_flags;
                } else {
                        /* PANIC("pte exists"); */
                        *pte = phyaddress | entry_flags;
                }
                invalidate();
        } else {
                // if there is no pde , let's create it.
                // Create phyaddr at kernel pool
                uint_32 pde_phyaddr = (uint_32) get_physical_page(&kernel_pool);
                *pde = pde_phyaddr | entry_flags;
                // Clear pte target address 1 page 4kb
                // top 10 ->
                memset((void *) ((int) pte & 0xfffff000), 0, PAGE_SIZE);
                ASSERT(!(*pte & 0x00000001));
                *pte = phyaddress | entry_flags;
        }
        if (vaddress < KERNEL_BASE) {
                TCB_t *current = running_thread();
                if (current != NULL && current->mm != NULL)
                        current->mm->generation++;
        }
}

#define FRAMEBUFFER_MAX_PDE_COUNT 4

static const struct phys_resource *kernel_framebuffer_resource;

int map_kernel_framebuffer_pinned(const struct phys_resource *resource,
                                  uint_32 size)
{
        const uintptr_t vaddr = KERNEL_FRAMEBUFFER_VADDR;
        void *new_page_tables[FRAMEBUFFER_MAX_PDE_COUNT];
        uint_32 new_pde_indexes[FRAMEBUFFER_MAX_PDE_COUNT];
        uint_32 new_pde_count = 0;
        uintptr_t paddr;

        if (resource == NULL ||
            resource->state != PHYS_RESOURCE_REGISTERED ||
            resource->type != PHYS_RESOURCE_MMIO ||
            resource->cache_mode != VM_CACHE_UNCACHED ||
            refcount_read(&resource->refs) < 2 ||
            resource->start > 0xffffffffULL || size == 0 ||
            resource->start > 0x100000000ULL - size ||
            (resource->start & (PAGE_SIZE - 1U)) != 0 ||
            (size & (PAGE_SIZE - 1U)) != 0 ||
            size > 16U * 1024U * 1024U ||
            !phys_resource_contains(resource, resource->start, size))
                return -1;
        paddr = (uintptr_t) resource->start;

        uint_32 page_count = size / PAGE_SIZE;
        lock_fetch(&kernel_pool.lock);
        if (kernel_framebuffer_resource != NULL) {
                lock_release(&kernel_pool.lock);
                return -1;
        }

        /* Reject the whole request before changing any existing page table. */
        for (uint_32 page = 0; page < page_count; page++) {
                uint_32 current_vaddr = vaddr + page * PAGE_SIZE;
                uint_32 *pde = pde_ptr(current_vaddr);
                uint_32 *pte = pte_ptr(current_vaddr);

                if ((*pde & PG_P_SET) && (*pde & PG_US_U)) {
                        lock_release(&kernel_pool.lock);
                        return -1;
                }
                if ((*pde & PG_P_SET) && (*pte & PG_P_SET)) {
                        lock_release(&kernel_pool.lock);
                        return -1;
                }
        }

        for (uint_32 page = 0; page < page_count; page += 1024) {
                uint_32 current_vaddr = vaddr + page * PAGE_SIZE;
                uint_32 *pde = pde_ptr(current_vaddr);
                if (*pde & PG_P_SET)
                        continue;
                void *page_table = get_physical_page(&kernel_pool);
                if (page_table == NULL) {
                        while (new_pde_count > 0)
                                free_physical_page(
                                    &kernel_pool,
                                    (uint_32) new_page_tables[--new_pde_count]);
                        lock_release(&kernel_pool.lock);
                        return -1;
                }
                new_page_tables[new_pde_count] = page_table;
                new_pde_indexes[new_pde_count++] = current_vaddr >> 22;
        }

        for (uint_32 index = 0; index < new_pde_count; index++) {
                uint_32 pde_vaddr = new_pde_indexes[index] << 22;
                *pde_ptr(pde_vaddr) =
                    (uint_32) new_page_tables[index] | PG_RW_W | PG_P_SET;
                invalidate();
                memset((void *) ((uint_32) pte_ptr(pde_vaddr) & 0xfffff000),
                       0, PAGE_SIZE);
        }

        for (uint_32 page = 0; page < page_count; page++) {
                uint_32 current_vaddr = vaddr + page * PAGE_SIZE;
                uint_32 current_paddr = paddr + page * PAGE_SIZE;
                uint_32 *pte = pte_ptr(current_vaddr);
                uint_32 expected = current_paddr | PG_RW_W | PG_PWT |
                                   PG_PCD | PG_P_SET;

                *pte = expected;
                __asm__ volatile("invlpg (%0)" : : "r"(current_vaddr) : "memory");
                ASSERT((*pte & (0xfffff000U | PG_US_U | PG_RW_W | PG_PWT |
                                PG_PCD | PG_P_SET)) == expected);
        }
        kernel_framebuffer_resource = resource;
        lock_release(&kernel_pool.lock);
        return 0;
}

static int put_page_and_flush(void *v_addr,
                              void *phy_addr,
                              struct mm_struct *mm)
{
        uint_32 vaddress = (uint_32) v_addr;
        uint_32 phyaddress = (uint_32) phy_addr;
        uint_32 entry_flags = page_entry_flags(vaddress);
        uint_32 *pde = pde_ptr(vaddress);
        uint_32 *pte = pte_ptr(vaddress);

        // test P bit of vaddress
        if (*pde & 0x00000001) {
                // TODO:
                // test if pte is exist
                // should re-consider v-address start
                /* ASSERT(!(*pte & 0x00000001)); */
                if ((!(*pte & 0x00000001))) {
                        *pte = phyaddress | entry_flags;
                } else {
                        /* PANIC("pte exists"); */
                        *pte = phyaddress | entry_flags;
                }
                flush_cr3(mm->pgdir);
        } else {
                // if there is no pde , let's create it.
                // Create phyaddr at kernel pool
                uint_32 pde_phyaddr = (uint_32) get_physical_page(&kernel_pool);
                if (pde_phyaddr == 0)
                        return -1;
                *pde = pde_phyaddr | entry_flags;
                // Clear pte target address 1 page 4kb
                // top 10 ->
                memset((void *) ((int) pte & 0xfffff000), 0, PAGE_SIZE);
                ASSERT(!(*pte & 0x00000001));
                *pte = phyaddress | entry_flags;
        }
        mm->generation++;
        return 0;
}



// Get a free 4k phycial memory aka 1 page in pool
static void *get_physical_page(struct pool *mpool)
{
        int_32 start_pos = -1;
        start_pos = find_block_bitmap(&mpool->pool_bitmap, 1);
        if (start_pos == -1) {
                return NULL;
        }
        set_value_bitmap(&mpool->pool_bitmap, start_pos, 1);
        return (void *) (start_pos * PAGE_SIZE + mpool->phy_addr_start);
}

// Release target address at mpool
static void free_physical_page(struct pool *mpool, uint_32 phy_addr_page)
{
        if (phy_addr_page < mpool->phy_addr_start ||
            (phy_addr_page & (PAGE_SIZE - 1U)) != 0)
                PANIC("free bad phy address");
        uint_32 pos = (phy_addr_page - mpool->phy_addr_start) / PAGE_SIZE;
        if (pos >= mpool->pool_bitmap.map_bytes_length * 8U)
                PANIC("free bad address over length");
        if (!get_value_bitmap(&mpool->pool_bitmap, pos))
                PANIC("free physical page twice");
        set_value_bitmap(&mpool->pool_bitmap, pos, PG_VACANT);
}

uint_32 alloc_kernel_page_frame(void)
{
        uint_32 physical;

        lock_fetch(&kernel_pool.lock);
        physical = (uint_32) get_physical_page(&kernel_pool);
        lock_release(&kernel_pool.lock);
        return physical;
}

uint_32 alloc_user_page_frame(void)
{
        uint_32 physical;

        lock_fetch(&user_pool.lock);
        physical = (uint_32) get_physical_page(&user_pool);
        lock_release(&user_pool.lock);
        return physical;
}

void free_kernel_page_frame(uint_32 physical)
{
        ASSERT(physical != 0 && (physical & (PAGE_SIZE - 1U)) == 0);
        lock_fetch(&kernel_pool.lock);
        free_physical_page(&kernel_pool, physical);
        lock_release(&kernel_pool.lock);
}

void free_user_page_frame(uint_32 physical)
{
        ASSERT(physical != 0 && (physical & (PAGE_SIZE - 1U)) == 0);
        lock_fetch(&user_pool.lock);
        free_physical_page(&user_pool, physical);
        lock_release(&user_pool.lock);
}

void free_phy_page(uint_32 phy_addr_page)
{
        if (phy_addr_page >= user_pool.phy_addr_start) {
                free_physical_page(&user_pool, phy_addr_page);
        } else {
                free_physical_page(&kernel_pool, phy_addr_page);
        }
}


/**
 * Return a continued virtual address space in page size
 *
 * @param poolt pool type
 * @param pg_cnt number of page count
 * @return start address
 *****************************************************************************/
static void *get_virtual_pages(pool_type poolt, uint_32 pg_cnt)
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
                                         start_pos + i, PG_OCCUPIED);
                }
                v_start_addr =
                    start_pos * PAGE_SIZE + kernel_viraddr.vaddr_start;
                return (void *) v_start_addr;
        } else if (poolt == MP_USER) {
                ASSERT(cur->mm != NULL);
                start_pos = find_block_bitmap(&cur->mm->user_vaddr.vaddr_bitmap,
                                              pg_cnt);
                if (start_pos == -1) {
                        return NULL;
                }
                for (int i = 0; i < pg_cnt; i++) {
                        set_value_bitmap(&cur->mm->user_vaddr.vaddr_bitmap,
                                         start_pos + i, PG_OCCUPIED);
                }
                v_start_addr =
                    start_pos * PAGE_SIZE + cur->mm->user_vaddr.vaddr_start;
                cur->mm->generation++;
                return (void *) v_start_addr;
        } else {
                PANIC("[mm]: Wrong memory pool type.");
                return NULL;
        }
}

static void __free_addr_bitmap(struct bitmap *map,
                               uint_32 addr_start,
                               uint_32 addr,
                               uint_32 pos,
                               uint_32 pg_cnt)
{
        if (addr < addr_start)
                PANIC("[mm]:free bad phy address");
        uint_32 capacity = map->map_bytes_length * 8U;
        if (pos >= capacity || pg_cnt > capacity - pos)
                PANIC("[mm]:free bad address over length");
        for (int i = 0; i < pg_cnt; i++) {
                set_value_bitmap(map, pos + i, PG_VACANT);
        }
}

/**
 * Free a continued virtual address spaces/ which is genarated by
 * get_virtual_pages()
 *****************************************************************************/
static void free_virtual_pages(pool_type poolt,
                               uint_32 vaddress,
                               uint_32 pg_cnt)
{
        if (poolt == MP_KERNEL) {
                uint_32 offset = (vaddress - kernel_viraddr.vaddr_start);
                uint_32 pos = offset / PAGE_SIZE;
                __free_addr_bitmap(&kernel_viraddr.vaddr_bitmap,
                                   kernel_viraddr.vaddr_start, vaddress, pos,
                                   pg_cnt);
        } else {
                TCB_t *cur = running_thread();
                ASSERT(cur->mm != NULL);
                uint_32 offset =
                    (vaddress - cur->mm->user_vaddr.vaddr_start);
                uint_32 pos = offset / PAGE_SIZE;
                __free_addr_bitmap(&cur->mm->user_vaddr.vaddr_bitmap,
                                   cur->mm->user_vaddr.vaddr_start, vaddress,
                                   pos, pg_cnt);
                cur->mm->generation++;
        }
}

// Get free vaddress and paddress and put them together
// 1. get free vaddress from vpool according to kernel or user
// 2. get free phy address from pool using get_physical_page(pool)
// 3. put vaddress and paddress together using put_page(vaddr, paddr)
/**
 * Allocating a continued virtual address spaces with (non-continued)physical
 *address.
 *
 * @param param write here param Comments write here
 * @return return Comments write here
 *****************************************************************************/
static void *malloc_page(enum mem_pool_type poolt, uint_32 pg_cnt)
{
        ASSERT(pg_cnt > 0);
        void *vaddr_start = get_virtual_pages(poolt, pg_cnt);
        if (vaddr_start == NULL) {
                return NULL;
        }
        uint_32 vaddr = (uint_32) vaddr_start;
        struct pool *mem_pool = poolt & MP_KERNEL ? &kernel_pool : &user_pool;

        while (pg_cnt--) {
                void *phyaddrs = get_physical_page(mem_pool);
                if (phyaddrs == NULL) {
                        return NULL;
                }
                put_page((void *) vaddr, phyaddrs);
                vaddr += PAGE_SIZE;
        }
        return vaddr_start;
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
void free_page(enum mem_pool_type poolt, void *_vaddr, uint_32 pg_cnt)
{
        uint_32 vaddr = (uint_32) _vaddr;
        struct pool *mem_pool = poolt & MP_KERNEL ? &kernel_pool : &user_pool;
        free_virtual_pages(poolt, vaddr, pg_cnt);

        uint_32 phy_addr;
        for (int i = 0; i < pg_cnt; i++) {
                phy_addr = virtual_addr_to_physical_addr((void *) vaddr);
                free_physical_page(mem_pool, phy_addr);
                remove_page((void *) vaddr);
                if (poolt == MP_USER)
                        running_thread()->mm->generation++;
                vaddr += PAGE_SIZE;
        }
}

static void __lazy_alloc(uintptr_t addr, pool_type type)
{
        struct pool *pool;
        if (type == MP_KERNEL) {
                pool = &kernel_pool;
        } else if (type == MP_USER) {
                pool = &user_pool;
        } else {
                PANIC("[mm]: Wrong physical memory pool type.");
        }
        uintptr_t paddr = get_physical_page(pool);
        if (!paddr) {
                PANIC("[mm]: not enough physical memory");
                return;
        }
        put_page(addr, paddr);
}

void mm_lazy_alloc(uintptr_t addr)
{
        if (is_kernel_space(addr)) {
                __lazy_alloc(addr, MP_KERNEL);
        } else {
                __lazy_alloc(addr, MP_USER);
        }
}

// Covert paddress to position
static int_32 paddress2position(uintptr_t address, struct pool *pool)
{
        return (address - pool->phy_addr_start) / PAGE_SIZE;
}

// Covert vaddress to bitmap position
static int vaddr2pos(uintptr_t vaddr, struct _virtual_addr *vpool)
{
        uintptr_t offset = (vaddr - vpool->vaddr_start);
        uint_32 bit_idx = offset / PAGE_SIZE;
        ASSERT(bit_idx >= 0);
        return bit_idx;
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

static int mark_kernel_vaddr_reserved(uintptr_t vaddr)
{
        uint_32 bit_idx = vaddr2pos(vaddr, &kernel_viraddr);
        set_value_bitmap(&kernel_viraddr.vaddr_bitmap, bit_idx, PG_OCCUPIED);
        return 0;
}

static void reserve_bootstrap_stack_vaddr(void)
{
        uintptr_t stack_page = (uintptr_t) running_thread();
        uintptr_t capacity =
            kernel_viraddr.vaddr_bitmap.map_bytes_length * 8UL * PAGE_SIZE;

        if (stack_page < kernel_viraddr.vaddr_start ||
            stack_page - kernel_viraddr.vaddr_start >= capacity)
                return;

        /* The boot stack remains live until make_main_thread() relocates it. */
        mark_kernel_vaddr_reserved(stack_page);
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


// for create process
// get free vaddress and paddress and put them together
void *malloc_page_with_vaddr(enum mem_pool_type poolt, uint_32 vaddr_start)
{
        struct pool *mem_pool = poolt & MP_KERNEL ? &kernel_pool : &user_pool;
        int_32 bit_idx = -1;
        TCB_t *cur = running_thread();
        uint_32 offset;
        if (cur->mm == NULL && poolt == MP_KERNEL) {
                offset = (vaddr_start - kernel_viraddr.vaddr_start);
                bit_idx = offset / PAGE_SIZE;
                ASSERT(bit_idx >= 0);
                set_value_bitmap(&kernel_viraddr.vaddr_bitmap, bit_idx, 1);
        } else if (cur->mm != NULL && poolt == MP_USER) {
                offset = (vaddr_start - cur->mm->user_vaddr.vaddr_start);
                bit_idx = offset / PAGE_SIZE;
                ASSERT(bit_idx >= 0);
                set_value_bitmap(&cur->mm->user_vaddr.vaddr_bitmap, bit_idx, 1);
                cur->mm->generation++;
        } else {
                PANIC("[mm]: worng pool type.");
        }
        void *phyaddrs = get_physical_page(mem_pool);
        if (phyaddrs == NULL) {
                return NULL;
        }
        put_page((void *) vaddr_start, phyaddrs);
        return (void *) vaddr_start;
}


// invoke by fork() fork.c
void *get_phy_free_page_with_vaddr(enum mem_pool_type poolt,
                                   uint_32 vaddr,
                                   struct mm_struct *mm)
{
        struct pool *mem_pool = poolt == MP_KERNEL ? &kernel_pool : &user_pool;
        if (mm == NULL || mm->pgdir == NULL)
                return NULL;
        lock_fetch(&mem_pool->lock);
        void *page_phyaddr = get_physical_page(mem_pool);
        if (page_phyaddr == NULL) {
                lock_release(&mem_pool->lock);
                return NULL;
        }
        if (put_page_and_flush((void *) vaddr, page_phyaddr, mm) < 0) {
                free_physical_page(mem_pool, (uint_32) page_phyaddr);
                lock_release(&mem_pool->lock);
                return NULL;
        }
        lock_release(&mem_pool->lock);
        return (void *) vaddr;
}

struct mem_block *set_posion_memory(struct arena *area,
                                    struct mem_block_desc *descs,
                                    uint_32 desc_idx)
{
        struct mem_block *block;
        uint_32 block_idx;
        unsigned long flags;
        local_irq_save(flags);
        for (block_idx = 0; block_idx < descs[desc_idx].blocks_per_arena;
             block_idx++) {
#ifdef CONFIG_POSION_MEMORY
                block = kasan_posion_arena2block(area, block_idx,
                                                 KASAN_SAFE_REDZONE_SIZE);
                kasan_posion((uintptr_t) block, descs[desc_idx].block_size,
                             KASAN_SAFE_REDZONE_SIZE,
                             (char) KASAN_KMALLOC_REDZONE);
#else
                block = arena2block(area, block_idx);
#endif
                list_add_tail(&block->free_elem, &area->desc->free_list);
        }
        local_irq_restore(flags);
        return block;
}


static void *malloc_internal(uint_32 size, pool_type pool_t)
{
        struct pool *mem_pool;
        uint_32 pool_size;
        struct mem_block_desc *descs;
        if (pool_t == MP_KERNEL) {
                mem_pool = &kernel_pool;
                pool_size = kernel_pool.pool_size;
                descs = k_block_descs;
        } else if (pool_t == MP_USER) {
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

                        /* block = set_posion_memory(area, descs, 6); */

                        return (void *)(area + 1);  // struct arena* + 1 = sizeof(arena) bytes
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
#ifdef CONFIG_POSION_MEMORY
                                block = kasan_posion_arena2block(
                                    area, block_idx, KASAN_SAFE_REDZONE_SIZE);
                                kasan_posion((uintptr_t) block,
                                             descs[desc_idx].block_size,
                                             KASAN_SAFE_REDZONE_SIZE,
                                             (char) KASAN_KMALLOC_REDZONE);
#else
                                block = arena2block(area, block_idx);
#endif
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

                if (a->desc) {
#ifdef CONFIG_POSION_MEMORY
                        kasan_protect_free(a->desc->block_size,
                                           (uintptr_t) block);
#endif
                }
                ASSERT(a->large == 0 || a->large == 1);
                if (a->desc == NULL &&
                    a->large == true) {  // arena is equal or over 1024B
                        free_page(p_type, a, a->cnt);
                } else {
                        /* If less than 1024B, first free memory to
                         * desc->free_list
                         **/
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
                                        struct mem_block *b;
#ifdef CONFIG_POSION_MEMORY
                                        b = kasan_posion_arena2block(
                                            a, block_idx,
                                            a->desc->redzone_size);
#else
                                        b = arena2block(a, block_idx);
#endif
                                        list_del_init(&b->free_elem);
                                }
                                free_page(p_type, a, 1);
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
        if (cur->mm == NULL) {
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
                if (cur->mm == NULL) {
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

/* Kernel pool and user pool manage physical memory.
 *
 * */
static bool page_count_to_bytes(uint_32 page_count, uint_32 *bytes_out)
{
        if (bytes_out == NULL || page_count > 0xffffffffU / PAGE_SIZE)
                return false;
        *bytes_out = page_count * PAGE_SIZE;
        return true;
}

uint_32 mem_pool_fit_page_count(uint_32 page_count,
                                uint_32 bitmap_window_bytes)
{
        uint_32 capacity_blocks = bitmap_window_bytes / 3U;
        uint_32 capacity_pages;

        page_count &= ~15U;
        if (capacity_blocks > 0xfffffff0U / 16U)
                capacity_pages = 0xfffffff0U;
        else
                capacity_pages = capacity_blocks * 16U;
        if (capacity_pages < page_count)
                page_count = capacity_pages;
        return page_count;
}

static void mem_pool_init(uint_32 alloc_end)
{
        // 1 page dir table and 255 page table
        //                      no.769 ~ no.1022 pde
        //                      no.768 and no.0 pg
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
        uint_32 used_mem = MEM_POOL_START;
        if (alloc_end <= used_mem)
                PANIC("[mm]: E820 has no contiguous allocator range");
        uint_32 free_mem = alloc_end - used_mem;
        uint_32 all_free_pages = mem_pool_fit_page_count(
            free_mem / PAGE_SIZE, MEM_BITMAP_END - MEM_BITMAP_BASE);

        /* Two physical-pool bitmaps plus the kernel virtual bitmap fit here. */
        if (all_free_pages == 0)
                PANIC("[mm]: E820 allocator range is too small");

        /* kernel used memory vs user used memory
         * */
        uint_32 kernel_free_page = all_free_pages / 2;
        uint_32 user_free_page = all_free_pages - kernel_free_page;
        uint_32 kernel_pool_bytes;
        uint_32 user_pool_bytes;

        if (!page_count_to_bytes(kernel_free_page, &kernel_pool_bytes) ||
            !page_count_to_bytes(user_free_page, &user_pool_bytes))
                PANIC("[mm]: physical pool size overflows");


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
        if (kernel_pool_bytes > 0xffffffffU - kp_start)
                PANIC("[mm]: user pool address overflows");
        uint_32 up_start = kp_start + kernel_pool_bytes;
        if (up_start > alloc_end || user_pool_bytes > alloc_end - up_start)
                PANIC("[mm]: physical pools exceed E820 allocator range");

        kernel_pool.phy_addr_start = kp_start;

        user_pool.phy_addr_start = up_start;

        kernel_pool.pool_size = kernel_pool_bytes;
        user_pool.pool_size = user_pool_bytes;

        kernel_pool.pool_bitmap.map_bytes_length = kbm_length;
        user_pool.pool_bitmap.map_bytes_length = ubm_length;

        if (kbm_length > MEM_BITMAP_END - MEM_BITMAP_BASE ||
            ubm_length > MEM_BITMAP_END - MEM_BITMAP_BASE - kbm_length ||
            kbm_length >
                MEM_BITMAP_END - MEM_BITMAP_BASE - kbm_length - ubm_length)
                PANIC("[mm]: memory bitmaps exceed reserved window");

        // kernel pool bit map fix at MEM_BITMAP_BASE 0xc0060000
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
#ifdef CONFIG_POSION_MEMORY
                uint_32 blocks_count = ((PAGE_SIZE - sizeof(struct arena)) -
                                        KASAN_SAFE_REDZONE_SIZE) /
                                       (block_size + KASAN_SAFE_REDZONE_SIZE);
                desc_array[desc_idx].redzone_size = KASAN_SAFE_REDZONE_SIZE;
#else
                uint_32 blocks_count =
                    (PAGE_SIZE - sizeof(struct arena)) / block_size;
                desc_array[desc_idx].redzone_size = 0;
#endif
                desc_array[desc_idx].blocks_per_arena = blocks_count;

                INIT_LIST_HEAD(&desc_array[desc_idx].free_list);
                block_size *= 2;
        }
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
// 0xFF40_0000              +-------------------------------------+ shadow memory 1 page 4kb
//                          |                                     |
//                          |     96 pages for                    |
//                          |     shadow map 3MB real memory      |
//                          |                                     |
// 0xFF46_0000              |-------------------------------------|
//                          |                                     |
// 0xFF80_0000              +-------------------------------------+ kernel stack pool bottom
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
                paddress = get_physical_page(&kernel_pool);
                PANIC_IF(!paddress, "[mm]: not enough physical memory");
                put_page(alloc_start, paddress);
                /* mark_kernel_vaddr_reserved(alloc_start); */
                alloc_start -= 0x1000UL;
        }
}

// only alloc 1 page
static void alloc_shadow_memory()
{
        uintptr_t paddress = get_physical_page(&kernel_pool);
        if (!paddress) {
                PANIC("[mm]: not enough physical memory for shadow memory");
                return;
        }
        // shadow memory controled address start at K_HEAP_START
        put_page(KHEAP_SHA_MEM_START, paddress);
}

void mem_init(void)
{
        struct bootmem_entry entries[BOOTMEM_MAX_ENTRIES];
        uint_32 count;
        uint_32 alloc_end;

        if (bootmem_init_from_handoff() < 0)
                PANIC("[mm]: invalid E820 boot handoff");

        count = bootmem_count();
        if (count == 0 || count > BOOTMEM_MAX_ENTRIES)
                PANIC("[mm]: invalid E820 snapshot count");

        printk("\n");
        printk("baselow    basehight    lenghtlow    lenhight    type    \n");
        for (uint_32 index = 0; index < count; index++) {
                const struct bootmem_entry *entry = bootmem_get(index);

                ASSERT(entry != NULL);
                entries[index] = *entry;
                printk("%x         %x           %x           %x          %x\n",
                       entry->base_low, entry->base_high, entry->length_low,
                       entry->length_high, entry->type);
        }
        if (phys_resources_init_from_bootmem(entries, count) < 0)
                PANIC("[mm]: cannot build physical resource registry");
        if (bootmem_find_usable_end(entries, count, MEM_POOL_START,
                                    &alloc_end) < 0)
                PANIC("[mm]: no contiguous E820 allocator range");
        INFO("Allocator physical end %x", alloc_end);

        mem_pool_init(alloc_end);
        reserve_bootstrap_stack_vaddr();
        block_desc_init(k_block_descs);

#ifdef CONFIG_POSION_MEMORY
        alloc_shadow_memory();
#endif
        alloc_kstack_pool(K_THREAD_MAX * K_STACKSZ_IN_PAGE);

        /* VGA text buffer is mapped at virtual 0xC00B8000 by early page
         * tables (linear = phys | 0xC0000000).  If kernel heap is allowed
         * to allocate this virtual page, put_page() will silently rewrite
         * the PTE and our writes to 0xC00B8000 stop reaching VGA — the
         * screen looks frozen.  Mark it occupied so the heap skips it. */
        mark_kernel_vaddr_reserved(0xC00B8000UL);

        /* mark_kernel_vaddr_reserved(KHEAP_SHA_MEM_START); */

        // init local_cpu
        struct cpu_local *cpu = NULL;
        each_cpu(cpu)
        {
                char *vaddr = malloc_page(MP_KERNEL, INTR_STACKSZ_PAGE);
                cpu->irq_stack_top = vaddr + (PAGE_SIZE << 1);
                ASSERT(cpu->irq_stack_top != NULL);
        }
}
