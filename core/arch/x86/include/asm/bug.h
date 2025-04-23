#ifndef _ARCH_X86_BUG_H
#define _ARCH_X86_BUG_H

#define HAVE_ARCH_BUG
#define BUG()                            \
        do {                             \
                __asm__ volatile("ud2"); \
        } while (0)

#define BUG_ON(condition) do {if(condition) BUG(); }while(0)

#ifndef HAVE_ARCH_BUG
#include <asm-generic/bug.h>
#endif

#endif /*END ARCH_X86_BUG_H*/
