#ifndef _FROG_KERNEL_WAIT2_H
#define _FROG_KERNEL_WAIT2_H

#include <frog/poll.h>
#include <frog/types.h>

int_32 sys_wait2(struct pollfd *user_fds, uint_32 count, int_32 timeout_ms);

#ifdef CONFIG_FROG_TEST_WAIT2
int_32 wait2_test_command(uint_32 command);
#endif

#endif
