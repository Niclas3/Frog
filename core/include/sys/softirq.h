#ifndef __SYS_SOFTIRQ_H
#define __SYS_SOFTIRQ_H

#include <ostype.h>

// all softirq types
enum {
    HI_SOFTIRQ = 0,
    TIMER_SOFTIRQ,
    NET_TX_SOFTIRQ,
    NET_RX_SOFTIRQ,
    BLOCK_SOFTIRQ,
    HRTIMER_SOFTIRQ,

    NR_SOFTIRQ
};


struct softirq_action{
    void (*action)(struct softirq_action*);
};



void softirq_init(void);

#endif
