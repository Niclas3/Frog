#include <sys/softirq.h>
#include <sys/int.h>

//`g_softirq_pending` this global variable is for marking different types
// softirqs
uint_32 g_softirq_pending = 0x0;

// actual softirq action handler
// register by register_softirq()
// this is a global array 
struct softirq_action g_softirq_handlers[NR_SOFTIRQ];

// clear typical softirq type marked bits
void clear_softirq(uint_32 type)
{
    g_softirq_pending &= ~(1UL << type);
}

void do_softirq(void)
{
    struct softirq_action *h = g_softirq_handlers;
    do {
        uint_32 type = 0;
        if (g_softirq_pending & (1 << type)) {
	    clear_softirq(type);
            h->action(h);
        }
	type++;
	h++;
    } while (g_softirq_pending);
}

void register_softirq(uint_32 type, void (*handler)(struct softirq_action *))
{
    g_softirq_handlers[type].action = handler;
}

// raise a softirq at a ISR when it should be mark as panding.
void raise_softirq(uint_32 type)
{
    g_softirq_pending |= (1 << type);
}

// Exit IRQ from this function
void irq_exit(void)
{
    // test if there is no softirq panding 
    if (g_softirq_pending) {
        enum intr_status old_status = intr_enable();
        do_softirq();
        intr_set_status(old_status);
    }

    //test TIF_NEED_RESCHED
}


void softirq_init(void) {}
