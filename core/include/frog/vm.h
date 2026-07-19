#ifndef _FROG_VM_H
#define _FROG_VM_H

#include <frog/list.h>
#include <frog/memory.h>
#include <frog/semaphore.h>
#include <frog/spinlock.h>
#include <frog/types.h>

struct vm_mapping;

enum vm_area_state {
        VM_PREPARING,
        VM_ACTIVE,
        VM_UNMAPPING,
};

struct mm_struct {
        uint_32 *pgdir;
        virtual_addr user_vaddr;
        struct list_head vma_list;
        unsigned long long generation;
        struct lock mmap_lock;
        spinlock_t pt_lock;
};

struct vm_area {
        struct list_head elem;
        struct mm_struct *mm;
        uint_32 start;
        uint_32 end;
        uint_32 prot;
        uint_32 flags;
        uint_32 page_offset;
        enum vm_area_state state;
        struct vm_mapping *mapping;
};

struct mm_struct *mm_create(void);
void mm_destroy(struct mm_struct *mm);

/* The caller serializes these helpers with mm->mmap_lock. */
int vm_area_insert(struct mm_struct *mm, struct vm_area *vma);
struct vm_area *vm_area_find(struct mm_struct *mm, uint_32 address);
struct vm_area *vm_area_find_exact(struct mm_struct *mm,
                                   uint_32 start,
                                   uint_32 end);
struct vm_area *vm_area_remove_exact(struct mm_struct *mm,
                                     uint_32 start,
                                     uint_32 end);

#endif
