#ifndef __FS_PACKAGEFS_H
#include <ostype.h>
#include <sys/ioctl.h>
struct partition;
struct dir;
struct file;


bool is_pkg_file(struct partition *part, int_32 inode_nr);
bool is_pkg_fd(int_32 fd);


int_32 open_pkg(struct partition *part,
                struct dir *parent_d,
                char *name,
                uint_32 inode_nr,
                uint_8 flags);

int_32 packagefs_create(struct partition *part,
                        struct dir *parent_d,
                        char *name,
                        void *target);

void packagefs_init(void);

uint_32 read_server(struct file *file, void *buf, uint_32 count);
uint_32 write_server(struct file *file, const void *buf, uint_32 count);
int_32 ioctl_server(struct file *file, unsigned long request, void *argp);

uint_32 read_client(struct file *file, void *buf, uint_32 count);
uint_32 write_client(struct file *file, const void *buf, uint_32 count);
int_32 ioctl_client(struct file *file, unsigned long request, void *argp);

#endif
