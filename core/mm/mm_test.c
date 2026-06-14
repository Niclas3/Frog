#include <frog/memory.h>
#include <frog/string.h>
#include <frog/types.h>
#include <kernel/debug.h>

#include "./mem_egg.h"

#define PASS(name)         INFO("[mm-test]: PASS  " name)
#define FAIL(name, ...)    WARN("[mm-test]: FAIL  " name, ##__VA_ARGS__)

/*
 * 1. Slab basic: alloc/fill/free one block per slab tier (16..1024 bytes).
 *    Catches double-free, free_list corruption, or zero-size-block bugs.
 */
static void mm_slab_basic(void)
{
        uint_32 sizes[] = { 16, 32, 64, 128, 256, 512, 1024 };
        uint_32 n = sizeof(sizes) / sizeof(sizes[0]);

        for (uint_32 i = 0; i < n; i++) {
                uint_8 *p = kmalloc(sizes[i]);
                if (!p) {
                        FAIL("slab_basic: kmalloc(%u) returned NULL", sizes[i]);
                        return;
                }
                uint_8 pat = (uint_8)(0xA0 + i);
                memset(p, pat, sizes[i]);
                if (p[0] != pat || p[sizes[i] - 1] != pat) {
                        FAIL("slab_basic: pattern mismatch at size %u", sizes[i]);
                        kfree(p);
                        return;
                }
                kfree(p);
        }
        PASS("slab_basic (16/32/64/128/256/512/1024 bytes)");
}

/*
 * 2. Slab isolation: two adjacent allocs in the same tier must not alias.
 *    Catches duplicate-block-in-free_list bug.
 */
static void mm_slab_isolation(void)
{
        uint_8 *a = kmalloc(256);
        uint_8 *b = kmalloc(256);
        if (!a || !b) {
                FAIL("slab_isolation: alloc failed");
                if (a) kfree(a);
                if (b) kfree(b);
                return;
        }
        if (a == b) {
                FAIL("slab_isolation: two kmalloc(256) returned same pointer!");
                kfree(a);
                return;
        }
        memset(a, 0xAA, 256);
        memset(b, 0xBB, 256);
        for (int i = 0; i < 256; i++) {
                if (a[i] != 0xAA) {
                        FAIL("slab_isolation: a[%d]=0x%x (expected 0xAA)", i, a[i]);
                        kfree(a); kfree(b);
                        return;
                }
        }
        kfree(a);
        kfree(b);
        PASS("slab_isolation (two 256-byte blocks don't alias)");
}

/*
 * 3. Large-alloc pointer offset:
 *    Regression for Bug 1 — `return (void *) area + 1` (1-byte void* arith)
 *    instead of `return (void *)(area + 1)` (12-byte struct arena* arith).
 *
 *    The returned pointer must be exactly sizeof(struct arena) = 12 bytes past
 *    the page-aligned base.  Before the fix the offset would be 1.
 *
 *    We skip kfree on failure to avoid crashing on corrupted arena header.
 */
static void mm_large_alloc_offset(void)
{
        void *p = kmalloc(2048);
        if (!p) {
                FAIL("large_alloc_offset: kmalloc(2048) returned NULL");
                return;
        }
        uint_32 offset   = (uint_32)p & 0xFFFU;
        uint_32 expected = sizeof(struct arena);   /* 12 */

        if (offset != expected) {
                FAIL("large_alloc_offset: offset=%u expected=%u"
                     " (arena header pointer bug still present)",
                     offset, expected);
                /* Intentionally leak — kfree would crash on corrupted header. */
                return;
        }
        kfree(p);
        PASS("large_alloc_offset (ptr = page_base + sizeof(arena))");
}

/*
 * 4. Large-alloc write-through:
 *    Fill the entire allocated region, then kfree.  If Bug 1 is present,
 *    the fill overwrites arena->large (byte 8) turning it 0 or garbage,
 *    and kfree crashes when it follows the wrong path.
 *
 *    Tests three representative sizes covering 1-page and 2-page arenas.
 */
