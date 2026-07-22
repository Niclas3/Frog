#ifndef _FROG_VM_H
#define _FROG_VM_H

#include <frog/list.h>
#include <frog/memory.h>
#include <frog/refcount.h>
#include <frog/semaphore.h>
#include <frog/spinlock.h>
#include <frog/types.h>

#define VM_MMAP_START      0x40000000U
#define VM_MMAP_END        0x80000000U
#define VM_MAP_MAX_LENGTH  (16U * 1024U * 1024U)
#define VM_DEVICE_PTE_FLAGS \
        (PG_P_SET | PG_RW_W | PG_US_U | PG_PWT | PG_PCD)

struct device;
struct file;
struct phys_resource;
struct vm_mapping;

enum vm_area_state {
        VM_PREPARING,
        VM_ACTIVE,
        VM_UNMAPPING,
};

enum vm_backing_type {
        VM_BACKING_RAM_OWNED = 1,
        VM_BACKING_DEVICE_BORROWED,
};

enum vm_mapping_state {
        VM_MAPPING_NEW = 0,
        VM_MAPPING_PREPARED,
        VM_MAPPING_CLOSED,
};

struct vm_operations {
        void (*close)(struct vm_mapping *mapping);
};

struct vm_mapping {
        refcount_t refs;
        enum vm_backing_type backing_type;
        enum vm_mapping_state state;
        struct file *file;
        struct device *device;
        struct phys_resource *resource;
        const struct vm_operations *vm_ops;
        void *private_data;
        uint_32 pte_flags;
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
struct mm_struct *mm_clone_for_fork(struct mm_struct *parent);
void mm_release_address_space(struct mm_struct *mm);
int vm_handle_user_page_fault(struct mm_struct *mm, uint_32 address);

/*
 * Image builders use these on an unpublished mm. Each mapped page owns its
 * RAM frame, and mm_release_address_space() reclaims it on rollback.
 */
int vm_user_map_owned_page(struct mm_struct *mm, uint_32 address);
int vm_user_write_owned(struct mm_struct *mm,
                        uint_32 address,
                        const void *source,
                        uint_32 length);
int vm_user_protect_owned(struct mm_struct *mm,
                          uint_32 start,
                          uint_32 length,
                          bool writable);

struct vm_mapping *vm_mapping_alloc(enum vm_backing_type backing_type,
                                    const struct vm_operations *vm_ops);
bool vm_mapping_get_live(struct vm_mapping *mapping);
void vm_mapping_put(struct vm_mapping *mapping);
/* On success mapping owns the caller's file, device and resource references. */
int vm_mapping_prepare_device(struct vm_mapping *mapping,
                              struct file *file,
                              struct device *device,
                              struct phys_resource *resource,
                              void *private_data);
struct vm_area *vm_area_alloc(uint_32 length,
                              uint_32 prot,
                              uint_32 flags,
                              uint_32 page_offset,
                              struct vm_mapping *mapping);

/* The caller serializes metadata-only helpers with mm->mmap_lock. */
int vm_area_insert(struct mm_struct *mm, struct vm_area *vma);
struct vm_area *vm_area_find(struct mm_struct *mm, uint_32 address);
struct vm_area *vm_area_find_exact(struct mm_struct *mm,
                                   uint_32 start,
                                   uint_32 end);
/* Takes mmap_lock internally and includes inherited mappings after fork. */
bool vm_mm_maps_device(struct mm_struct *mm, const struct device *device);
struct vm_area *vm_area_remove_exact(struct mm_struct *mm,
                                     uint_32 start,
                                     uint_32 end);

/*
 * On success vm_map_pfn_range() transfers vma and its mapping reference to mm.
 * On failure ownership remains with the caller. vm_unmap_exact() consumes and
 * frees the matching VMA and drops its mapping reference after releasing locks.
 * Both operations currently require mm to be the active, single-CPU address
 * space; SMP remains disabled until cross-CPU TLB shootdown exists.
 */
int vm_map_pfn_range(struct mm_struct *mm,
                     struct vm_area *vma,
                     uint_32 *mapped_start);
int vm_unmap_exact(struct mm_struct *mm, uint_32 start, uint_32 length);

/* The caller holds mm->mmap_lock. Zero means no fitting range. */
uint_32 vm_find_unmapped_area(struct mm_struct *mm, uint_32 length);

#ifdef CONFIG_QEMU_TEST
void vm_test_fail_map_after(int installed_ptes);
void vm_test_fail_fork_after(int clone_steps);
#endif

#endif
