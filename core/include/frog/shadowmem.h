#ifndef __FROG_SHADOWMEMORY_H
#define __FROG_SHADOWMEMORY_H
#include <frog/types.h>
// shadow memory things
#define KHEAP_SHA_MEM_START 0xFF400000
#define KASAN_PROTECTED_START K_HEAP_START
#define KASAN_FREE_PAGE 0xFF           // page was freed
#define KASAN_PAGE_REDZONE 0xFE        // redzone for kmalloc_large allocations
#define KASAN_KMALLOC_REDZONE 0xFC     // redzone inside slub object
#define KASAN_KMALLOC_FREE 0xFB        // object was free(kmem_cache_free/kfree)
#define KASAN_GLOBAL_REDZONE 0xFA      // redzone for global variable
#define KASAN_SAFE_REDZONE_SIZE 32     // 32 bytes
#define KASAN_DEFAULT_REDZONE_SIZE 16  // 8 bytes
#define KASAN_REDZONE_SCALE_SHIFT 3U

struct arena;

struct shadow_memory {
        uintptr_t shadow_start;
        uintptr_t guard_addr_start;
        uint_32 guard_size;   // KHEAP_SHA_MEM_START-K_HEAP_START
        uint_32 scale_shift;  // shadow : real = 1 : (1 << scale_shift)
};

int kasan_posion(uintptr_t addr,
                 uint_32 real_block_sz,
                 uint_32 real_redzone_sz,
                 char redzone_flag);
unsigned char kasan_check(uintptr_t addr, uint_32 size, bool is_write);

struct mem_block *kasan_posion_arena2block(struct arena *a,
                                           uint_32 idx,
                                           uint_32 block_size,
                                           uint_32 redzone_sz);
void kasan_protect_free(int blk_sz, uintptr_t addr);


#endif
