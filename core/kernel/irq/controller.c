#include <asm/int.h>
#include <asm/interrupt.h>
#include <frog/interrupt.h>
#include <frog/spinlock.h>
#include <frog/threads.h>
#include <frog/types.h>

#include <kernel/debug.h>

extern void do_isr(int int_nr);
/**
 *  All interrupt real handlers table
 *  register c function into this global table*/
Inthandle_t *intr_table[IDT_DESC_CNT];

// register a function to
void register_intr_handler(uint_32 int_vector_code, Inthandle_t handler)
{
        intr_table[int_vector_code] = handler;
}

extern void do_isr(int int_nr)
{
        Inthandle_t *ISR_handler = intr_table[int_nr];
        if (in_interrupt()) {
                this_cpu()->in_irq++;  // for now no need to add spinlock
                ISR_handler((void *) int_nr);
                this_cpu()->in_irq--;
        } else {
                this_cpu()->in_irq++;  // for now no need to add spinlock
                this_cpu()->current_thread = running_thread();
                ____enter_intr_stack();
                ISR_handler((void *) int_nr);
                ____exit_intr_stack();

                /* INFO("%s interrupt stack top: %x\n",((TCB_t *)this_cpu()->current_thread)->name, this_cpu()->irq_stack_top); */
                if (((TCB_t *) this_cpu()->current_thread)->need_schedule == true) {
                        this_cpu()->in_irq--;
                        schedule();
                } else {
                        this_cpu()->in_irq--;
                }
        }

}
