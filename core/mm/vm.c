#include <asm/page.h>

#include <frog/bitmap.h>
#include <frog/errno.h>
#include <frog/math.h>
#include <frog/memory.h>
#include <frog/mman.h>
#include <frog/phys_resource.h>
#include <frog/process.h>
#include <frog/string.h>
#include <frog/threads.h>
#include <frog/vm.h>
#include <kernel/assert.h>
#include <kernel/device.h>
#include <kernel/panic.h>
#include <kernel/vfs.h>

#include "./mm_helper.h"

#define VM_PDE_SIZE          (PAGE_SIZE * 1024U)
#define VM_MAX_PT_RESERVE    5U
#define VM_PT_BATCH_PAGES    64U
#define VM_PTE_CPU_BITS      0x060U
#define VM_USER_PDE_COUNT    768U
#define VM_PAGE_ENTRY_MASK   0x00000fffU

struct vm_pt_reserve {
        uint_32 frames[VM_MAX_PT_RESERVE];
        uint_32 count;
        uint_32 consumed;
};

struct vm_clone_page {
        uint_32 address;
        uint_32 frame;
        uint_32 pte_flags;
};

struct vm_clone_pt {
        uint_32 pde_index;
        uint_32 frame;
        uint_32 pde_flags;
};

struct vm_clone_reserve {
        struct mm_struct *child;
        struct vm_area **vmas;
        struct vm_mapping **private_mappings;
        struct vm_clone_page *pages;
        struct vm_clone_pt *pts;
        void *copy_page;
        uint_32 vma_count;
        uint_32 page_count;
        uint_32 pt_count;
        uint_32 acquired_vmas;
};

extern struct list_head thread_all_list;

static bool vm_page_table_empty(uint_32 address);

#ifdef CONFIG_QEMU_TEST
static int vm_fail_after_ptes = -1;
static int vm_fail_after_owned_frames = -1;
static int vm_fail_after_fork_steps = -1;

void vm_test_fail_map_after(int installed_ptes)
{
        vm_fail_after_ptes = installed_ptes;
}

void vm_test_fail_owned_alloc_after(int allocated_frames)
{
        vm_fail_after_owned_frames = allocated_frames;
}

void vm_test_fail_fork_after(int clone_steps)
{
        vm_fail_after_fork_steps = clone_steps;
}
#endif

static int vm_take_fail_after(void)
{
#ifdef CONFIG_QEMU_TEST
        int fail_after = vm_fail_after_ptes;

        vm_fail_after_ptes = -1;
        return fail_after;
#endif
        return -1;
}

static int vm_take_fork_fail_after(void)
{
#ifdef CONFIG_QEMU_TEST
        int fail_after = vm_fail_after_fork_steps;

        vm_fail_after_fork_steps = -1;
        return fail_after;
#endif
        return -1;
}

static int vm_take_owned_alloc_fail_after(void)
{
#ifdef CONFIG_QEMU_TEST
        int fail_after = vm_fail_after_owned_frames;

        vm_fail_after_owned_frames = -1;
        return fail_after;
#endif
        return -1;
}

static bool vm_area_unlinked(const struct vm_area *vma)
{
        return vma->elem.next == &vma->elem &&
               vma->elem.prev == &vma->elem;
}

static void vm_list_add_before(struct list_head *node,
                               struct list_head *position)
{
        node->next = position;
        node->prev = position->prev;
        position->prev->next = node;
        position->prev = node;
}

static void vm_invlpg(uint_32 address)
{
        __asm__ volatile("invlpg (%0)" : : "r"(address) : "memory");
}

static void vm_reload_cr3(void)
{
        uint_32 cr3;

        __asm__ volatile("movl %%cr3, %0" : "=r"(cr3));
        __asm__ volatile("movl %0, %%cr3" : : "r"(cr3) : "memory");
}

static uint_32 vm_read_cr3(void)
{
        uint_32 cr3;

        __asm__ volatile("movl %%cr3, %0" : "=r"(cr3));
        return cr3 & 0xfffff000U;
}

static void vm_write_cr3(uint_32 cr3)
{
        __asm__ volatile("movl %0, %%cr3" : : "r"(cr3) : "memory");
}

/* The caller holds mm->pt_lock, so interrupts cannot observe the temporary CR3. */
static uint_32 vm_activate_target_locked(struct mm_struct *mm)
{
        uint_32 previous = vm_read_cr3();
        uint_32 target = addr_v2p((uint_32) mm->pgdir) & 0xfffff000U;

        if (previous != target)
                vm_write_cr3(target);
        return previous;
}

static void vm_restore_target_locked(uint_32 previous)
{
        if (vm_read_cr3() != previous)
                vm_write_cr3(previous);
}

static bool vm_current_mm(const struct mm_struct *mm)
{
        TCB_t *current = running_thread();
        uint_32 cr3;

        if (current == NULL || current->mm != mm || mm->pgdir == NULL)
                return false;
        __asm__ volatile("movl %%cr3, %0" : "=r"(cr3));
        return (cr3 & 0xfffff000U) ==
               (addr_v2p((uint_32) mm->pgdir) & 0xfffff000U);
}

static bool vm_owned_builder_mm(const struct mm_struct *mm)
{
        struct list_head *position;
        unsigned long flags;
        bool unpublished = true;

        if (mm == NULL || mm->pgdir == NULL)
                return false;
        local_irq_save(flags);
        list_for_each(position, &thread_all_list) {
                TCB_t *thread = list_entry(position, TCB_t, all_list_tag);

                if (thread->mm == mm) {
                        unpublished = false;
                        break;
                }
        }
        local_irq_restore(flags);
        return unpublished;
}

static int vm_checked_range(uint_32 start,
                            uint_32 length,
                            uint_32 lower,
                            uint_32 upper,
                            uint_32 *end)
{
        unsigned long long range_end;

        if (end == NULL || length == 0 ||
            (start & (PAGE_SIZE - 1U)) != 0 ||
            (length & (PAGE_SIZE - 1U)) != 0)
                return -EINVAL;
        range_end = (unsigned long long) start + length;
        if (start < lower || range_end > upper || range_end > 0xffffffffULL)
                return -EOVERFLOW;
        *end = (uint_32) range_end;
        return 0;
}

static int vm_bitmap_bounds(const struct mm_struct *mm,
                            uint_32 start,
                            uint_32 length,
                            uint_32 *first_bit,
                            uint_32 *page_count)
{
        unsigned long long pages;
        unsigned long long first;
        unsigned long long bit_end;
        unsigned long long bit_capacity;

        if (mm == NULL || first_bit == NULL || page_count == NULL ||
            mm->user_vaddr.vaddr_bitmap.bits == NULL ||
            start < mm->user_vaddr.vaddr_start ||
            (start & (PAGE_SIZE - 1U)) != 0 || length == 0 ||
            (length & (PAGE_SIZE - 1U)) != 0)
                return -EINVAL;
        pages = length / PAGE_SIZE;
        first = (start - mm->user_vaddr.vaddr_start) / PAGE_SIZE;
        bit_end = first + pages;
        bit_capacity =
            (unsigned long long) mm->user_vaddr.vaddr_bitmap.map_bytes_length *
            8U;
        if (pages > 0xffffffffULL || first > 0xffffffffULL ||
            bit_end < first || bit_end > bit_capacity)
                return -EOVERFLOW;
        *first_bit = (uint_32) first;
        *page_count = (uint_32) pages;
        return 0;
}

static bool vm_bitmap_range_free(const struct mm_struct *mm,
                                 uint_32 start,
                                 uint_32 length,
                                 uint_32 *conflict_address)
{
        uint_32 first_bit;
        uint_32 page_count;

        if (vm_bitmap_bounds(mm, start, length, &first_bit, &page_count) != 0)
                return false;
        for (uint_32 page = 0; page < page_count; page++) {
                if (get_value_bitmap(
                        (struct bitmap *) &mm->user_vaddr.vaddr_bitmap,
                        first_bit + page)) {
                        if (conflict_address != NULL)
                                *conflict_address = start + page * PAGE_SIZE;
                        return false;
                }
        }
        return true;
}

static int vm_bitmap_set_range(struct mm_struct *mm,
                               uint_32 start,
                               uint_32 length,
                               uint_8 value)
{
        uint_32 first_bit;
        uint_32 page_count;
        int result = vm_bitmap_bounds(mm, start, length, &first_bit,
                                      &page_count);

        if (result != 0)
                return result;
        for (uint_32 page = 0; page < page_count; page++)
                set_value_bitmap(&mm->user_vaddr.vaddr_bitmap,
                                 first_bit + page, value);
        mm->generation++;
        return 0;
}

static struct vm_area *vm_area_overlap(struct mm_struct *mm,
                                       uint_32 start,
                                       uint_32 end)
{
        struct list_head *position;

        list_for_each(position, &mm->vma_list) {
                struct vm_area *vma =
                    list_entry(position, struct vm_area, elem);

                if (end <= vma->start)
                        return NULL;
                if (start < vma->end)
                        return vma;
        }
        return NULL;
}

static bool vm_pte_range_free(struct mm_struct *mm,
                              uint_32 start,
                              uint_32 length,
                              uint_32 *conflict_address)
{
        uint_32 pages = length / PAGE_SIZE;

        for (uint_32 batch = 0; batch < pages;
             batch += VM_PT_BATCH_PAGES) {
                uint_32 batch_end = batch + VM_PT_BATCH_PAGES;

                if (batch_end > pages)
                        batch_end = pages;
                spin_lock(mm->pt_lock);
                for (uint_32 page = batch; page < batch_end; page++) {
                        uint_32 address = start + page * PAGE_SIZE;
                        uint_32 *pde = pde_ptr(address);

                        if ((*pde & PG_P_SET) &&
                            (*pte_ptr(address) & PG_P_SET)) {
                                if (conflict_address != NULL)
                                        *conflict_address = address;
                                spin_unlock(mm->pt_lock);
                                return false;
                        }
                }
                spin_unlock(mm->pt_lock);
        }
        return true;
}

