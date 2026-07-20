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
        child_thread->status = THREAD_TASK_READY;
        child_thread->ticks = child_thread->priority;
        child_thread->parent_pid = parent_thread->pid;
        INIT_LIST_HEAD(&child_thread->general_tag);
        INIT_LIST_HEAD(&child_thread->all_list_tag);
        INIT_LIST_HEAD(&child_thread->proc_list_tag);
        block_desc_init(child_thread->u_block_descs);

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

static void retain_open_files(TCB_t *thread)
{
        for (int_32 local_fd = 0; local_fd < MAX_FILES_OPEN_PER_PROC;
             local_fd++) {
                int_32 global_fd = thread->fd_table[local_fd];
                if (global_fd < 0 || global_fd >= MAX_FILE_OPEN)
                        continue;
                struct file *f = g_file_table[global_fd];
                fd_retain(f);
        }
}

static int copy_process(TCB_t *child_thread, TCB_t *parent_thread)
{
        if (copy_tcb_stack0(child_thread, parent_thread) == -1)
                goto fail;
        child_thread->mm = mm_clone_for_fork(parent_thread->mm);
        if (child_thread->mm == NULL)
                goto fail;
        build_child_stack(child_thread, parent_thread);
        return 0;

fail:
        return -1;
}

uint_32 sys_fork(void)
{
        TCB_t *parent_thread = running_thread();
        TCB_t *child_thread;
        unsigned long syscall_flags;
        uint_32 result;

        /* The syscall interrupt gate enters with IF clear; cloning may sleep. */
        local_irq_save(syscall_flags);
        local_irq_enable();
        child_thread = get_kernel_page(1);

        if (child_thread == NULL) {
                result = (uint_32) -1;
                goto restore_irqs;
        }
        memset(child_thread, 0, PAGE_SIZE);
        child_thread->pid = (pid_t) -1;

        ASSERT(parent_thread->mm != NULL &&
               parent_thread->mm->pgdir != NULL);
        if (copy_process(child_thread, parent_thread) == -1)
                goto fail;

        unsigned long flags;
        local_irq_save(flags);
        if (thread_publish(child_thread) < 0) {
                local_irq_restore(flags);
                goto fail;
        }
        retain_open_files(child_thread);
        local_irq_restore(flags);

        result = child_thread->pid;
        goto restore_irqs;

fail:
        process_release_address_space(child_thread);
        thread_release_pid(child_thread->pid);
        free_page(MP_KERNEL, child_thread, 1);
        result = (uint_32) -1;

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
