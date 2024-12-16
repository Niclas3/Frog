#include <debug.h>
#include <math.h>
#include <sys/sched.h>
#include <sys/threads.h>

#include <frog/timer.h>
#include <frog/piti8253.h>
#include <const.h>

#include <protect.h>
#include <sys/int.h>

extern struct list_head thread_ready_list;

#define mil_seconds_per_intr DIV_ROUND_UP(1000, HZ)

/**
 * Total ticks count since open timer interrupt
 */
uint_32 volatile ticks = 0;

extern void schedule(void);
extern void init_timervecs (void);

void init_timer_manager(void)
{
    //init timer resource
    init_timervecs();
    // register timer interrupt handler
    register_r0_intr_handler(INT_VECTOR_INNER_CLOCK,
                             (Inthandle_t *) inthandler20);
}

static inline void add_to_readylist(TCB_t *p)
{
    list_add_tail(&p->general_tag, &thread_ready_list);
}

static inline void move_last_readylist(TCB_t *p)
{
    list_del(&p->general_tag);
    list_add_tail(&p->general_tag, &thread_ready_list);
}

/*
 * Wake up a process. Put it on the run-queue if it's not
 * already there.  The "current" process is always on the
 * run-queue (except when the actual re-schedule is in
 * progress), and as such you're allowed to do the simpler
 * "current->state = TASK_RUNNING" to mark yourself runnable
 * without the overhead of this.
 */
static inline int try_to_wake_up(TCB_t *p, int synchronous)
{
    int success = 0;

    /*
     * We want the common case fall through straight, thus the goto.
     */
    enum intr_status old_int_status = intr_disable();
    p->status = THREAD_TASK_READY;
    if (task_on_readylist(p))
        goto out;
    add_to_readylist(p);
    success = 1;
out:
    intr_set_status(old_int_status);
    return success;
}

/*
 * The core wakeup function.  Non-exclusive wakeups (nr_exclusive == 0) just
 * wake everything up.  If it's an exclusive wakeup (nr_exclusive == small +ve
 * number) then we wake all the non-exclusive tasks and one exclusive task.
 *
 * There are circumstances in which we can try to wake a task which has already
 * started to run but is not in state TASK_RUNNING.  try_to_wake_up() returns
 * zero in this (rare) case, and we handle it by contonuing to scan the queue.
 */
static inline void __wake_up_common(wait_queue_head_t *q,
                                    unsigned int mode,
                                    int nr_exclusive,
                                    const int sync)
{
    struct list_head *tmp;
    TCB_t *p;

    CHECK_MAGIC_WQHEAD(q);
    WQ_CHECK_LIST_HEAD(&q->task_list);

    list_for_each (tmp, &q->task_list) {
        unsigned int state;
        wait_queue_t *curr = list_entry(tmp, wait_queue_t, task_list);

        CHECK_MAGIC(curr->__magic);
        p = curr->task;
        state = p->status;
        if (state & mode) {
            WQ_NOTE_WAKER(curr);
            if (try_to_wake_up(p, sync))
                break;
        }
    }
}

void __wake_up(wait_queue_head_t *q, unsigned int mode, int nr)
{
    if (q) {
        __wake_up_common(q, mode, nr, 0);
    }
}

inline uint_32 wake_up_process(TCB_t * p)
{
	return try_to_wake_up(p, 0);
}

// this is callback function by timer
static void process_timeout(unsigned long __data)
{
	TCB_t * p = (TCB_t*) __data;
	wake_up_process(p);
}

/* int 0x20;
 * Interrupt handler for inner Clock
 **/
void inthandler20(void)
{
    timer_bh();
    ack(INT_VECTOR_INNER_CLOCK);

    TCB_t *cur_thread = running_thread();
    ASSERT(cur_thread->stack_magic == 0x19900921);
    cur_thread->elapsed_ticks++;
    /* ticks++; */
    (*(uint_32 *) &ticks)++;

    if (cur_thread->ticks == 0) {
        schedule();
    } else {
        cur_thread->ticks--;
    }

    return;
}

/**
 * schedule_timeout - sleep until timeout
 * @timeout: timeout value in ticks (aka jiffies)
 */
int_32 schedule_timeout(int_32 timeout)
{
	struct timer_list timer;
	unsigned long expire;
        TCB_t *current = running_thread();

	switch (timeout)
	{
	case MAX_SCHEDULE_TIMEOUT:
		/*
		 * These two special cases are useful to be comfortable
		 * in the caller. Nothing more. We could take
		 * MAX_SCHEDULE_TIMEOUT from one of the negative value
		 * but I' d like to return a valid offset (>=0) to allow
		 * the caller to do everything it want with the retval.
		 */
		schedule();
		goto out;
	default:
		/*
		 * Another bit of PARANOID. Note that the retval will be
		 * 0 since no piece of kernel is supposed to do a check
		 * for a negative retval of schedule_timeout() (since it
		 * should never happens anyway). You just have the printk()
		 * that will tell you if something is gone wrong and where.
		 */
		if (timeout < 0)
		{
			/* printk(KERN_ERR "schedule_timeout: wrong timeout " */
			/*        "value %lx from %p\n", timeout, */
			/*        __builtin_return_address(0)); */
			current->status = THREAD_TASK_RUNNING;
			goto out;
		}
	}

	expire = timeout + ticks;

	init_timer(&timer);
	timer.expires = expire;
	timer.data = (unsigned long) current;
	timer.function = process_timeout;

	add_timer(&timer);
	schedule();
	del_timer_sync(&timer);

	timeout = expire - ticks;

 out:
	return timeout < 0 ? 0 : timeout;

}

/*
 * Test this function entry timing (tick) with current ticks, if less than
 * target ticks number aka sleep_ticks, then yield current thread once.
 * */
static void tick_to_sleep(uint_32 sleep_ticks)
{
    uint_32 start_tick = ticks;
    while (ticks - start_tick < sleep_ticks) {
        thread_yield();
    }
}

void mtime_sleep(uint_32 m_seconds)
{
    ASSERT(mil_seconds_per_intr != 0);
    uint_32 sleep_ticks = DIV_ROUND_UP(m_seconds, mil_seconds_per_intr);
    ASSERT(sleep_ticks > 0);
    tick_to_sleep(sleep_ticks);
}
