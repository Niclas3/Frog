#include <frog/timer.h>
#include <sys/int.h>

/*
 * Event timer code
 */
#define TVN_BITS 6
#define TVR_BITS 8
#define TVN_SIZE (1 << TVN_BITS)  // 2^6
#define TVR_SIZE (1 << TVR_BITS)  // 2^8
#define TVN_MASK (TVN_SIZE - 1)
#define TVR_MASK (TVR_SIZE - 1)

extern uint_32 volatile ticks;
struct timer_vec {
    int index;  // next time intr using timer_vec
    struct list_head vec[TVN_SIZE];
};

struct timer_vec_root {
    int index;
    struct list_head vec[TVR_SIZE];
};

struct timer_vec tv5;
struct timer_vec tv4;
struct timer_vec tv3;
struct timer_vec tv2;
struct timer_vec_root tv1;

struct timer_vec *const tvecs[] = {(struct timer_vec *) &tv1, &tv2, &tv3,
                                          &tv4, &tv5};

static struct list_head *run_timer_list_running;

#define NOOF_TVECS (sizeof(tvecs) / sizeof(tvecs[0]))

void init_timervecs (void)
{
	int i;

	for (i = 0; i < TVN_SIZE; i++) {
		INIT_LIST_HEAD(tv5.vec + i);
		INIT_LIST_HEAD(tv4.vec + i);
		INIT_LIST_HEAD(tv3.vec + i);
		INIT_LIST_HEAD(tv2.vec + i);
	}
	for (i = 0; i < TVR_SIZE; i++)
		INIT_LIST_HEAD(tv1.vec + i);
}

static unsigned long timer_jiffies;

static inline void internal_add_timer(struct timer_list *timer)
{
    /*
     * must be cli-ed when calling this
     */
    unsigned long expires = timer->expires;
    unsigned long idx = expires - timer_jiffies;
    struct list_head *vec;

    if (run_timer_list_running)
        vec = run_timer_list_running;
    else if (idx < TVR_SIZE) {
        int i = expires & TVR_MASK;
        vec = tv1.vec + i;
    } else if (idx < 1 << (TVR_BITS + TVN_BITS)) {
        int i = (expires >> TVR_BITS) & TVN_MASK;
        vec = tv2.vec + i;
    } else if (idx < 1 << (TVR_BITS + 2 * TVN_BITS)) {
        int i = (expires >> (TVR_BITS + TVN_BITS)) & TVN_MASK;
        vec = tv3.vec + i;
    } else if (idx < 1 << (TVR_BITS + 3 * TVN_BITS)) {
        int i = (expires >> (TVR_BITS + 2 * TVN_BITS)) & TVN_MASK;
        vec = tv4.vec + i;
    } else if ((signed long) idx < 0) {
        /* can happen if you add a timer with expires == jiffies,
         * or you set a timer to go off in the past
         */
        vec = tv1.vec + tv1.index;
    } else if (idx <= 0xffffffffUL) {
        int i = (expires >> (TVR_BITS + 3 * TVN_BITS)) & TVN_MASK;
        vec = tv5.vec + i;
    } else {
        /* Can only get here on architectures with 64-bit jiffies */
        INIT_LIST_HEAD(&timer->list);
        return;
    }
    /*
     * Timers are FIFO!
     */
    list_add(&timer->list, vec->prev);
}

void add_timer(struct timer_list *timer)
{
    enum intr_status old_status = intr_disable();
    if (timer_pending(timer)) {
        goto bug;
    }
    internal_add_timer(timer);
    intr_set_status(old_status);
    return;
bug:
    intr_set_status(old_status);
}

static inline int detach_timer(struct timer_list *timer)
{
    if (!timer_pending(timer))
        return 0;
    list_del(&timer->list);
    return 1;
}

int del_timer(struct timer_list *timer)
{
    int ret;
    enum intr_status old_status = intr_disable();

    ret = detach_timer(timer);
    timer->list.next = timer->list.prev = NULL;

    intr_set_status(old_status);
    return ret;
}

static inline void cascade_timers(struct timer_vec *tv)
{
    /* cascade all the timers from tv up one level */
    struct list_head *head, *curr, *next;

    head = tv->vec + tv->index;
    curr = head->next;
    /*
     * We are removing _all_ timers from the list, so we don't  have to
     * detach them individually, just clear the list afterwards.
     */
    while (curr != head) {
        struct timer_list *tmp;

        tmp = list_entry(curr, struct timer_list, list);
        next = curr->next;
        list_del(curr);  // not needed
        internal_add_timer(tmp);
        curr = next;
    }
    INIT_LIST_HEAD(head);
    tv->index = (tv->index + 1) & TVN_MASK;
}

static inline void run_timer_list(void)
{
    enum intr_status old_status = intr_disable();
    while ((long) (ticks - timer_jiffies) >= 0) {
        LIST_HEAD(queued);
        struct list_head *head, *curr;
        if (!tv1.index) {
            int n = 1;
            do {
                cascade_timers(tvecs[n]);
            } while (tvecs[n]->index == 1 && ++n < NOOF_TVECS);
        }
        run_timer_list_running = &queued;
    repeat:
        head = tv1.vec + tv1.index;
        curr = head->next;
        if (curr != head) {
            struct timer_list *timer;
            void (*fn)(unsigned long);
            unsigned long data;

            timer = list_entry(curr, struct timer_list, list);
            fn = timer->function;
            data = timer->data;

            detach_timer(timer);
            timer->list.next = timer->list.prev = NULL;
            timer_enter(timer);
            intr_set_status(old_status);

            fn(data);
            old_status = intr_disable();
            timer_exit();
            goto repeat;
        }
        run_timer_list_running = NULL;
        ++timer_jiffies;
        tv1.index = (tv1.index + 1) & TVR_MASK;

        curr = queued.next;
        while (curr != &queued) {
            struct timer_list *timer;
            timer = list_entry(curr, struct timer_list, list);
            curr = curr->next;
            internal_add_timer(timer);
        }
    }
    intr_set_status(old_status);
}

void timer_bh(void)
{
	run_timer_list();
}
