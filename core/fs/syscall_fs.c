#include <frog/memory.h>
#include <frog/string.h>
#include <frog/types.h>
#include <kernel/fd.h>
#include <kernel/syscall_fs.h>
#include <kernel/vfs.h>

int_32 sys_open(const char *path, uint_8 flags)
{
        struct file *f = vfs_open((char *)path, flags);
        if (!f)
                return -1;
        int fd = fd_alloc(f);
        if (fd == -1)
                vfs_close(f);
        return fd;
}

int_32 sys_close(int_32 local_fd)
{
        struct file *f = fd_get(local_fd);
        if (!f)
                return -1;
        fd_put(local_fd);
        return vfs_close(f);
}

int_32 sys_read(int_32 fd, void *buf, uint_32 count)
{
        struct file *f = fd_get(fd);
        if (!f)
                return -1;
        return vfs_read(f, buf, count);
}

int_32 sys_write(int_32 fd, const void *buf, uint_32 count)
{
        struct file *f = fd_get(fd);
        if (!f)
                return -1;
        return vfs_write(f, buf, count);
}

int_32 sys_lseek(int_32 fd, int_32 offset, uint_8 whence)
{
        struct file *f = fd_get(fd);
        if (!f)
                return -1;
        return vfs_lseek(f, offset, whence);
}

int_32 sys_unlink(const char *path)
{
        struct dentry *d = vfs_lookup(path);
        if (!d)
                return -1;
        return vfs_unlink(d);
}

int_32 sys_mkdir(const char *path)
{
        uint_32 len = strlen(path);
        char *copy = kmalloc(len + 1);
        if (!copy)
                return -1;
        strcpy(copy, path);

        int slash = (int)len - 1;
        while (slash >= 0 && copy[slash] != '/')
                slash--;
        if (slash < 0) {
                kfree(copy);
                return -1;
        }

        copy[slash] = '\0';
        const char *parent_path = (slash == 0) ? "/" : copy;
        const char *name = copy + slash + 1;

        struct dentry *parent = vfs_lookup(parent_path);
        if (!parent) {
                kfree(copy);
                return -1;
        }

        struct dentry *child = kmalloc(sizeof(struct dentry));
        if (!child) {
                kfree(copy);
                return -1;
        }
        child->d_name = kmalloc(strlen(name) + 1);
        if (!child->d_name) {
                kfree(child);
                kfree(copy);
                return -1;
        }
        strcpy(child->d_name, name);
        child->d_parent = parent;
        child->d_inode = NULL;
        child->d_mounted = false;
        child->d_type = FT_DIRECTORY;
        INIT_LIST_HEAD(&child->d_subdirs);
        INIT_LIST_HEAD(&child->d_child_node);

        int_32 ret = vfs_mkdir(parent, child);
        kfree(copy);
        return ret;
}
