#include <kernel/fd.h>
#include <frog/errno.h>
#include <frog/irqflags.h>
#include <frog/threads.h>
#include <kernel/vfs.h>

struct file *g_file_table[MAX_FILE_OPEN];

int fd_alloc(struct file *f)
{
        if (!f)
                return -EINVAL;
        unsigned long flags;
        local_irq_save(flags);
        int global_fd = -1;
        for (int i = 0; i < MAX_FILE_OPEN; i++) {
                if (!g_file_table[i]) {
                        g_file_table[i] = f;
                        global_fd = i;
                        break;
                }
        }
        if (global_fd == -1) {
                local_irq_restore(flags);
                return -ENFILE;
        }

        TCB_t *t = running_thread();
        for (int i = 0; i < MAX_FILES_OPEN_PER_PROC; i++) {
                if (t->fd_table[i] == -1) {
                        t->fd_table[i] = global_fd;
                        local_irq_restore(flags);
                        return i;
                }
        }
        g_file_table[global_fd] = NULL;
        local_irq_restore(flags);
        return -EMFILE;
}

struct file *fd_get(int local_fd)
{
        if (local_fd < 0 || local_fd >= MAX_FILES_OPEN_PER_PROC)
                return NULL;
        unsigned long flags;
        local_irq_save(flags);
        int global_fd = running_thread()->fd_table[local_fd];
        if (global_fd < 0 || global_fd >= MAX_FILE_OPEN) {
                local_irq_restore(flags);
                return NULL;
        }
        struct file *f = g_file_table[global_fd];
        local_irq_restore(flags);
        return f;
}

int fd_release(int local_fd, struct file **last_file)
{
        if (!last_file)
                return -EINVAL;
        *last_file = NULL;
        if (local_fd < 0 || local_fd >= MAX_FILES_OPEN_PER_PROC)
                return -EBADF;
        unsigned long flags;
        local_irq_save(flags);
        TCB_t *t = running_thread();
        int global_fd = t->fd_table[local_fd];
        if (global_fd < 0 || global_fd >= MAX_FILE_OPEN) {
                local_irq_restore(flags);
                return -EBADF;
        }

        struct file *f = g_file_table[global_fd];
        if (!f) {
                t->fd_table[local_fd] = -1;
                local_irq_restore(flags);
                return -EBADF;
        }
        if (!f->f_count) {
                t->fd_table[local_fd] = -1;
                g_file_table[global_fd] = NULL;
                local_irq_restore(flags);
                return -EUCLEAN;
        }

        t->fd_table[local_fd] = -1;
        f->f_count--;
        if (!f->f_count) {
                g_file_table[global_fd] = NULL;
                *last_file = f;
        }

        local_irq_restore(flags);
        return 0;
}

void fd_retain(struct file *f)
{
        if (!f)
                return;
        unsigned long flags;
        local_irq_save(flags);
        f->f_count++;
        local_irq_restore(flags);
}
