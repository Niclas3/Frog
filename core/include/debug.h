#ifndef __OS_DEBUG_H
#define __OS_DEBUG_H

void panic_print(char* filename, int line, const char* func, const char* condition);

#define PAINC(...) panic_print(__FILE__, __LINE__, __func__, __VA_ARGS__)

#ifdef NDEBUG
    #define ASSERT(CONDITION) ((void))
#else
#define ASSERT(CONDITION)\
        if(CONDITION){} else{\
            PAINC(#CONDITION);\
        }
#endif
#endif