static int vm_pt_reserve_prepare(struct vm_pt_reserve *reserve,
                                 uint_32 length)
{
        unsigned long long worst_span;
        uint_32 count;

        if (reserve == NULL || length == 0 || length > VM_MAP_MAX_LENGTH ||
            (length & (PAGE_SIZE - 1U)) != 0)
                return -EINVAL;
        memset(reserve, 0, sizeof(*reserve));
        worst_span = (unsigned long long) length + VM_PDE_SIZE - PAGE_SIZE;
        count = (uint_32) ((worst_span + VM_PDE_SIZE - 1U) / VM_PDE_SIZE);
        if (count == 0 || count > VM_MAX_PT_RESERVE)
                return -EOVERFLOW;

        for (uint_32 index = 0; index < count; index++) {
                reserve->frames[index] = alloc_kernel_page_frame();
                if (reserve->frames[index] == 0) {
                        while (index > 0)
                                free_kernel_page_frame(
                                    reserve->frames[--index]);
                        memset(reserve, 0, sizeof(*reserve));
                        return -ENOMEM;
                }
        }
        reserve->count = count;
        return 0;
}

static uint_32 vm_pt_reserve_take(struct vm_pt_reserve *reserve)
{
        if (reserve->consumed == reserve->count)
                return 0;
        return reserve->frames[reserve->consumed++];
}

static void vm_pt_reserve_release_unused(struct vm_pt_reserve *reserve)
{
        while (reserve->count > reserve->consumed)
                free_kernel_page_frame(reserve->frames[--reserve->count]);
}

static void vm_release_frames(uint_32 *frames, uint_32 count)
{
        for (uint_32 index = 0; index < count; index++)
                free_kernel_page_frame(frames[index]);
}

static void vm_rollback_ptes(struct mm_struct *mm,
                             struct vm_area *vma,
                             uint_32 installed,
                             uint_32 *new_pde_indexes,
                             uint_32 new_pde_count,
                             uint_32 *release_frames,
                             uint_32 *release_count)
{
        for (uint_32 batch = 0; batch < installed;
             batch += VM_PT_BATCH_PAGES) {
                uint_32 batch_end = batch + VM_PT_BATCH_PAGES;

                if (batch_end > installed)
                        batch_end = installed;
                spin_lock(mm->pt_lock);
                for (uint_32 page = batch; page < batch_end; page++) {
                        uint_32 address = vma->start + page * PAGE_SIZE;
                        uint_32 *pte = pte_ptr(address);

                        ASSERT(*pte & PG_P_SET);
                        *pte = 0;
                        vm_invlpg(address);
                }
                spin_unlock(mm->pt_lock);
        }
        for (uint_32 index = 0; index < new_pde_count; index++) {
                uint_32 address = new_pde_indexes[index] << 22;
                uint_32 *pde;

                spin_lock(mm->pt_lock);
                pde = pde_ptr(address);
                ASSERT(*pde & PG_P_SET);
                ASSERT(vm_page_table_empty(address));
                release_frames[(*release_count)++] = *pde & 0xfffff000U;
                *pde = 0;
                vm_reload_cr3();
                spin_unlock(mm->pt_lock);
        }
}

static int vm_install_device_ptes(struct mm_struct *mm,
                                  struct vm_area *vma,
                                  uint_32 physical,
                                  struct vm_pt_reserve *reserve,
                                  uint_32 *release_frames,
                                  uint_32 *release_count,
                                  int fail_after)
{
        uint_32 new_pde_indexes[VM_MAX_PT_RESERVE];
        uint_32 new_pde_count = 0;
        uint_32 installed = 0;
        uint_32 pages = (vma->end - vma->start) / PAGE_SIZE;

        for (uint_32 batch = 0; batch < pages;
             batch += VM_PT_BATCH_PAGES) {
                uint_32 batch_end = batch + VM_PT_BATCH_PAGES;

                if (batch_end > pages)
                        batch_end = pages;
                spin_lock(mm->pt_lock);
                for (uint_32 page = batch; page < batch_end; page++) {
                        uint_32 address = vma->start + page * PAGE_SIZE;
                        uint_32 *pde = pde_ptr(address);

                        if (*pde & PG_P_SET) {
                                if (!(*pde & PG_US_U) ||
                                    !(*pde & PG_RW_W)) {
                                        spin_unlock(mm->pt_lock);
                                        return -EACCES;
                                }
                                if (*pte_ptr(address) & PG_P_SET) {
                                        spin_unlock(mm->pt_lock);
                                        return -EEXIST;
                                }
                        }
                }
                spin_unlock(mm->pt_lock);
        }

        for (uint_32 address = vma->start & ~(VM_PDE_SIZE - 1U);
             address < vma->end; address += VM_PDE_SIZE) {
                uint_32 *pde;
                uint_32 frame;

                spin_lock(mm->pt_lock);
                pde = pde_ptr(address);
                if (*pde & PG_P_SET) {
                        spin_unlock(mm->pt_lock);
                        continue;
                }
                frame = vm_pt_reserve_take(reserve);
                if (frame == 0)
                        PANIC("VM page-table reserve was underestimated");
                *pde = frame | PG_P_SET | PG_RW_W | PG_US_U;
                new_pde_indexes[new_pde_count++] = address >> 22;
                vm_reload_cr3();
                memset((void *) ((uint_32) pte_ptr(address) & 0xfffff000U),
                       0, PAGE_SIZE);
                spin_unlock(mm->pt_lock);
        }

        if (fail_after == 0) {
                vm_rollback_ptes(mm, vma, installed, new_pde_indexes,
                                  new_pde_count, release_frames,
                                  release_count);
                return -ENOMEM;
        }
        for (uint_32 batch = 0; batch < pages;
             batch += VM_PT_BATCH_PAGES) {
                uint_32 batch_end = batch + VM_PT_BATCH_PAGES;

                if (batch_end > pages)
                        batch_end = pages;
                spin_lock(mm->pt_lock);
                for (uint_32 page = batch; page < batch_end; page++) {
                        uint_32 address = vma->start + page * PAGE_SIZE;
                        uint_32 expected = (physical + page * PAGE_SIZE) |
                                           vma->mapping->pte_flags;
                        uint_32 *pte = pte_ptr(address);

                        ASSERT(!(*pte & PG_P_SET));
                        *pte = expected;
                        vm_invlpg(address);
                        installed++;
                        if (fail_after == (int) installed) {
                                spin_unlock(mm->pt_lock);
                                vm_rollback_ptes(
                                    mm, vma, installed, new_pde_indexes,
                                    new_pde_count, release_frames,
                                    release_count);
                                return -ENOMEM;
                        }
                }
                spin_unlock(mm->pt_lock);
        }
        return 0;
}

static int vm_install_owned_ptes(struct mm_struct *mm,
                                 struct vm_area *vma,
                                 const uint_32 *frames,
                                 struct vm_pt_reserve *reserve,
                                 uint_32 *release_frames,
                                 uint_32 *release_count,
                                 int fail_after)
{
        uint_32 new_pde_indexes[VM_MAX_PT_RESERVE];
        uint_32 new_pde_count = 0;
        uint_32 installed = 0;
        uint_32 pages = (vma->end - vma->start) / PAGE_SIZE;

        for (uint_32 batch = 0; batch < pages;
             batch += VM_PT_BATCH_PAGES) {
                uint_32 batch_end = batch + VM_PT_BATCH_PAGES;

                if (batch_end > pages)
                        batch_end = pages;
                spin_lock(mm->pt_lock);
                for (uint_32 page = batch; page < batch_end; page++) {
                        uint_32 address = vma->start + page * PAGE_SIZE;
                        uint_32 *pde = pde_ptr(address);

                        if (*pde & PG_P_SET) {
                                if ((*pde & (PG_US_U | PG_RW_W)) !=
                                    (PG_US_U | PG_RW_W)) {
                                        spin_unlock(mm->pt_lock);
                                        return -EACCES;
                                }
                                if (*pte_ptr(address) & PG_P_SET) {
                                        spin_unlock(mm->pt_lock);
                                        return -EEXIST;
                                }
                        }
                }
                spin_unlock(mm->pt_lock);
        }

        for (uint_32 address = vma->start & ~(VM_PDE_SIZE - 1U);
             address < vma->end; address += VM_PDE_SIZE) {
                uint_32 *pde;
                uint_32 frame;

                spin_lock(mm->pt_lock);
                pde = pde_ptr(address);
                if (*pde & PG_P_SET) {
                        spin_unlock(mm->pt_lock);
                        continue;
                }
                frame = vm_pt_reserve_take(reserve);
                if (frame == 0)
                        PANIC("VM page-table reserve was underestimated");
                *pde = frame | PG_P_SET | PG_RW_W | PG_US_U;
                new_pde_indexes[new_pde_count++] = address >> 22;
                vm_reload_cr3();
                memset((void *) ((uint_32) pte_ptr(address) & 0xfffff000U),
                       0, PAGE_SIZE);
                spin_unlock(mm->pt_lock);
        }

        if (fail_after == 0) {
                vm_rollback_ptes(mm, vma, installed, new_pde_indexes,
                                  new_pde_count, release_frames,
                                  release_count);
                return -ENOMEM;
        }
        for (uint_32 batch = 0; batch < pages;
             batch += VM_PT_BATCH_PAGES) {
                uint_32 batch_end = batch + VM_PT_BATCH_PAGES;

                if (batch_end > pages)
                        batch_end = pages;
                spin_lock(mm->pt_lock);
                for (uint_32 page = batch; page < batch_end; page++) {
                        uint_32 address = vma->start + page * PAGE_SIZE;
                        uint_32 *pte = pte_ptr(address);

                        ASSERT(!(*pte & PG_P_SET));
                        *pte = frames[page] | VM_RAM_PTE_FLAGS;
                        vm_invlpg(address);
                        installed++;
                        if (fail_after == (int) installed) {
                                spin_unlock(mm->pt_lock);
                                vm_rollback_ptes(
                                    mm, vma, installed, new_pde_indexes,
                                    new_pde_count, release_frames,
                                    release_count);
                                return -ENOMEM;
                        }
                }
                spin_unlock(mm->pt_lock);
        }
        return 0;
}

