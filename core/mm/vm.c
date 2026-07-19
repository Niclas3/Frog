#include <asm/page.h>

#include <frog/errno.h>
#include <frog/memory.h>
#include <frog/vm.h>
#include <kernel/assert.h>

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

int vm_area_insert(struct mm_struct *mm, struct vm_area *vma)
{
        struct list_head *position;

        if (mm == NULL || vma == NULL || vma->start >= vma->end ||
            (vma->start & (PAGE_SIZE - 1)) != 0 ||
            (vma->end & (PAGE_SIZE - 1)) != 0 ||
            (vma->mm != NULL && vma->mm != mm) ||
            !vm_area_unlinked(vma))
                return -EINVAL;

        list_for_each (position, &mm->vma_list) {
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
        list_for_each (position, &mm->vma_list) {
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
