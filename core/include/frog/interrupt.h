#ifndef _FROG_INTERRUPT_H
#define _FROG_INTERRUPT_H
#include <frog/types.h>
#include <asm/int.h>

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


struct softirq_action {
        void (*action)(struct softirq_action *);
};
extern void softirq_init(void);

// Send EOI to interrupt controller 
// @intno number of int vector what interrupt call it
extern void ack(uint_32 intno);

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



extern void register_intr_handler(uint_32 int_vector_code, Inthandle_t handler);

static inline void register_r0_intr_handler(uint_32 int_vector_code,
                                            Inthandle_t handler)
{
        register_intr_handler(int_vector_code, handler);
}

#endif
