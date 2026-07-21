#include <frog/errno.h>
#include <frog/memory.h>
#include <frog/poll.h>
#include <frog/semaphore.h>
#include <frog/string.h>
#include <kernel/assert.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <kernel/vfs.h>

struct dentry *global_root_dentry;
static struct lock namespace_lock;

void vfs_namespace_lock(void)
{
        lock_fetch(&namespace_lock);
}

void vfs_namespace_unlock(void)
{
        lock_release(&namespace_lock);
}

static bool validate_path(const char *path)
{
        if (!path || path[0] != '/')
                return false;

        uint_32 path_len = 0;
        while (path_len <= PATH_NAME_MAX && path[path_len] != '\0')
                path_len++;
        if (!path_len || path_len > PATH_NAME_MAX)
                return false;
        if (path_len > 1 && path[path_len - 1] == '/')
                return false;

        uint_32 component_len = 0;
        const char *component = path + 1;
        for (uint_32 i = 1; i <= path_len; i++) {
                if (path[i] != '/' && path[i] != '\0') {
                        component_len++;
                        if (component_len > FILE_NAME_MAX)
                                return false;
                        continue;
                }

                if (!component_len)
                        return path_len == 1;
                if ((component_len == 1 && component[0] == '.') ||
                    (component_len == 2 && component[0] == '.' &&
                     component[1] == '.'))
                        return false;

                component = path + i + 1;
                component_len = 0;
        }
        return true;
}

void dentry_add_child(struct dentry *parent, struct dentry *child)
{
        ASSERT(parent && child);
        child->d_parent = parent;

        if (!list_find_element(&child->d_child_node, &parent->d_subdirs)) {
                list_add_tail(&child->d_child_node, &parent->d_subdirs);
        }
}


static bool is_same_dir_name(const char *name, const char *name2)
{
        return strcmp(name, name2) == 0;
}

static int validate_component_name(const char *name)
{
        if (!name || !name[0])
                return -EINVAL;
        uint_32 len = 0;
        while (len <= FILE_NAME_MAX && name[len]) {
                if (name[len] == '/')
                        return -EINVAL;
                len++;
        }
        if (len > FILE_NAME_MAX)
                return -ENAMETOOLONG;
        if ((len == 1 && name[0] == '.') ||
            (len == 2 && name[0] == '.' && name[1] == '.'))
                return -EINVAL;
        return 0;
}


// Search dentry tree, from root dentry, if find target dentry return pointer to
// res variable.
// Only search one layer under root dentry.
static int search_from_dentry(struct dentry *root,
                              const char *name,
                              struct dentry **res)
{
        struct list_head *pos;
        list_for_each (pos, &root->d_subdirs) {
                struct dentry *d =
                    container_of(pos, struct dentry, d_child_node);
                if (d) {
                        if (is_same_dir_name(d->d_name, name)) {
                                *res = d;
                                return 1;
                        } else {
                                continue;
                        }
                }
        }
        *res = NULL;
        return 0;
}

static void free_lookup_dentry(struct dentry *dentry)
{
        if (!dentry)
                return;
        kfree(dentry->d_name);
        kfree(dentry);
}

static struct dentry *alloc_lookup_dentry(struct dentry *parent,
                                          const char *name,
                                          uint_32 name_len)
{
        struct dentry *target = kmalloc(sizeof(*target));
        if (!target)
                return NULL;
        memset(target, 0, sizeof(*target));

        target->d_name = kmalloc(name_len + 1);
        if (!target->d_name) {
                kfree(target);
                return NULL;
        }
        memcpy(target->d_name, name, name_len);
        target->d_name[name_len] = '\0';
        target->d_parent = parent;
        target->d_sb = parent->d_sb;
        INIT_LIST_HEAD(&target->d_subdirs);
        INIT_LIST_HEAD(&target->d_child_node);
        return target;
}

static struct dentry *do_lookup(const char *path)
{
        struct dentry *current = global_root_dentry;
        if (!current || !strcmp(path, "/"))
                return current;

