#ifndef __FS_SELECT_H
#define __FS_SELECT_H
#include <frog/types.h>
struct timeval;

extern uint_32 sys_wait2(int n, int_32 *fds, struct timeval *tvp);
#endif
