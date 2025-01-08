#ifndef _TYPECHECK_H
#define _TYPECHECK_H
#include <frog/compiler.h>

#define typecheck(type, a)                  \
        ({                                  \
                type __tmp;                 \
                typeof(a) __tmp2;           \
                (void) (&__tmp == &__tmp2); \
                1;                           \
        })

#define typecheck_pointer(x)     \
        ({                       \
                typeof(x) __tmp; \
                (void) sizeof(*__tmp);  \
                1;                \
        })

#define typecheck_func(type, func) \
        ({                         \
                type __tmp;        \
                __tmp = func;      \
                1;                  \
        })

#endif
