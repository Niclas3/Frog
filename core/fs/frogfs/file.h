#ifndef __FS_FILE_H
#define __FS_FILE_H
#include <frog/types.h>
struct super_block;
struct file;

int_32 read_file(struct super_block *sb,
                 struct file *file,
                 void *buf,
                 uint_32 count);
int_32 write_file(struct super_block *sb,
                  struct file *file,
                  const void *buf,
                  uint_32 write_len);

#endif
