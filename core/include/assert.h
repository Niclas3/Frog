#ifndef _FROG_ASSERT_H
#define _FROG_ASSERT_H

#include <asm/bug.h>

#define ASSERT(condition) BUG_ON(!(condition))

#endif