static bool vm_page_table_empty(uint_32 address)
{
        uint_32 *table =
            (uint_32 *) ((uint_32) pte_ptr(address) & 0xfffff000U);

        for (uint_32 index = 0; index < 1024U; index++) {
                if (table[index] & PG_P_SET)
                        return false;
        }
        return true;
}

struct mm_struct *mm_create(void)
{
        struct mm_struct *mm = get_kernel_page(1);

        if (mm == NULL)
                return NULL;
        INIT_LIST_HEAD(&mm->vma_list);
        lock_init(&mm->mmap_lock);
        spin_init(mm->pt_lock);
        return mm;
}

void mm_destroy(struct mm_struct *mm)
{
        if (mm == NULL)
                return;
        ASSERT(mm->pgdir == NULL);
        ASSERT(mm->user_vaddr.vaddr_bitmap.bits == NULL);
        ASSERT(mm->user_vaddr.vaddr_bitmap.map_bytes_length == 0);
        ASSERT(list_is_empty(&mm->vma_list));
        free_page(MP_KERNEL, mm, 1);
}

struct vm_mapping *vm_mapping_alloc(enum vm_backing_type backing_type,
                                    const struct vm_operations *vm_ops)
{
        struct vm_mapping *mapping;

        if (backing_type != VM_BACKING_RAM_OWNED &&
            backing_type != VM_BACKING_DEVICE_BORROWED)
                return NULL;
        mapping = kmalloc(sizeof(*mapping));
        if (mapping == NULL)
                return NULL;
        memset(mapping, 0, sizeof(*mapping));
        refcount_init(&mapping->refs, 1);
        mapping->backing_type = backing_type;
        mapping->state = VM_MAPPING_NEW;
        mapping->vm_ops = vm_ops;
        return mapping;
}

bool vm_mapping_get_live(struct vm_mapping *mapping)
{
        return mapping != NULL && mapping->state == VM_MAPPING_PREPARED &&
               refcount_get_live(&mapping->refs);
}

void vm_mapping_put(struct vm_mapping *mapping)
{
        if (mapping == NULL)
                return;
        if (!refcount_put(&mapping->refs))
                return;
        if (mapping->state == VM_MAPPING_PREPARED) {
                mapping->state = VM_MAPPING_CLOSED;
                if (mapping->backing_type ==
                    VM_BACKING_DEVICE_BORROWED) {
                        ASSERT(mapping->vm_ops != NULL &&
                               mapping->vm_ops->close != NULL);
                        mapping->vm_ops->close(mapping);
                } else {
                        ASSERT(mapping->backing_type ==
                                   VM_BACKING_RAM_OWNED &&
                               mapping->vm_ops == NULL &&
                               mapping->file == NULL &&
                               mapping->device == NULL &&
                               mapping->resource == NULL &&
                               mapping->private_data == NULL &&
                               mapping->pte_flags == VM_RAM_PTE_FLAGS);
                }
        } else {
                ASSERT(mapping->state == VM_MAPPING_NEW);
        }
        kfree(mapping);
}

int vm_mapping_prepare_device(struct vm_mapping *mapping,
                              struct file *file,
                              struct device *device,
                              struct phys_resource *resource,
                              void *private_data)
{
        if (mapping == NULL || mapping->backing_type !=
                                   VM_BACKING_DEVICE_BORROWED ||
            mapping->state != VM_MAPPING_NEW ||
            refcount_read(&mapping->refs) != 1 || mapping->file != NULL ||
            mapping->device != NULL || mapping->resource != NULL ||
            mapping->vm_ops == NULL || mapping->vm_ops->close == NULL ||
            file == NULL || device == NULL || resource == NULL ||
            refcount_read(&file->f_refs) == 0 ||
            device->state != DEVICE_LIVE ||
            refcount_read(&device->refs) < 2 ||
            resource->state != PHYS_RESOURCE_REGISTERED ||
            resource->type != PHYS_RESOURCE_MMIO ||
            resource->cache_mode != VM_CACHE_UNCACHED ||
            refcount_read(&resource->refs) < 2)
                return -EINVAL;

        mapping->file = file;
        mapping->device = device;
        mapping->resource = resource;
        mapping->private_data = private_data;
        mapping->pte_flags = VM_DEVICE_PTE_FLAGS;
        mapping->state = VM_MAPPING_PREPARED;
        return 0;
}

int vm_mapping_prepare_owned(struct vm_mapping *mapping)
{
        if (mapping == NULL ||
            mapping->backing_type != VM_BACKING_RAM_OWNED ||
            mapping->state != VM_MAPPING_NEW ||
            refcount_read(&mapping->refs) != 1 ||
            mapping->file != NULL || mapping->device != NULL ||
            mapping->resource != NULL || mapping->vm_ops != NULL ||
            mapping->private_data != NULL || mapping->pte_flags != 0)
                return -EINVAL;

        mapping->pte_flags = VM_RAM_PTE_FLAGS;
        mapping->state = VM_MAPPING_PREPARED;
        return 0;
}

struct vm_area *vm_area_alloc(uint_32 length,
                              uint_32 prot,
                              uint_32 flags,
                              uint_32 page_offset,
                              struct vm_mapping *mapping)
{
        struct vm_area *vma;

        if (mapping == NULL || length == 0 ||
            (length & (PAGE_SIZE - 1U)) != 0 ||
            length > VM_MAP_MAX_LENGTH ||
            (page_offset & (PAGE_SIZE - 1U)) != 0)
                return NULL;
        vma = kmalloc(sizeof(*vma));
        if (vma == NULL)
                return NULL;
        memset(vma, 0, sizeof(*vma));
        INIT_LIST_HEAD(&vma->elem);
        vma->end = length;
        vma->prot = prot;
        vma->flags = flags;
        vma->page_offset = page_offset;
        vma->state = VM_PREPARING;
        vma->mapping = mapping;
        return vma;
}

int vm_area_insert(struct mm_struct *mm, struct vm_area *vma)
{
        struct list_head *position;

        if (mm == NULL || vma == NULL || vma->start >= vma->end ||
            (vma->start & (PAGE_SIZE - 1U)) != 0 ||
            (vma->end & (PAGE_SIZE - 1U)) != 0 ||
            (vma->mm != NULL && vma->mm != mm) ||
            !vm_area_unlinked(vma))
                return -EINVAL;

        list_for_each(position, &mm->vma_list) {
                struct vm_area *current =
                    list_entry(position, struct vm_area, elem);

                if (vma->end <= current->start)
                        break;
                if (vma->start < current->end)
                        return -EEXIST;
        }

        vma->mm = mm;
        vm_list_add_before(&vma->elem, position);
        mm->generation++;
        return 0;
}

struct vm_area *vm_area_find(struct mm_struct *mm, uint_32 address)
{
        struct list_head *position;

        if (mm == NULL)
                return NULL;
        list_for_each(position, &mm->vma_list) {
                struct vm_area *vma =
                    list_entry(position, struct vm_area, elem);

                if (address < vma->start)
                        return NULL;
                if (address < vma->end)
                        return vma;
        }
        return NULL;
}

struct vm_area *vm_area_find_exact(struct mm_struct *mm,
                                   uint_32 start,
                                   uint_32 end)
{
        struct vm_area *vma = vm_area_find(mm, start);

        if (vma != NULL && vma->start == start && vma->end == end)
                return vma;
        return NULL;
}

bool vm_mm_maps_device(struct mm_struct *mm, const struct device *device)
{
        struct list_head *position;
        bool found = false;

        if (mm == NULL || device == NULL)
                return false;
        lock_fetch(&mm->mmap_lock);
        list_for_each(position, &mm->vma_list) {
                struct vm_area *vma =
                    list_entry(position, struct vm_area, elem);
                struct vm_mapping *mapping = vma->mapping;

                if ((vma->state == VM_ACTIVE ||
                     vma->state == VM_PREPARING) &&
                    mapping != NULL &&
                    mapping->state == VM_MAPPING_PREPARED &&
                    mapping->device == device) {
                        found = true;
                        break;
                }
        }
        lock_release(&mm->mmap_lock);
        return found;
}

struct vm_area *vm_area_remove_exact(struct mm_struct *mm,
                                     uint_32 start,
                                     uint_32 end)
{
        struct vm_area *vma = vm_area_find_exact(mm, start, end);

        if (vma == NULL)
                return NULL;
        list_del_init(&vma->elem);
        vma->mm = NULL;
        mm->generation++;
        return vma;
}

uint_32 vm_find_unmapped_area(struct mm_struct *mm, uint_32 length)
{
        uint_32 candidate = VM_MMAP_START;
        uint_32 ignored_end;
        uint_32 ignored_first;
        uint_32 ignored_pages;

        if (mm == NULL || !vm_current_mm(mm) ||
            vm_checked_range(candidate, length, VM_MMAP_START, VM_MMAP_END,
                             &ignored_end) != 0 ||
            vm_bitmap_bounds(mm, candidate, length, &ignored_first,
                             &ignored_pages) != 0 ||
            length > VM_MAP_MAX_LENGTH)
                return 0;

        while ((unsigned long long) candidate + length <= VM_MMAP_END) {
                uint_32 end = candidate + length;
                uint_32 conflict = 0;
                struct vm_area *overlap = vm_area_overlap(mm, candidate, end);

                if (overlap != NULL) {
                        candidate = overlap->end;
                        continue;
                }
                if (!vm_bitmap_range_free(mm, candidate, length, &conflict)) {
                        if (conflict < candidate)
                                return 0;
                        candidate = conflict + PAGE_SIZE;
                        continue;
                }
                if (!vm_pte_range_free(mm, candidate, length, &conflict)) {
                        candidate = conflict + PAGE_SIZE;
                        continue;
                }
                return candidate;
        }
        return 0;
}

