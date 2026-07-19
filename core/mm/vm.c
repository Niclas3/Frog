#include <asm/page.h>

#include <frog/bitmap.h>
#include <frog/errno.h>
#include <frog/memory.h>
#include <frog/phys_resource.h>
#include <frog/process.h>
#include <frog/string.h>
#include <frog/threads.h>
#include <frog/vm.h>
#include <kernel/assert.h>
#include <kernel/device.h>
#include <kernel/panic.h>

#include "./mm_helper.h"

#define VM_PDE_SIZE          (PAGE_SIZE * 1024U)
#define VM_MAX_PT_RESERVE    5U
#define VM_PT_BATCH_PAGES    64U
#define VM_PTE_CPU_BITS      0x060U

struct vm_pt_reserve {
        uint_32 frames[VM_MAX_PT_RESERVE];
        uint_32 count;
        uint_32 consumed;
};

static bool vm_page_table_empty(uint_32 address);

#ifdef CONFIG_QEMU_TEST
static int vm_fail_after_ptes = -1;

void vm_test_fail_map_after(int installed_ptes)
{
        vm_fail_after_ptes = installed_ptes;
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
                ASSERT(mapping->vm_ops != NULL &&
                       mapping->vm_ops->close != NULL);
                mapping->state = VM_MAPPING_CLOSED;
                mapping->vm_ops->close(mapping);
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

int vm_unmap_exact(struct mm_struct *mm, uint_32 start, uint_32 length)
{
        uint_32 release_frames[VM_MAX_PT_RESERVE];
        uint_32 release_count = 0;
        uint_32 end;
        struct vm_area *vma;
        struct vm_mapping *mapping;
        int result;

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
                        uint_32 *pte = pte_ptr(address);
                        uint_32 expected =
                            ((uint_32) vma->mapping->resource->start +
                             vma->page_offset + page * PAGE_SIZE) |
                            vma->mapping->pte_flags;

                        if (!(*pde & PG_P_SET) ||
                            (*pte & ~VM_PTE_CPU_BITS) != expected)
                                PANIC("active VMA page-table entry changed");
                }
                spin_unlock(mm->pt_lock);
        }

        spin_lock(mm->pt_lock);
        vma->state = VM_UNMAPPING;
        mm->generation++;
        spin_unlock(mm->pt_lock);

        for (uint_32 batch = 0; batch < pages;
             batch += VM_PT_BATCH_PAGES) {
                uint_32 batch_end = batch + VM_PT_BATCH_PAGES;

                if (batch_end > pages)
                        batch_end = pages;
                spin_lock(mm->pt_lock);
                for (uint_32 page = batch; page < batch_end; page++) {
                        uint_32 address = start + page * PAGE_SIZE;
                        uint_32 *pte = pte_ptr(address);

                        *pte = 0;
                        vm_invlpg(address);
                }
                spin_unlock(mm->pt_lock);
        }

        for (uint_32 address = start & ~(VM_PDE_SIZE - 1U);
             address < end; address += VM_PDE_SIZE) {
                uint_32 *pde;

                spin_lock(mm->pt_lock);
                pde = pde_ptr(address);
                if ((*pde & PG_P_SET) && vm_page_table_empty(address)) {
                        if (release_count == VM_MAX_PT_RESERVE)
                                PANIC("VM page-table release list overflow");
                        release_frames[release_count++] =
                            *pde & 0xfffff000U;
                        *pde = 0;
                        vm_reload_cr3();
                }
                spin_unlock(mm->pt_lock);
        }

        ASSERT(vm_area_remove_exact(mm, start, end) == vma);
        result = vm_bitmap_set_range(mm, start, length, 0);
        ASSERT(result == 0);
        mapping = vma->mapping;
        lock_release(&mm->mmap_lock);

        vm_release_frames(release_frames, release_count);
        kfree(vma);
        vm_mapping_put(mapping);
        return 0;
}
