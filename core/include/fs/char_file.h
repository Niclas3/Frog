#pragma once
#include <ostype.h>
#include <fs/fs.h>
#include <fs/file.h>

// bool is_char_file(int_32 fd);
bool is_char_file(struct partition *part, int_32 inode_nr);
bool is_char_fd(int_32 fd);

int_32 char_file_create(struct partition *part,
                        struct dir *parent_d,
                        char *name,
                        void *target);

int_32 open_char_file(struct partition *part, uint_32 inode_nr, uint_8 flags);
int_32 close_char_file(struct file *file);

uint_32 read_char_file(int_32 fd, void *buf, uint_32 count);
uint_32 write_char_file(int_32 fd, const void *buf, uint_32 count);
