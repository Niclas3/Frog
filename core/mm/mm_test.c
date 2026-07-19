#include <asm/page.h>

#include <frog/errno.h>
#include <frog/irqflags.h>
#include <frog/memory.h>
#include <frog/process.h>
#include <frog/string.h>
#include <frog/types.h>
#include <frog/uaccess.h>
#include <frog/vm.h>
#include <kernel/debug.h>
#include <kernel/mm_test.h>
#include <kernel/qemu_test.h>

#include "./mem_egg.h"
#include "./mm_helper.h"

#define PASS(name)         INFO("[mm-test]: PASS  " name)
static int mm_test_failures;
#define FAIL(name, ...)                                                   \
        do {                                                              \
                mm_test_failures++;                                       \
                WARN("[mm-test]: FAIL  " name, ##__VA_ARGS__);           \
        } while (0)

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
#endif

int mm_regression_test(void)
{
        mm_test_failures = 0;
        INFO("[mm-test]: ===== memory regression tests =====");
        mm_slab_basic();
        mm_slab_isolation();
        mm_large_alloc_offset();
        mm_large_alloc_writethrough();
        mm_large_alloc_multi();
        mm_slab_reuse();
        mm_supervisor_permissions();
        mm_uaccess_range();
        mm_vma_metadata();
        INFO("[mm-test]: ===== done =====");
        return mm_test_failures;
}
