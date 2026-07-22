#include <kernel/fd.h>
#include <kernel/panic.h>
#include <kernel/assert.h>
#include <frog/math.h>
#include <frog/exit.h>
#include <frog/errno.h>
#include <frog/irqflags.h>
#include <frog/process.h>
#include <frog/threads.h>
#include <frog/uaccess.h>

extern struct list_head thread_all_list;

static pid_t init_process_pid = -1;

void set_init_process_pid(pid_t pid)
{
    ASSERT(pid >= 0 && init_process_pid == -1);
    init_process_pid = pid;
}

static void close_process_files(TCB_t *thread)
{
    fd_close_all(thread);
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
static bool reparent_children(TCB_t *parent, TCB_t *adopter)
{
    struct list_head *position;
    bool adopted_zombie = false;

    list_for_each(position, &thread_all_list) {
        TCB_t *child = container_of(position, TCB_t, all_list_tag);

        if (child->parent_pid != parent->pid)
            continue;
        ASSERT(adopter != NULL);
        child->parent_pid = adopter->pid;
        if (child->status == THREAD_TASK_HANGING)
            adopted_zombie = true;
    }
    return adopted_zombie;
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
            int_32 child_status = child->exit_status;

            if (status_loc != NULL &&
                copy_to_user(status_loc, &child_status,
                             sizeof(child_status)) < 0) {
                local_irq_restore(flags);
                return -EFAULT;
            }

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
    TCB_t *parent;
    TCB_t *init = NULL;
    bool adopted_zombie;

    /* The syscall/exception gate clears IF; file and VM teardown may sleep. */
    local_irq_enable();
    if (child->pid == init_process_pid)
        PANIC("init process must not exit");
    if (init_process_pid < 0 ||
        (init = pid2thread(init_process_pid)) == NULL)
        PANIC("user process exit without a live init");
    child->exit_status = status;
    close_process_files(child);
    process_release_address_space(child);

    unsigned long flags;
    local_irq_save(flags);
    adopted_zombie = reparent_children(child, init);
    if (adopted_zombie && init->status == THREAD_TASK_WAITING)
        thread_unblock(init);

    parent = child->parent_pid >= 0 ? pid2thread(child->parent_pid) : NULL;
    if (parent == NULL)
        PANIC("user process exit without a live parent");
    if (parent != NULL && parent->status == THREAD_TASK_WAITING)
        thread_unblock(parent);

    thread_block(THREAD_TASK_HANGING);
    local_irq_restore(flags);
}