static void mm_large_alloc_writethrough(void)
{
        uint_32 sizes[] = { 1025, 2048, 4096 };
        uint_32 n = sizeof(sizes) / sizeof(sizes[0]);

        for (uint_32 i = 0; i < n; i++) {
                uint_8 *p = kmalloc(sizes[i]);
                if (!p) {
                        FAIL("large_writethrough: kmalloc(%u) returned NULL", sizes[i]);
                        return;
                }
                memset(p, 0x5A, sizes[i]);
                if (p[0] != 0x5A || p[sizes[i] - 1] != 0x5A) {
                        FAIL("large_writethrough: pattern mismatch at size %u", sizes[i]);
                        kfree(p);
                        return;
                }
                kfree(p);   /* crash here = arena header was corrupted */
        }
        PASS("large_writethrough (1025/2048/4096 bytes, kfree must not crash)");
}

/*
 * 5. Large-alloc multi-cycle:
 *    Allocate N large buffers simultaneously, verify patterns don't bleed
 *    across buffers, then free all.  After Bug 1+2 fixes, the virtual-page
 *    bitmap must be restored on each kfree so the next cycle can succeed.
 *    N=8 avoids exhausting the physical pool in a typical test environment.
 */
static void mm_large_alloc_multi(void)
{
#define MULTI_N 8
        void *ptrs[MULTI_N];

        for (int i = 0; i < MULTI_N; i++) {
                ptrs[i] = kmalloc(2048);
                if (!ptrs[i]) {
                        FAIL("large_alloc_multi: iteration %d returned NULL", i);
                        for (int j = 0; j < i; j++) kfree(ptrs[j]);
                        return;
                }
                memset(ptrs[i], (int)(0xC0 + i), 2048);
        }

        for (int i = 0; i < MULTI_N; i++) {
                uint_8 *b = ptrs[i];
                if (b[0] != (uint_8)(0xC0 + i) || b[2047] != (uint_8)(0xC0 + i)) {
                        FAIL("large_alloc_multi: slot %d pattern corrupted (0x%x)",
                             i, b[0]);
                        for (int j = 0; j < MULTI_N; j++) kfree(ptrs[j]);
                        return;
                }
        }

        for (int i = 0; i < MULTI_N; i++) kfree(ptrs[i]);
        PASS("large_alloc_multi (8 x 2048-byte alloc-all/verify-all/free-all)");
#undef MULTI_N
}

/*
 * 6. Slab reuse after free:
 *    Free a block then re-alloc; the allocator must return the same (or a
 *    valid) address, not NULL or a previously-freed pointer in use elsewhere.
 *    Catches use-after-free from double-free_list insertion.
 */
static void mm_slab_reuse(void)
{
        uint_8 *a = kmalloc(128);
        if (!a) {
                FAIL("slab_reuse: first alloc returned NULL");
                return;
        }
        memset(a, 0xDE, 128);
        kfree(a);

        uint_8 *b = kmalloc(128);
        if (!b) {
                FAIL("slab_reuse: re-alloc returned NULL");
                return;
        }
        /* b may equal a (block reuse) or be a different block — both valid */
        memset(b, 0xAD, 128);
        if (b[0] != 0xAD || b[127] != 0xAD) {
                FAIL("slab_reuse: re-alloc pattern check failed");
                kfree(b);
                return;
        }
        kfree(b);
        PASS("slab_reuse (128-byte block reuse after free)");
}

void mm_regression_test(void)
{
        INFO("[mm-test]: ===== memory regression tests =====");
        mm_slab_basic();
        mm_slab_isolation();
        mm_large_alloc_offset();
        mm_large_alloc_writethrough();
        mm_large_alloc_multi();
        mm_slab_reuse();
        INFO("[mm-test]: ===== done =====");
}
