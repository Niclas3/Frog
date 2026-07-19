#include <frog/irqflags.h>
#include <frog/math.h>
#include <frog/string.h>
#include <frog/process.h>
#include <frog/threads.h>
#include <frog/vm.h>

#include <const.h>
#include <kernel/assert.h>

#include <asm/processor-flags.h> // FOR EFLAGS
#include <asm/tss.h>
#include <asm/descriptor.h>
#include <asm/page.h>
#include "../../mm/mm_helper.h"



extern struct list_head thread_ready_list;
extern struct list_head thread_all_list;


/* #define CELLING(X, STEP) (((X) + (STEP) -1) / (STEP)) */

// Use this function jmp code from ring0 to ring3
extern void intr_exit(void);


/*
 * Control flow from switch() to this function like kernel_thread() in thread.h
 * this function makes a stack for process under ring3
 * jmp intr_exit can help us from ring0 to ring3
 *
 * !! Code can not jump from ring0 to ring3 it only allows to jump ring3 to
 * ring0, but we can use asm 'ret' to pertend that code returns from ring0 to
 * ring3
 * */
static void start_process(void *filename)
{
        void *function = filename;
        TCB_t *cur = running_thread();
        // To the bottom of context_register
        cur->self_kstack = (uint_32 *) ((uint_32) cur->self_kstack +
                                        sizeof(struct thread_stack));
        struct context_registers *proc_stack =
            (struct context_registers *) cur->self_kstack;
        proc_stack->edi = proc_stack->esi = proc_stack->ebp = proc_stack->esp =
            0;
        proc_stack->eax = proc_stack->ebx = proc_stack->ecx = proc_stack->edx =
            0;
        proc_stack->gs = 0;
        proc_stack->ds = proc_stack->es = proc_stack->fs =
            CREATE_SELECTOR(SEL_IDX_DATA_DPL_3, TI_GDT, RPL3);
        proc_stack->cs = CREATE_SELECTOR(SEL_IDX_CODE_DPL_3, TI_GDT, RPL3);
        proc_stack->eip = function;
        proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_IF_1 | EFLAGS_RESERVED);
        proc_stack->esp_ptr = (void *) (USER_STACK3_VADDR + PAGE_SIZE);
        proc_stack->ss = CREATE_SELECTOR(SEL_IDX_DATA_DPL_3, TI_GDT, RPL3);
        __asm__ volatile(
            "movl %0, %%esp;\
        jmp intr_exit" ::"g"(proc_stack)
            : "memory");
}

static void start_process_ring1(void *filename)
{
        void *function = filename;
        TCB_t *cur = running_thread();
        cur->self_kstack +=
            sizeof(struct thread_stack);  // To the bottom of context_register
        struct context_registers *proc_stack =
            (struct context_registers *) cur->self_kstack;
        proc_stack->edi = proc_stack->esi = proc_stack->ebp = proc_stack->esp =
            0;
        proc_stack->eax = proc_stack->ebx = proc_stack->ecx = proc_stack->edx =
            0;
        proc_stack->gs = 0;
        proc_stack->ds = proc_stack->es = proc_stack->fs =
            CREATE_SELECTOR(SEL_IDX_DATA_DPL_1, TI_GDT, RPL1);
        proc_stack->cs = CREATE_SELECTOR(SEL_IDX_CODE_DPL_1, TI_GDT, RPL1);
        proc_stack->eip = function;
        proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_IF_1 | EFLAGS_RESERVED);

        // stack top
        proc_stack->esp_ptr = (void *) (USER_STACK3_VADDR + PAGE_SIZE);
        proc_stack->ss = CREATE_SELECTOR(SEL_IDX_DATA_DPL_1, TI_GDT, RPL1);
        __asm__ volatile(
            "movl %0, %%esp;\
                      jmp intr_exit" ::"g"(proc_stack)
            : "memory");
}

/*
 * Change current process page dir to new physical address
 * If current thread does not have an mm use default page dir aka
 * kernel page dir address (0x100000)
 */
void page_dir_activate(TCB_t *thread)
{
        uint_32 pagedir_phy_addr = 0x100000;  // default pagedir address 
        if (thread != NULL && thread->mm != NULL &&
            thread->mm->pgdir != NULL) {
                pagedir_phy_addr = addr_v2p((uint_32) thread->mm->pgdir);
        }
        __asm__ volatile("movl %0, %%cr3;"
                         :
                         : "r"(pagedir_phy_addr)
                         : "memory");
}

