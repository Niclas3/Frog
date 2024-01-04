#pragma once
#include <ostype.h>
/* We define these the same for all machines.
   Changes from this to the outside world should be done in `_exit'.  */
#define	EXIT_FAILURE	1	/* Failing exit status.  */
#define	EXIT_SUCCESS	0	/* Successful exit status.  */

#ifndef pid_t
typedef uint_32 pid_t;
#endif
void sys_exit(int_32 status);
pid_t sys_wait(int_32 *status_loc);
