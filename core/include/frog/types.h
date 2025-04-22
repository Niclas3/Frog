#ifndef _FROG_TYPES_H
#define _FROG_TYPES_H

#include <stdbool.h>

// sizeof(uint_32); // 0x4
typedef unsigned int uint_32;

// sizeof(uint_16); // 0x2
typedef unsigned short uint_16;

// sizeof(uint_8);   // 0x1
typedef unsigned char uint_8;

// sizeof(uint_32); // 0x4
typedef int int_32;

// sizeof(uint_16); // 0x2
typedef short int_16;

// sizeof(uint_8);   // 0x1
typedef char int_8;

// sizeof(half_byte); //0x4
typedef uint_32 time_t;
typedef uint_32 suseconds_t;

typedef unsigned int size_t;
typedef unsigned int dev_t;

typedef void* (Inthandle_t)(void*);

typedef struct {
    unsigned int value: 4;
} half_byte;

#ifndef NULL
#define NULL ((void *) 0)
#endif


#if defined __STDC__ && defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L
/* have a C99 compiler */
typedef _Bool boolean;
#else
/* do not have a C99 compiler */
typedef unsigned char boolean;
#endif

/**
 * Define the Boolean macros only if they are not already defined.
 */
#ifndef __bool_true_false_are_defined
#define bool boolean
#define false 0 
#define true 1
#define __bool_true_false_are_defined 1
#endif /* __bool_true_false_are_defined */

#endif
