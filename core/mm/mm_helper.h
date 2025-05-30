#ifndef __MM_PAGE_TABLE_HELPER
#define __MM_PAGE_TABLE_HELPER
#include <frog/memory.h>
#include <frog/shadowmem.h>
#include <frog/types.h>
#include "mem_egg.h"

#define KERNEL_BASE 0xc0000000

// upper 10 bits pde
#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)
// mid   10 bits pte
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)

void mm_lazy_alloc(uintptr_t addr);
static inline uint_32 *pte_ptr(uint_32 vaddr)
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

static inline uint_32 *pde_ptr(uint_32 vaddr)
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


static inline bool is_kernel_space(uintptr_t addr)
{
        return addr >= KERNEL_BASE;
}

#define PAGE_FAULT_ERR_P(erro) ((erro) &0x00000001UL)
#define PAGE_FAULT_ERR_WR(erro) ((erro) &0x00000002UL)

static inline bool page_present(uint_32 err_code)
{
        return PAGE_FAULT_ERR_P(err_code);
}

extern struct pool kernel_pool;
static bool is_shadow_memory_region(uintptr_t addr)
{
        uintptr_t start = KHEAP_SHA_MEM_START;
        uint_32 pool_size = kernel_pool.pool_size;
        // NOTICE:
        // if kernel pool is larger than 4MB use 4MB as shadow memory
        // else use original size.
        uint_32 size = pool_size > 0x400000UL ? 0x400000UL : pool_size;
        return (addr >= start) &&
               (addr < start + (size >> KASAN_REDZONE_SCALE_SHIFT));
}

static bool is_kernel_heap_memory_region(uintptr_t addr)
{
        uintptr_t start = K_HEAP_START;
        return (addr >= start) && (addr < start + kernel_pool.pool_size);
}
static bool is_user_space_region(uintptr_t addr)
{
        return addr < KERNEL_BASE;
}

static inline bool is_lazy_alloc_region(uintptr_t addr)
{
        // shadow memory region
        if (is_shadow_memory_region(addr) ||
            is_kernel_heap_memory_region(addr) || is_user_space_region(addr)) {
                return true;
        }
        return false;
}

static inline bool is_write_access(uint_32 err_code)
{
        return PAGE_FAULT_ERR_WR(err_code);
}

static inline bool page_writable(uint_32 fault_addr)
{
        uint_32 *ptd = pde_ptr(fault_addr);
        uint_32 *pte = pte_ptr(fault_addr);
        if ((*ptd & 0x00000001) && (*ptd & 0x00000002)) {
                if ((*pte & 0x00000001) && (*pte & 0x00000002)) {
                        return true;
                } else {
                        return false;
                }
        } else {
                return false;
        }
}

#endif
