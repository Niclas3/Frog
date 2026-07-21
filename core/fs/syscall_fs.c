#include <frog/errno.h>
#include <frog/fcntl.h>
#include <frog/irqflags.h>
#include <frog/memory.h>
#include <frog/mman.h>
#include <frog/threads.h>
#include <frog/types.h>
#include <frog/uaccess.h>
#include <frog/vm.h>
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
        return fd_close(local_fd);
}

int_32 sys_read(int_32 fd, void *buf, uint_32 count)
{
        struct file *f = fdget(fd);
        if (!f)
                return -EBADF;
        int_32 result = vfs_read(f, buf, count);
        (void) file_put(f);
        return result;
}

int_32 sys_write(int_32 fd, const void *buf, uint_32 count)
{
        struct file *f = fdget(fd);
        if (!f)
                return -EBADF;
        int_32 result = vfs_write(f, buf, count);
        (void) file_put(f);
        return result;
}

int_32 sys_lseek(int_32 fd, int_32 offset, uint_8 whence)
{
        struct file *f = fdget(fd);
        if (!f)
                return -EBADF;
        int_32 result = vfs_lseek(f, offset, whence);
        (void) file_put(f);
        return result;
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
        struct file *f = fdget(fd);
        if (!f)
                return -EBADF;
        int_32 result = vfs_ioctl(f, request, argp);
        (void) file_put(f);
        return result;
}

static int_32 do_sys_mmap(const struct frog_mmap_args *user_args)
{
        struct frog_mmap_args args;
        struct vm_mapping *mapping;
        struct vm_area *vma;
        struct file *file;
        uint_32 mapped;
        bool file_consumed;
        int_32 result;

        if (copy_from_user(&args, user_args, sizeof(args)) != 0)
                return -EFAULT;
        if (args.addr != 0 || args.length == 0 ||
            (args.length & (PAGE_SIZE - 1U)) != 0 ||
            args.length > VM_MAP_MAX_LENGTH ||
            args.prot != (PROT_READ | PROT_WRITE) || args.offset != 0)
                return -EINVAL;
        if (args.flags != MAP_SHARED) {
                if ((args.flags & (MAP_PRIVATE | MAP_FIXED |
                                   MAP_ANONYMOUS)) != 0)
                        return -EOPNOTSUPP;
                return -EINVAL;
        }

        file = fdget(args.fd);
        if (file == NULL)
                return -EBADF;
        if ((file->f_flag & O_ACCMODE) != O_RDWR) {
                (void) file_put(file);
                return -EACCES;
        }

        mapping = vm_mapping_alloc(VM_BACKING_DEVICE_BORROWED, NULL);
        if (mapping == NULL) {
                (void) file_put(file);
                return -ENOMEM;
        }
        vma = vm_area_alloc(args.length, args.prot, args.flags,
                            args.offset, mapping);
        if (vma == NULL) {
                vm_mapping_put(mapping);
                (void) file_put(file);
                return -ENOMEM;
        }

        /* A successful driver callback consumes file through the mapping. */
        result = vfs_mmap(file, vma);
        file_consumed = mapping->state == VM_MAPPING_PREPARED &&
                        mapping->file == file;
        if (result < 0 || !file_consumed) {
                if (!file_consumed)
                        (void) file_put(file);
                vm_mapping_put(mapping);
                kfree(vma);
                return result < 0 ? result : -ENODEV;
        }

        result = vm_map_pfn_range(running_thread()->mm, vma, &mapped);
        if (result < 0) {
                kfree(vma);
                vm_mapping_put(mapping);
                return result;
        }
        return (int_32) mapped;
}

int_32 sys_mmap(const struct frog_mmap_args *user_args)
{
        unsigned long syscall_flags;
        int_32 result;

        /* The syscall interrupt gate enters with IF clear; mmap may sleep. */
        local_irq_save(syscall_flags);
        local_irq_enable();
        result = do_sys_mmap(user_args);
        local_irq_restore(syscall_flags);
        return result;
}

static int_32 do_sys_munmap(void *addr, uint_32 length)
{
        return vm_unmap_exact(running_thread()->mm, (uint_32) addr, length);
}

int_32 sys_munmap(void *addr, uint_32 length)
{
        unsigned long syscall_flags;
        int_32 result;

        /* Unmap releases mapping-owned objects after dropping VM locks. */
        local_irq_save(syscall_flags);
        local_irq_enable();
        result = do_sys_munmap(addr, length);
        local_irq_restore(syscall_flags);
        return result;
}