int vm_map_pfn_range(struct mm_struct *mm,
                     struct vm_area *vma,
                     uint_32 *mapped_start)
{
        struct vm_pt_reserve reserve;
        struct vm_mapping *mapping;
        uint_32 release_frames[VM_MAX_PT_RESERVE];
        uint_32 release_count = 0;
        uint_32 original_length;
        uint_32 physical;
        uint_32 chosen;
        unsigned long long physical_start;
        int fail_after = vm_take_fail_after();
        int result;

        if (mapped_start == NULL)
                return -EINVAL;
        *mapped_start = 0;
        if (mm == NULL || vma == NULL || !vm_current_mm(mm) ||
            vma->mm != NULL || !vm_area_unlinked(vma) ||
            vma->state != VM_PREPARING || vma->start != 0 ||
            vma->end == 0 || vma->end > VM_MAP_MAX_LENGTH ||
            (vma->end & (PAGE_SIZE - 1U)) != 0 || vma->mapping == NULL)
                return -EINVAL;
        mapping = vma->mapping;
        if (mapping->backing_type != VM_BACKING_DEVICE_BORROWED ||
            mapping->state != VM_MAPPING_PREPARED ||
            mapping->vm_ops == NULL || mapping->vm_ops->close == NULL ||
            mapping->file == NULL || mapping->device == NULL ||
            mapping->resource == NULL ||
            mapping->resource->state != PHYS_RESOURCE_REGISTERED ||
            mapping->resource->type != PHYS_RESOURCE_MMIO ||
            mapping->resource->cache_mode != VM_CACHE_UNCACHED ||
            refcount_read(&mapping->resource->refs) < 2 ||
            mapping->pte_flags != VM_DEVICE_PTE_FLAGS)
                return -EINVAL;

        original_length = vma->end;
        physical_start = mapping->resource->start + vma->page_offset;
        if (physical_start < mapping->resource->start ||
            physical_start > 0xffffffffULL ||
            physical_start > 0x100000000ULL - original_length ||
            (physical_start & (PAGE_SIZE - 1U)) != 0 ||
            !phys_resource_contains(mapping->resource, physical_start,
                                    original_length))
                return -EOVERFLOW;
        physical = (uint_32) physical_start;

        result = vm_pt_reserve_prepare(&reserve, original_length);
        if (result != 0)
                return result;

        lock_fetch(&mm->mmap_lock);
        chosen = vm_find_unmapped_area(mm, original_length);
        if (chosen == 0) {
                result = -ENOMEM;
                goto unlock;
        }
        result = vm_bitmap_set_range(mm, chosen, original_length, 1);
        if (result != 0)
                goto unlock;
        vma->start = chosen;
        vma->end = chosen + original_length;
        result = vm_area_insert(mm, vma);
        if (result != 0)
                goto release_bitmap;

        result = vm_install_device_ptes(mm, vma, physical, &reserve,
                                        release_frames, &release_count,
                                        fail_after);
        if (result == 0) {
                spin_lock(mm->pt_lock);
                vma->state = VM_ACTIVE;
                mm->generation++;
                spin_unlock(mm->pt_lock);
        }
        if (result != 0) {
                ASSERT(vm_area_remove_exact(mm, vma->start, vma->end) == vma);
                goto release_bitmap;
        }

        *mapped_start = chosen;
        lock_release(&mm->mmap_lock);
        vm_pt_reserve_release_unused(&reserve);
        vm_release_frames(release_frames, release_count);
        return 0;

release_bitmap:
        ASSERT(vm_bitmap_set_range(mm, chosen, original_length, 0) == 0);
        vma->start = 0;
        vma->end = original_length;
        vma->state = VM_PREPARING;
unlock:
        lock_release(&mm->mmap_lock);
        vm_pt_reserve_release_unused(&reserve);
        vm_release_frames(release_frames, release_count);
        return result;
}

int vm_map_owned_range(struct mm_struct *mm,
                       struct vm_area *vma,
                       uint_32 *mapped_start)
{
        struct vm_pt_reserve reserve;
        struct vm_mapping *mapping;
        uint_32 release_frames[VM_MAX_PT_RESERVE];
        uint_32 release_count = 0;
        uint_32 original_length;
        uint_32 *frames = NULL;
        uint_32 frame_count = 0;
        uint_32 pages;
        uint_32 chosen;
        int fail_after = vm_take_fail_after();
        int alloc_fail_after;
        int result;

        if (mapped_start == NULL)
                return -EINVAL;
        *mapped_start = 0;
        if (mm == NULL || vma == NULL || !vm_current_mm(mm) ||
            vma->mm != NULL || !vm_area_unlinked(vma) ||
            vma->state != VM_PREPARING || vma->start != 0 ||
            vma->end == 0 || vma->end > VM_MAP_MAX_LENGTH ||
            (vma->end & (PAGE_SIZE - 1U)) != 0 ||
            vma->prot != (PROT_READ | PROT_WRITE) ||
            vma->flags != (MAP_PRIVATE | MAP_ANONYMOUS) ||
            vma->page_offset != 0 || vma->mapping == NULL)
                return -EINVAL;
        mapping = vma->mapping;
        if (mapping->backing_type != VM_BACKING_RAM_OWNED ||
            mapping->state != VM_MAPPING_PREPARED ||
            mapping->vm_ops != NULL || mapping->file != NULL ||
            mapping->device != NULL || mapping->resource != NULL ||
            mapping->private_data != NULL ||
            mapping->pte_flags != VM_RAM_PTE_FLAGS)
                return -EINVAL;

        alloc_fail_after = vm_take_owned_alloc_fail_after();
        original_length = vma->end;
        pages = original_length / PAGE_SIZE;
        if (pages > 0xffffffffU / sizeof(*frames))
                return -EOVERFLOW;
        frames = kmalloc(pages * sizeof(*frames));
        if (frames == NULL)
                return -ENOMEM;
        memset(frames, 0, pages * sizeof(*frames));
        for (; frame_count < pages; frame_count++) {
                if (alloc_fail_after == (int) frame_count) {
                        result = -ENOMEM;
                        goto release_user_frames;
                }
                frames[frame_count] = alloc_user_page_frame();
                if (frames[frame_count] == 0) {
                        result = -ENOMEM;
                        goto release_user_frames;
                }
        }
        if (alloc_fail_after == (int) frame_count) {
                result = -ENOMEM;
                goto release_user_frames;
        }
        result = vm_pt_reserve_prepare(&reserve, original_length);
        if (result != 0)
                goto release_user_frames;

        lock_fetch(&mm->mmap_lock);
        chosen = vm_find_unmapped_area(mm, original_length);
        if (chosen == 0) {
                result = -ENOMEM;
                goto unlock;
        }
        result = vm_bitmap_set_range(mm, chosen, original_length, 1);
        if (result != 0)
                goto unlock;
        vma->start = chosen;
        vma->end = chosen + original_length;
        result = vm_area_insert(mm, vma);
        if (result != 0)
                goto release_bitmap;

        result = vm_install_owned_ptes(mm, vma, frames, &reserve,
                                       release_frames, &release_count,
                                       fail_after);
        if (result == 0) {
                memset((void *) chosen, 0, original_length);
                spin_lock(mm->pt_lock);
                vma->state = VM_ACTIVE;
                mm->generation++;
                spin_unlock(mm->pt_lock);
        } else {
                ASSERT(vm_area_remove_exact(mm, vma->start,
                                            vma->end) == vma);
                goto release_bitmap;
        }

        *mapped_start = chosen;
        lock_release(&mm->mmap_lock);
        vm_pt_reserve_release_unused(&reserve);
        vm_release_frames(release_frames, release_count);
        frame_count = 0;
        kfree(frames);
        return 0;

release_bitmap:
        ASSERT(vm_bitmap_set_range(mm, chosen, original_length, 0) == 0);
        vma->start = 0;
        vma->end = original_length;
        vma->state = VM_PREPARING;
unlock:
        lock_release(&mm->mmap_lock);
        vm_pt_reserve_release_unused(&reserve);
        vm_release_frames(release_frames, release_count);
release_user_frames:
        for (uint_32 index = 0; index < frame_count; index++)
                free_user_page_frame(frames[index]);
        kfree(frames);
        return result;
}

/*
 * The caller holds mmap_lock. RAM teardown may temporarily drop it only after
 * publishing VM_UNMAPPING, so user frames can be returned outside both VM
 * locks while the VMA continues to reserve and identify the range.
 */
