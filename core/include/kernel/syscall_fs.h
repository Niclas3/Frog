#ifndef __KERNEL_SYSCALL_FS_H
#define __KERNEL_SYSCALL_FS_H

#include <frog/types.h>

int_32 sys_open(const char *path, uint_8 flags);
int_32 sys_close(int_32 local_fd);
int_32 sys_read(int_32 fd, void *buf, uint_32 count);
int_32 sys_write(int_32 fd, const void *buf, uint_32 count);
int_32 sys_lseek(int_32 fd, int_32 offset, uint_8 whence);
int_32 sys_unlink(const char *path);
int_32 sys_mkdir(const char *path);

#endif
