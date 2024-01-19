#ifndef __FROG_PIT8253_H
#define __FROG_PIT8253_H
#include <ostype.h>

// #define IRQ0_FREQUENCY   100
// #define IRQ0_FREQUENCY   1000
#define IRQ0_FREQUENCY   9000
// #define IRQ0_FREQUENCY   12000

// init PIT (programmable interval timer)
void init_PIT8253(void);

#endif
