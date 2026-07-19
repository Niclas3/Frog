#include <frog/interrupt.h>
#include <frog/irqflags.h>
#include <frog/softirq.h>
#include <kernel/cpu.h>
#include <kernel/debug.h>
#include <kernel/assert.h>
#include <asm/interrupt.h>



// clear typical softirq type marked bits
void clear_softirq(uint_32 type)
{
        if (type >= NR_SOFTIRQ)
                return;
        this_cpu()->softirq_pending &= ~(1UL << type);
}

void do_softirq(void)
{
        struct cpu_local *cpu = this_cpu();

        for (uint_32 loop = 0; loop < MAX_SOFTIRQ_LOOP; loop++) {
                unsigned long flags;
                local_irq_save(flags);
                uint_32 pending = cpu->softirq_pending &
                                  ((1UL << NR_SOFTIRQ) - 1);
                cpu->softirq_pending &= ~pending;
                local_irq_restore(flags);

                if (pending == 0)
                        return;
                for (uint_32 type = 0; type < NR_SOFTIRQ; type++) {
                        if (!(pending & (1UL << type)))
                                continue;
                        struct softirq_action *action =
                            &cpu->softirq_handlers[type];
                        if (action->action != NULL)
                                action->action(action);
                }
        }
        WARN("do_softirq: pending work remains after %d rounds\n",
             MAX_SOFTIRQ_LOOP);
}

void register_softirq(uint_32 type, void (*handler)(struct softirq_action *))
{
        ASSERT(type < NR_SOFTIRQ);
        ASSERT(handler != NULL);
        this_cpu()->softirq_handlers[type].action = handler;
}

// raise a softirq at a ISR when it should be mark as panding.
void raise_softirq(uint_32 type)
{
        ASSERT(type < NR_SOFTIRQ);
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