void process_activate(TCB_t *thread)
{
        ASSERT(thread != NULL);

        page_dir_activate(thread);
        if (thread->mm != NULL) {
                update_tss_esp0(thread);
        }
}

uint_32 *create_page_dir(void)
{
        uint_32 *page_dir_vaddr = get_kernel_page(1);
        if (page_dir_vaddr == NULL) {
                return NULL;
        }
        // 1024 = 4 * 256;
        memcpy((uint_32 *) ((uint_32) page_dir_vaddr + 0x300 * 4),
               (uint_32 *) (0xfffff000 + 0x300 * 4), 1024);
        uint_32 new_page_dir_phy_addr = addr_v2p((uint_32) page_dir_vaddr);
        // Add last pde to pd phy_addr
        page_dir_vaddr[1023] =
            new_page_dir_phy_addr | PG_RW_W | PG_P_SET;
        return page_dir_vaddr;
}

static int create_user_vaddr_bitmap(struct mm_struct *mm)
{
        uint_32 bitmap_len =
            DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PAGE_SIZE, 8);
        mm->user_vaddr.vaddr_start = USER_VADDR_START;
        uint_32 bitmap_pg_cnt = DIV_ROUND_UP(bitmap_len, PAGE_SIZE);
        mm->user_vaddr.vaddr_bitmap.bits = get_kernel_page(bitmap_pg_cnt);
        if (mm->user_vaddr.vaddr_bitmap.bits == NULL)
                return -1;
        mm->user_vaddr.vaddr_bitmap.map_bytes_length = bitmap_len;
        init_bitmap(&mm->user_vaddr.vaddr_bitmap);
        return 0;
}

static int create_initial_user_stack(TCB_t *thread)
{
        TCB_t *current = running_thread();
        uint_32 bit_idx =
            (USER_STACK3_VADDR - thread->mm->user_vaddr.vaddr_start) /
            PAGE_SIZE;
        unsigned long flags;

        local_irq_save(flags);
        page_dir_activate(thread);
        void *stack = get_phy_free_page_with_vaddr(
            MP_USER, USER_STACK3_VADDR, thread->mm);
        page_dir_activate(current);
        local_irq_restore(flags);
        if (stack == NULL)
                return -1;
        set_value_bitmap(&thread->mm->user_vaddr.vaddr_bitmap, bit_idx, 1);
        thread->mm->generation++;
        return 0;
}

static bool user_image_valid(const struct user_image *image)
{
        uint_32 image_end;

        if (image == NULL || image->data == NULL || image->size == 0 ||
            image->size > PAGE_SIZE || image->load_addr != USER_IMAGE_VADDR ||
            (image->load_addr & (PAGE_SIZE - 1)) != 0)
                return false;
        image_end = image->load_addr + image->size;
        if (image_end < image->load_addr || image_end > USER_STACK3_VADDR ||
            image->entry < image->load_addr || image->entry >= image_end)
                return false;
        return true;
}

static int load_user_image(TCB_t *thread, const struct user_image *image)
{
        TCB_t *current = running_thread();
        uint_32 bit_idx =
            (image->load_addr - thread->mm->user_vaddr.vaddr_start) /
            PAGE_SIZE;
        unsigned long flags;
        int result = -1;

        local_irq_save(flags);
        page_dir_activate(thread);
        uint_32 *pde = pde_ptr(image->load_addr);
        if ((*pde & PG_P_SET) && (*pte_ptr(image->load_addr) & PG_P_SET))
                goto out;
        if (get_phy_free_page_with_vaddr(MP_USER, image->load_addr,
                                         thread->mm) == NULL)
                goto out;

        memset((void *) image->load_addr, 0, PAGE_SIZE);
        memcpy((void *) image->load_addr, image->data, image->size);
        set_value_bitmap(&thread->mm->user_vaddr.vaddr_bitmap, bit_idx, 1);
        thread->mm->generation++;
        result = 0;

out:
        page_dir_activate(current);
        local_irq_restore(flags);
        return result;
}

