#ifndef __FROG_TIMER_H
#define __FROG_TIMER_H
#include <ostype.h>
#include <list.h>

struct timer_list {
	struct list_head list;
	uint_32 expires;
	uint_32 data;             
	void (*function)(uint_32);
};

extern void add_timer(struct timer_list * timer);
extern int del_timer(struct timer_list * timer);

#ifdef CONFIG_SMP
extern int del_timer_sync(struct timer_list * timer);
extern void sync_timers(void);
#else
#define timer_enter(t)		do { } while (0)
#define timer_exit()		do { } while (0)
#define del_timer_sync(t)	del_timer(t)
#define sync_timers()		do { } while (0)
#endif

static inline void init_timer(struct timer_list * timer)
{
	timer->list.next = timer->list.prev = NULL;
}

static inline int timer_pending (const struct timer_list * timer)
{
	return timer->list.next != NULL;
}

void timer_bh(void);
#endif
