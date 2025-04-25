#ifndef _FROG_KERNEL_CPU_H
#define _FROG_KERNEL_CPU_H

#include <frog/types.h>
#include <frog/softirq.h>

#define MAX_CPU 1

//`g_softirq_pending` this global variable is for marking different types
// softirqs

// actual softirq action handler
// register by register_softirq()
// this is a global array

struct cpu_local {
        uint_32 in_irq;           // current layer of interrupts
        uint_32 softirq_pending;  // are there softirq need execute
        struct softirq_action softirq_handlers[NR_SOFTIRQ]; // softirq handlers 
        void * irq_stack_top;     // top of interrupt stack
        void * current_thread;    // (optional )
};

extern struct cpu_local cpu_locals[MAX_CPU];

static inline struct cpu_local *this_cpu(void){
        return &cpu_locals[0];
}

#endif
