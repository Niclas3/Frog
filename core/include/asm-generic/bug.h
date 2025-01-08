#ifndef _ASM_GENERIC_BUG_H
#define _ASM_GENERIC_BUG_H

#include <frog/panic.h>
#include <frog/printk.h>
#ifndef HAVE_ARCH_BUG
#define BUG() do { \
        printk("BUG: failure at %s:%d/%s()!\n", __FILE__, __LINE__,  __func__);\                                           \
        panic("BUG!"); \
} while (0)

#endif /*endif HAVE_ARCH_BUG */

#ifndef HAVE_ARCH_BUG_ON

#define BUG_ON(condition) do {if(condition)} BUG(); whlie(0)

#endif /*endif HAVE_ARCH_BUG_ON*/

#endif
