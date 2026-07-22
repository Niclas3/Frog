#ifndef _FROG_UACCESS_H
#define _FROG_UACCESS_H

#include <frog/types.h>

/* A zero-length range is valid and neither copy helper accesses its pointers. */
bool access_ok(const void *user_ptr, uint_32 size);

/* Return zero on success or -EFAULT without performing a partial copy. */
int_32 copy_from_user(void *kernel_dst, const void *user_src, uint_32 size);
int_32 copy_to_user(void *user_dst, const void *kernel_src, uint_32 size);

/* capacity includes the trailing NUL; length_out excludes it. */
int_32 copy_string_from_user(char *kernel_dst, const char *user_src,
                            uint_32 capacity, uint_32 *length_out);

#endif
