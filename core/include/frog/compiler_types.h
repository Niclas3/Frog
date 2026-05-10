#ifndef _FROG_COMPILER_TYPES_H
#define _FROG_COMPILER_TYPES_H
#include <frog/compiler_attributes.h>
/**
 * This file is for all different compilers like gcc clangs attributes
 * for now just `gcc`
 *
 * INCLUDE THIS FILE
 * if you want some compiler attrubutes or types.
 * */

/* Indirect macros required for expanded argument pasting, eg. __LINE__. */
#define ___PASTE(a,b) a##b
#define __PASTE(a,b) ___PASTE(a,b)

/* test two types the same type */
#define __same_type(a, b) __builtin_types_compatible_p(typeof(a), typeof(b))

#define STATIC_ASSERT(cond, msg) \
    typedef char static_assertion_##msg[(cond) ? 1 : -1]

#endif
