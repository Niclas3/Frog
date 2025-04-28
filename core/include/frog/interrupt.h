#ifndef _FROG_INTERRUPT_H
#define _FROG_INTERRUPT_H
#include <frog/types.h>
#include <asm/int.h>
#include <frog/softirq.h>

extern void register_intr_handler(uint_32 int_vector_code, Inthandle_t handler);

static inline void register_r0_intr_handler(uint_32 int_vector_code,
                                            Inthandle_t handler)
{
        register_intr_handler(int_vector_code, handler);
}

#endif
