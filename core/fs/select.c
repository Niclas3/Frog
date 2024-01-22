#include <frog/poll.h>
#include <fs/file.h>
#include <fs/fs.h>
#include <fs/inode.h>

#include <fs/select.h>
#include <sys/fork.h>
#include <sys/memory.h>
#include <sys/sched.h>
#include <sys/threads.h>

#include <sys/int.h>

#include <const.h>
#include <errno-base.h>

#include <debug.h>

struct poll_table_entry {
    struct file *filp;
    wait_queue_t wait;
    wait_queue_head_t *wait_address;
};

struct poll_table_page {
    struct poll_table_page *next;
    struct poll_table_entry *entry;
    struct poll_table_entry entries[0];
};
#define DEFAULT_POLLMASK (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM)

#define POLL_TABLE_FULL(table) \
    ((unsigned long) ((table)->entry + 1) > PG_SIZE + (unsigned long) (table))

/* cur->status = THREAD_TASK_WAITING; */
/* schedule_timeout(299); */

void __pollwait(struct file *filp,
                wait_queue_head_t *wait_address,
                poll_table *p)
{
    struct poll_table_page *table = p->table;
    TCB_t *current = running_thread();

    if (!table || POLL_TABLE_FULL(table)) {
        struct poll_table_page *new_table;

        new_table = (struct poll_table_page *) get_kernel_page(1);
        if (!new_table) {
            p->error = -ENOMEM;
            current->status = THREAD_TASK_RUNNING;
            return;
        }
        new_table->entry = new_table->entries;
        new_table->next = table;
        p->table = new_table;
        table = new_table;
    }

    /* Add a new entry */
    {
        struct poll_table_entry *entry = table->entry;
        table->entry = entry + 1;
        entry->filp = filp;
        entry->wait_address = wait_address;
        init_waitqueue_entry(&entry->wait, current);
        add_wait_queue(wait_address, &entry->wait);
    }
}

void poll_freewait(poll_table *pt)
{
    struct poll_table_page *p = pt->table;
    while (p) {
        struct poll_table_entry *entry;
        struct poll_table_page *old;

        entry = p->entry;
        do {
            entry--;
            remove_wait_queue(entry->wait_address, &entry->wait);
            /* fput(entry->filp); */
        } while (entry > p->entries);
        old = p;
        p = p->next;
        mfree_page(MP_KERNEL, old, 1);
    }
}

#define POLLIN_SET (POLLRDNORM | POLLRDBAND | POLLIN | POLLHUP | POLLERR)
#define POLLOUT_SET (POLLWRBAND | POLLWRNORM | POLLOUT | POLLERR)
#define POLLEX_SET (POLLPRI)

static int_32 do_wait2(int n, int_32 *fds, uint_32 *timeout)
{
    poll_table table, *wait;
    int retval = 0;
    int_32 __timeout = *timeout;

    poll_initwait(&table);
    wait = &table;
    if (!__timeout)
        wait = NULL;
    retval = -1;
    TCB_t *cur = running_thread();
    for (;;) {
        enum intr_status old_status = intr_disable();
        cur->status = THREAD_TASK_WAITING;
        for (int i = 0; i < n; i++) {
            uint_32 mask = DEFAULT_POLLMASK;
            // go through all file
            struct file *f = get_file(fds[i]);
            mask = sys_poll(f, wait);
            if ((mask & POLLIN_SET)) {
                retval = i;
                wait = NULL;
            }
        }

        wait = NULL;
        if ((retval != -1) || !__timeout)
            break;
        if (table.error) {
            retval = table.error;
            break;
        }
        __timeout = schedule_timeout(__timeout);
        intr_set_status(old_status);
    }

    cur->status = THREAD_TASK_RUNNING;

    enum intr_status old_status = intr_disable();
    poll_freewait(&table);
    intr_set_status(old_status);

    /*
     * Up-to-date the caller timeout.
     */
    *timeout = __timeout;
    return retval;
}

#define MAX_SELECT_SECONDS ((uint_32) (MAX_SCHEDULE_TIMEOUT / HZ) - 1)
uint_32 sys_wait2(int n, int_32 *fds, struct timeval *tvp)
{
    ASSERT(fds != NULL);
    uint_32 wait_ticks = 0;
    if (tvp) {
        // covert seconds to ticks
        // 1000000 micoseconds = 1 seconds
        wait_ticks = (tvp->tv_sec + tvp->tv_usec / 1000000) * HZ;
    }

    // test all fd is fds must be char devices (for now)
    for (int i = 0; i < n; i++) {
        struct file *f = get_file(fds[i]);
        ASSERT(IS_FT_CHAR(f->fd_inode));
    }
    int_32 res = do_wait2(n, fds, &wait_ticks);

    return res;
}
