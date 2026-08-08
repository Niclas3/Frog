#ifndef _ARCH_X86_i8253_H
#define _ARCH_X86_i8253_H
#include <const.h>
#include <frog/types.h>

#define PIT_INPUT_FREQUENCY  1193180U
#define PIT_COUNTER0_DIVISOR (PIT_INPUT_FREQUENCY / HZ)

// init PIT (programmable interval timer)
void init_PIT8253(void);

#ifdef CONFIG_FROG_TEST_TIME
void i8253_regression_test(void);
#endif

#endif
