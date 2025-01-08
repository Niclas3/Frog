#ifndef _FROG_BITS_H
#define _FROG_BITS_H

#include <asm/bitsperlong.h>
#include <frog/const.h>

/* 
 * generate a mask (all set 1) from h to l
 * e.g.
 * _GENMASK(5, 3) -> 0b00111000
 * */
#define _GENMASK(h, l) \
    (((~_UL(0)) - (_UL(1) << (l)) + 1) &  \
     (~_UL(0) >> (BITS_PER_LONG - 1 - (h))))

#define BIT_MAST(n)
#define BIT_ULL_MASK(n)
#define BIT_WORD(n)
#define BIT_ULL_WORD(n)
#define BITS_PER_BYTE 8
#define GENMASK_INTPUT_CHECK(h, l)
#endif