        const char *cursor = path + 1;
        while (*cursor) {
                const char *end = cursor;
                while (*end && *end != '/')
                        end++;
                uint_32 name_len = end - cursor;

                char name[FILE_NAME_MAX + 1];
                memcpy(name, cursor, name_len);
                name[name_len] = '\0';

                struct dentry *res = NULL;
                if (!search_from_dentry(current, name, &res)) {
                        if (!current->d_inode || !current->d_inode->i_op ||
                            !current->d_inode->i_op->lookup)
                                return NULL;

                        struct dentry *target =
                            alloc_lookup_dentry(current, name, name_len);
                        if (!target)
                                return NULL;
                        res = current->d_inode->i_op->lookup(
                            current->d_inode, target);
                        if (!res) {
                                free_lookup_dentry(target);
                                return NULL;
                        }
                        if (res != target)
                                free_lookup_dentry(target);
                }

                if (res->d_mounted) {
                        res = find_mounted_dentry(res);
                        if (!res)
                                return NULL;
                }
                current = res;
                cursor = *end ? end + 1 : end;
        }

        return current;
}

struct dentry *vfs_lookup(const char *path)
{
        vfs_namespace_lock();
        struct dentry *result =
            validate_path(path) ? do_lookup(path) : NULL;
        vfs_namespace_unlock();
        return result;
}


/**
 * lookup some path in some dir
 *
. *****************************************************************************/
struct dentry *dentry_lookup(struct dentry *parent, const char *name)
{
        struct dentry *res;
        if (parent && name && search_from_dentry(parent, name, &res)) {
                return res;
        }
        return NULL;
}

static int open_filep(struct dentry *d,
                      uint_32 flags,
                      struct file **file_out)
{
        if (!d || !d->d_inode || !file_out)
                return -ENOENT;

        struct inode *dinode = d->d_inode;
        if (dinode->i_count == 0xffffU)
                return -EMFILE;

        struct file *f = (struct file *) kmalloc(sizeof(struct file));
        if (!f)
                return -ENOMEM;
        memset(f, 0, sizeof(struct file));

        f->f_inode = dinode;
        f->f_dentry = d;
        f->f_flag = flags;
        refcount_init(&f->f_refs, 1);
        f->f_count = 0;
        f->f_pos = 0;
        f->f_op = dinode->i_fop;
        f->private_data = NULL;

        if (f->f_op && f->f_op->open) {
                int ret = f->f_op->open(dinode, f);
                if (ret < 0) {
                        kfree(f);
                        return ret;
                }
        }

        dinode->i_count++;
        *file_out = f;
        return 0;
}

static int lookup_parent(const char *path,
                         struct dentry **parent_out,
                         const char **name_out,
                         uint_32 *name_len_out)
{
        if (!parent_out || !name_out || !name_len_out ||
            !validate_path(path))
                return -EINVAL;

        uint_32 path_len = strlen(path);
        if (path_len <= 1)
                return -EBUSY;

        const char *last_slash = path + path_len - 1;
        while (last_slash > path && *last_slash != '/')
                last_slash--;

        uint_32 name_len = path + path_len - last_slash - 1;
        uint_32 parent_len = last_slash == path ? 1 : last_slash - path;
        char *parent_path = kmalloc(parent_len + 1);
        if (!parent_path)
                return -ENOMEM;
        memcpy(parent_path, path, parent_len);
        parent_path[parent_len] = '\0';

        struct dentry *parent = vfs_lookup(parent_path);
        kfree(parent_path);
        if (!parent)
                return -ENOENT;
        if (!parent->d_inode || parent->d_type != FT_DIRECTORY)
                return -ENOTDIR;

        *parent_out = parent;
        *name_out = last_slash + 1;
        *name_len_out = name_len;
        return 0;
}

static int create_file(const char *path,
                       uint_32 flags,
                       struct dentry **dentry_out)
{
        if (!path || !(flags & O_CREAT) || !dentry_out)
                return -EINVAL;

