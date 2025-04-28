#ifndef __SYS_SOFTIRQ_H
#define __SYS_SOFTIRQ_H
#include <frog/interrupt.h>

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

#define MAX_SOFTIRQ_LOOP 8

struct softirq_action {
        void (*action)(struct softirq_action *);
};
extern void softirq_init(void);

// Send EOI to interrupt controller 
// @intno number of int vector what interrupt call it
extern void ack(uint_32 intno);

// enter interrupt stack and some irq hooks
extern void irq_enter(void);
// exit ISR
extern void irq_exit(void);

//Softirq
// this function to pick a softirq action and invoke it.
extern void do_softirq(void);

// register a function when a type of softirq raised invoke that function by
// do_softirq()

extern void register_softirq(uint_32 type, void (*handler)(struct softirq_action *));

// raise a softirq at a ISR when it should be mark as panding.
extern void raise_softirq(uint_32 type);

// clear typical softirq type marked bits
extern void clear_softirq(uint_32 type);

#endif
