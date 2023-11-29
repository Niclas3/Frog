target remote localhost:1234

file core/build/core_symbol.img

# b bootpack.c:114
# b systask.c:12

# b msg_receive
# b msg_send
# b sys_sendrec
# b task_sys
# b u_funf

b UkiMain
# b identify_disk
# b bootpack.c:99
# b bootpack.c:99
# b read_dpt
# b ide.c:338
# b next_dpt
# b ide.c:485
# b partition_info
# b fs.c:66
# b fs_init
# b ide_init
# b partition_format
# b scan_partitions
# b mount_partition
# b fs.c:103
# b fs.c:64
# b fs.c:222
# b fs.c: 159
# b bootpack.c:101
# b bootpack.c:115
# b file.c:258
# b sys_open
# b func
# b locale_inode
# b bootpack.c:127
# b bootpack.c:240
#-------------------------
# b bootpack.c:150
# b bootpack.c:157 if i == 12
# b bootpack.c:151

# b func
# b bootpack.c: 256
# b bootpack.c:241
# b funcb
# b 147 if i == 99
# b 147 if i == 23
# b file.c:621
b 117
# b sys_mkdir
# b flush_dir_entry
# b mouse_consumer
# b keyboard_consumer
# b file_write
# b file_read
# b file.c:435
# b file.c:324
#-------------------------
# b 122
# b memory.c:238 if desc_idx == 5
#-------------------------
# b file_write
# b file.c:256
# b file.c:268
# b file.c:271
# b file.c:269
# b file.c:275
# b ide_read
# b file_write
# b fs.c:374
# b u_fung
# b inode_bitmap_alloc
# b file.c:218
# b fs.c:525
# b sys_open
# b path_dirs
# b search_file
# b path_peel
# b install_thread_fd
# b init_bitmap
# b bootpack.c:93
# b dir.c:156
# b path_depth

# b inode.c:140
# b fs.c:67
# b fs.c:191
# b list_length
# b get_dpt
# b enable_mouse
# b ide_init
# b bootpack.c:106
# b ide_read
# b int.c:89
# b protect.c:114
# b exception_handler
tui en
c

