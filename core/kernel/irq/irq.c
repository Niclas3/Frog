#include <frog/interrupt.h>
#include <asm/interrupt.h>
/**
 * This irq.c is full of hardware irq routine.
 *
 *****************************************************************************/
void ack(uint_32 intno)
{
        arch_ack(intno);
}

