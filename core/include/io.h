#ifndef __LIB_IO_H
#define __LIB_IO_H
#include <ostype.h>

static inline void outb(uint_16 port, uint_8 data) {
   __asm__ volatile ( "outb %b0, %w1" : : "a" (data), "Nd" (port));
}

static inline uint_8 inb(uint_16 port) {
   uint_8 data;
   __asm__ volatile ("inb %w1, %b0" : "=a" (data) : "Nd" (port));
   return data;
}

//write data from addr to port
static inline void outsw(uint_16 port, const void* addr, uint_32 word_cnt) {
   __asm__ volatile ("cld; rep outsw" : "+S" (addr), "+c" (word_cnt) : "d" (port));
}

//read data from addr to port
static inline void insw(uint_16 port, void* addr, uint_32 word_cnt) {
   __asm__ volatile ("cld; rep insw" : "+D" (addr), "+c" (word_cnt) : "d" (port) : "memory");
}

#endif