static void vm_detach_active_vma_locked(
    struct mm_struct *mm,
    struct vm_area *vma,
    uint_32 *release_frames,
    uint_32 *release_count)
{
        uint_32 start = vma->start;
        uint_32 end = vma->end;
        uint_32 length = end - start;
        uint_32 pages = length / PAGE_SIZE;
        struct vm_mapping *mapping = vma->mapping;
        bool owned;
        int result;

        ASSERT(mm != NULL && mm->pgdir != NULL && vma != NULL &&
               vma->mm == mm && vma->state == VM_ACTIVE &&
               mapping != NULL &&
               release_frames != NULL && release_count != NULL);
        owned = mapping->backing_type == VM_BACKING_RAM_OWNED;
        ASSERT((owned && mapping->state == VM_MAPPING_PREPARED &&
                mapping->vm_ops == NULL && mapping->file == NULL &&
                mapping->device == NULL && mapping->resource == NULL &&
                mapping->private_data == NULL &&
                mapping->pte_flags == VM_RAM_PTE_FLAGS) ||
               (!owned && mapping->backing_type ==
                              VM_BACKING_DEVICE_BORROWED &&
                mapping->state == VM_MAPPING_PREPARED &&
                mapping->resource != NULL &&
                mapping->pte_flags == VM_DEVICE_PTE_FLAGS));

        for (uint_32 batch = 0; batch < pages;
             batch += VM_PT_BATCH_PAGES) {
                uint_32 batch_end = batch + VM_PT_BATCH_PAGES;
                uint_32 previous;

                if (batch_end > pages)
                        batch_end = pages;
                spin_lock(mm->pt_lock);
                previous = vm_activate_target_locked(mm);
                for (uint_32 page = batch; page < batch_end; page++) {
                        uint_32 address = start + page * PAGE_SIZE;
                        uint_32 *pde = pde_ptr(address);
                        uint_32 *pte = pte_ptr(address);
                        uint_32 actual = *pte & ~VM_PTE_CPU_BITS;

                        if (!(*pde & PG_P_SET))
                                PANIC("active VMA page-table entry changed");
                        if (owned) {
                                uint_32 flags = actual &
                                    VM_PAGE_ENTRY_MASK;

                                if ((actual & 0xfffff000U) == 0 ||
                                    flags != VM_RAM_PTE_FLAGS)
                                        PANIC("active RAM VMA entry changed");
                        } else {
                                uint_32 expected =
                                    ((uint_32) mapping->resource->start +
                                     vma->page_offset + page * PAGE_SIZE) |
                                    mapping->pte_flags;

                                if (actual != expected)
                                        PANIC("active device VMA entry changed");
                        }
                }
                vm_restore_target_locked(previous);
                spin_unlock(mm->pt_lock);
        }

        spin_lock(mm->pt_lock);
        vma->state = VM_UNMAPPING;
        mm->generation++;
        spin_unlock(mm->pt_lock);

        for (uint_32 batch = 0; batch < pages;
             batch += VM_PT_BATCH_PAGES) {
                uint_32 batch_end = batch + VM_PT_BATCH_PAGES;
                uint_32 owned_frames[VM_PT_BATCH_PAGES];
                uint_32 owned_count = 0;
                uint_32 previous;

                if (batch_end > pages)
                        batch_end = pages;
                spin_lock(mm->pt_lock);
                previous = vm_activate_target_locked(mm);
                for (uint_32 page = batch; page < batch_end; page++) {
                        uint_32 address = start + page * PAGE_SIZE;
                        uint_32 *pte = pte_ptr(address);

                        if (owned)
                                owned_frames[owned_count++] =
                                    *pte & 0xfffff000U;
                        *pte = 0;
                        vm_invlpg(address);
                }
                vm_restore_target_locked(previous);
                spin_unlock(mm->pt_lock);
                if (owned_count != 0) {
                        lock_release(&mm->mmap_lock);
                        for (uint_32 index = 0; index < owned_count;
                             index++)
                                free_user_page_frame(owned_frames[index]);
                        lock_fetch(&mm->mmap_lock);
                        ASSERT(vma->mm == mm &&
                               vma->state == VM_UNMAPPING &&
                               vma->mapping == mapping);
                }
        }

        for (uint_32 address = start & ~(VM_PDE_SIZE - 1U);
             address < end; address += VM_PDE_SIZE) {
                uint_32 *pde;
                uint_32 previous;

                spin_lock(mm->pt_lock);
                previous = vm_activate_target_locked(mm);
                pde = pde_ptr(address);
                if ((*pde & PG_P_SET) && vm_page_table_empty(address)) {
                        if (*release_count == VM_MAX_PT_RESERVE)
                                PANIC("VM page-table release list overflow");
                        release_frames[(*release_count)++] =
                            *pde & 0xfffff000U;
                        *pde = 0;
                        vm_reload_cr3();
                }
                vm_restore_target_locked(previous);
                spin_unlock(mm->pt_lock);
        }

        ASSERT(vm_area_remove_exact(mm, start, end) == vma);
        result = vm_bitmap_set_range(mm, start, length, 0);
        ASSERT(result == 0);
}

int vm_unmap_exact(struct mm_struct *mm, uint_32 start, uint_32 length)
{
        uint_32 release_frames[VM_MAX_PT_RESERVE];
        uint_32 release_count = 0;
        uint_32 end;
        struct vm_area *vma;
        struct vm_mapping *mapping;

        if (mm == NULL || !vm_current_mm(mm) ||
            vm_checked_range(start, length, VM_MMAP_START, VM_MMAP_END,
                             &end) != 0 ||
            length > VM_MAP_MAX_LENGTH)
                return -EINVAL;

        lock_fetch(&mm->mmap_lock);
        vma = vm_area_find_exact(mm, start, end);
        if (vma == NULL || vma->state != VM_ACTIVE) {
                lock_release(&mm->mmap_lock);
                return -EINVAL;
        }
        vm_detach_active_vma_locked(mm, vma, release_frames,
                                    &release_count);
        mapping = vma->mapping;
        lock_release(&mm->mmap_lock);

        vm_release_frames(release_frames, release_count);
        kfree(vma);
        vm_mapping_put(mapping);
        return 0;
}

static int vm_clone_count_locked(struct mm_struct *parent,
                                 uint_32 *vma_count,
                                 uint_32 *page_count,
                                 uint_32 *pt_count)
{
        struct list_head *position;
        uint_32 pages = 0;
        uint_32 vmas = 0;
        uint_32 pts = 0;
        uint_32 bit_capacity;

        if (parent == NULL || parent->pgdir == NULL || vma_count == NULL ||
            page_count == NULL || pt_count == NULL ||
            parent->user_vaddr.vaddr_bitmap.bits == NULL ||
            parent->user_vaddr.vaddr_bitmap.map_bytes_length == 0)
                return -EINVAL;

        list_for_each(position, &parent->vma_list) {
                struct vm_area *vma =
                    list_entry(position, struct vm_area, elem);

                uint_32 vma_pages =
                    (vma->end - vma->start) / PAGE_SIZE;

                if (vma->state != VM_ACTIVE || vma->mapping == NULL ||
                    vma->mapping->state != VM_MAPPING_PREPARED)
                        return -EINVAL;
                if (vma->mapping->backing_type == VM_BACKING_RAM_OWNED) {
                        if (vma->mapping->vm_ops != NULL ||
                            vma->mapping->file != NULL ||
                            vma->mapping->device != NULL ||
                            vma->mapping->resource != NULL ||
                            vma->mapping->private_data != NULL ||
                            vma->mapping->pte_flags != VM_RAM_PTE_FLAGS ||
                            vma_pages > 0xffffffffU - pages)
                                return -EINVAL;
                        pages += vma_pages;
                } else if (vma->mapping->backing_type !=
                           VM_BACKING_DEVICE_BORROWED) {
                        return -EINVAL;
                }
                vmas++;
        }

        spin_lock(parent->pt_lock);
        for (uint_32 pde_index = 0; pde_index < VM_USER_PDE_COUNT;
             pde_index++) {
                if (parent->pgdir[pde_index] & PG_P_SET)
                        pts++;
        }
        spin_unlock(parent->pt_lock);

        bit_capacity =
            parent->user_vaddr.vaddr_bitmap.map_bytes_length * 8U;
        for (uint_32 bit = 0; bit < bit_capacity; bit++) {
                uint_32 address;
                uint_32 *pde;

                if (!get_value_bitmap(&parent->user_vaddr.vaddr_bitmap, bit))
                        continue;
                address = parent->user_vaddr.vaddr_start + bit * PAGE_SIZE;
                if (address < parent->user_vaddr.vaddr_start ||
                    address >= 0xc0000000U)
                        break;
                if (vm_area_find(parent, address) != NULL)
                        continue;

                spin_lock(parent->pt_lock);
                pde = pde_ptr(address);
                if ((*pde & PG_P_SET) &&
                    (*pte_ptr(address) & PG_P_SET)) {
                        if (pages == 0xffffffffU) {
                                spin_unlock(parent->pt_lock);
                                return -EOVERFLOW;
                        }
                        pages++;
                }
                spin_unlock(parent->pt_lock);
        }

        *vma_count = vmas;
        *page_count = pages;
        *pt_count = pts;
        return 0;
}

static void vm_clone_release_aux(struct vm_clone_reserve *reserve)
{
        if (reserve->copy_page != NULL)
                free_page(MP_KERNEL, reserve->copy_page, 1);
        for (uint_32 index = 0;
             reserve->private_mappings != NULL &&
             index < reserve->vma_count; index++)
                vm_mapping_put(reserve->private_mappings[index]);
        kfree(reserve->vmas);
        kfree(reserve->private_mappings);
        kfree(reserve->pages);
        kfree(reserve->pts);
        reserve->copy_page = NULL;
        reserve->vmas = NULL;
        reserve->private_mappings = NULL;
        reserve->pages = NULL;
        reserve->pts = NULL;
}

