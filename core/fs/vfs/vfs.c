#include <frog/errno.h>
#include <frog/memory.h>
#include <frog/string.h>
#include <kernel/assert.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <kernel/vfs.h>

struct dentry *global_root_dentry;

static inline bool validate_path(char *path)
{
        return true;
}

// /home/zm/a/test.md
char **next_path_components(char **p_path, char *component)
{
        if (*p_path == NULL) {
                memset(component, 0, FILE_NAME_MAX);
                return NULL;
        }
        char *path = *p_path;
        int path_len = strlen(path);
        char *path_head = path;
        if (path_len == 0)
                return NULL;

        char *cursor = path;
        while (*cursor != '\0') {
                if (*cursor == '/') {
                        break;
                }
                cursor++;
        }

        int name_len =
            *cursor == '\0' ? cursor - path_head : cursor - path_head + 1;

        memcpy(component, path_head, name_len);

        int is_root = name_len == 1;
        if (component[name_len - 1] == '/' && !is_root) {
                component[name_len - 1] = '\0';
        } else {
                component[name_len] = '\0';
        }

        if (path_len == name_len) {
                *p_path = NULL;
        } else {
                *p_path = (path_head + name_len);
        }
        return p_path;
}

/**
 * NEED PASS COPY OF PATH, this function will change param `path`
 * and trade `last_name` as a container, call memset(last_name, 0, len);
 * each time.
 *
 * divide path into `directory` + `file`
 * e.g ready path: `/home/tom/Desktop/city.img`
 *     break down to parts: `/home/tom/Desktop/` and `city.img`
 * like a reverse version (from tail) `car`
 * each path components out name without tail-slash
 *
 * @param whole_path path ready to be peeled
 * @param last_name last file(or directory) name
 * @return anther path without last_name
 *****************************************************************************/
char *path_pop_tail(char *path, char *last_name)
{
        int path_len = strlen(path);
        char *path_head = path;
        if (path_len == 0)
                return NULL;
        if (path_len == 1 && !strcmp("/", path)) {
                memcpy(last_name, "/", 1);
                return path;
        }
        // 1. test if path is valid path
        char *cursor = path;
        char *last_slash_pos = NULL;
        while (*cursor != '\0') {
                int is_end_slash = cursor - path_head == path_len - 1;
                if (*cursor == '/' && !is_end_slash) {
                        last_slash_pos = cursor;
                }
                cursor++;
        }
        // `name_offset` +1 is position after slash like '/abc' => 'abc'
        int name_offset = last_slash_pos - path_head + 1;
        int name_len = path_len - name_offset;

        memcpy(last_name, path_head + name_offset, name_len);

        if (last_name[name_len - 1] == '/') {
                last_name[name_len - 1] = '\0';
        } else {
                last_name[name_len] = '\0';
        }

        path[name_offset] = '\0';
        return path;
}


void dentry_add_child(struct dentry *parent, struct dentry *child)
{
        ASSERT(parent && child);
        child->d_parent = parent;

        if (!list_find_element(&child->d_child_node, &parent->d_subdirs)) {
                list_add_tail(&child->d_child_node, &parent->d_subdirs);
        }
}


static bool is_same_dir_name(char *name, char *name2)
{
        return strcmp(name, name2) == 0;
}


