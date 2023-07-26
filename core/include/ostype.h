#ifndef OSTYPE_H
#define OSTYPE_H

// sizeof(int_32); // 0x4
typedef unsigned int int_32;

// sizeof(int_16); // 0x2
typedef short int_16;

// sizeof(int_8);   // 0x1
typedef unsigned char int_8;

// sizeof(half_byte); //0x4
typedef struct {
    unsigned int value: 4;
} half_byte;

#endif