        struct dentry *parent = NULL;
        const char *name = NULL;
        uint_32 name_len = 0;
        int ret = lookup_parent(path, &parent, &name, &name_len);
        if (ret < 0)
                return ret;
        if (!parent->d_inode->i_op || !parent->d_inode->i_op->create)
                return -EROFS;

        struct dentry *existing = dentry_lookup(parent, name);
        if (existing) {
                if (flags & O_EXCL)
                        return -EEXIST;
                *dentry_out = existing;
                return 0;
        }

        struct dentry *child = alloc_lookup_dentry(parent, name, name_len);
        if (!child)
                return -ENOMEM;
        child->d_type = FT_REGULAR;

        ret = parent->d_inode->i_op->create(parent->d_inode, child,
                                            FT_REGULAR);
        if (ret < 0 || !child->d_inode) {
                free_lookup_dentry(child);
                return ret < 0 ? ret : -EIO;
        }

        dentry_add_child(parent, child);
        *dentry_out = child;
        return 0;
}

int_32 vfs_open_file(const char *path,
                     uint_32 flags,
                     struct file **file_out)
{
        if (!file_out)
                return -EINVAL;
        *file_out = NULL;
        vfs_namespace_lock();
        uint_32 supported_flags = O_ACCMODE | O_CREAT | O_EXCL | O_TRUNC |
                                  O_APPEND | O_NONBLOCK | O_DIRECTORY;
        if (!validate_path(path) || (flags & ~supported_flags) ||
            (flags & O_ACCMODE) > O_RDWR ||
            ((flags & O_EXCL) && !(flags & O_CREAT)) ||
            ((flags & O_TRUNC) && (flags & O_ACCMODE) == O_RDONLY)) {
                vfs_namespace_unlock();
                return -EINVAL;
        }

        struct dentry *d = vfs_lookup(path);
        if (d && (flags & O_CREAT) && (flags & O_EXCL)) {
                vfs_namespace_unlock();
                return -EEXIST;
        }
        if (!d) {
                if ((flags & O_CREAT) && !(flags & O_DIRECTORY)) {
                        int ret = create_file(path, flags, &d);
                        if (ret < 0) {
                                vfs_namespace_unlock();
                                return ret;
                        }
                } else {
                        vfs_namespace_unlock();
                        return (flags & O_CREAT) ? -EINVAL : -ENOENT;
                }
        }
        if (!d) {
                vfs_namespace_unlock();
                return -EIO;
        }
        if ((flags & O_DIRECTORY) && d->d_type != FT_DIRECTORY) {
                vfs_namespace_unlock();
                return -ENOTDIR;
        }
        if (d->d_type == FT_DIRECTORY &&
            ((flags & O_ACCMODE) != O_RDONLY || (flags & O_TRUNC))) {
                vfs_namespace_unlock();
                return -EISDIR;
        }
        int ret = open_filep(d, flags, file_out);
        vfs_namespace_unlock();
        return ret;
}

struct file *vfs_open(const char *path, uint_32 flags)
{
        struct file *file = NULL;
        return vfs_open_file(path, flags, &file) < 0 ? NULL : file;
}

bool file_get_live(struct file *f)
{
        return f != NULL && refcount_get_live(&f->f_refs);
}

int_32 file_put(struct file *f)
{
        if (f == NULL)
                return -EBADF;
        if (!refcount_put(&f->f_refs))
                return 0;

        ASSERT(f->f_count == 0);
        vfs_namespace_lock();
        int_32 ret = 0;
        struct inode *inode = f->f_inode;
        if (f->f_op && f->f_op->close)
                ret = f->f_op->close(f);
        if (!inode || !inode->i_count) {
                if (ret == 0)
                        ret = -EUCLEAN;
        } else {
                inode->i_count--;
        }

        kfree(f);
        vfs_namespace_unlock();
        return ret;
}

int_32 vfs_close(struct file *f)
{
        return file_put(f);
}