// Search dentry tree, from root dentry, if find target dentry return pointer to
// res variable.
// Only search one layer under root dentry.
static int search_from_dentry(struct dentry *root,
                              struct dentry *target,
                              struct dentry **res)
{
        struct list_head *pos;
        list_for_each (pos, &root->d_subdirs) {
                struct dentry *d =
                    container_of(pos, struct dentry, d_child_node);
                if (d) {
                        if (is_same_dir_name(d->d_name, target->d_name)) {
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

// NOTE:
// - dir: temporary lookup container, freed manually
// - dir->d_name: kmalloc'd each round, must free
// - res: real dentry returned from filesystem, owned by VFS/cache, DO NOT free
// /dev/input/event0
static struct dentry *do_lookup(const char *path)
{
        ASSERT(path);
        struct dentry *current = global_root_dentry;
        char *component = kmalloc(FILE_NAME_MAX);
        memset(component, 0, FILE_NAME_MAX);
        char **path_rst =
            next_path_components((char **) &path, (char *) component);
        path_rst = next_path_components(path_rst, component);

        if (current->d_inode && current->d_inode->i_op) {
                struct dentry *dir = kmalloc(sizeof(struct dentry));
                memset(dir, 0, sizeof(struct dentry));
                do {
                        char *name = kmalloc(FILE_NAME_MAX + 1);
                        memcpy(name, component, FILE_NAME_MAX);
                        dir->d_name = name;
                        dir->d_parent = current;

                        // Search dentry first if nothing get then call
                        // xxxfs->lookup()
                        // if find target dentry then set res
                        struct dentry *res = NULL;
                        if (!search_from_dentry(current, dir, &res)) {
                                res = current->d_inode->i_op->lookup(
                                    current->d_inode, dir);
                        }

                        if (res != NULL) {
                                if (res->d_mounted) {
                                        // 1. find mounted inode
                                        struct dentry *mp =
                                            find_mounted_dentry(res);
                                        if (!mp) {
                                                PANIC(
                                                    "[vfs]: cannot find mount "
                                                    "point from mount list.");
                                        }
                                        current = mp;
                                } else {
                                        // 2. not a mounted point
                                        current = res;
                                }
                                path_rst = next_path_components(
                                    path_rst, (char *) component);

                        } else {
                                INFO("path does not find ");
                                kfree(name);
                                kfree(dir);
                                return NULL;
                        }
                        kfree(name);
                } while (strcmp(component, ""));
                kfree(dir);
        }

        return current;
}

struct dentry *vfs_lookup(const char *path)
{
        if (!path || !validate_path((char *) path))
                return NULL;
        int len = strlen(path);
        char *mpath = kmalloc(len + 1);
        strncpy(mpath, path, len);
        struct dentry *d = do_lookup(mpath);
        kfree(mpath);
        return d;
}


/**
 * lookup some path in some dir
 *
. *****************************************************************************/
struct dentry *dentry_lookup(struct dentry *parent, char *name)
{
        struct dentry *target = kmalloc(sizeof(struct dentry));
        target->d_name = name;
        struct dentry *res;
        if (search_from_dentry(parent, target, &res)) {
                kfree(target);
                return res;
        } else {
                kfree(target);
                return NULL;
        }
}

static struct file *open_filep(struct dentry *d, uint_8 flags)
{
        if (!d || !d->d_inode) {
                return NULL;  // ENOENT;
        }

        struct inode *dinode = d->d_inode;

        struct file *f = (struct file *) kmalloc(sizeof(struct file));
        if (!f) {
                return NULL;  // ENOMEM;
        }
        memset(f, 0, sizeof(struct file));

        f->f_inode = dinode;
        f->f_dentry = d;
        f->f_flag = flags;
        f->f_pos = 0;
        f->f_op = dinode->i_fop;
        f->private_data = NULL;

        if (f->f_op && f->f_op->open) {
                int ret = f->f_op->open(dinode, f);
                if (ret < 0) {
                        kfree(f);
                        return NULL;
                }
        }

        return f;
}

static struct dentry *create_file(char *path, uint_8 flags)
{
        if (!path || !(flags & O_CREAT))
                return NULL;

        int path_len = strlen(path);
        if (path_len <= 1 || path[0] != '/' || path[path_len - 1] == '/')
                return NULL;

        const char *last_slash = NULL;
        for (int i = 0; i < path_len; i++) {
                if (path[i] == '/')
                        last_slash = &path[i];
        }
        if (!last_slash)
                return NULL;

        int name_len = path_len - (last_slash - path) - 1;
        if (name_len <= 0 || name_len > FILE_NAME_MAX)
                return NULL;

        char *name = kmalloc(name_len + 1);
        if (!name)
                return NULL;
        memcpy(name, last_slash + 1, name_len);
        name[name_len] = '\0';

        int parent_len = (last_slash == path) ? 1 : (int) (last_slash - path);
        char *parent_path = kmalloc(parent_len + 1);
        if (!parent_path) {
                kfree(name);
                return NULL;
        }
        memcpy(parent_path, path, parent_len);
        parent_path[parent_len] = '\0';

        struct dentry *parent = vfs_lookup(parent_path);
        kfree(parent_path);

        if (!parent || !parent->d_inode || !parent->d_inode->i_op ||
            !parent->d_inode->i_op->create) {
                kfree(name);
                return NULL;
        }

        struct dentry *existing = dentry_lookup(parent, name);
        if (existing) {
                kfree(name);
                return (flags & O_EXCL) ? NULL : existing;
        }

        struct dentry *child = kmalloc(sizeof(struct dentry));
        if (!child) {
                kfree(name);
                return NULL;
        }
        memset(child, 0, sizeof(struct dentry));
        child->d_name = name;
        child->d_parent = parent;
        child->d_sb = parent->d_inode->i_sb;
        child->d_type = FT_REGULAR;
        child->d_mounted = false;
        INIT_LIST_HEAD(&child->d_subdirs);
        INIT_LIST_HEAD(&child->d_child_node);

        int ret = parent->d_inode->i_op->create(parent->d_inode, child,
                                                FT_REGULAR);
        if (ret < 0) {
                kfree(child);
                kfree(name);
                return NULL;
        }

        dentry_add_child(parent, child);
        return child;
}

struct file *vfs_open(char *path, uint_8 flags)
{
        struct dentry *d = vfs_lookup(path);
        if (!d) {
                if (flags & O_CREAT) {
                        d = create_file(path, flags);
                } else {
                        return NULL;
                }
        }
        if (!d)
                return NULL;
        struct file *f = open_filep(d, flags);
        return f;
}

int_32 vfs_close(struct file *f)
{
        if (!f)
                return -1;
        if (f->f_op && f->f_op->close)
                f->f_op->close(f);

        kfree(f);
        return 0;
}

int_32 vfs_write(struct file *f, const void *buf, uint_32 count)
{
        if (!f || !buf)
                return -1;
        if (f->f_op && f->f_op->write) {
                int size = f->f_op->write(f, buf, count);
                return size;
        }
        return -1;
}

int_32 vfs_read(struct file *f, void *buf, uint_32 count)
{
        if (!f || !buf)
                return -1;
        if (f->f_op && f->f_op->read) {
                int size = f->f_op->read(f, buf, count);
                return size;
        }
        return -1;
}

int_32 vfs_lseek(struct file *f, int_32 offset, uint_8 whence)
{
        if (!f)
                return -1;
        if (f->f_op && f->f_op->lseek) {
                int size = f->f_op->lseek(f, offset, whence);
                return size;
        }
        return -1;
}

uint_32 vfs_poll(struct file *file, struct poll_table_struct *wait)
{
        if (!file || !wait)
                return -1;
        if (file->f_op && file->f_op->poll) {
                int res = file->f_op->poll(file, wait);
                return res;
        }

        return 0;
}

uint_32 vfs_ioctl(struct file *file, uint_32 request, void *argp)
{
        if (!file)
                return -1;
        if (file->f_op && file->f_op->ioctl) {
                int res = file->f_op->ioctl(file, request, argp);
                return res;
        }
        return 0;
}


int_32 vfs_mkdir(struct dentry *parent, struct dentry *child)
{
        if (!parent || !child || !parent->d_inode || !parent->d_inode->i_op ||
            !parent->d_inode->i_op->mkdir)
                return -1;

        int_32 ret = parent->d_inode->i_op->mkdir(parent->d_inode, child,
                                                  FT_DIRECTORY);
        if (ret < 0)
                return ret;

        dentry_add_child(parent, child);

        return 0;
}

int_32 vfs_unlink(struct dentry *dir)
{
        if (!dir || !dir->d_parent || !dir->d_inode)
                return -1;

        struct inode *dir_parent = dir->d_parent->d_inode;

        return dir_parent->i_op->unlink(dir_parent, dir);
}

int_32 vfs_rmdir(struct dentry *dir)
{
        if (!dir || !dir->d_parent || !dir->d_inode) {
                return -1;
        }
        struct inode *p_inode = dir->d_parent->d_inode;

        return p_inode->i_op->rmdir(p_inode, dir);
}

struct dentry *vfs_opendir(const char *name)
{
        return NULL;
}

int_32 vfs_closedir(struct dentry *dirp)
{
        return 0;
}

int_32 vfs_readdir(struct file *dir, struct dentry *entry_out)
{
        return 0;
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
        struct fs_type *fstype = find_fs_type(fs_type);

        if (fstype) {
                struct super_block *sb =
                    fstype->mount(fstype, flags, dev_name, data);

                struct mount_entry *entry = kmalloc(sizeof(struct mount_entry));
                if (!entry) {
                        WARN("No enough memory for mount entries");
                        return -1;
                }
                struct dentry *mp = vfs_lookup(pathname);
                if (!mp) {
                        WARN("Not find this mount point %s:pathname %s",
                             fs_type, pathname);
                        return -1;
                }
                sb->s_mountpoint = mp;

                entry->sb = sb;
                entry->mount_point = mp;

                if (mp->d_inode) {
                        kfree(mp->d_inode);
                        mp->d_inode = NULL;
                }
                mp->d_inode = sb->s_root;

                mp->d_mounted = true;

                add_mount_list(&entry->mount_node);
                return 0;
        } else {
                WARN("We don't have this fs type %s", fs_type);
                return -1;
        }
}

int_32 vfs_init(void)
{
        init_mount_list();
        init_fs_type_list();
        return 0;
}