static void vm_clone_discard(struct vm_clone_reserve *reserve)
{
        struct mm_struct *child = reserve->child;

        if (child != NULL) {
                for (uint_32 index = 0;
                     reserve->vmas != NULL &&
                     index < reserve->acquired_vmas; index++) {
                        struct vm_area *vma = reserve->vmas[index];

                        if (vma == NULL)
                                continue;
                        if (vma->mm == child) {
                                list_del_init(&vma->elem);
                                vma->mm = NULL;
                        }
                        vm_mapping_put(vma->mapping);
                        vma->mapping = NULL;
                }
                for (uint_32 index = 0;
                     reserve->vmas != NULL && index < reserve->vma_count;
                     index++) {
                        kfree(reserve->vmas[index]);
                        reserve->vmas[index] = NULL;
                }

                if (child->pgdir != NULL) {
                        for (uint_32 index = 0; index < VM_USER_PDE_COUNT;
                             index++)
                                child->pgdir[index] = 0;
                }
        }

        for (uint_32 index = 0;
             reserve->pages != NULL && index < reserve->page_count; index++) {
                if (reserve->pages[index].frame != 0) {
                        free_user_page_frame(reserve->pages[index].frame);
                        reserve->pages[index].frame = 0;
                }
        }
        for (uint_32 index = 0;
             reserve->pts != NULL && index < reserve->pt_count; index++) {
                if (reserve->pts[index].frame != 0) {
                        free_kernel_page_frame(reserve->pts[index].frame);
                        reserve->pts[index].frame = 0;
                }
        }

        if (child != NULL) {
                uint_8 *bits = child->user_vaddr.vaddr_bitmap.bits;
                uint_32 bytes =
                    child->user_vaddr.vaddr_bitmap.map_bytes_length;

                if (bits != NULL) {
                        free_page(MP_KERNEL, bits,
                                  DIV_ROUND_UP(bytes, PAGE_SIZE));
                        child->user_vaddr.vaddr_bitmap.bits = NULL;
                        child->user_vaddr.vaddr_bitmap.map_bytes_length = 0;
                }
                if (child->pgdir != NULL) {
                        free_page(MP_KERNEL, child->pgdir, 1);
                        child->pgdir = NULL;
                }
                mm_destroy(child);
                reserve->child = NULL;
        }
        vm_clone_release_aux(reserve);
}

static int vm_clone_reserve_prepare(struct mm_struct *parent,
                                    uint_32 vma_count,
                                    uint_32 page_count,
                                    uint_32 pt_count,
                                    struct vm_clone_reserve *reserve)
{
        uint_32 bitmap_bytes =
            parent->user_vaddr.vaddr_bitmap.map_bytes_length;

        memset(reserve, 0, sizeof(*reserve));
        reserve->vma_count = vma_count;
        reserve->page_count = page_count;
        reserve->pt_count = pt_count;
        reserve->child = mm_create();
        if (reserve->child == NULL)
                goto fail;
        reserve->child->user_vaddr.vaddr_start =
            parent->user_vaddr.vaddr_start;
        reserve->child->user_vaddr.vaddr_bitmap.bits =
            get_kernel_page(DIV_ROUND_UP(bitmap_bytes, PAGE_SIZE));
        if (reserve->child->user_vaddr.vaddr_bitmap.bits == NULL)
                goto fail;
        reserve->child->user_vaddr.vaddr_bitmap.map_bytes_length =
            bitmap_bytes;
        reserve->child->pgdir = create_page_dir();
        if (reserve->child->pgdir == NULL)
                goto fail;

        if (vma_count != 0) {
                if (vma_count > 0xffffffffU / sizeof(*reserve->vmas))
                        goto fail;
                reserve->vmas = kmalloc(vma_count * sizeof(*reserve->vmas));
                if (reserve->vmas == NULL)
                        goto fail;
                memset(reserve->vmas, 0,
                       vma_count * sizeof(*reserve->vmas));
                reserve->private_mappings =
                    kmalloc(vma_count * sizeof(*reserve->private_mappings));
                if (reserve->private_mappings == NULL)
                        goto fail;
                memset(reserve->private_mappings, 0,
                       vma_count * sizeof(*reserve->private_mappings));
                for (uint_32 index = 0; index < vma_count; index++) {
                        reserve->vmas[index] = kmalloc(sizeof(struct vm_area));
                        if (reserve->vmas[index] == NULL)
                                goto fail;
                        memset(reserve->vmas[index], 0,
                               sizeof(struct vm_area));
                        INIT_LIST_HEAD(&reserve->vmas[index]->elem);
                        reserve->private_mappings[index] =
                            vm_mapping_alloc(VM_BACKING_RAM_OWNED, NULL);
                        if (reserve->private_mappings[index] == NULL)
                                goto fail;
                }
        }
        if (page_count != 0) {
                if (page_count > 0xffffffffU / sizeof(*reserve->pages))
                        goto fail;
                reserve->pages =
                    kmalloc(page_count * sizeof(*reserve->pages));
                if (reserve->pages == NULL)
                        goto fail;
                memset(reserve->pages, 0,
                       page_count * sizeof(*reserve->pages));
                reserve->copy_page = get_kernel_page(1);
                if (reserve->copy_page == NULL)
                        goto fail;
                for (uint_32 index = 0; index < page_count; index++) {
                        reserve->pages[index].frame = alloc_user_page_frame();
                        if (reserve->pages[index].frame == 0)
                                goto fail;
                }
        }
        if (pt_count != 0) {
                if (pt_count > 0xffffffffU / sizeof(*reserve->pts))
                        goto fail;
                reserve->pts = kmalloc(pt_count * sizeof(*reserve->pts));
                if (reserve->pts == NULL)
                        goto fail;
                memset(reserve->pts, 0, pt_count * sizeof(*reserve->pts));
                for (uint_32 index = 0; index < pt_count; index++) {
                        reserve->pts[index].frame =
                            alloc_kernel_page_frame();
                        if (reserve->pts[index].frame == 0)
                                goto fail;
                }
        }
        return 0;

fail:
        vm_clone_discard(reserve);
        return -ENOMEM;
}

static bool vm_clone_should_fail(int fail_after, uint_32 *step)
{
        if (fail_after >= 0 && (int) *step == fail_after)
                return true;
        (*step)++;
        return false;
}

static int vm_clone_install_pt(struct vm_clone_reserve *reserve,
                               uint_32 index)
{
        struct mm_struct *child = reserve->child;
        struct vm_clone_pt *pt = &reserve->pts[index];
        uint_32 address = pt->pde_index << 22;
        uint_32 previous;

        if (child->pgdir[pt->pde_index] & PG_P_SET)
                return -EEXIST;
        child->pgdir[pt->pde_index] = pt->frame | pt->pde_flags;
        spin_lock(child->pt_lock);
        previous = vm_activate_target_locked(child);
        memset((void *) ((uint_32) pte_ptr(address) & 0xfffff000U),
               0, PAGE_SIZE);
        vm_restore_target_locked(previous);
        spin_unlock(child->pt_lock);
        return 0;
}

static int vm_clone_install_pte(struct mm_struct *child,
                                uint_32 address,
                                uint_32 entry,
                                const void *source)
{
        uint_32 previous;
        uint_32 *pde;
        uint_32 *pte;
        int result = 0;

        spin_lock(child->pt_lock);
        previous = vm_activate_target_locked(child);
        pde = pde_ptr(address);
        pte = pte_ptr(address);
        if (!(*pde & PG_P_SET) || (*pte & PG_P_SET)) {
                result = -EEXIST;
        } else {
                *pte = entry;
                vm_invlpg(address);
                if (source != NULL)
                        memcpy((void *) address, source, PAGE_SIZE);
        }
        vm_restore_target_locked(previous);
        spin_unlock(child->pt_lock);
        return result;
}

