#include <sys/syscall.h>

/* X is 0–6, which are  the  number  of  arguments taken by the system call */
// NUMBER sub-routine number
/* argN is the name of the Nth argument */
#define _syscall0(NUMBER)                                                     \
    ({                                                                        \
        int __res;                                                            \
        __asm__ volatile("int $0x93" : "=a"(__res) : "a"(NUMBER) : "memory"); \
        __res;                                                                \
    });

#define _syscall1(NUMBER, ARG1)                   \
    ({                                            \
        int __res;                                \
        __asm__ volatile("int $0x93"              \
                         : "=a"(__res)            \
                         : "a"(NUMBER), "b"(ARG1) \
                         : "memory");             \
        __res;                                    \
    });

#define _syscall2(NUMBER, ARG1, ARG2)             \
    ({                                            \
        int __res;                                \
        __asm__ volatile("int $0x93"              \
                         : "=a"(__res)            \
                         : "a"(NUMBER), "b"(ARG2) \
                         : "memory");             \
        __res;                                    \
    });

#define _syscall3(NUMBER, ARG1, ARG2, ARG3)                             \
    ({                                                                  \
        int __res;                                                      \
        __asm__ volatile("int $0x93"                                    \
                         : "=a"(__res)                                  \
                         : "a"(NUMBER), "b"(ARG1), "c"(ARG2), "d"(ARG3) \
                         : "memory");                                   \
        __res;                                                          \
    });

uint_32 getpid(void){
    return _syscall0(SYS_getpid);
}

uint_32 write(char* str){
    return _syscall1(SYS_write, str);
}
