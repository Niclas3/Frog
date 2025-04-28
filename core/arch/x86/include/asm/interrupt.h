#ifndef _ASM_INTERRUPT_H
#define _ASM_INTERRUPT_H
#include <frog/compiler.h>
#include <kernel/cpu.h>

static __always_inline void arch_ack(unsigned int intno)
{
        // send EOI to PIC
        // if intno is bigger than 7  you
        // should send EOI to slave chip first then master
        /* mov al, %4 */
        /* out 0xa0, al  ;; send ack to slaver  0x60+number*/
        /* mov al, %3 */
        /* out 0x20, al  ;; send ack to master  0x60+number*/
        int irqs_num = intno ^ 0x20;
        int port = 0x20;
        if (intno > 0x7) {
                // send to slave PIC
                __asm__ volatile("out %0, %1" ::"a"(0x20), "Nd"(0xa0));
        }
        // send to master PIC
        __asm__ volatile("out %0, %1" ::"a"(0x20), "Nd"(0x20));
};

static __always_inline void ____enter_intr_stack(void)
{
        struct cpu_local *cpu = this_cpu();
        __asm__ volatile("movl %%esp, %0\n\t"
                         "movl %1, %%esp\n\t"
                         :"=m" (cpu->saved_esp)
                         :"r" (cpu->irq_stack_top)
                         );
}

static __always_inline void ____exit_intr_stack(void)
{

        struct cpu_local *cpu = this_cpu();
        __asm__ volatile("movl %0, %%esp\n\t"
                        :
                        : "m" (cpu->saved_esp));
}

#endif
