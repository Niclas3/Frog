#include <frog/memory.h>
#include <frog/shadowmem.h>
#include <frog/string.h>
#include <frog/threads.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include "./mem_egg.h"

#define VADDR2SHADDR(vaddr)    \
        (KHEAP_SHA_MEM_START + \
         (((vaddr) -KASAN_PROTECTED_START) >> KASAN_REDZONE_SCALE_SHIFT))

int kasan_posion(uintptr_t addr,
                 uint_32 real_block_sz,
                 uint_32 real_redzone_sz,
                 char redzone_flag)
{
        uintptr_t shd_mem = VADDR2SHADDR(addr);
        uint_32 shd_redzone_sz = real_redzone_sz >> KASAN_REDZONE_SCALE_SHIFT;
        uint_32 shd_blk_sz = real_block_sz >> KASAN_REDZONE_SCALE_SHIFT;
        uintptr_t l_rz_start = shd_mem - shd_redzone_sz;
        uintptr_t r_rz_start = shd_mem + shd_blk_sz;

        int count = shd_redzone_sz;
        while (count--) {
                *(char *) l_rz_start = redzone_flag;
                *(char *) r_rz_start = redzone_flag;
                l_rz_start++;
                r_rz_start++;
        }

        return 0;
}

unsigned char kasan_check(uintptr_t addr, uint_32 size, bool is_write)
{
        uintptr_t shddr = VADDR2SHADDR(addr);
        char val = *(char *) VADDR2SHADDR(addr);
        return val;
}


// Return a mem_block from a arena[idx]
struct mem_block *kasan_posion_arena2block(struct arena *a,
                                           uint_32 idx,
                                           uint_32 redzone_sz)
{
        return (struct mem_block *) ((uintptr_t) a + sizeof(struct arena) +
                                     (idx * a->desc->block_size) +
                                     ((idx + 1) * redzone_sz));
}


void kasan_protect_free(int blk_sz, uintptr_t addr)
{
        // Test double free
        unsigned char r = kasan_check(addr, blk_sz, 0);
        if (r < 0 && r == KASAN_KMALLOC_FREE) {
                DEBUG("[mm]: double free at %x", addr);
                PANIC("[mm]: double free");
        }
        // Test invalid free
        uint_32 next_redzone_offset = (1 << KASAN_REDZONE_SCALE_SHIFT);
        uintptr_t left = addr - next_redzone_offset;
        uintptr_t right = (addr + blk_sz) ;
        unsigned char l_shdm = kasan_check(left, blk_sz, 0);
        unsigned char r_shdm = kasan_check(right, blk_sz, 0);
        if (l_shdm == KASAN_KMALLOC_REDZONE &&
            r_shdm == KASAN_KMALLOC_REDZONE) {
                return;
        } else {
                DEBUG("[mm]: invalid free at %x", addr);
                PANIC("[mm]: invalid free");
        }
}
