#ifndef __KERNEL_FD_H
#define __KERNEL_FD_H

#define MAX_FILE_OPEN 128

struct file;
extern struct file *g_file_table[MAX_FILE_OPEN];

int          fd_alloc(struct file *f);
struct file *fd_get(int local_fd);
int          fd_release(int local_fd, struct file **last_file);
void         fd_retain(struct file *f);

#endif
