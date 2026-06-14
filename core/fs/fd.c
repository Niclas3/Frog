#include <kernel/fd.h>
#include <frog/threads.h>

struct file *g_file_table[MAX_FILE_OPEN];

int fd_alloc(struct file *f)
{
        int global_fd = -1;
        for (int i = 3; i < MAX_FILE_OPEN; i++) {
                if (!g_file_table[i]) {
                        g_file_table[i] = f;
                        global_fd = i;
                        break;
                }
        }
        if (global_fd == -1)
                return -1;

        TCB_t *t = running_thread();
        for (int i = 3; i < MAX_FILES_OPEN_PER_PROC; i++) {
                if (t->fd_table[i] == -1) {
                        t->fd_table[i] = global_fd;
                        return i;
                }
        }
        g_file_table[global_fd] = NULL;
        return -1;
}

struct file *fd_get(int local_fd)
{
        if (local_fd < 0 || local_fd >= MAX_FILES_OPEN_PER_PROC)
                return NULL;
        int global_fd = running_thread()->fd_table[local_fd];
        if (global_fd < 0 || global_fd >= MAX_FILE_OPEN)
                return NULL;
        return g_file_table[global_fd];
}

void fd_put(int local_fd)
{
        if (local_fd < 0 || local_fd >= MAX_FILES_OPEN_PER_PROC)
                return;
        TCB_t *t = running_thread();
        int global_fd = t->fd_table[local_fd];
        if (global_fd >= 0 && global_fd < MAX_FILE_OPEN)
                g_file_table[global_fd] = NULL;
        t->fd_table[local_fd] = -1;
}
