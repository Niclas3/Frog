#include <kernel/assert.h>
#include <frog/irqflags.h>

#include <frog/fork.h>
#include <frog/memory.h>
#include <frog/process.h>
#include <frog/string.h>
#include <frog/threads.h>
#include <frog/vm.h>

#include <asm/page.h>

#include <kernel/fd.h>
#include <kernel/vfs.h>

extern void intr_exit(void);

extern struct list_head thread_ready_list;
extern struct list_head thread_all_list;

static int_32 copy_tcb_stack0(TCB_t *child_thread, TCB_t *parent_thread)
{
        if (parent_thread->mm == NULL)
                return -1;

        memcpy(child_thread, parent_thread, PAGE_SIZE);
        child_thread->mm = NULL;
        child_thread->pid = fork_pid();
        if (child_thread->pid == (pid_t) -1)
                return -1;
        child_thread->elapsed_ticks = 0;
        child_thread->need_schedule = false;
        child_thread->exit_status = 0;
        child_thread->status = THREAD_TASK_READY;
        child_thread->ticks = child_thread->priority;
        child_thread->parent_pid = parent_thread->pid;
        INIT_LIST_HEAD(&child_thread->general_tag);
        INIT_LIST_HEAD(&child_thread->all_list_tag);
        INIT_LIST_HEAD(&child_thread->proc_list_tag);
        if (block_desc_clone_prepare(child_thread->u_block_descs,
                                     parent_thread->u_block_descs) < 0)
                return -1;
        memset(&child_thread->p_message, 0,
               sizeof(child_thread->p_message));
        child_thread->p_recvfrom = NO_TASK;
        child_thread->p_sendto = NO_TASK;
        child_thread->p_flags = 0;
        child_thread->p_intr_present = 0;
        child_thread->p_sending_queue = NULL;
        child_thread->p_next_sending = NULL;

        uint_32 name_len = strlen(child_thread->name);
        strncpy(child_thread->name + name_len, "_fork",
                TASK_NAME_LEN - name_len - 1);
        child_thread->name[TASK_NAME_LEN - 1] = '\0';
        return 0;
}

static int_32 build_child_stack(TCB_t *child_thread, TCB_t *parent_thread)
{
        struct context_registers *intr_0_stack =
            (struct context_registers *) ((uint_32) child_thread + PAGE_SIZE -
                                          sizeof(struct context_registers));

        intr_0_stack->eax = 0;

        uint_32 *ret_addr_in_thread_stack = (uint_32 *) intr_0_stack - 1;
        uint_32 *esi_addr = (uint_32 *) intr_0_stack - 2;
        uint_32 *edi_addr = (uint_32 *) intr_0_stack - 3;
        uint_32 *ebx_addr = (uint_32 *) intr_0_stack - 4;
        uint_32 *ebp_ptr_in_thread_stack = (uint_32 *) intr_0_stack - 5;
        *ret_addr_in_thread_stack = (uint_32) intr_exit;
        child_thread->self_kstack = ebp_ptr_in_thread_stack;

        (void)esi_addr;
        (void)edi_addr;
        (void)ebx_addr;
        (void)parent_thread;

        return 0;
}

static int copy_process(TCB_t *child_thread, TCB_t *parent_thread)
{
        unsigned long flags;

        if (block_desc_validate_user_for_fork(
                parent_thread->u_block_descs) < 0)
                goto fail;
        if (copy_tcb_stack0(child_thread, parent_thread) == -1)
                goto fail;
        child_thread->mm = mm_clone_for_fork(parent_thread->mm);
        if (child_thread->mm == NULL)
                goto fail;
        local_irq_save(flags);
        page_dir_activate(child_thread);
        block_desc_clone_fixup(child_thread->u_block_descs);
        page_dir_activate(parent_thread);
        local_irq_restore(flags);
        build_child_stack(child_thread, parent_thread);
        return 0;

fail:
        return -1;
}

pid_t sys_fork(void)
{
        TCB_t *parent_thread = running_thread();
        TCB_t *child_thread;
        unsigned long syscall_flags;
        pid_t result;
        bool files_retained = false;

        /* The syscall interrupt gate enters with IF clear; cloning may sleep. */
        local_irq_save(syscall_flags);
        local_irq_enable();
        child_thread = get_kernel_page(1);

        if (child_thread == NULL) {
                result = -1;
                goto restore_irqs;
        }
        memset(child_thread, 0, PAGE_SIZE);
        child_thread->pid = (pid_t) -1;

        ASSERT(parent_thread->mm != NULL &&
               parent_thread->mm->pgdir != NULL);
        if (copy_process(child_thread, parent_thread) == -1)
                goto fail;
        if (fd_retain_table(child_thread) != 0)
                goto fail;
        files_retained = true;

        unsigned long flags;
        local_irq_save(flags);
        if (thread_publish(child_thread) < 0) {
                local_irq_restore(flags);
                goto fail;
        }
        local_irq_restore(flags);

        result = child_thread->pid;
        goto restore_irqs;

fail:
        if (files_retained)
                fd_close_all(child_thread);
        process_release_address_space(child_thread);
        thread_release_pid(child_thread->pid);
        free_page(MP_KERNEL, child_thread, 1);
        result = -1;

restore_irqs:
        local_irq_restore(syscall_flags);
        return result;
}

void add_wait_queue(wait_queue_head_t *q, wait_queue_t *wait)
{
        unsigned long flags;
        local_irq_save(flags);
        __add_wait_queue(q, wait);
        local_irq_restore(flags);
}

void remove_wait_queue(wait_queue_head_t *q, wait_queue_t *wait)
{
        unsigned long flags;
        local_irq_save(flags);
        __remove_wait_queue(q, wait);
        local_irq_restore(flags);
}
