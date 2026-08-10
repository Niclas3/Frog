#include <frog/irqflags.h>
#include <frog/errno.h>
#include <frog/math.h>
#include <frog/string.h>
#include <frog/process.h>
#include <frog/threads.h>
#include <frog/vm.h>

#include <const.h>
#include <kernel/assert.h>
#include <kernel/disk_init_loader.h>
#include <kernel/process.h>

#include <asm/processor-flags.h> // FOR EFLAGS
#include <asm/tss.h>
#include <asm/descriptor.h>
#include <asm/page.h>
#include "../../mm/mm_helper.h"



extern struct list_head thread_ready_list;
extern struct list_head thread_all_list;
extern TCB_t *main_thread;


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
        TCB_t *cur = running_thread();

        (void) filename;
        // To the bottom of context_register
        cur->self_kstack = (uint_32 *) ((uint_32) cur->self_kstack +
                                        sizeof(struct thread_stack));
        struct context_registers *proc_stack =
            (struct context_registers *) cur->self_kstack;
        proc_stack->edi = proc_stack->esi = proc_stack->ebp = proc_stack->esp =
            0;
        proc_stack->eax = proc_stack->edx = 0;
        proc_stack->ebx = cur->user_argv;
        proc_stack->ecx = cur->user_argc;
        proc_stack->gs = 0;
        proc_stack->ds = proc_stack->es = proc_stack->fs =
            CREATE_SELECTOR(SEL_IDX_DATA_DPL_3, TI_GDT, RPL3);
        proc_stack->cs = CREATE_SELECTOR(SEL_IDX_CODE_DPL_3, TI_GDT, RPL3);
        proc_stack->eip = (void *) cur->user_entry;
        proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_IF_1 | EFLAGS_RESERVED);
        proc_stack->esp_ptr = (void *) cur->user_stack;
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

int process_publish_initial_user_mm_owned(
    struct mm_struct *owned_mm,
    const char *name,
    const struct process_startup_context *startup,
    pid_t *pid_out)
{
        TCB_t *current = running_thread();
        TCB_t *thread = NULL;
        pid_t allocated_pid = -1;
        unsigned long flags;
        int result = -EINVAL;

        if (pid_out != NULL)
                *pid_out = -1;
        if (owned_mm == NULL || owned_mm->pgdir == NULL || name == NULL ||
            startup == NULL || pid_out == NULL || current == NULL ||
            current != main_thread || current->pid != 1 || current->mm != NULL ||
            process_has_published_user_mm() ||
            startup->entry < USER_VADDR_START ||
            startup->entry >= 0xc0000000U ||
            startup->stack < USER_VADDR_START ||
            startup->stack > 0xc0000000U ||
            (startup->argc != 0 &&
             (startup->argv < USER_VADDR_START ||
              startup->argv >= 0xc0000000U)))
                goto fail;

        thread = get_kernel_page(1);
        if (thread == NULL) {
                result = -ENOMEM;
                goto fail;
        }
        if (init_thread(thread, name, DEFAULT_PRIORITY) < 0) {
                result = -EAGAIN;
                goto fail;
        }
        allocated_pid = thread->pid;
        if (allocated_pid <= 1) {
                result = -EAGAIN;
                goto fail;
        }

        thread->mm = owned_mm;
        thread->user_entry = startup->entry;
        thread->user_stack = startup->stack;
        thread->user_argc = startup->argc;
        thread->user_argv = startup->argv;
        create_thread(thread, start_process, NULL);
        block_desc_init(thread->u_block_descs);
        ASSERT(thread->parent_pid == -1);

        /*
         * PID 1 starts owned by the boot main thread. The replacement TCB is
         * still private while its allocated PID becomes main's replacement.
         * IRQ exclusion makes the temporary PID-1 gap unobservable and no two
         * published/schedulable threads ever share a PID.
         */
        local_irq_save(flags);
        if (current != running_thread() || current->pid != 1 ||
            current->mm != NULL || process_has_published_user_mm()) {
                local_irq_restore(flags);
                result = -EBUSY;
                goto fail;
        }
        current->pid = allocated_pid;
        thread->pid = 1;
#ifdef CONFIG_FROG_TEST_DISK_INIT_LOADER
        if (disk_init_loader_test_take_failure(
                DISK_INIT_LOADER_FAIL_AFTER_PID_SWAP)) {
                thread->pid = allocated_pid;
                current->pid = 1;
                local_irq_restore(flags);
                result = -ENOMEM;
                goto fail;
        }
#endif
        if (thread_publish(thread) < 0) {
                thread->pid = allocated_pid;
                current->pid = 1;
                local_irq_restore(flags);
                result = -EAGAIN;
                goto fail;
        }
        local_irq_restore(flags);

        *pid_out = 1;
        return 0;

fail:
        if (thread != NULL) {
                thread->mm = NULL;
                thread_release_pid(thread->pid);
                free_page(MP_KERNEL, thread, 1);
        }
        mm_release_address_space(owned_mm);
        return result;
}

pid_t process_execute_init_image(const struct user_image *image)
{
        TCB_t *current = running_thread();
        struct mm_struct *mm;
        struct process_startup_context startup;
        pid_t pid = -1;
        int result;

        ASSERT(current != NULL);
        if (current->mm != NULL || process_has_published_user_mm() ||
            !user_image_valid(image))
                return -1;
        mm = process_create_user_mm();
        if (mm == NULL)
                return -1;
        result = vm_user_map_owned_page(mm, image->load_addr);
        if (result == 0)
                result = vm_user_write_owned(mm, image->load_addr,
                                             image->data, image->size);
        if (result == 0)
                result = vm_user_map_owned_page(mm, USER_STACK3_VADDR);
        if (result != 0) {
                mm_release_address_space(mm);
                return -1;
        }

        startup.entry = image->entry;
        startup.stack = USER_STACK3_VADDR + PAGE_SIZE;
        startup.argc = 0;
        startup.argv = 0;
        result = process_publish_initial_user_mm_owned(
            mm, "init", &startup, &pid);
        return result == 0 ? pid : -1;
}
