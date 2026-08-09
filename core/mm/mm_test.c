#include <asm/page.h>

#include <frog/bootmem.h>
#include <frog/errno.h>
#include <frog/fcntl.h>
#include <frog/irqflags.h>
#include <frog/memory.h>
#include <frog/mman.h>
#include <frog/kernel.h>
#include <frog/phys_resource.h>
#include <frog/process.h>
#include <frog/refcount.h>
#include <frog/string.h>
#include <frog/test.h>
#include <frog/threads.h>
#include <frog/types.h>
#include <frog/uaccess.h>
#include <frog/vm.h>
#include <kernel/assert.h>
#include <kernel/debug.h>
#include <kernel/device.h>
#include <kernel/fd.h>
#include <kernel/mm_test.h>
#include <kernel/qemu_test.h>
#include <kernel/vfs.h>

#include "./mem_egg.h"
#include "./mm_helper.h"

#define PASS(name)         INFO("[mm-test]: PASS  " name)
static int mm_test_failures;
#define FAIL(name, ...)                                                   \
        do {                                                              \
                mm_test_failures++;                                       \
                WARN("[mm-test]: FAIL  " name, ##__VA_ARGS__);           \
        } while (0)

static void mm_block_desc_fork_metadata(void)
{
        struct mem_block_desc parent[DESC_CNT];
        struct mem_block_desc child[DESC_CNT];
        struct list_head first;
        struct list_head last;
        bool passed;

        block_desc_init(parent);
        INIT_LIST_HEAD(&first);
        INIT_LIST_HEAD(&last);
        list_add_tail(&first, &parent[0].free_list);
        list_add_tail(&last, &parent[0].free_list);

        passed = sizeof(struct arena) == 12 &&
                 block_desc_clone_prepare(child, parent) == 0;
        if (passed) {
                block_desc_clone_fixup(child);
                passed = child[0].free_list.next == &first &&
                         child[0].free_list.prev == &last &&
                         first.prev == &child[0].free_list &&
                         first.next == &last && last.prev == &first &&
                         last.next == &child[0].free_list &&
                         child[1].free_list.next == &child[1].free_list &&
                         child[1].free_list.prev == &child[1].free_list;
        }

        if (passed)
                PASS("block_desc_fork_metadata");
        else
                FAIL("block_desc_fork_metadata");
}

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

static void mm_supervisor_permissions(void)
{
        uint_32 kernel_addr = (uint_32) mm_regression_test;
        uint_32 kernel_pde = *pde_ptr(kernel_addr);
        uint_32 kernel_pte = *pte_ptr(kernel_addr);
        uint_32 identity_pde = *pde_ptr(0);
        uint_32 recursive_pde = *pde_ptr(0xfffff000U);
        bool passed = (kernel_pde & PG_P_SET) &&
                      (kernel_pte & PG_P_SET) &&
                      !(kernel_pde & PG_US_U) &&
                      !(kernel_pte & PG_US_U) &&
                      (identity_pde & PG_P_SET) &&
                      !(identity_pde & PG_US_U) &&
                      (recursive_pde & PG_P_SET) &&
                      !(recursive_pde & PG_US_U);
        void *page = get_kernel_page(1);

        if (page == NULL) {
                FAIL("supervisor_permissions: kernel page allocation failed");
                return;
        }
        uint_32 dynamic_pde = *pde_ptr((uint_32) page);
        uint_32 dynamic_pte = *pte_ptr((uint_32) page);
        passed = (dynamic_pde & PG_P_SET) &&
                 (dynamic_pte & PG_P_SET) &&
                 !(dynamic_pde & PG_US_U) &&
                 !(dynamic_pte & PG_US_U) && passed;
        free_page(MP_KERNEL, page, 1);

        if (!passed) {
                FAIL("supervisor_permissions: kernel or recursive entry is user-accessible");
                return;
        }
        PASS("supervisor_permissions (identity, kernel, recursive, dynamic)");
}

/* access_ok() is a pure range check and is safe before a user pgdir exists. */
static void mm_uaccess_range(void)
{
        bool passed = true;

        passed = access_ok(NULL, 0) && passed;
        passed = access_ok((void *) 0xffffffffU, 0) && passed;
        passed = !access_ok(NULL, 1) && passed;
        passed = !access_ok((void *) (USER_VADDR_START - 1), 1) && passed;
        passed = access_ok((void *) USER_VADDR_START, 1) && passed;
        passed = access_ok((void *) 0xbfffffffU, 1) && passed;
        passed = !access_ok((void *) 0xbfffffffU, 2) && passed;
        passed = access_ok((void *) USER_VADDR_START,
                           0xc0000000U - USER_VADDR_START) && passed;
        passed = !access_ok((void *) USER_VADDR_START,
                            0xc0000000U - USER_VADDR_START + 1) && passed;
        passed = !access_ok((void *) USER_VADDR_START, 0xffffffffU) && passed;

        if (!passed) {
                FAIL("uaccess_range: boundary or overflow check failed");
                return;
        }
        PASS("uaccess_range (zero, bounds, and overflow)");
}

static void mm_bootmem_range(void)
{
        struct bootmem_entry entry = { 0 };
        bool passed = !bootmem_range_valid(NULL) &&
                      !bootmem_range_valid(&entry);

        entry.length_low = 1;
        passed = bootmem_range_valid(&entry) && passed;

        entry.base_low = 0xffffffffU;
        entry.base_high = 0xffffffffU;
        passed = !bootmem_range_valid(&entry) && passed;

        entry.base_low = 0xfffffffeU;
        passed = bootmem_range_valid(&entry) && passed;

        entry.base_low = 0;
        entry.base_high = 0;
        entry.length_low = 0;
        entry.length_high = 1;
        passed = bootmem_range_valid(&entry) && passed;

        if (!passed) {
                FAIL("bootmem_range: zero or 64-bit overflow check failed");
                return;
        }
        PASS("bootmem_range (zero and 64-bit overflow)");
}

static void mm_bootmem_allocator_range(void)
{
        struct bootmem_entry hole[] = {
            { 0x00100000U, 0, 0x00200000U, 0, BOOTMEM_TYPE_USABLE },
            { 0x00400000U, 0, 0x00400000U, 0, BOOTMEM_TYPE_USABLE },
        };
        struct bootmem_entry embedded_reserved[] = {
            { 0x00100000U, 0, 0x00700000U, 0, BOOTMEM_TYPE_USABLE },
            { 0x00480000U, 0, 0x00080000U, 0, 2 },
        };
        struct bootmem_entry covers_start[] = {
            { 0x00100000U, 0, 0x00700000U, 0, BOOTMEM_TYPE_USABLE },
            { 0x00180000U, 0, 0x00100000U, 0, 2 },
        };
        struct bootmem_entry invalid_tail[] = {
            { 0x00100000U, 0, 0x00700000U, 0, BOOTMEM_TYPE_USABLE },
            { 0, 0, 0, 0, 2 },
        };
        struct bootmem_entry four_gib = {
            0, 0, 0, 1, BOOTMEM_TYPE_USABLE
        };
        uint_32 end = 0;
        bool passed = true;

        passed = bootmem_find_usable_end(hole, 2, 0x00180000U, &end) == 0 &&
                 end == 0x00300000U && passed;
        passed = bootmem_find_usable_end(hole, 2, 0x00380000U, &end) ==
                     -EINVAL &&
                 passed;
        passed = bootmem_find_usable_end(embedded_reserved, 2, 0x00200000U,
                                         &end) == 0 &&
                 end == 0x00480000U && passed;
        passed = bootmem_find_usable_end(covers_start, 2, 0x00200000U,
                                         &end) == -EINVAL &&
                 passed;
        passed = bootmem_find_usable_end(invalid_tail, 2, 0x00200000U,
                                         &end) == -EINVAL &&
                 passed;
        passed = bootmem_find_usable_end(&four_gib, 1, 0x00200000U, &end) ==
                     0 &&
                 end == 0xfffff000U && passed;
        passed = bootmem_find_usable_end(NULL, 1, 0x00200000U, &end) ==
                     -EINVAL &&
                 bootmem_find_usable_end(hole, 0, 0x00200000U, &end) ==
                     -EINVAL &&
                 passed;

        if (!passed) {
                FAIL("bootmem_allocator: contiguous range selection failed");
                return;
        }
        PASS("bootmem_allocator (holes, reserved ranges, and 4GiB cap)");
}

static void mm_pool_bitmap_capacity(void)
{
        uint_32 pages = mem_pool_fit_page_count(0xfffff000U / PAGE_SIZE,
                                                0x00010000U);
        bool passed = pages == 349520U &&
                      (pages / 16U) * 3U <= 0x00010000U &&
                      ((pages + 16U) / 16U) * 3U > 0x00010000U;

        passed = mem_pool_fit_page_count(335U, 0x00010000U) == 320U &&
                 mem_pool_fit_page_count(16U, 2U) == 0U && passed;
        if (!passed) {
                FAIL("pool_bitmap_capacity: page cap calculation failed");
                return;
        }
        PASS("pool_bitmap_capacity (4GiB range capped to 64KiB window)");
}

static void mm_refcount_lifecycle(void)
{
        refcount_t ref;
        bool passed = true;

        refcount_init(&ref, 1);
        passed = refcount_read(&ref) == 1 && passed;
        passed = refcount_get_live(&ref) && refcount_read(&ref) == 2 &&
                 passed;
        passed = !refcount_put(&ref) && refcount_read(&ref) == 1 && passed;
        passed = refcount_put(&ref) && refcount_read(&ref) == 0 && passed;
        passed = !refcount_get_live(&ref) && refcount_read(&ref) == 0 &&
                 passed;

        refcount_init(&ref, UINT_MAX);
        passed = !refcount_get_live(&ref) &&
                 refcount_read(&ref) == UINT_MAX && passed;

        if (!passed) {
                FAIL("refcount_lifecycle: transition, zero, or overflow check failed");
                return;
        }
        PASS("refcount_lifecycle (live get, zero, and saturation)");
}

static void mm_phys_resource_registry(void)
{
        struct phys_resource_registry registry;
        struct phys_resource ram;
        struct phys_resource left_adjacent;
        struct phys_resource right_adjacent;
        struct phys_resource overlaps[5];
        struct phys_resource invalid;
        bool passed = true;

        phys_resource_registry_init(&registry);
        phys_resource_init(&ram);
        phys_resource_init(&left_adjacent);
        phys_resource_init(&right_adjacent);
        for (uint_32 index = 0; index < 5; index++)
                phys_resource_init(&overlaps[index]);
        phys_resource_init(&invalid);

        passed = phys_resource_registry_register(
                     &registry, &ram, 0x1000ULL, 0x1000ULL,
                     PHYS_RESOURCE_RAM, VM_CACHE_WRITE_BACK) == 0 &&
                 passed;
        passed = phys_resource_registry_register(
                     &registry, &overlaps[0], 0x1000ULL, 0x1000ULL,
                     PHYS_RESOURCE_MMIO, VM_CACHE_UNCACHED) == -EBUSY &&
                 passed;
        passed = phys_resource_registry_register(
                     &registry, &overlaps[1], 0x1400ULL, 0x0200ULL,
                     PHYS_RESOURCE_MMIO, VM_CACHE_UNCACHED) == -EBUSY &&
                 passed;
        passed = phys_resource_registry_register(
                     &registry, &overlaps[2], 0x0800ULL, 0x2000ULL,
                     PHYS_RESOURCE_MMIO, VM_CACHE_UNCACHED) == -EBUSY &&
                 passed;
        passed = phys_resource_registry_register(
                     &registry, &overlaps[3], 0x0800ULL, 0x1000ULL,
                     PHYS_RESOURCE_MMIO, VM_CACHE_UNCACHED) == -EBUSY &&
                 passed;
        passed = phys_resource_registry_register(
                     &registry, &overlaps[4], 0x1800ULL, 0x1000ULL,
                     PHYS_RESOURCE_MMIO, VM_CACHE_UNCACHED) == -EBUSY &&
                 passed;
        for (uint_32 index = 0; index < 5; index++) {
                passed = overlaps[index].state == PHYS_RESOURCE_NEW &&
                         refcount_read(&overlaps[index].refs) == 0 && passed;
        }

        passed = phys_resource_registry_register(
                     &registry, &left_adjacent, 0, 0x1000ULL,
                     PHYS_RESOURCE_MMIO, VM_CACHE_UNCACHED) == 0 &&
                 phys_resource_registry_register(
                     &registry, &right_adjacent, 0x2000ULL, 0x1000ULL,
                     PHYS_RESOURCE_MMIO, VM_CACHE_UNCACHED) == 0 &&
                 passed;
        passed = phys_resource_registry_register(
                     &registry, &invalid, 0x4000ULL, 0,
                     PHYS_RESOURCE_MMIO, VM_CACHE_UNCACHED) == -EINVAL &&
                 phys_resource_registry_register(
                     &registry, &invalid, ~0ULL - 0x100ULL, 0x200ULL,
                     PHYS_RESOURCE_MMIO, VM_CACHE_UNCACHED) == -EOVERFLOW &&
                 phys_resource_registry_register(
                     &registry, &invalid, 0x4000ULL, 0x1000ULL,
                     (enum phys_resource_type) 99,
                     VM_CACHE_UNCACHED) == -EINVAL &&
                 phys_resource_registry_register(
                     &registry, &invalid, 0x4000ULL, 0x1000ULL,
                     PHYS_RESOURCE_MMIO,
                     (enum vm_cache_mode) 99) == -EINVAL &&
                 phys_resource_registry_register(
                     &registry, &invalid, 0x4000ULL, 0x1000ULL,
                     PHYS_RESOURCE_MMIO, VM_CACHE_WRITE_BACK) == -EINVAL &&
                 invalid.state == PHYS_RESOURCE_NEW && passed;
        passed = !phys_resource_contains(&invalid, 0x4000ULL, 1) &&
                 phys_resource_contains(&ram, 0x1000ULL, 0x1000ULL) &&
                 phys_resource_contains(&ram, 0x1000ULL, 1) &&
                 phys_resource_contains(&ram, 0x1fffULL, 1) &&
                 !phys_resource_contains(&ram, 0x1000ULL, 0) &&
                 !phys_resource_contains(&ram, 0x0fffULL, 1) &&
                 !phys_resource_contains(&ram, 0x1fffULL, 2) &&
                 !phys_resource_contains(&ram, ~0ULL - 1, 3) && passed;

        passed = phys_resource_get_live(&ram) &&
                 refcount_read(&ram.refs) == 2 && passed;
        passed = phys_resource_registry_unregister(&registry, &ram) ==
                     -EBUSY &&
                 ram.state == PHYS_RESOURCE_REGISTERED &&
                 ram.registry == &registry && refcount_read(&ram.refs) == 2 &&
                 passed;
        phys_resource_put(&ram);
        passed = refcount_read(&ram.refs) == 1 &&
                 phys_resource_registry_unregister(&registry, &ram) == 0 &&
                 ram.state == PHYS_RESOURCE_DEAD && ram.registry == NULL &&
                 refcount_read(&ram.refs) == 0 &&
                 ram.node.next == &ram.node && ram.node.prev == &ram.node &&
                 !phys_resource_get_live(&ram) &&
                 !phys_resource_contains(&ram, 0x1000ULL, 1) &&
                 phys_resource_registry_register(
                     &registry, &ram, 0x4000ULL, 0x1000ULL,
                     PHYS_RESOURCE_MMIO, VM_CACHE_UNCACHED) == -EINVAL &&
                 passed;

        passed = phys_resource_registry_unregister(&registry,
                                                   &left_adjacent) == 0 &&
                 phys_resource_registry_unregister(&registry,
                                                   &right_adjacent) == 0 &&
                 list_is_empty(&registry.resources) && passed;

        if (!passed) {
                FAIL("phys_resource_registry: overlap, lifetime, or range check failed");
                return;
        }
        PASS("phys_resource_registry (ranges, adjacency, and lifetime)");
}

static void mm_phys_resource_bootmem_snapshot(void)
{
        struct bootmem_entry entries[] = {
            { 0x00100000U, 0, 0x00700000U, 0, BOOTMEM_TYPE_USABLE },
            { 0x00700000U, 0, 0x00200000U, 0, BOOTMEM_TYPE_USABLE },
            { 0x00800000U, 0, 0x00100000U, 0, 2 },
            { 0, 1, 0x00200000U, 0, BOOTMEM_TYPE_USABLE },
        };
        struct phys_resource_registry registry;
        struct phys_resource ram[3];
        struct phys_resource high_overlap;
        uint_32 allocator_end = 0;
        bool passed = true;

        phys_resource_init(&high_overlap);
        passed = bootmem_find_usable_end(entries, 4, 0x00200000U,
                                         &allocator_end) == 0 &&
                 allocator_end == 0x00800000U && passed;
        passed = phys_resource_registry_init_from_bootmem(
                     &registry, ram, 3, entries, 4) == 0 &&
                 ram[0].state == PHYS_RESOURCE_REGISTERED &&
                 ram[0].start == 0x00100000ULL &&
                 ram[0].end == 0x00800000ULL &&
                 ram[1].state == PHYS_RESOURCE_REGISTERED &&
                 ram[1].start == 0x00700000ULL &&
                 ram[1].end == 0x00900000ULL &&
                 ram[2].state == PHYS_RESOURCE_REGISTERED &&
                 ram[2].start == 0x100000000ULL &&
                 ram[2].end == 0x100200000ULL && passed;

        /* The high range is outside the contiguous allocator but still RAM. */
        passed = phys_resource_registry_register(
                     &registry, &high_overlap, 0x100100000ULL, 0x1000ULL,
                     PHYS_RESOURCE_MMIO, VM_CACHE_UNCACHED) == -EBUSY &&
                 high_overlap.state == PHYS_RESOURCE_NEW && passed;
        passed = phys_resource_registry_unregister(&registry, &ram[0]) == 0 &&
                 phys_resource_registry_unregister(&registry, &ram[1]) == 0 &&
                 phys_resource_registry_unregister(&registry, &ram[2]) == 0 &&
                 list_is_empty(&registry.resources) && passed;

        if (!passed) {
                FAIL("phys_resource_bootmem: complete E820 RAM snapshot was not retained");
                return;
        }
        PASS("phys_resource_bootmem (complete and overlapping E820 snapshot)");
}

static void mm_test_vma_init(struct vm_area *vma,
                             uint_32 start,
                             uint_32 end)
{
        memset(vma, 0, sizeof(*vma));
        INIT_LIST_HEAD(&vma->elem);
        vma->start = start;
        vma->end = end;
        vma->state = VM_PREPARING;
}

static void mm_vma_metadata(void)
{
        struct mm_struct *mm = mm_create();
        struct vm_area left;
        struct vm_area adjacent;
        struct vm_area middle;
        struct vm_area right;
        struct vm_area unaligned;
        struct vm_area empty;
        struct vm_area reversed;
        struct vm_area overlap_left;
        struct vm_area overlap_middle;
        struct vm_area overlap_exact;
        bool passed = true;

        if (mm == NULL) {
                FAIL("vma_metadata: mm allocation failed");
                return;
        }
        passed = mm->pgdir == NULL &&
                 mm->user_vaddr.vaddr_bitmap.bits == NULL &&
                 mm->user_vaddr.vaddr_bitmap.map_bytes_length == 0 &&
                 list_is_empty(&mm->vma_list) && mm->generation == 0;

        mm_test_vma_init(&left, 0x40000000U, 0x40002000U);
        mm_test_vma_init(&adjacent, 0x40002000U, 0x40004000U);
        mm_test_vma_init(&middle, 0x40004000U, 0x40006000U);
        mm_test_vma_init(&right, 0x40008000U, 0x40009000U);
        passed = vm_area_insert(mm, &right) == 0 && passed;
        passed = vm_area_insert(mm, &left) == 0 && passed;
        passed = vm_area_insert(mm, &middle) == 0 && passed;
        passed = mm->vma_list.next == &left.elem &&
                 left.elem.next == &middle.elem &&
                 middle.elem.next == &right.elem &&
                 right.elem.next == &mm->vma_list && passed;

        mm_test_vma_init(&unaligned, 0x40000001U, 0x40001000U);
        mm_test_vma_init(&empty, 0x50000000U, 0x50000000U);
        mm_test_vma_init(&reversed, 0xfffff000U, 0x00001000U);
        passed = vm_area_insert(mm, &unaligned) == -EINVAL && passed;
        passed = vm_area_insert(mm, &empty) == -EINVAL && passed;
        passed = vm_area_insert(mm, &reversed) == -EINVAL && passed;

        mm_test_vma_init(&overlap_left, 0x40001000U, 0x40003000U);
        mm_test_vma_init(&overlap_middle, 0x40003000U, 0x40007000U);
        mm_test_vma_init(&overlap_exact, 0x40004000U, 0x40006000U);
        passed = vm_area_insert(mm, &overlap_left) == -EEXIST && passed;
        passed = vm_area_insert(mm, &overlap_middle) == -EEXIST && passed;
        passed = vm_area_insert(mm, &overlap_exact) == -EEXIST && passed;
        passed = vm_area_insert(mm, &adjacent) == 0 && passed;
        passed = mm->generation == 4 && passed;

        passed = vm_area_find(mm, 0x3fffffffU) == NULL && passed;
        passed = vm_area_find(mm, left.start) == &left && passed;
        passed = vm_area_find(mm, left.end) == &adjacent && passed;
        passed = vm_area_find(mm, middle.end) == NULL && passed;
        passed = vm_area_find(mm, right.end) == NULL && passed;
        passed = vm_area_find_exact(mm, middle.start, middle.end) ==
                     &middle &&
                 vm_area_find_exact(mm, middle.start,
                                    middle.end - PAGE_SIZE) == NULL &&
                 passed;

        unsigned long long generation = mm->generation;
        passed = vm_area_remove_exact(mm, middle.start,
                                      middle.end - PAGE_SIZE) == NULL &&
                 mm->generation == generation && passed;
        struct vm_area *removed_middle =
            vm_area_remove_exact(mm, middle.start, middle.end);
        struct vm_area *removed_left =
            vm_area_remove_exact(mm, left.start, left.end);
        struct vm_area *removed_adjacent =
            vm_area_remove_exact(mm, adjacent.start, adjacent.end);
        struct vm_area *removed_right =
            vm_area_remove_exact(mm, right.start, right.end);
        passed = removed_middle == &middle && middle.mm == NULL &&
                 middle.elem.next == &middle.elem &&
                 middle.elem.prev == &middle.elem &&
                 removed_left == &left && removed_adjacent == &adjacent &&
                 removed_right == &right && list_is_empty(&mm->vma_list) &&
                 mm->generation == 8 && passed;

        if (!list_is_empty(&mm->vma_list)) {
                passed = false;
                while (!list_is_empty(&mm->vma_list)) {
                        struct list_head *node = mm->vma_list.next;
                        struct vm_area *vma =
                            list_entry(node, struct vm_area, elem);
                        list_del_init(node);
                        vma->mm = NULL;
                }
        }

        mm_destroy(mm);
        if (!passed) {
                FAIL("vma_metadata: ordering, boundary, overlap, or exact lookup failed");
                return;
        }
        PASS("vma_metadata (ordered, non-overlapping, exact remove)");
}

#ifdef CONFIG_FROG_TEST_PROCESS
#define VM_PROCESS_TEST_PHYSICAL      0x000b8000U
#define VM_PROCESS_TEST_KERNEL_ALIAS  0xc00b8000U
#define VM_PROCESS_TEST_LENGTH        (3U * PAGE_SIZE)
#define VM_PROCESS_TEST_PDE_SIZE      (1024U * PAGE_SIZE)
#define VM_PROCESS_TEST_CPU_PTE_BITS  0x060U

struct vm_process_test_fixture {
        bool initialized;
        bool active;
        bool busy_checked;
        bool busy_ok;
        bool rollback_pending;
        struct phys_resource resource;
        struct device device;
        struct inode inode;
        struct file *file;
        int file_fd;
        int global_fd;
        struct vm_mapping *mapping;
        struct vm_area *vma;
        uint_32 mapped;
        uint_32 close_count;
        uint_32 file_close_count;
        uint_32 active_close_count;
        uint_32 device_release_count;
        uint_32 rollback_kernel_frames;
        uint_32 rollback_user_frames;
        uint_8 saved_byte;
};

static struct vm_process_test_fixture vm_process_fixture;
extern struct pool user_pool;

static void vm_process_device_release(struct device *device)
{
        struct vm_process_test_fixture *fixture =
            container_of(device, struct vm_process_test_fixture, device);

        fixture->device_release_count++;
}

static int_32 vm_process_file_close(struct file *file)
{
        struct vm_process_test_fixture *fixture = file->private_data;

        ASSERT(fixture != NULL && fixture->file == file);
        fixture->file_close_count++;
        fixture->file = NULL;
        return 0;
}

static int_32 vm_process_file_mmap(struct file *file, struct vm_area *vma);

static const struct file_operations vm_process_file_ops = {
        .close = vm_process_file_close,
        .mmap = vm_process_file_mmap,
};

static void vm_process_mapping_close(struct vm_mapping *mapping)
{
        struct vm_process_test_fixture *fixture = mapping->private_data;

        ASSERT(fixture != NULL && mapping->resource == &fixture->resource &&
               mapping->device == &fixture->device &&
               mapping->file == fixture->file &&
               refcount_read(&mapping->file->f_refs) > 0);
        if (fixture->active && fixture->mapping == mapping) {
                fixture->mapping = NULL;
                fixture->vma = NULL;
                fixture->active = false;
        }
        fixture->close_count++;
        (void) file_put(mapping->file);
        phys_resource_put(mapping->resource);
        device_put(mapping->device);
}

static const struct vm_operations vm_process_mapping_ops = {
        .close = vm_process_mapping_close,
};

static int_32 vm_process_file_mmap(struct file *file, struct vm_area *vma)
{
        struct vm_process_test_fixture *fixture;
        struct vm_mapping *mapping;
        bool resource_pinned = false;

        if (file == NULL || vma == NULL || file->private_data == NULL)
                return -ENODEV;
        fixture = file->private_data;
        mapping = vma->mapping;
        if (!fixture->initialized || fixture->file != file ||
            fixture->active)
                return fixture->active ? -EBUSY : -ENODEV;
        if (mapping == NULL || mapping->state != VM_MAPPING_NEW ||
            vma->start != 0 || vma->end != VM_PROCESS_TEST_LENGTH ||
            vma->prot != (PROT_READ | PROT_WRITE) ||
            vma->flags != MAP_SHARED || vma->page_offset != 0)
                return -EINVAL;

        if (!device_get_live(&fixture->device))
                return -ENODEV;
        lock_fetch(&fixture->device.lock);
        if (fixture->device.state == DEVICE_LIVE)
                resource_pinned = phys_resource_get_live(&fixture->resource);
        lock_release(&fixture->device.lock);
        if (!resource_pinned) {
                device_put(&fixture->device);
                return -ENODEV;
        }

        mapping->vm_ops = &vm_process_mapping_ops;
        if (vm_mapping_prepare_device(mapping, file, &fixture->device,
                                      &fixture->resource, fixture) != 0) {
                mapping->vm_ops = NULL;
                phys_resource_put(&fixture->resource);
                device_put(&fixture->device);
                return -ENODEV;
        }
        fixture->mapping = mapping;
        fixture->vma = vma;
        fixture->active_close_count = fixture->close_count;
        fixture->active = true;
        return 0;
}

static uint_32 vm_process_bitmap_bit(struct mm_struct *mm, uint_32 address)
{
        uint_32 bit = (address - mm->user_vaddr.vaddr_start) / PAGE_SIZE;

        return get_value_bitmap(&mm->user_vaddr.vaddr_bitmap, bit);
}

static bool vm_process_bitmap_range(struct mm_struct *mm,
                                    uint_32 start,
                                    uint_32 length,
                                    uint_32 expected)
{
        for (uint_32 address = start; address < start + length;
             address += PAGE_SIZE) {
                if (!!vm_process_bitmap_bit(mm, address) != !!expected)
                        return false;
        }
        return true;
}

static bool vm_process_pte_absent(uint_32 address)
{
        uint_32 *pde = pde_ptr(address);

        return !(*pde & PG_P_SET) || !(*pte_ptr(address) & PG_P_SET);
}

static bool vm_process_pte_range_absent(uint_32 start, uint_32 length)
{
        for (uint_32 address = start; address < start + length;
             address += PAGE_SIZE) {
                if (!vm_process_pte_absent(address))
                        return false;
        }
        return true;
}

static bool vm_process_pte_range_matches(uint_32 start,
                                         uint_32 physical,
                                         uint_32 length)
{
        for (uint_32 offset = 0; offset < length; offset += PAGE_SIZE) {
                uint_32 address = start + offset;
                uint_32 expected =
                    (physical + offset) | VM_DEVICE_PTE_FLAGS;
                uint_32 *pde = pde_ptr(address);

                if ((*pde & (PG_P_SET | PG_RW_W | PG_US_U)) !=
                        (PG_P_SET | PG_RW_W | PG_US_U) ||
                    (*pte_ptr(address) & ~VM_PROCESS_TEST_CPU_PTE_BITS) !=
                        expected)
                        return false;
        }
        return true;
}

static uint_32 vm_process_pool_frames_used(struct pool *pool)
{
        uint_32 used = 0;
        uint_32 capacity = pool->pool_bitmap.map_bytes_length * 8U;

        for (uint_32 bit = 0; bit < capacity; bit++) {
                if (get_value_bitmap(&pool->pool_bitmap, bit))
                        used++;
        }
        return used;
}

static int vm_process_fixture_init(struct vm_process_test_fixture *fixture)
{
        int result;

        if (fixture->initialized)
                return -EBUSY;
        memset(fixture, 0, sizeof(*fixture));
        fixture->file_fd = -1;
        fixture->global_fd = -1;
        fixture->file = kmalloc(sizeof(*fixture->file));
        if (fixture->file == NULL)
                return -ENOMEM;
        memset(fixture->file, 0, sizeof(*fixture->file));
        fixture->inode.i_count = 1;
        fixture->inode.i_fop = &vm_process_file_ops;
        fixture->file->f_inode = &fixture->inode;
        fixture->file->f_op = &vm_process_file_ops;
        fixture->file->f_flag = O_RDWR;
        fixture->file->private_data = fixture;
        refcount_init(&fixture->file->f_refs, 1);
        device_init(&fixture->device, vm_process_device_release);
        fixture->device.name = "vm-process-test";
        refcount_init(&fixture->device.refs, 1);
        fixture->device.state = DEVICE_LIVE;
        phys_resource_init(&fixture->resource);

        lock_fetch(&fixture->device.lock);
        result = phys_resource_register(&fixture->resource,
                                        VM_PROCESS_TEST_PHYSICAL,
                                        VM_PROCESS_TEST_LENGTH,
                                        PHYS_RESOURCE_MMIO,
                                        VM_CACHE_UNCACHED);
        lock_release(&fixture->device.lock);
        if (result != 0) {
                (void) file_put(fixture->file);
                ASSERT(device_begin_unregister(&fixture->device) == 0);
                ASSERT(device_finish_unregister(&fixture->device) == 0);
                return result;
        }

        result = fd_alloc(fixture->file);
        if (result < 0) {
                (void) file_put(fixture->file);
                ASSERT(device_begin_unregister(&fixture->device) == 0);
                lock_fetch(&fixture->device.lock);
                ASSERT(phys_resource_unregister(&fixture->resource) == 0);
                lock_release(&fixture->device.lock);
                ASSERT(device_finish_unregister(&fixture->device) == 0);
                return result;
        }
        fixture->file_fd = result;
        fixture->global_fd = running_thread()->fd_table[result];
        fixture->saved_byte =
            *(volatile uint_8 *) VM_PROCESS_TEST_KERNEL_ALIAS;
        *(volatile uint_8 *) VM_PROCESS_TEST_KERNEL_ALIAS =
            FROG_TEST_VM_SEED;
        fixture->initialized = true;
        return result;
}

static bool vm_process_fixture_destroy(struct vm_process_test_fixture *fixture)
{
        bool passed = fixture->initialized && !fixture->active &&
                      refcount_read(&fixture->resource.refs) == 1 &&
                      refcount_read(&fixture->device.refs) == 1;

        if (!fixture->initialized)
                return false;
        if (fixture->file != NULL) {
                passed = fixture->file_fd >= 0 &&
                         fixture->file_fd < MAX_FILES_OPEN_PER_PROC &&
                         fixture->file->f_count == 1 &&
                         refcount_read(&fixture->file->f_refs) == 1 &&
                         fixture->inode.i_count == 1 && passed;
                if (fixture->file_fd >= 0 &&
                    fixture->file_fd < MAX_FILES_OPEN_PER_PROC) {
                        int close_result =
                            fd_close_for(running_thread(), fixture->file_fd);
                        if (running_thread()->fd_table[fixture->file_fd] == -1)
                                fixture->file_fd = -1;
                        passed = close_result == 0 && passed;
                }
        }
        passed = fixture->file == NULL && fixture->file_fd == -1 &&
                 fixture->file_close_count == 1 &&
                 fixture->inode.i_count == 0 && passed;
        if (device_begin_unregister(&fixture->device) != 0)
                return false;
        lock_fetch(&fixture->device.lock);
        int resource_result = phys_resource_unregister(&fixture->resource);
        lock_release(&fixture->device.lock);
        if (resource_result != 0) {
                ASSERT(device_cancel_unregister(&fixture->device) == 0);
                return false;
        }
        int device_result = device_finish_unregister(&fixture->device);

        passed = device_result == 0 &&
                 fixture->resource.state == PHYS_RESOURCE_DEAD &&
                 refcount_read(&fixture->resource.refs) == 0 &&
                 fixture->device.state == DEVICE_DEAD &&
                 refcount_read(&fixture->device.refs) == 0 &&
                 fixture->device_release_count == 1 && passed;
        fixture->initialized = false;
        return passed;
}

static int vm_process_new_vma(struct vm_process_test_fixture *fixture,
                              struct vm_mapping **mapping_out,
                              struct vm_area **vma_out)
{
        struct vm_mapping *mapping = vm_mapping_alloc(
            VM_BACKING_DEVICE_BORROWED, &vm_process_mapping_ops);
        struct vm_area *vma;
        bool file_pinned = false;
        bool resource_pinned = false;

        if (mapping == NULL)
                return -ENOMEM;
        vma = vm_area_alloc(VM_PROCESS_TEST_LENGTH, 0, 0, 0, mapping);
        if (vma == NULL) {
                vm_mapping_put(mapping);
                return -ENOMEM;
        }

        if (!file_get_live(fixture->file))
                goto fail_file;
        file_pinned = true;
        if (!device_get_live(&fixture->device))
                goto fail_file;
        lock_fetch(&fixture->device.lock);
        if (fixture->device.state == DEVICE_LIVE)
                resource_pinned = phys_resource_get_live(&fixture->resource);
        lock_release(&fixture->device.lock);
        if (!resource_pinned)
                goto fail_device;
        if (vm_mapping_prepare_device(mapping, fixture->file,
                                      &fixture->device, &fixture->resource,
                                      fixture) != 0)
                goto fail_resource;

        *mapping_out = mapping;
        *vma_out = vma;
        return 0;

fail_resource:
        phys_resource_put(&fixture->resource);
fail_device:
        device_put(&fixture->device);
fail_file:
        if (file_pinned)
                (void) file_put(fixture->file);
        kfree(vma);
        vm_mapping_put(mapping);
        return -ENODEV;
}

static int vm_process_insert_blocker(struct mm_struct *mm,
                                     struct vm_area *blocker)
{
        memset(blocker, 0, sizeof(*blocker));
        INIT_LIST_HEAD(&blocker->elem);
        blocker->start = VM_MMAP_START;
        blocker->end = VM_MMAP_START + VM_PROCESS_TEST_PDE_SIZE - PAGE_SIZE;
        blocker->state = VM_ACTIVE;
        lock_fetch(&mm->mmap_lock);
        int result = vm_area_insert(mm, blocker);
        lock_release(&mm->mmap_lock);
        return result;
}

static bool vm_process_remove_blocker(struct mm_struct *mm,
                                      struct vm_area *blocker)
{
        lock_fetch(&mm->mmap_lock);
        struct vm_area *removed =
            vm_area_remove_exact(mm, blocker->start, blocker->end);
        lock_release(&mm->mmap_lock);
        return removed == blocker;
}

static bool vm_process_failure_round(struct vm_process_test_fixture *fixture,
                                     int fail_after)
{
        struct mm_struct *mm = running_thread()->mm;
        struct vm_mapping *mapping = NULL;
        struct vm_area *vma = NULL;
        struct vm_area blocker;
        uint_32 target = VM_MMAP_START + VM_PROCESS_TEST_PDE_SIZE - PAGE_SIZE;
        uint_32 mapped = 0;
        uint_32 close_before = fixture->close_count;
        bool passed = vm_process_insert_blocker(mm, &blocker) == 0;

        if (!passed || vm_process_new_vma(fixture, &mapping, &vma) != 0) {
                if (passed)
                        (void) vm_process_remove_blocker(mm, &blocker);
                return false;
        }

        uint_32 frames_before = vm_process_pool_frames_used(&kernel_pool);
        vm_test_fail_map_after(fail_after);
        int result = vm_map_pfn_range(mm, vma, &mapped);
        uint_32 frames_after = vm_process_pool_frames_used(&kernel_pool);
        vm_test_fail_map_after(-1);

        passed = result == -ENOMEM && mapped == 0 &&
                 frames_before == frames_after && vma->mm == NULL &&
                 vma->start == 0 && vma->end == VM_PROCESS_TEST_LENGTH &&
                 vma->state == VM_PREPARING &&
                 vm_process_pte_range_absent(target,
                                             VM_PROCESS_TEST_LENGTH) &&
                 vm_process_bitmap_range(mm, target,
                                         VM_PROCESS_TEST_LENGTH, 0) && passed;
        if (result == 0) {
                passed = vm_unmap_exact(mm, mapped,
                                        VM_PROCESS_TEST_LENGTH) == 0 && passed;
                mapping = NULL;
                vma = NULL;
        } else {
                kfree(vma);
                vm_mapping_put(mapping);
        }
        passed = fixture->close_count == close_before + 1 &&
                 refcount_read(&fixture->resource.refs) == 1 &&
                 refcount_read(&fixture->device.refs) == 1 &&
                 fixture->file != NULL && fixture->file->f_count == 1 &&
                 refcount_read(&fixture->file->f_refs) == 1 &&
                 fixture->file_close_count == 0 &&
                 vm_process_remove_blocker(mm, &blocker) && passed;
        return passed;
}

static bool vm_process_cross_pde_round(
    struct vm_process_test_fixture *fixture)
{
        struct mm_struct *mm = running_thread()->mm;
        struct vm_mapping *mapping = NULL;
        struct vm_area *vma = NULL;
        struct vm_area blocker;
        uint_32 target = VM_MMAP_START + VM_PROCESS_TEST_PDE_SIZE - PAGE_SIZE;
        uint_32 mapped = 0;
        uint_32 close_before = fixture->close_count;
        bool passed = vm_process_insert_blocker(mm, &blocker) == 0;

        if (!passed || vm_process_new_vma(fixture, &mapping, &vma) != 0) {
                if (passed)
                        (void) vm_process_remove_blocker(mm, &blocker);
                return false;
        }
        vm_test_fail_map_after(-1);
        int result = vm_map_pfn_range(mm, vma, &mapped);

        passed = result == 0 && mapped == target &&
                 vm_process_pte_range_matches(mapped,
                                              VM_PROCESS_TEST_PHYSICAL,
                                              VM_PROCESS_TEST_LENGTH) &&
                 vm_process_bitmap_range(mm, mapped,
                                         VM_PROCESS_TEST_LENGTH, 1) && passed;
        if (result == 0) {
                passed = vm_unmap_exact(mm, mapped,
                                        VM_PROCESS_TEST_LENGTH) == 0 && passed;
                mapping = NULL;
                vma = NULL;
        } else {
                kfree(vma);
                vm_mapping_put(mapping);
        }
        passed = fixture->close_count == close_before + 1 &&
                 vm_process_pte_range_absent(target,
                                             VM_PROCESS_TEST_LENGTH) &&
                 vm_process_bitmap_range(mm, target,
                                         VM_PROCESS_TEST_LENGTH, 0) &&
                 refcount_read(&fixture->resource.refs) == 1 &&
                 refcount_read(&fixture->device.refs) == 1 &&
                 fixture->file != NULL && fixture->file->f_count == 1 &&
                 refcount_read(&fixture->file->f_refs) == 1 &&
                 fixture->file_close_count == 0 &&
                 vm_process_remove_blocker(mm, &blocker) && passed;
        return passed;
}

int_32 mm_vm_process_prepare(void)
{
        struct vm_process_test_fixture *fixture = &vm_process_fixture;
        int_32 fd;
        bool rollback_ok = true;

        if (running_thread()->mm == NULL) {
                frog_test_case("vm.fixture", 0);
                return -ENODEV;
        }
        fd = vm_process_fixture_init(fixture);
        if (fd < 0) {
                frog_test_case("vm.pfn-map", 0);
                return fd;
        }
        for (int fail_after = 0; fail_after <= 3; fail_after++)
                rollback_ok = vm_process_failure_round(fixture, fail_after) &&
                              rollback_ok;
        frog_test_case("vm.pfn-rollback", rollback_ok);
        frog_test_case("vm.pfn-cross-pde",
                       vm_process_cross_pde_round(fixture));
        return fd;
}

int_32 mm_vm_process_arm_fork_failure(uint_32 step)
{
        struct vm_process_test_fixture *fixture = &vm_process_fixture;

        if (!fixture->initialized || !fixture->active ||
            running_thread()->mm == NULL ||
            step >= FROG_TEST_VM_FORK_FAIL_COUNT ||
            refcount_read(&fixture->mapping->refs) != 1 ||
            fixture->rollback_pending)
                return -EINVAL;

        fixture->rollback_kernel_frames =
            vm_process_pool_frames_used(&kernel_pool);
        fixture->rollback_user_frames =
            vm_process_pool_frames_used(&user_pool);
        fixture->rollback_pending = true;
        vm_test_fail_fork_after((int) step);
        return 0;
}

int_32 mm_vm_process_verify_refs(uint_32 expected)
{
        struct vm_process_test_fixture *fixture = &vm_process_fixture;
        struct mm_struct *mm = running_thread()->mm;
        struct vm_area *vma;
        uint_32 mapped;
        bool passed;

        if (!fixture->initialized || !fixture->active ||
            fixture->mapping == NULL || fixture->vma == NULL ||
            mm == NULL || expected == 0)
                return -EINVAL;

        mapped = fixture->vma->start;
        lock_fetch(&mm->mmap_lock);
        vma = vm_area_find_exact(mm, mapped,
                                 mapped + VM_PROCESS_TEST_LENGTH);
        passed = vma != NULL && vma->state == VM_ACTIVE &&
                 vma->mapping == fixture->mapping &&
                 mapped == FROG_TEST_VM_EXPECTED_ADDR &&
                 vm_process_pte_range_matches(mapped,
                                              VM_PROCESS_TEST_PHYSICAL,
                                              VM_PROCESS_TEST_LENGTH) &&
                 vm_process_bitmap_range(mm, mapped,
                                         VM_PROCESS_TEST_LENGTH, 1);
        lock_release(&mm->mmap_lock);
        fixture->mapped = mapped;

        if (!fixture->busy_checked) {
                fixture->busy_ok =
                    device_begin_unregister(&fixture->device) == -EBUSY &&
                    fixture->device.state == DEVICE_LIVE;
                fixture->busy_checked = true;
                frog_test_case("vm.device-busy", fixture->busy_ok);
        }

        passed = refcount_read(&fixture->mapping->refs) == expected &&
                 refcount_read(&fixture->resource.refs) == 2 &&
                 refcount_read(&fixture->device.refs) == 2 &&
                 fixture->file != NULL &&
                 fixture->file->f_count == expected &&
                 refcount_read(&fixture->file->f_refs) == expected + 1 &&
                 fixture->file_close_count == 0 &&
                 fixture->close_count == fixture->active_close_count &&
                 fixture->busy_ok && passed;

        if (fixture->rollback_pending) {
                passed = expected == 1 &&
                         vm_process_pool_frames_used(&kernel_pool) ==
                             fixture->rollback_kernel_frames &&
                         vm_process_pool_frames_used(&user_pool) ==
                             fixture->rollback_user_frames && passed;
                fixture->rollback_pending = false;
        }
        return passed ? 0 : -EUCLEAN;
}

int_32 mm_vm_process_verify_cleanup(void)
{
        struct vm_process_test_fixture *fixture = &vm_process_fixture;
        struct mm_struct *mm = running_thread()->mm;
        TCB_t *thread = running_thread();
        uint_32 mapped = fixture->mapped;

        if (!fixture->initialized || fixture->active ||
            fixture->mapping != NULL || fixture->vma != NULL || mm == NULL)
                return -EINVAL;
        bool alias_ok =
            *(volatile uint_8 *) VM_PROCESS_TEST_KERNEL_ALIAS ==
            FROG_TEST_VM_WRITTEN;
        bool descriptor_gone =
            fixture->file_fd >= 0 &&
            fixture->file_fd < MAX_FILES_OPEN_PER_PROC &&
            thread->fd_table[fixture->file_fd] == -1 &&
            fixture->global_fd >= 0 && fixture->global_fd < MAX_FILE_OPEN &&
            g_file_table[fixture->global_fd] == NULL;
        bool lifetime_ok = mapped == FROG_TEST_VM_EXPECTED_ADDR &&
                           fixture->busy_checked && fixture->busy_ok &&
                           fixture->close_count ==
                               fixture->active_close_count + 1 &&
                           refcount_read(&fixture->resource.refs) == 1 &&
                           refcount_read(&fixture->device.refs) == 1 &&
                           fixture->file == NULL &&
                           fixture->file_close_count == 1 &&
                           fixture->inode.i_count == 0 && descriptor_gone &&
                           vm_process_pte_range_absent(
                               mapped, VM_PROCESS_TEST_LENGTH) &&
                           vm_process_bitmap_range(
                               mm, mapped, VM_PROCESS_TEST_LENGTH, 0);

        *(volatile uint_8 *) VM_PROCESS_TEST_KERNEL_ALIAS = fixture->saved_byte;
        if (descriptor_gone) {
                fixture->file_fd = -1;
                fixture->global_fd = -1;
        }
        bool fixture_destroyed = vm_process_fixture_destroy(fixture);
        bool destroy_ok = lifetime_ok && fixture_destroyed;
        frog_test_case("vm.user-alias", alias_ok);
        frog_test_case("vm.mapping-lifetime", destroy_ok);
        return alias_ok && destroy_ok ? 0 : -EUCLEAN;
}

static void mm_test_invlpg(uint_32 addr)
{
        __asm__ volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

void mm_uaccess_process_regression(void)
{
        uint_8 kernel_source[16];
        uint_8 kernel_dest[16];
        uint_8 *user = get_user_page(2);
        if (user == NULL) {
                frog_test_case("uaccess.user-pages", 0);
                return;
        }

        uint_32 user_required = PG_P_SET | PG_US_U;
        uint_32 kernel_addr = (uint_32) mm_uaccess_process_regression;
        int image_user =
            (*pde_ptr(USER_IMAGE_VADDR) & user_required) == user_required &&
            (*pte_ptr(USER_IMAGE_VADDR) & user_required) == user_required;
        int allocated_user =
            (*pde_ptr((uint_32) user) & user_required) == user_required &&
            (*pte_ptr((uint_32) user) & user_required) == user_required;
        int kernel_supervisor =
            (*pde_ptr(kernel_addr) & PG_P_SET) &&
            (*pte_ptr(kernel_addr) & PG_P_SET) &&
            !(*pde_ptr(kernel_addr) & PG_US_U) &&
            !(*pte_ptr(kernel_addr) & PG_US_U);
        uint_32 recursive_pde = *pde_ptr(0xfffff000U);
        int recursive_supervisor =
            (recursive_pde & PG_P_SET) && !(recursive_pde & PG_US_U);

        for (uint_32 idx = 0; idx < sizeof(kernel_source); idx++)
                kernel_source[idx] = (uint_8) (0x80U + idx);

        memset(kernel_dest, 0, sizeof(kernel_dest));
        int single_ok =
            copy_to_user(user + 32, kernel_source, sizeof(kernel_source)) == 0 &&
            copy_from_user(kernel_dest, user + 32, sizeof(kernel_dest)) == 0 &&
            memcmp(kernel_source, kernel_dest, sizeof(kernel_source)) == 0;

        uint_8 *cross = user + PAGE_SIZE - 8;
        memset(kernel_dest, 0, sizeof(kernel_dest));
        int cross_ok =
            copy_to_user(cross, kernel_source, sizeof(kernel_source)) == 0 &&
            copy_from_user(kernel_dest, cross, sizeof(kernel_dest)) == 0 &&
            memcmp(kernel_source, kernel_dest, sizeof(kernel_source)) == 0;

        memset(cross, 0x5a, 8);
        memset(kernel_dest, 0x33, sizeof(kernel_dest));
        uint_32 second_page = (uint_32) user + PAGE_SIZE;
        uint_32 *second_pte = pte_ptr(second_page);
        unsigned long irq_flags;
        local_irq_save(irq_flags);
        uint_32 saved_second_pte = *second_pte;
        *second_pte = 0;
        mm_test_invlpg(second_page);
        int missing_to =
            copy_to_user(cross, kernel_source, sizeof(kernel_source));
        int missing_from =
            copy_from_user(kernel_dest, cross, sizeof(kernel_dest));
        int missing_unchanged = 1;
        for (uint_32 idx = 0; idx < 8; idx++) {
                if (cross[idx] != 0x5a)
                        missing_unchanged = 0;
        }
        for (uint_32 idx = 0; idx < sizeof(kernel_dest); idx++) {
                if (kernel_dest[idx] != 0x33)
                        missing_unchanged = 0;
        }
        *second_pte = saved_second_pte;
        mm_test_invlpg(second_page);
        local_irq_restore(irq_flags);
        int missing_ok = missing_to == -EFAULT && missing_from == -EFAULT &&
                         missing_unchanged;

        uint_32 first_page = (uint_32) user;
        uint_32 *first_pte = pte_ptr(first_page);
        uint_32 *first_pde = pde_ptr(first_page);

        local_irq_save(irq_flags);
        uint_32 saved_first_pte = *first_pte;
        *first_pte = saved_first_pte & ~PG_US_U;
        mm_test_invlpg(first_page);
        int pte_user_ok =
            copy_from_user(kernel_dest, user, 1) == -EFAULT &&
            copy_to_user(user, kernel_source, 1) == -EFAULT;
        *first_pte = saved_first_pte;
        mm_test_invlpg(first_page);
        local_irq_restore(irq_flags);

        local_irq_save(irq_flags);
        uint_32 saved_first_pde = *first_pde;
        *first_pde = saved_first_pde & ~PG_US_U;
        mm_test_invlpg(first_page);
        int pde_user_ok =
            copy_from_user(kernel_dest, user, 1) == -EFAULT &&
            copy_to_user(user, kernel_source, 1) == -EFAULT;
        *first_pde = saved_first_pde;
        mm_test_invlpg(first_page);
        local_irq_restore(irq_flags);

        user[0] = 0x6d;
        kernel_dest[0] = 0;
        local_irq_save(irq_flags);
        saved_first_pte = *first_pte;
        *first_pte = saved_first_pte & ~PG_RW_W;
        mm_test_invlpg(first_page);
        int pte_write_ok =
            copy_to_user(user, kernel_source, 1) == -EFAULT &&
            copy_from_user(kernel_dest, user, 1) == 0 &&
            kernel_dest[0] == 0x6d;
        *first_pte = saved_first_pte;
        mm_test_invlpg(first_page);
        local_irq_restore(irq_flags);

        kernel_dest[0] = 0;
        local_irq_save(irq_flags);
        saved_first_pde = *first_pde;
        *first_pde = saved_first_pde & ~PG_RW_W;
        mm_test_invlpg(first_page);
        int pde_write_ok =
            copy_to_user(user, kernel_source, 1) == -EFAULT &&
            copy_from_user(kernel_dest, user, 1) == 0 &&
            kernel_dest[0] == 0x6d;
        *first_pde = saved_first_pde;
        mm_test_invlpg(first_page);
        local_irq_restore(irq_flags);

        frog_test_case("uaccess.single-page", single_ok);
        frog_test_case("uaccess.cross-page", cross_ok);
        frog_test_case("uaccess.no-partial-copy", missing_ok);
        frog_test_case("uaccess.pte-user", pte_user_ok);
        frog_test_case("uaccess.pde-user", pde_user_ok);
        frog_test_case("uaccess.pte-write", pte_write_ok);
        frog_test_case("uaccess.pde-write", pde_write_ok);
        frog_test_case("paging.user-image", image_user);
        frog_test_case("paging.user-allocator", allocated_user);
        frog_test_case("paging.kernel-supervisor", kernel_supervisor);
        frog_test_case("paging.recursive-supervisor", recursive_supervisor);
        free_page(MP_USER, user, 2);
}
#else
void mm_uaccess_process_regression(void) {}
int_32 mm_vm_process_prepare(void) { return -EOPNOTSUPP; }
int_32 mm_vm_process_verify_cleanup(void) { return -EOPNOTSUPP; }
#endif

#if defined(CONFIG_FROG_TEST_ANONYMOUS_MMAP) || \
    defined(CONFIG_FROG_TEST_USER_ALLOCATOR) || \
    defined(CONFIG_FROG_TEST_DESKTOP_SOAK)
extern struct pool user_pool;

static uint_32 mm_test_pool_frames_used(struct pool *pool)
{
        uint_32 used = 0;
        uint_32 capacity = pool->pool_bitmap.map_bytes_length * 8U;

        for (uint_32 bit = 0; bit < capacity; bit++) {
                if (get_value_bitmap(&pool->pool_bitmap, bit))
                        used++;
        }
        return used;
}
#endif

#ifdef CONFIG_FROG_TEST_DESKTOP_SOAK
static uint_32 desktop_soak_user_frames;
static bool desktop_soak_snapshot_live;

int_32 mm_desktop_soak_snapshot(void)
{
        if (desktop_soak_snapshot_live)
                return -EBUSY;
        desktop_soak_user_frames = mm_test_pool_frames_used(&user_pool);
        desktop_soak_snapshot_live = true;
        return 0;
}

int_32 mm_desktop_soak_verify(void)
{
        bool passed = desktop_soak_snapshot_live &&
            mm_test_pool_frames_used(&user_pool) == desktop_soak_user_frames;

        desktop_soak_snapshot_live = false;
        return passed ? 0 : -EUCLEAN;
}
#endif

#ifdef CONFIG_FROG_TEST_ANONYMOUS_MMAP
static uint_32 anon_mmap_user_frames;
static bool anon_mmap_snapshot_live;

int_32 mm_anon_mmap_test_command(uint_32 command)
{
        if (command == FROG_TEST_ANON_MMAP_SNAPSHOT) {
                if (anon_mmap_snapshot_live)
                        return -EBUSY;
                anon_mmap_user_frames =
                    mm_test_pool_frames_used(&user_pool);
                anon_mmap_snapshot_live = true;
                return 0;
        }
        if (command == FROG_TEST_ANON_MMAP_VERIFY) {
                bool passed = anon_mmap_snapshot_live &&
                    mm_test_pool_frames_used(&user_pool) ==
                        anon_mmap_user_frames;

                anon_mmap_snapshot_live = false;
                return passed ? 0 : -EUCLEAN;
        }
        if (command >= FROG_TEST_ANON_MMAP_FAIL_BASE &&
            command < FROG_TEST_ANON_MMAP_FAIL_BASE +
                          FROG_TEST_ANON_MMAP_FAIL_COUNT) {
                vm_test_fail_map_after(
                    command - FROG_TEST_ANON_MMAP_FAIL_BASE);
                return 0;
        }
        if (command >= FROG_TEST_ANON_ALLOC_FAIL_BASE &&
            command < FROG_TEST_ANON_ALLOC_FAIL_BASE +
                          FROG_TEST_ANON_ALLOC_FAIL_COUNT) {
                vm_test_fail_owned_alloc_after(
                    command - FROG_TEST_ANON_ALLOC_FAIL_BASE);
                return 0;
        }
        if (command >= FROG_TEST_ANON_FORK_FAIL_BASE &&
            command < FROG_TEST_ANON_FORK_FAIL_BASE +
                          FROG_TEST_ANON_FORK_FAIL_COUNT) {
                vm_test_fail_fork_after(
                    command - FROG_TEST_ANON_FORK_FAIL_BASE);
                return 0;
        }
        return -EINVAL;
}
#else
int_32 mm_anon_mmap_test_command(uint_32 command)
{
        (void) command;
        return -EOPNOTSUPP;
}
#endif

#ifdef CONFIG_FROG_TEST_USER_ALLOCATOR
static uint_32 user_allocator_frames;
static bool user_allocator_snapshot_live;

int_32 mm_user_allocator_test_command(uint_32 command)
{
        if (command == FROG_TEST_USER_ALLOC_SNAPSHOT) {
                if (user_allocator_snapshot_live)
                        return -EBUSY;
                user_allocator_frames =
                    mm_test_pool_frames_used(&user_pool);
                user_allocator_snapshot_live = true;
                return 0;
        }
        if (command == FROG_TEST_USER_ALLOC_VERIFY) {
                bool passed = user_allocator_snapshot_live &&
                    mm_test_pool_frames_used(&user_pool) ==
                        user_allocator_frames;

                user_allocator_snapshot_live = false;
                return passed ? 0 : -EUCLEAN;
        }
        if (command == FROG_TEST_USER_ALLOC_FAIL_NEXT) {
                vm_test_fail_owned_alloc_after(0);
                return 0;
        }
        return -EINVAL;
}
#else
int_32 mm_user_allocator_test_command(uint_32 command)
{
        (void) command;
        return -EOPNOTSUPP;
}
#endif

int mm_regression_test(void)
{
        mm_test_failures = 0;
        INFO("[mm-test]: ===== memory regression tests =====");
        mm_block_desc_fork_metadata();
        mm_slab_basic();
        mm_slab_isolation();
        mm_large_alloc_offset();
        mm_large_alloc_writethrough();
        mm_large_alloc_multi();
        mm_slab_reuse();
        mm_supervisor_permissions();
        mm_uaccess_range();
        mm_bootmem_range();
        mm_bootmem_allocator_range();
        mm_pool_bitmap_capacity();
        mm_refcount_lifecycle();
        mm_phys_resource_registry();
        mm_phys_resource_bootmem_snapshot();
        mm_vma_metadata();
        INFO("[mm-test]: ===== done =====");
        return mm_test_failures;
}