void process_release_address_space(TCB_t *thread)
{
        if (thread == NULL || thread->mm == NULL)
                return;

        struct mm_struct *mm = thread->mm;
        TCB_t *current = running_thread();
        unsigned long flags;
        local_irq_save(flags);
        if (mm->pgdir != NULL) {
                page_dir_activate(thread);
                for (uint_32 pde_idx = 0; pde_idx < 768; pde_idx++) {
                        uint_32 pde = mm->pgdir[pde_idx];
                        if (!(pde & PG_P_SET))
                                continue;
                        uint_32 *pt = pte_ptr(pde_idx * 0x400000);
                        for (uint_32 pte_idx = 0; pte_idx < 1024; pte_idx++) {
                                if (pt[pte_idx] & PG_P_SET)
                                        free_phy_page(pt[pte_idx] & 0xfffff000);
                        }
                        free_phy_page(pde & 0xfffff000);
                }
                if (current == thread)
                        page_dir_activate(NULL);
                else
                        page_dir_activate(current);
                free_page(MP_KERNEL, mm->pgdir, 1);
                mm->pgdir = NULL;
        }

        uint_8 *bits = mm->user_vaddr.vaddr_bitmap.bits;
        uint_32 bytes = mm->user_vaddr.vaddr_bitmap.map_bytes_length;
        if (bits != NULL && bytes != 0) {
                free_page(MP_KERNEL, bits, DIV_ROUND_UP(bytes, PAGE_SIZE));
                mm->user_vaddr.vaddr_bitmap.bits = NULL;
                mm->user_vaddr.vaddr_bitmap.map_bytes_length = 0;
        }
        thread->mm = NULL;
        local_irq_restore(flags);
        mm_destroy(mm);
}

uint_32 process_execute(void *filename, char *name)
{
        TCB_t *thread = get_kernel_page(1);
        if (thread == NULL)
                return (uint_32) -1;
        if (init_thread(thread, name, DEFAULT_PRIORITY) < 0)
                goto fail_tcb;
        thread->mm = mm_create();
        if (thread->mm == NULL)
                goto fail_pid;
        if (create_user_vaddr_bitmap(thread->mm) < 0)
                goto fail_address_space;
        create_thread(thread, start_process, filename);
        thread->mm->pgdir = create_page_dir();
        if (thread->mm->pgdir == NULL)
                goto fail_address_space;
        if (create_initial_user_stack(thread) < 0)
                goto fail_address_space;
        block_desc_init(thread->u_block_descs);
        if (thread_publish(thread) < 0)
                goto fail_address_space;
        return thread->pid;

fail_address_space:
        process_release_address_space(thread);
fail_pid:
        thread_release_pid(thread->pid);
fail_tcb:
        free_page(MP_KERNEL, thread, 1);
        return (uint_32) -1;
}

uint_32 process_execute_image(const struct user_image *image,
                              const char *name)
{
        TCB_t *thread;

        if (!user_image_valid(image))
                return (uint_32) -1;
        thread = get_kernel_page(1);
        if (thread == NULL)
                return (uint_32) -1;
        if (init_thread(thread, name, DEFAULT_PRIORITY) < 0)
                goto fail_tcb;
        thread->mm = mm_create();
        if (thread->mm == NULL)
                goto fail_pid;
        if (create_user_vaddr_bitmap(thread->mm) < 0)
                goto fail_address_space;
        create_thread(thread, start_process, (void *) image->entry);
        thread->mm->pgdir = create_page_dir();
        if (thread->mm->pgdir == NULL || load_user_image(thread, image) < 0 ||
            create_initial_user_stack(thread) < 0)
                goto fail_address_space;
        block_desc_init(thread->u_block_descs);
        if (thread_publish(thread) < 0)
                goto fail_address_space;
        return thread->pid;

fail_address_space:
        process_release_address_space(thread);
fail_pid:
        thread_release_pid(thread->pid);
fail_tcb:
        free_page(MP_KERNEL, thread, 1);
        return (uint_32) -1;
}

void process_execute_ring1(void *filename, char *name)
{
        TCB_t *thread = get_kernel_page(1);
        if (thread == NULL)
                return;
        if (init_thread(thread, name, DEFAULT_PRIORITY) < 0)
                goto fail_tcb;
        thread->mm = mm_create();
        if (thread->mm == NULL)
                goto fail_pid;
        if (create_user_vaddr_bitmap(thread->mm) < 0)
                goto fail_address_space;
        create_thread(thread, start_process_ring1, filename);
        thread->mm->pgdir = create_page_dir();
        if (thread->mm->pgdir == NULL ||
            create_initial_user_stack(thread) < 0 ||
            thread_publish(thread) < 0)
                goto fail_address_space;
        return;

fail_address_space:
        process_release_address_space(thread);
fail_pid:
        thread_release_pid(thread->pid);
fail_tcb:
        free_page(MP_KERNEL, thread, 1);
}