int_32 vfs_write(struct file *f, const void *buf, uint_32 count)
{
        if (!f)
                return -EBADF;
        if ((f->f_flag & O_ACCMODE) == O_RDONLY)
                return -EBADF;
        if (f->f_dentry && f->f_dentry->d_type == FT_DIRECTORY)
                return -EISDIR;
        if (!count)
                return 0;
        if (!buf)
                return -EFAULT;
        if (!f->f_op || !f->f_op->write)
                return -EINVAL;
        return f->f_op->write(f, buf, count);
}

int_32 vfs_read(struct file *f, void *buf, uint_32 count)
{
        if (!f)
                return -EBADF;
        if ((f->f_flag & O_ACCMODE) == O_WRONLY)
                return -EBADF;
        if (f->f_dentry && f->f_dentry->d_type == FT_DIRECTORY)
                return -EISDIR;
        if (!count)
                return 0;
        if (!buf)
                return -EFAULT;
        if (!f->f_op || !f->f_op->read)
                return -EINVAL;
        return f->f_op->read(f, buf, count);
}

int_32 vfs_lseek(struct file *f, int_32 offset, uint_8 whence)
{
        if (!f)
                return -EBADF;
        if (whence < SEEK_SET || whence > SEEK_END)
                return -EINVAL;
        if (!f->f_op || !f->f_op->lseek)
                return -ESPIPE;
        return f->f_op->lseek(f, offset, whence);
}

uint_32 vfs_poll(struct file *file, struct poll_table_struct *wait)
{
        if (!file)
                return POLLNVAL;
        if (file->f_op && file->f_op->poll)
                return file->f_op->poll(file, wait);

        uint_32 mask = 0;
        if ((file->f_flag & O_ACCMODE) != O_WRONLY)
                mask |= POLLIN | POLLRDNORM;
        if ((file->f_flag & O_ACCMODE) != O_RDONLY)
                mask |= POLLOUT | POLLWRNORM;
        return mask;
}

int_32 vfs_ioctl(struct file *file, uint_32 request, void *argp)
{
        if (!file)
                return -EBADF;
        if (file->f_op && file->f_op->ioctl)
                return file->f_op->ioctl(file, request, argp);
        return -ENOTTY;
}

int_32 vfs_mmap(struct file *file, struct vm_area *vma)
{
        if (file == NULL)
                return -EBADF;
        if (vma == NULL)
                return -EINVAL;
        if (file->f_op == NULL || file->f_op->mmap == NULL)
                return -EOPNOTSUPP;
        return file->f_op->mmap(file, vma);
}


int_32 vfs_mkdir(struct dentry *parent, struct dentry *child)
{
        vfs_namespace_lock();
        if (!parent || !child || !child->d_name || !parent->d_inode) {
                vfs_namespace_unlock();
                return -EINVAL;
        }
        int name_ret = validate_component_name(child->d_name);
        if (name_ret < 0) {
                vfs_namespace_unlock();
                return name_ret;
        }
        if (parent->d_type != FT_DIRECTORY) {
                vfs_namespace_unlock();
                return -ENOTDIR;
        }
        if (!parent->d_inode->i_op || !parent->d_inode->i_op->mkdir) {
                vfs_namespace_unlock();
                return -EROFS;
        }
        if (child->d_parent != parent || child->d_inode ||
            dentry_lookup(parent, child->d_name)) {
                vfs_namespace_unlock();
                return -EEXIST;
        }

        int_32 ret = parent->d_inode->i_op->mkdir(parent->d_inode, child,
                                                  FT_DIRECTORY);
        if (ret < 0) {
                vfs_namespace_unlock();
                return ret;
        }
        if (!child->d_inode) {
                vfs_namespace_unlock();
                return -EIO;
        }

        dentry_add_child(parent, child);
        vfs_namespace_unlock();
        return 0;
}

