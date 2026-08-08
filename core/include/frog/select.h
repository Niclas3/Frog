#ifndef __FS_SELECT_H
#define __FS_SELECT_H
#include <frog/poll.h>
#include <frog/types.h>

int_32 wait2(struct pollfd *fds, uint_32 count, int_32 timeout_ms);
#endif
