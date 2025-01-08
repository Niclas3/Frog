#include <asm/int.h>
#include <frog/interrupt.h>
#include <frog/types.h>

/**
 *  All interrupt real handlers table
 *  register c function into this global table*/
Inthandle_t *intr_table[IDT_DESC_CNT];

// register a function to
void register_intr_handler(uint_32 int_vector_code, Inthandle_t handler)
{
        intr_table[int_vector_code] = handler;
}