int_32 vfs_mkdir_path(const char *path)
{
        vfs_namespace_lock();
        struct dentry *parent = NULL;
        const char *name = NULL;
        uint_32 name_len = 0;
        int ret = lookup_parent(path, &parent, &name, &name_len);
        if (ret < 0) {
                vfs_namespace_unlock();
                return ret;
        }
        if (dentry_lookup(parent, name) || vfs_lookup(path)) {
                vfs_namespace_unlock();
                return -EEXIST;
        }

        struct dentry *child = alloc_lookup_dentry(parent, name, name_len);
        if (!child) {
                vfs_namespace_unlock();
                return -ENOMEM;
        }
        child->d_type = FT_DIRECTORY;
        ret = vfs_mkdir(parent, child);
        if (ret < 0)
                free_lookup_dentry(child);
        vfs_namespace_unlock();
        return ret;
}

int_32 vfs_unlink(struct dentry *dir)
{
        vfs_namespace_lock();
        if (!dir || !dir->d_parent || !dir->d_inode) {
                vfs_namespace_unlock();
                return -EINVAL;
        }
        if (dir->d_type == FT_DIRECTORY) {
                vfs_namespace_unlock();
                return -EISDIR;
        }
        if (dir->d_mounted || dir->d_inode->i_count) {
                vfs_namespace_unlock();
                return -EBUSY;
        }

        struct inode *dir_parent = dir->d_parent->d_inode;
        if (!dir_parent || !dir_parent->i_op || !dir_parent->i_op->unlink) {
                vfs_namespace_unlock();
                return -EINVAL;
        }

        int_32 ret = dir_parent->i_op->unlink(dir_parent, dir);
        if (ret < 0) {
                vfs_namespace_unlock();
                return ret;
        }

        list_del(&dir->d_child_node);
        free_lookup_dentry(dir);
        vfs_namespace_unlock();
        return 0;
}

int_32 vfs_rmdir(struct dentry *dir)
{
        vfs_namespace_lock();
        if (!dir || !dir->d_parent || !dir->d_inode) {
                vfs_namespace_unlock();
                return -EINVAL;
        }
        if (dir == global_root_dentry || dir->d_parent == dir) {
                vfs_namespace_unlock();
                return -EBUSY;
        }
        if (dir->d_type != FT_DIRECTORY) {
                vfs_namespace_unlock();
                return -ENOTDIR;
        }
        if (dir->d_mounted || dir->d_inode->i_count) {
                vfs_namespace_unlock();
                return -EBUSY;
        }
        if (!list_is_empty(&dir->d_subdirs)) {
                vfs_namespace_unlock();
                return -ENOTEMPTY;
        }
        struct inode *p_inode = dir->d_parent->d_inode;
        if (!p_inode || !p_inode->i_op || !p_inode->i_op->rmdir) {
                vfs_namespace_unlock();
                return -EINVAL;
        }

        int_32 ret = p_inode->i_op->rmdir(p_inode, dir);
        if (ret < 0) {
                vfs_namespace_unlock();
                return ret;
        }

        list_del(&dir->d_child_node);
        free_lookup_dentry(dir);
        vfs_namespace_unlock();
        return 0;
}

int_32 vfs_unlink_path(const char *path)
{
        vfs_namespace_lock();
        struct dentry *dentry = vfs_lookup(path);
        int_32 ret = dentry ? vfs_unlink(dentry) : -ENOENT;
        vfs_namespace_unlock();
        return ret;
}

int_32 vfs_rmdir_path(const char *path)
{
        vfs_namespace_lock();
        struct dentry *dentry = vfs_lookup(path);
        int_32 ret = dentry ? vfs_rmdir(dentry) : -ENOENT;
        vfs_namespace_unlock();
        return ret;
}

