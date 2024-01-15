#ifndef __FS_SELECT_H
#define __FS_SELECT_H
#include <ostype.h>
#include <sys/time.h>

uint_32 sys_wait2(int n, int_32 *fds, struct timeval *tvp);
#endif
