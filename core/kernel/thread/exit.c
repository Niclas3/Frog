#include <kernel/fd.h>
#include <kernel/panic.h>
#include <kernel/assert.h>
#include <kernel/syscall_fs.h>
#include <frog/math.h>
#include <frog/exit.h>
#include <frog/irqflags.h>
#include <frog/process.h>
#include <frog/threads.h>

extern struct list_head thread_all_list;

static pid_t init_process_pid = -1;

void set_init_process_pid(pid_t pid)
{
    ASSERT(pid != (pid_t) -1);
    init_process_pid = pid;
}

static void close_process_files(TCB_t *thread)
{
    // close file descriptor
    for (int fd_idx = 0; fd_idx < MAX_FILES_OPEN_PER_PROC; fd_idx++) {
        if (thread->fd_table[fd_idx] != -1)
            sys_close(fd_idx);
    }
}

// list_walk() callback
static bool find_child(struct list_head *ele, pid_t ppid)
{
    TCB_t *cur = container_of(ele, TCB_t, all_list_tag);
    if (cur->parent_pid == ppid) {
        return true;
    }
    return false;
}

// list_walk() callback
static bool find_hanging_child(struct list_head *ele, pid_t ppid)
{
    TCB_t *cur = container_of(ele, TCB_t, all_list_tag);
    if (cur->parent_pid == ppid && cur->status == THREAD_TASK_HANGING) {
        return true;
    }
    return false;
}

// list_walk() callback
static bool proc_init_adopt_a_child(struct list_head *ele, pid_t pid)
{
    TCB_t *cur = container_of(ele, TCB_t, all_list_tag);
    if (cur->parent_pid == pid) {
        cur->parent_pid = init_process_pid;
    }
    return false;
}


pid_t sys_wait(int_32 *status_loc)
{
    TCB_t *parent = running_thread();
    while (1) {
        unsigned long flags;
        local_irq_save(flags);
        struct list_head *child_node =
            list_walker(&thread_all_list, find_hanging_child, parent->pid);
        if (child_node != NULL) {
            TCB_t *child = container_of(child_node, TCB_t, all_list_tag);
            if (status_loc != NULL)
                *status_loc = child->exit_status;

            pid_t child_pid = child->pid;
            thread_exit(child, false);
            local_irq_restore(flags);
            return child_pid;
        }
        child_node = list_walker(&thread_all_list, find_child, parent->pid);
        if (child_node == NULL) {
            local_irq_restore(flags);
            return -1;
        } else {
            thread_block(THREAD_TASK_WAITING);
            local_irq_restore(flags);
        }
    }
}

void sys_exit(int_32 status)
{
    TCB_t *child = running_thread();

    /* The syscall/exception gate clears IF; file and VM teardown may sleep. */
    local_irq_enable();
    child->exit_status = status;
    if (child->parent_pid == -1) {
        PANIC("sys_exit: child parent is -1\n");
    }
    close_process_files(child);
    process_release_address_space(child);

    unsigned long flags;
    local_irq_save(flags);
    list_walker(&thread_all_list, proc_init_adopt_a_child, child->pid);
    TCB_t *parent = pid2thread(child->parent_pid);
    if (parent != NULL && parent->status == THREAD_TASK_WAITING) {
        thread_unblock(parent);
    }
    thread_block(THREAD_TASK_HANGING);
    local_irq_restore(flags);
}