static int vm_clone_commit_locked(struct mm_struct *parent,
                                  struct vm_clone_reserve *reserve,
                                  unsigned long long generation,
                                  int fail_after)
{
        struct mm_struct *child = reserve->child;
        struct list_head *position;
        uint_32 vma_index = 0;
        uint_32 page_index = 0;
        uint_32 pt_index = 0;
        uint_32 step = 0;
        uint_32 bit_capacity;

        if (parent->generation != generation)
                return -EAGAIN;

        memcpy(child->user_vaddr.vaddr_bitmap.bits,
               parent->user_vaddr.vaddr_bitmap.bits,
               parent->user_vaddr.vaddr_bitmap.map_bytes_length);

        list_for_each(position, &parent->vma_list) {
                struct vm_area *source =
                    list_entry(position, struct vm_area, elem);
                struct vm_area *clone;

                if (vma_index == reserve->vma_count ||
                    source->state != VM_ACTIVE || source->mapping == NULL ||
                    source->mapping->state != VM_MAPPING_PREPARED ||
                    vm_clone_should_fail(fail_after, &step))
                        return -ENOMEM;

                clone = reserve->vmas[vma_index];
                clone->start = source->start;
                clone->end = source->end;
                clone->prot = source->prot;
                clone->flags = source->flags;
                clone->page_offset = source->page_offset;
                clone->state = VM_ACTIVE;
                if (source->mapping->backing_type ==
                    VM_BACKING_DEVICE_BORROWED) {
                        if (!vm_mapping_get_live(source->mapping))
                                return -ENOMEM;
                        clone->mapping = source->mapping;
                } else if (source->mapping->backing_type ==
                           VM_BACKING_RAM_OWNED) {
                        clone->mapping =
                            reserve->private_mappings[vma_index];
                        if (vm_mapping_prepare_owned(clone->mapping) != 0)
                                return -EINVAL;
                        reserve->private_mappings[vma_index] = NULL;
                } else {
                        return -EINVAL;
                }
                if (vm_area_insert(child, clone) != 0) {
                        vm_mapping_put(clone->mapping);
                        clone->mapping = NULL;
                        return -EINVAL;
                }
                reserve->acquired_vmas++;
                vma_index++;
        }
        if (vma_index != reserve->vma_count)
                return -EAGAIN;

        spin_lock(parent->pt_lock);
        for (uint_32 pde_index = 0; pde_index < VM_USER_PDE_COUNT;
             pde_index++) {
                uint_32 pde = parent->pgdir[pde_index];

                if (!(pde & PG_P_SET))
                        continue;
                if (pt_index == reserve->pt_count) {
                        spin_unlock(parent->pt_lock);
                        return -EAGAIN;
                }
                reserve->pts[pt_index].pde_index = pde_index;
                reserve->pts[pt_index].pde_flags =
                    (pde & VM_PAGE_ENTRY_MASK & ~VM_PTE_CPU_BITS) |
                    PG_P_SET | PG_RW_W | PG_US_U;
                pt_index++;
        }
        spin_unlock(parent->pt_lock);
        if (pt_index != reserve->pt_count)
                return -EAGAIN;

        bit_capacity =
            parent->user_vaddr.vaddr_bitmap.map_bytes_length * 8U;
        for (uint_32 bit = 0; bit < bit_capacity; bit++) {
                uint_32 address;
                uint_32 pte_value = 0;

                if (!get_value_bitmap(&parent->user_vaddr.vaddr_bitmap, bit))
                        continue;
                address = parent->user_vaddr.vaddr_start + bit * PAGE_SIZE;
                if (address < parent->user_vaddr.vaddr_start ||
                    address >= 0xc0000000U)
                        break;
                struct vm_area *vma = vm_area_find(parent, address);

                if (vma != NULL && vma->mapping->backing_type ==
                                       VM_BACKING_DEVICE_BORROWED)
                        continue;

                spin_lock(parent->pt_lock);
                if ((*pde_ptr(address) & PG_P_SET) &&
                    (*pte_ptr(address) & PG_P_SET))
                        pte_value = *pte_ptr(address);
                spin_unlock(parent->pt_lock);
                if (!(pte_value & PG_P_SET))
                        continue;
                if (page_index == reserve->page_count)
                        return -EAGAIN;
                reserve->pages[page_index].address = address;
                reserve->pages[page_index].pte_flags =
                    pte_value & VM_PAGE_ENTRY_MASK & ~VM_PTE_CPU_BITS;
                page_index++;
        }
        if (page_index != reserve->page_count)
                return -EAGAIN;

        for (uint_32 index = 0; index < reserve->pt_count; index++) {
                if (vm_clone_install_pt(reserve, index) != 0)
                        return -EEXIST;
        }

        for (uint_32 index = 0; index < reserve->vma_count; index++) {
                struct vm_area *clone = reserve->vmas[index];
                struct vm_area *source = vm_area_find_exact(
                    parent, clone->start, clone->end);
                uint_32 pages = (clone->end - clone->start) / PAGE_SIZE;

                if (source == NULL || source->state != VM_ACTIVE ||
                    source->mapping == NULL || clone->mapping == NULL)
                        return -EAGAIN;
                if (source->mapping->backing_type ==
                    VM_BACKING_RAM_OWNED) {
                        if (clone->mapping->backing_type !=
                            VM_BACKING_RAM_OWNED)
                                return -EAGAIN;
                        continue;
                }
                if (source->mapping != clone->mapping ||
                    source->mapping->backing_type !=
                        VM_BACKING_DEVICE_BORROWED)
                        return -EAGAIN;
                for (uint_32 page = 0; page < pages; page++) {
                        uint_32 address = clone->start + page * PAGE_SIZE;
                        uint_32 expected =
                            ((uint_32) clone->mapping->resource->start +
                             clone->page_offset + page * PAGE_SIZE) |
                            clone->mapping->pte_flags;
                        uint_32 actual;

                        spin_lock(parent->pt_lock);
                        actual = *pte_ptr(address);
                        spin_unlock(parent->pt_lock);
                        if ((actual & ~VM_PTE_CPU_BITS) != expected)
                                return -EUCLEAN;
                        if (vm_clone_should_fail(fail_after, &step))
                                return -ENOMEM;
                        if (vm_clone_install_pte(child, address, expected,
                                                 NULL) != 0)
                                return -EEXIST;
                }
        }

        for (uint_32 index = 0; index < reserve->page_count; index++) {
                struct vm_clone_page *page = &reserve->pages[index];

                memcpy(reserve->copy_page, (void *) page->address, PAGE_SIZE);
                if (vm_clone_should_fail(fail_after, &step))
                        return -ENOMEM;
                if (vm_clone_install_pte(
                        child, page->address,
                        page->frame | page->pte_flags,
                        reserve->copy_page) != 0)
                        return -EEXIST;
        }

        child->generation = generation;
        return 0;
}

struct mm_struct *mm_clone_for_fork(struct mm_struct *parent)
{
        int fail_after = vm_take_fork_fail_after();

        if (!vm_current_mm(parent))
                return NULL;

        for (uint_32 attempt = 0; attempt < 3U; attempt++) {
                struct vm_clone_reserve reserve;
                unsigned long long generation;
                uint_32 vma_count;
                uint_32 page_count;
                uint_32 pt_count;
                int result;

                lock_fetch(&parent->mmap_lock);
                result = vm_clone_count_locked(parent, &vma_count,
                                               &page_count, &pt_count);
                generation = parent->generation;
                lock_release(&parent->mmap_lock);
                if (result != 0)
                        return NULL;

                if (vm_clone_reserve_prepare(parent, vma_count, page_count,
                                             pt_count, &reserve) != 0)
                        return NULL;

                lock_fetch(&parent->mmap_lock);
                result = vm_clone_commit_locked(parent, &reserve, generation,
                                                fail_after);
                lock_release(&parent->mmap_lock);
                if (result == 0) {
                        struct mm_struct *child = reserve.child;

                        for (uint_32 index = 0; index < reserve.vma_count;
                             index++)
                                reserve.vmas[index] = NULL;
                        for (uint_32 index = 0; index < reserve.page_count;
                             index++)
                                reserve.pages[index].frame = 0;
                        for (uint_32 index = 0; index < reserve.pt_count;
                             index++)
                                reserve.pts[index].frame = 0;
                        reserve.child = NULL;
                        vm_clone_release_aux(&reserve);
                        return child;
                }

                vm_clone_discard(&reserve);
                if (result != -EAGAIN)
                        return NULL;
        }
        return NULL;
}

static void vm_release_all_mappings(struct mm_struct *mm)
{
        while (!list_is_empty(&mm->vma_list)) {
                uint_32 release_frames[VM_MAX_PT_RESERVE];
                uint_32 release_count = 0;
                struct vm_area *vma;
                struct vm_mapping *mapping;

                lock_fetch(&mm->mmap_lock);
                vma = list_entry(mm->vma_list.next,
                                 struct vm_area, elem);
                if (vma->state != VM_ACTIVE)
                        PANIC("teardown found a transitional VMA");
                vm_detach_active_vma_locked(mm, vma, release_frames,
                                            &release_count);
                mapping = vma->mapping;
                lock_release(&mm->mmap_lock);

                vm_release_frames(release_frames, release_count);
                kfree(vma);
                vm_mapping_put(mapping);
        }
}

static void vm_release_legacy_pages(struct mm_struct *mm)
{
        uint_32 target = addr_v2p((uint_32) mm->pgdir) & 0xfffff000U;

        for (uint_32 pde_index = 0; pde_index < VM_USER_PDE_COUNT;
             pde_index++) {
                uint_32 pde = mm->pgdir[pde_index];

                if (!(pde & PG_P_SET))
                        continue;
                for (uint_32 batch = 0; batch < 1024U;
                     batch += VM_PT_BATCH_PAGES) {
                        uint_32 frames[VM_PT_BATCH_PAGES];
                        uint_32 frame_count = 0;
                        uint_32 previous;

                        spin_lock(mm->pt_lock);
                        previous = vm_activate_target_locked(mm);
                        for (uint_32 index = batch;
                             index < batch + VM_PT_BATCH_PAGES; index++) {
                                uint_32 address =
                                    (pde_index << 22) + index * PAGE_SIZE;
                                uint_32 *pte = pte_ptr(address);

                                if (!(*pte & PG_P_SET))
                                        continue;
                                if (!(*pte & PG_US_U))
                                        PANIC("user page table owns supervisor page");
                                frames[frame_count++] = *pte & 0xfffff000U;
                                *pte = 0;
                                vm_invlpg(address);
                        }
                        vm_restore_target_locked(previous);
                        spin_unlock(mm->pt_lock);

                        for (uint_32 index = 0; index < frame_count; index++)
                                free_user_page_frame(frames[index]);
                }

                spin_lock(mm->pt_lock);
                uint_32 previous = vm_activate_target_locked(mm);
                ASSERT(vm_page_table_empty(pde_index << 22));
                mm->pgdir[pde_index] = 0;
                vm_reload_cr3();
                vm_restore_target_locked(previous);
                spin_unlock(mm->pt_lock);
                free_kernel_page_frame(pde & 0xfffff000U);
        }

        if (vm_read_cr3() == target) {
                unsigned long flags;

                local_irq_save(flags);
                vm_write_cr3(0x00100000U);
                local_irq_restore(flags);
        }
}

void mm_release_address_space(struct mm_struct *mm)
{
        uint_8 *bits;
        uint_32 bytes;

        if (mm == NULL)
                return;
        if (mm->pgdir != NULL) {
                vm_release_all_mappings(mm);
                vm_release_legacy_pages(mm);
        } else if (!list_is_empty(&mm->vma_list)) {
                PANIC("VMA list exists without a page directory");
        }

        bits = mm->user_vaddr.vaddr_bitmap.bits;
        bytes = mm->user_vaddr.vaddr_bitmap.map_bytes_length;
        if (bits != NULL) {
                free_page(MP_KERNEL, bits,
                          DIV_ROUND_UP(bytes, PAGE_SIZE));
                mm->user_vaddr.vaddr_bitmap.bits = NULL;
                mm->user_vaddr.vaddr_bitmap.map_bytes_length = 0;
        }
        if (mm->pgdir != NULL) {
                free_page(MP_KERNEL, mm->pgdir, 1);
                mm->pgdir = NULL;
        }
        mm_destroy(mm);
}

