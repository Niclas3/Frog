#pragma once
#include <frog/types.h>
/* We define these the same for all machines.
   Changes from this to the outside world should be done in `_exit'.  */
#define	EXIT_FAILURE	1	/* Failing exit status.  */
#define	EXIT_SUCCESS	0	/* Successful exit status.  */

void sys_exit(int_32 status);
void set_init_process_pid(pid_t pid);
pid_t sys_wait(int_32 *status_loc);
