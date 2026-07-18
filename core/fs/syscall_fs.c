#include <frog/errno.h>
#include <frog/types.h>
#include <kernel/fd.h>
#include <kernel/syscall_fs.h>
#include <kernel/vfs.h>

int_32 sys_open(const char *path, uint_32 flags)
{
        struct file *f = NULL;
        int ret = vfs_open_file(path, flags, &f);
        if (ret < 0)
                return ret;
        int fd = fd_alloc(f);
        if (fd < 0)
                vfs_close(f);
        return fd;
}

int_32 sys_close(int_32 local_fd)
{
        struct file *last = NULL;
        int ret = fd_release(local_fd, &last);
        if (ret < 0)
                return ret;
        return last ? vfs_close(last) : 0;
}

int_32 sys_read(int_32 fd, void *buf, uint_32 count)
{
        struct file *f = fd_get(fd);
        if (!f)
                return -EBADF;
        return vfs_read(f, buf, count);
}

int_32 sys_write(int_32 fd, const void *buf, uint_32 count)
{
        struct file *f = fd_get(fd);
        if (!f)
                return -EBADF;
        return vfs_write(f, buf, count);
}

int_32 sys_lseek(int_32 fd, int_32 offset, uint_8 whence)
{
        struct file *f = fd_get(fd);
        if (!f)
                return -EBADF;
        return vfs_lseek(f, offset, whence);
}

int_32 sys_unlink(const char *path)
{
        return vfs_unlink_path(path);
}

int_32 sys_mkdir(const char *path)
{
        return vfs_mkdir_path(path);
}

int_32 sys_rmdir(const char *path)
{
        return vfs_rmdir_path(path);
}

int_32 sys_ioctl(int_32 fd, uint_32 request, void *argp)
{
        struct file *f = fd_get(fd);
        if (!f)
                return -EBADF;
        return vfs_ioctl(f, request, argp);
}
