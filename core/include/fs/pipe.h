#ifndef __FS_PIPE_H
#define __FS_PIPE_H
#include <ostype.h>
#define PIPE_FLAG 0xffffff

bool is_pipe(int_32 fd);
int_32 sys_pipe(int_32 pipefd[2]);

int_32 open_pipe(int_32 fd);
void close_pipe(int_32 fd);

uint_32 read_pipe(int_32 fd, void *buf, uint_32 count);
uint_32 write_pipe(int_32 fd, const void *buf, uint_32 count);
int_32 pipe_check(int_32 fd);

#endif