/*
 * Recommended Mount Workflow (VFS-level perspective)
 *
 * In order to mount a new filesystem (e.g., devfs) onto a specific path
 * such as "/home/zm/dev", the mount point *must already exist* as a
 * valid directory entry (dentry) in the underlying filesystem (e.g., ext2).
 *
 * 1. The user or system must first ensure that "/home/zm/dev" exists,
 *    typically by calling mkdir("/home/zm/dev") on the ext2 filesystem.
 *
 * 2. The VFS then performs a path lookup on "/home/zm/dev" using:
 *       struct dentry *target = vfs_lookup("/home/zm/dev");
 *    If successful, `target` is the dentry corresponding to that path,
 *    owned by the ext2 filesystem.
 *
 * 3. The mount system then attaches the new filesystem (e.g., devfs)
 *    to this dentry by assigning:
 *       target->mounted_here = &devfs_mount;
 *
 * 4. From this point onward, any path resolution that reaches the "dev"
 *    component will first perform ext2's `lookup("dev")`, obtain the
 *    original ext2 dentry, and then detect `mounted_here != NULL`.
 *
 * 5. The VFS will then transparently redirect path resolution into the
 *    root dentry of the mounted filesystem (i.e., devfs->root_dentry),
 *    effectively treating "/home/zm/dev" as the new root of devfs.
 *
 * NOTE:
 * - The original dentry remains part of the ext2 tree, but VFS suppresses
 *   further traversal into that subtree once the mount is active.
 * - This design ensures clean separation of filesystem responsibilities
 *   and consistent behavior during path traversal.
 *
 * */
int_32 vfs_mount(const char *pathname,
                 const char *fs_type,
                 int flags,
                 const char *dev_name,
                 void *data)
{
        vfs_namespace_lock();
        if (!pathname || !fs_type) {
                vfs_namespace_unlock();
                return -EINVAL;
        }

        struct fs_type *fstype = find_fs_type(fs_type);
        if (!fstype || !fstype->mount) {
                vfs_namespace_unlock();
                return -ENODEV;
        }

        struct dentry *mp = vfs_lookup(pathname);
        if (!mp || mp->d_parent == mp || mp->d_mounted ||
            mp->d_type != FT_DIRECTORY) {
                vfs_namespace_unlock();
                return -EINVAL;
        }

        struct mount_entry *entry = kmalloc(sizeof(*entry));
        if (!entry) {
                vfs_namespace_unlock();
                return -ENOMEM;
        }
        memset(entry, 0, sizeof(*entry));
        INIT_LIST_HEAD(&entry->mount_node);

        struct super_block *sb = fstype->mount(fstype, flags, dev_name, data);
        if (!sb || !sb->s_root) {
                if (sb) {
                        if (sb->s_op && sb->s_op->put_super)
                                sb->s_op->put_super(sb);
                        else if (sb->s_root)
                                kfree(sb->s_root);
                        kfree(sb);
                }
                kfree(entry);
                vfs_namespace_unlock();
                return -EIO;
        }

        sb->s_mountpoint = mp;
        struct dentry *mounted_root = kmalloc(sizeof(*mounted_root));
        if (!mounted_root) {
                if (sb->s_op && sb->s_op->put_super)
                        sb->s_op->put_super(sb);
                else
                        kfree(sb->s_root);
                kfree(sb);
                kfree(entry);
                vfs_namespace_unlock();
                return -ENOMEM;
        }
        memset(mounted_root, 0, sizeof(*mounted_root));
        mounted_root->d_name = mp->d_name;
        mounted_root->d_inode = sb->s_root;
        mounted_root->d_parent = mounted_root;
        mounted_root->d_type = FT_DIRECTORY;
        mounted_root->d_sb = sb;
        INIT_LIST_HEAD(&mounted_root->d_subdirs);
        INIT_LIST_HEAD(&mounted_root->d_child_node);

        entry->fs_name = fstype->name;
        entry->sb = sb;
        entry->mount_point = mp;
        entry->mounted_root = mounted_root;

        mp->d_mounted = true;
        add_mount_list(&entry->mount_node);
        vfs_namespace_unlock();
        return 0;
}

int_32 vfs_init(void)
{
        lock_init(&namespace_lock);
        init_mount_list();
        init_fs_type_list();
        return 0;
}
