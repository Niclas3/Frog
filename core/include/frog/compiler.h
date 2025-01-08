#ifndef _FROG_COMPILER_H
#define _FROG_COMPILER_H
#include <frog/compiler_types.h>
/* 
 * I'am so curious about likely() things
 * first things first, 
 * long __builtin_expect(long exp, long c);
 * is a a builtin function in gcc.
 * It takes a expression and a mean;
 * if c == 1, it will happen.
 *    c == 0, it will not happen.
 *
 * BTW, ```!!(x)```, `!!` converts result of expression to `1` if the
 * result of expression is not `0`. 
 * according to C99 iso 6.5.3.3-5
 * The result of the logical negation operator ! is 0 if the value 
 * of its operand compares unequal to 0, 1 if the value of its operand compares
 * equal to 0. The result has type int.
 */
# define likely(x)	__builtin_expect(!!(x), 1)
# define unlikely(x)	__builtin_expect(!!(x), 0)


/*
 * generate a unique id when use macro
 * __COUNTER__ is gcc support, it can increase itself when it used in code.
 * __PASTE just `##` connects two symbol
 */
#define __UNIQUE_ID(prefix) __PASTE(__PASTE(__UNIQUE_ID_, prefix), __COUNTER__)

#endif
