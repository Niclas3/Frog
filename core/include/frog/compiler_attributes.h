#ifndef _FROG_COMPILER_ATTRIBUTES_H
#define _FROG_COMPILER_ATTRIBUTES_H

/*
 * Note: users of __always_inline currently do not write "inline" themselves,
 * which seems to be required by gcc to apply the attribute according
 * to its docs (and also "warning: always_inline function might not be
 * inlinable [-Wattributes]" is emitted).
 *
 * gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-always_005finline-function-attribute
 * clang: mentioned
 */
#define __always_inline         inline __attribute__((__always_inline__))

/*
 * Optional: not supported by clang
 *
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-externally_005fvisible-function-attribute
 */
#if __has_attribute(__externally_visible__)
# define __visible                      __attribute__((__externally_visible__))
#else
# define __visible
#endif

#endif
