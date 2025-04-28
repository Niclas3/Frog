#ifndef _FROG_KERNEL_CPU_H
#define _FROG_KERNEL_CPU_H

#include <frog/types.h>
#include <frog/softirq.h>

#define MAX_CPU 1

/* I map irq_stack at mem_init() in file mm/memory.c
 * */
struct cpu_local {
        uint_32 in_irq;           // current layer of interrupts
        uint_32 softirq_pending;  // are there softirq need execute
        struct softirq_action softirq_handlers[NR_SOFTIRQ]; // softirq handlers 
        void * irq_stack_top;     // top of interrupt stack
        void * saved_esp;         // esp need recover
        void * current_thread;    // (optional )
};

extern struct cpu_local cpu_locals[MAX_CPU];

static inline struct cpu_local *this_cpu(void){
        return &cpu_locals[0];
}


#define each_cpu(cpu) \
    for (int __cpu_idx = 0; __cpu_idx < MAX_CPU && ((cpu) = &cpu_locals[__cpu_idx], 1); __cpu_idx++)


#endif
