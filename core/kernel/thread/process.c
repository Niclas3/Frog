#include <frog/irqflags.h>
#include <frog/errno.h>
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

struct mm_struct *process_create_user_mm(void)
{
        struct mm_struct *mm = mm_create();

        if (mm == NULL)
                return NULL;
        if (create_user_vaddr_bitmap(mm) < 0)
                goto fail;
        mm->pgdir = create_page_dir();
        if (mm->pgdir == NULL)
                goto fail;
        return mm;

fail:
        mm_release_address_space(mm);
        return NULL;
}

int process_commit_user_image(struct mm_struct *new_mm,
                              const char *name,
                              uint_32 entry,
                              uint_32 stack,
                              uint_32 argc,
                              uint_32 argv,
                              struct mm_struct **old_mm_out)
{
        TCB_t *current = running_thread();
        struct context_registers *context;
        struct mm_struct *old_mm;
        unsigned long flags;

        if (current == NULL || current->mm == NULL || new_mm == NULL ||
            new_mm == current->mm ||
            new_mm->pgdir == NULL || name == NULL || old_mm_out == NULL ||
            entry < USER_VADDR_START || entry >= 0xc0000000U ||
            stack < USER_VADDR_START || stack >= 0xc0000000U ||
            argv < USER_VADDR_START || argv >= 0xc0000000U)
                return -EINVAL;

        context = (struct context_registers *)
            ((uint_32) current + PAGE_SIZE - sizeof(*context));
        local_irq_save(flags);
        old_mm = current->mm;
        current->mm = new_mm;
        page_dir_activate(current);

        memset(context, 0, sizeof(*context));
        context->ebx = argv;
        context->ecx = argc;
        context->ds = context->es = context->fs =
            CREATE_SELECTOR(SEL_IDX_DATA_DPL_3, TI_GDT, RPL3);
        context->cs = CREATE_SELECTOR(SEL_IDX_CODE_DPL_3, TI_GDT, RPL3);
        context->eip = (void *) entry;
        context->eflags = EFLAGS_IOPL_0 | EFLAGS_IF_1 | EFLAGS_RESERVED;
        context->esp_ptr = (void *) stack;
        context->ss = CREATE_SELECTOR(SEL_IDX_DATA_DPL_3, TI_GDT, RPL3);

        strncpy(current->name, name, TASK_NAME_LEN - 1);
        current->name[TASK_NAME_LEN - 1] = '\0';
        block_desc_init(current->u_block_descs);
        *old_mm_out = old_mm;
        local_irq_restore(flags);
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
        if (stack != NULL)
                memset(stack, 0, PAGE_SIZE);
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

        /* A scheduled teardown must never reactivate an address space in flight. */
        thread->mm = NULL;
        mm_release_address_space(mm);
}

static bool process_has_published_user_mm(void)
{
        struct list_head *position;
        unsigned long flags;
        bool found = false;

        local_irq_save(flags);
        list_for_each(position, &thread_all_list) {
                TCB_t *thread = list_entry(position, TCB_t, all_list_tag);

                if (thread->mm != NULL) {
                        found = true;
                        break;
                }
        }
        local_irq_restore(flags);
        return found;
}

pid_t process_execute_init_image(const struct user_image *image)
{
        TCB_t *current = running_thread();
        TCB_t *thread;

        ASSERT(current != NULL);
        if (current->mm != NULL || process_has_published_user_mm() ||
            !user_image_valid(image))
                return -1;
        thread = get_kernel_page(1);
        if (thread == NULL)
                return -1;
        if (init_thread(thread, "init", DEFAULT_PRIORITY) < 0)
                goto fail_tcb;
        ASSERT(thread->parent_pid == -1);
        thread->mm = process_create_user_mm();
        if (thread->mm == NULL)
                goto fail_address_space;
        create_thread(thread, start_process, (void *) image->entry);
        if (load_user_image(thread, image) < 0 ||
            create_initial_user_stack(thread) < 0)
                goto fail_address_space;
        block_desc_init(thread->u_block_descs);
        if (thread_publish(thread) < 0)
                goto fail_address_space;
        return thread->pid;

fail_address_space:
        process_release_address_space(thread);
        thread_release_pid(thread->pid);
fail_tcb:
        free_page(MP_KERNEL, thread, 1);
        return -1;
}
