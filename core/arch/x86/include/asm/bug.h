#ifndef _ARCH_X86_BUG_H
#define _ARCH_X86_BUG_H

#define HAVE_ARCH_BUG
void bug(const char *file, int line, const char *func);

#define BUG() bug(__FILE__, __LINE__, __func__)

#define BUG_ON(condition) do {if(condition) BUG(); }while(0)

#ifndef HAVE_ARCH_BUG
#include <asm-generic/bug.h>
#endif

#endif /*END ARCH_X86_BUG_H*/
