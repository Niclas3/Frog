#ifndef __KERNEL_FD_H
#define __KERNEL_FD_H

#define MAX_FILE_OPEN 128

struct file;
extern struct file *g_file_table[MAX_FILE_OPEN];

int          fd_alloc(struct file *f);
struct file *fd_get(int local_fd);
void         fd_put(int local_fd);

#endif
