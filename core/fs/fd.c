#include <kernel/fd.h>

#include <frog/errno.h>
#include <frog/irqflags.h>
#include <frog/kernel.h>
#include <frog/threads.h>
#include <kernel/assert.h>
#include <kernel/vfs.h>

struct file *g_file_table[MAX_FILE_OPEN];

int fd_alloc(struct file *file)
{
        if (file == NULL || refcount_read(&file->f_refs) == 0)
                return -EINVAL;

        TCB_t *thread = running_thread();
        unsigned long flags;
        int local_fd = -1;
        int global_fd = -1;
        int free_global_fd = -1;

        local_irq_save(flags);
        for (int index = 0; index < MAX_FILES_OPEN_PER_PROC; index++) {
                if (thread->fd_table[index] == -1) {
                        local_fd = index;
                        break;
                }
        }
        if (local_fd == -1) {
                local_irq_restore(flags);
                return -EMFILE;
        }

        for (int index = 0; index < MAX_FILE_OPEN; index++) {
                if (g_file_table[index] == file) {
                        if (global_fd != -1) {
                                local_irq_restore(flags);
                                return -EUCLEAN;
                        }
                        global_fd = index;
                } else if (g_file_table[index] == NULL &&
                           free_global_fd == -1) {
                        free_global_fd = index;
                }
        }

        if (file->f_count == UINT_MAX) {
                local_irq_restore(flags);
                return -EMFILE;
        }
        if (file->f_count == 0) {
                if (global_fd != -1) {
                        local_irq_restore(flags);
                        return -EUCLEAN;
                }
                if (free_global_fd == -1) {
                        local_irq_restore(flags);
                        return -ENFILE;
                }
                global_fd = free_global_fd;
                g_file_table[global_fd] = file;
        } else if (global_fd == -1) {
                local_irq_restore(flags);
                return -EUCLEAN;
        }

        file->f_count++;
        thread->fd_table[local_fd] = global_fd;
        local_irq_restore(flags);
        return local_fd;
}

struct file *fdget(int local_fd)
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
        struct file *file = g_file_table[global_fd];
        if (file == NULL || file->f_count == 0 || !file_get_live(file)) {
                local_irq_restore(flags);
                return NULL;
        }
        local_irq_restore(flags);
        return file;
}

static int fd_detach(TCB_t *thread, int local_fd, struct file **file_out)
{
        if (thread == NULL || file_out == NULL)
                return -EINVAL;
        *file_out = NULL;
        if (local_fd < 0 || local_fd >= MAX_FILES_OPEN_PER_PROC)
                return -EBADF;

        unsigned long flags;
        local_irq_save(flags);
        int global_fd = thread->fd_table[local_fd];
        if (global_fd == -1) {
                local_irq_restore(flags);
                return -EBADF;
        }
        if (global_fd < 0 || global_fd >= MAX_FILE_OPEN) {
                thread->fd_table[local_fd] = -1;
                local_irq_restore(flags);
                return -EUCLEAN;
        }

        struct file *file = g_file_table[global_fd];
        if (file == NULL || file->f_count == 0) {
                thread->fd_table[local_fd] = -1;
                if (file != NULL)
                        g_file_table[global_fd] = NULL;
                local_irq_restore(flags);
                return -EUCLEAN;
        }

        thread->fd_table[local_fd] = -1;
        file->f_count--;
        if (file->f_count == 0)
                g_file_table[global_fd] = NULL;
        *file_out = file;
        local_irq_restore(flags);
        return 0;
}

int fd_close_for(TCB_t *thread, int local_fd)
{
        struct file *file = NULL;
        int result = fd_detach(thread, local_fd, &file);

        if (result != 0)
                return result;
        return file_put(file);
}

int fd_close(int local_fd)
{
        return fd_close_for(running_thread(), local_fd);
}

void fd_close_all(TCB_t *thread)
{
        if (thread == NULL)
                return;
        for (int local_fd = 0; local_fd < MAX_FILES_OPEN_PER_PROC; local_fd++) {
                if (thread->fd_table[local_fd] != -1)
                        (void) fd_close_for(thread, local_fd);
        }
}

int fd_retain_table(TCB_t *thread)
{
        if (thread == NULL)
                return -EINVAL;

        struct file *retained[MAX_FILES_OPEN_PER_PROC];
        uint_32 retained_count = 0;
        int result = 0;
        unsigned long flags;

        local_irq_save(flags);
        for (int local_fd = 0; local_fd < MAX_FILES_OPEN_PER_PROC; local_fd++) {
                int global_fd = thread->fd_table[local_fd];

                if (global_fd == -1)
                        continue;
                if (global_fd < 0 || global_fd >= MAX_FILE_OPEN) {
                        result = -EUCLEAN;
                        goto rollback;
                }

                struct file *file = g_file_table[global_fd];
                if (file == NULL || file->f_count == 0) {
                        result = -EUCLEAN;
                        goto rollback;
                }
                if (file->f_count == UINT_MAX) {
                        result = -EMFILE;
                        goto rollback;
                }
                if (!file_get_live(file)) {
                        result = -EUCLEAN;
                        goto rollback;
                }
                file->f_count++;
                retained[retained_count++] = file;
        }
        local_irq_restore(flags);
        return 0;

rollback:
        for (uint_32 index = 0; index < retained_count; index++) {
                ASSERT(retained[index]->f_count > 1);
                retained[index]->f_count--;
        }
        local_irq_restore(flags);
        for (uint_32 index = 0; index < retained_count; index++)
                (void) file_put(retained[index]);
        return result;
}
