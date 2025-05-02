#include <frog/interrupt.h>
#include <frog/irqflags.h>
#include <frog/softirq.h>
#include <kernel/cpu.h>
#include <kernel/debug.h>
#include <asm/interrupt.h>



// clear typical softirq type marked bits
void clear_softirq(uint_32 type)
{
        this_cpu()->softirq_pending &= ~(1UL << type);
}

void do_softirq(void)
{
        struct softirq_action *h = this_cpu()->softirq_handlers;
        uint_32 type = 0;
        uint_32 loop = 0;
        do {
                if (this_cpu()->softirq_pending & (1 << type)) {
                        clear_softirq(type);
                        h->action(h);
                }
                type++; // maybe here will be a bug. type and h will overflow
                h++;    // if you take care of softirq_pending that fine.
                if(loop >= MAX_SOFTIRQ_LOOP){
                        WARN("do_softirq: possible softirq storm, breaking after %d loops \n", loop);
                        break;
                }
        } while (this_cpu()->softirq_pending);
}

void register_softirq(uint_32 type, void (*handler)(struct softirq_action *))
{
        this_cpu()->softirq_handlers[type].action = handler;
}

// raise a softirq at a ISR when it should be mark as panding.
void raise_softirq(uint_32 type)
{
        this_cpu()->softirq_pending |= (1 << type);
}

// Enter IRQ (use interrupt stack) from this function
void irq_enter(void)
{
        struct cpu_local *cur_cpu = this_cpu();
        /* INFO("interrupt stack top: %x\n", cur_cpu->irq_stack_top); */

        //!! enter interrupt stack CPU per stack
}

// Exit IRQ (from interrupt stack to kernel stack) from this function
void irq_exit(void)
{

        // test if there is no softirq panding
        if (this_cpu()->softirq_pending) {
                local_irq_enable();
                do_softirq();
                local_irq_disable();
        }
}


void softirq_init(void) {}
