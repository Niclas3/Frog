#ifndef OSTYPE_H
#define OSTYPE_H

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
typedef struct {
    unsigned int value: 4;
} half_byte;

#endif
