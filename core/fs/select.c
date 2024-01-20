#include <fs/file.h>
#include <fs/fs.h>
#include <fs/inode.h>
#include <frog/poll.h>

#include <fs/select.h>
#include <sys/fork.h>
#include <sys/sched.h>
#include <sys/threads.h>
#include <sys/memory.h>

#include <errno-base.h>
#include <const.h>

struct poll_table_entry {
	struct file * filp;
	wait_queue_t wait;
	wait_queue_head_t * wait_address;
};

struct poll_table_page {
	struct poll_table_page * next;
	struct poll_table_entry * entry;
	struct poll_table_entry entries[0];
};

#define POLL_TABLE_FULL(table) \
	((unsigned long)((table)->entry+1) > PG_SIZE + (unsigned long)(table))

/* cur->status = THREAD_TASK_WAITING; */
/* schedule_timeout(299); */

void __pollwait(struct file * filp, wait_queue_head_t * wait_address, poll_table *p)
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
		struct poll_table_entry * entry = table->entry;
		table->entry = entry+1;
	 	entry->filp = filp;
		entry->wait_address = wait_address;
		init_waitqueue_entry(&entry->wait, current);
		add_wait_queue(wait_address,&entry->wait);
	}
}

void poll_freewait(poll_table* pt)
{
	struct poll_table_page * p = pt->table;
	while (p) {
		struct poll_table_entry * entry;
		struct poll_table_page *old;

		entry = p->entry;
		do {
			entry--;
			remove_wait_queue(entry->wait_address,&entry->wait);
			/* fput(entry->filp); */
		} while (entry > p->entries);
		old = p;
		p = p->next;
                mfree_page(MP_KERNEL, old, 1);
	}
}

uint_32 sys_wait2(int n, int_32 *fds, struct timeval *tvp)
{
    TCB_t *cur = running_thread();
    /* for (;;) { */
    /*     for (int i = 0; i < n; i++) { */
    /*         // go through all file */
    /*         struct file *f = get_file(fds[i]); */
    /*     } */
    /* } */

    return 0;
}
