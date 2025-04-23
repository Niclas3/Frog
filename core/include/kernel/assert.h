#ifndef _FROG_ASSERT_H
#define _FROG_ASSERT_H

#include <asm/bug.h>

#ifdef NDEBUG
        #define ASSERT(expr) ((void) 0)
#else
        #define ASSERT(condition) BUG_ON(!(condition))
#endif

#endif
