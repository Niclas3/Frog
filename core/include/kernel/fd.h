#ifndef __KERNEL_FD_H
#define __KERNEL_FD_H

#define MAX_FILE_OPEN 128

struct file;
struct thread_control_block;
extern struct file *g_file_table[MAX_FILE_OPEN];

/* Success transfers the caller's strong file reference to the descriptor. */
int fd_alloc(struct file *file);
/* Returns a temporary strong reference that the caller must file_put(). */
struct file *fdget(int local_fd);
int fd_close(int local_fd);
int fd_close_for(struct thread_control_block *thread, int local_fd);
void fd_close_all(struct thread_control_block *thread);
void fd_close_cloexec(struct thread_control_block *thread);
int fd_retain_table(struct thread_control_block *thread);

#endif