int vm_user_map_owned_page(struct mm_struct *mm, uint_32 address)
{
        uint_32 user_frame;
        uint_32 pt_frame;
        uint_32 previous;
        uint_32 *pde;
        uint_32 *pte;
        bool installed_pde = false;
        int result;

        if (!vm_owned_builder_mm(mm))
                return -EBUSY;
        if (mm == NULL || mm->pgdir == NULL ||
            address < USER_VADDR_START || address >= 0xc0000000U ||
            (address & (PAGE_SIZE - 1U)) != 0)
                return -EINVAL;

        user_frame = alloc_user_page_frame();
        if (user_frame == 0)
                return -ENOMEM;
        pt_frame = alloc_kernel_page_frame();
        if (pt_frame == 0) {
                free_user_page_frame(user_frame);
                return -ENOMEM;
        }

        lock_fetch(&mm->mmap_lock);
        if (!vm_bitmap_range_free(mm, address, PAGE_SIZE, NULL) ||
            vm_area_find(mm, address) != NULL) {
                result = -EEXIST;
                goto unlock;
        }
        result = vm_bitmap_set_range(mm, address, PAGE_SIZE, 1);
        if (result != 0)
                goto unlock;

        spin_lock(mm->pt_lock);
        previous = vm_activate_target_locked(mm);
        pde = pde_ptr(address);
        if (!(*pde & PG_P_SET)) {
                *pde = pt_frame | PG_P_SET | PG_RW_W | PG_US_U;
                pt_frame = 0;
                installed_pde = true;
                vm_reload_cr3();
                memset((void *) ((uint_32) pte_ptr(address) & 0xfffff000U),
                       0, PAGE_SIZE);
        } else if ((*pde & (PG_US_U | PG_RW_W)) !=
                   (PG_US_U | PG_RW_W)) {
                result = -EACCES;
                goto restore;
        }

        pte = pte_ptr(address);
        if (*pte & PG_P_SET) {
                result = -EEXIST;
                goto restore;
        }
        *pte = user_frame | PG_P_SET | PG_RW_W | PG_US_U;
        user_frame = 0;
        vm_invlpg(address);
        memset((void *) address, 0, PAGE_SIZE);
        mm->generation++;
        result = 0;

restore:
        if (result != 0 && installed_pde) {
                ASSERT(vm_page_table_empty(address));
                pt_frame = *pde & 0xfffff000U;
                *pde = 0;
                vm_reload_cr3();
        }
        vm_restore_target_locked(previous);
        spin_unlock(mm->pt_lock);
        if (result != 0)
                ASSERT(vm_bitmap_set_range(mm, address, PAGE_SIZE, 0) == 0);

unlock:
        lock_release(&mm->mmap_lock);
        if (user_frame != 0)
                free_user_page_frame(user_frame);
        if (pt_frame != 0)
                free_kernel_page_frame(pt_frame);
        return result;
}

int vm_user_write_owned(struct mm_struct *mm,
                        uint_32 address,
                        const void *source,
                        uint_32 length)
{
        unsigned long long end;
        const uint_8 *input = source;
        int result = 0;

        if (!vm_owned_builder_mm(mm))
                return -EBUSY;
        if (length == 0)
                return 0;
        end = (unsigned long long) address + length;
        if (mm == NULL || mm->pgdir == NULL || source == NULL ||
            address < USER_VADDR_START || end > 0xc0000000ULL)
                return -EINVAL;

        lock_fetch(&mm->mmap_lock);
        while (length != 0) {
                uint_32 page = address & ~(PAGE_SIZE - 1U);
                uint_32 offset = address & (PAGE_SIZE - 1U);
                uint_32 chunk = PAGE_SIZE - offset;
                uint_32 first_bit;
                uint_32 page_count;
                uint_32 previous;
                uint_32 pde;
                uint_32 pte;

                if (chunk > length)
                        chunk = length;
                if (vm_bitmap_bounds(mm, page, PAGE_SIZE, &first_bit,
                                     &page_count) != 0 || page_count != 1 ||
                    !get_value_bitmap(&mm->user_vaddr.vaddr_bitmap,
                                      first_bit) ||
                    vm_area_find(mm, page) != NULL) {
                        result = -EFAULT;
                        break;
                }

                spin_lock(mm->pt_lock);
                previous = vm_activate_target_locked(mm);
                pde = *pde_ptr(page);
                pte = *pte_ptr(page);
                if ((pde & (PG_P_SET | PG_RW_W | PG_US_U)) !=
                        (PG_P_SET | PG_RW_W | PG_US_U) ||
                    (pte & (PG_P_SET | PG_RW_W | PG_US_U)) !=
                        (PG_P_SET | PG_RW_W | PG_US_U)) {
                        result = -EACCES;
                } else {
                        memcpy((void *) address, input, chunk);
                }
                vm_restore_target_locked(previous);
                spin_unlock(mm->pt_lock);
                if (result != 0)
                        break;

                address += chunk;
                input += chunk;
                length -= chunk;
        }
        lock_release(&mm->mmap_lock);
        return result;
}

int vm_user_protect_owned(struct mm_struct *mm,
                          uint_32 start,
                          uint_32 length,
                          bool writable)
{
        uint_32 end;
        int result;

        if (!vm_owned_builder_mm(mm))
                return -EBUSY;
        if (mm == NULL || mm->pgdir == NULL ||
            vm_checked_range(start, length, USER_VADDR_START,
                             0xc0000000U, &end) != 0)
                return -EINVAL;

        lock_fetch(&mm->mmap_lock);
        for (uint_32 address = start; address < end;
             address += PAGE_SIZE) {
                uint_32 first_bit;
                uint_32 page_count;
                uint_32 previous;
                uint_32 pde;
                uint_32 pte;

                if (vm_bitmap_bounds(mm, address, PAGE_SIZE, &first_bit,
                                     &page_count) != 0 || page_count != 1 ||
                    !get_value_bitmap(&mm->user_vaddr.vaddr_bitmap,
                                      first_bit) ||
                    vm_area_find(mm, address) != NULL) {
                        result = -EFAULT;
                        goto unlock;
                }
                spin_lock(mm->pt_lock);
                previous = vm_activate_target_locked(mm);
                pde = *pde_ptr(address);
                pte = *pte_ptr(address);
                vm_restore_target_locked(previous);
                spin_unlock(mm->pt_lock);
                if ((pde & (PG_P_SET | PG_US_U)) !=
                        (PG_P_SET | PG_US_U) ||
                    (pte & (PG_P_SET | PG_US_U)) !=
                        (PG_P_SET | PG_US_U)) {
                        result = -EFAULT;
                        goto unlock;
                }
        }

        for (uint_32 address = start; address < end;
             address += PAGE_SIZE) {
                uint_32 previous;
                uint_32 *pte;

                spin_lock(mm->pt_lock);
                previous = vm_activate_target_locked(mm);
                pte = pte_ptr(address);
                if (writable)
                        *pte |= PG_RW_W;
                else
                        *pte &= ~PG_RW_W;
                vm_invlpg(address);
                vm_restore_target_locked(previous);
                spin_unlock(mm->pt_lock);
        }
        mm->generation++;
        result = 0;

unlock:
        lock_release(&mm->mmap_lock);
        return result;
}

static bool vm_user_fault_reserved_locked(struct mm_struct *mm,
                                          uint_32 address,
                                          bool *missing_pde)
{
        uint_32 first_bit;
        uint_32 page_count;
        uint_32 previous;
        uint_32 *pde;
        bool reserved;

        if (vm_bitmap_bounds(mm, address, PAGE_SIZE, &first_bit,
                             &page_count) != 0 || page_count != 1 ||
            !get_value_bitmap(&mm->user_vaddr.vaddr_bitmap, first_bit) ||
            vm_area_find(mm, address) != NULL)
                return false;

        spin_lock(mm->pt_lock);
        previous = vm_activate_target_locked(mm);
        pde = pde_ptr(address);
        *missing_pde = !(*pde & PG_P_SET);
        reserved = *missing_pde || !(*pte_ptr(address) & PG_P_SET);
        vm_restore_target_locked(previous);
        spin_unlock(mm->pt_lock);
        return reserved;
}

int vm_handle_user_page_fault(struct mm_struct *mm, uint_32 address)
{
        uint_32 page_address = address & ~(PAGE_SIZE - 1U);
        uint_32 user_frame = 0;
        uint_32 pt_frame = 0;
        unsigned long long generation;
        bool missing_pde;
        int result = -EFAULT;

        if (mm == NULL || !vm_current_mm(mm) ||
            page_address < mm->user_vaddr.vaddr_start ||
            page_address >= 0xc0000000U)
                return -EFAULT;

        lock_fetch(&mm->mmap_lock);
        if (!vm_user_fault_reserved_locked(mm, page_address,
                                           &missing_pde)) {
                lock_release(&mm->mmap_lock);
                return -EFAULT;
        }
        generation = mm->generation;
        lock_release(&mm->mmap_lock);

        user_frame = alloc_user_page_frame();
        if (user_frame == 0)
                return -ENOMEM;
        if (missing_pde) {
                pt_frame = alloc_kernel_page_frame();
                if (pt_frame == 0) {
                        free_user_page_frame(user_frame);
                        return -ENOMEM;
                }
        }

        lock_fetch(&mm->mmap_lock);
        if (mm->generation != generation ||
            !vm_user_fault_reserved_locked(mm, page_address,
                                           &missing_pde))
                goto unlock;

        spin_lock(mm->pt_lock);
        uint_32 *pde = pde_ptr(page_address);
        if (!(*pde & PG_P_SET)) {
                if (pt_frame == 0) {
                        spin_unlock(mm->pt_lock);
                        goto unlock;
                }
                *pde = pt_frame | PG_P_SET | PG_RW_W | PG_US_U;
                pt_frame = 0;
                vm_reload_cr3();
                memset((void *) ((uint_32) pte_ptr(page_address) &
                                  0xfffff000U),
                       0, PAGE_SIZE);
        }
        uint_32 *pte = pte_ptr(page_address);
        if (*pte & PG_P_SET) {
                spin_unlock(mm->pt_lock);
                goto unlock;
        }
        *pte = user_frame | PG_P_SET | PG_RW_W | PG_US_U;
        user_frame = 0;
        vm_invlpg(page_address);
        mm->generation++;
        spin_unlock(mm->pt_lock);
        memset((void *) page_address, 0, PAGE_SIZE);
        result = 0;

unlock:
        lock_release(&mm->mmap_lock);
        if (user_frame != 0)
                free_user_page_frame(user_frame);
        if (pt_frame != 0)
                free_kernel_page_frame(pt_frame);
        return result;
}
