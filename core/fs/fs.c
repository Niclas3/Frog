#include <fs/dir.h>
#include <fs/fcntl.h>
#include <fs/fs.h>
#include <fs/inode.h>
#include <fs/super_block.h>

#include <device/ide.h>
#include <fs/file.h>
#include <math.h>
#include <string.h>
#include <sys/memory.h>
#include <sys/threads.h>

#include <debug.h>

// global variable define at ide.c
extern struct ide_channel channels[2];  // 2 different channels
extern uint_8 channel_cnt;
extern struct list_head partition_list;  // partition list
// global variable define file.c
extern struct file g_file_table[MAX_FILE_OPEN];
extern struct lock g_ft_lock;

struct partition mounted_part;  // the partition what we want to mount.

/**
 * In this file, we will create file system at some partition.
 * 1. Creating super block
 *    file system layout
 *  ┌───────────┬───────────┬───────┬──────┬──────┬────┬─────────────┐
 *  │ Operating │ super     │ free  │inode │inode │root│ free zone   │
 *  │ system    │ block     │ zone  │      │      │    │             │
 *  │ boot      │           │ bitmap│bitmap│table │dir │             │
 *  │ block     │           │       │      │      │    │             │
 *  └───────────┴───────────┴───────┴──────┴──────┴────┴─────────────┘
 *                             imap   zmap  inode (data zone)
 *   512B        512B
 *   0 sector    1 sector
 *****************************************************************************/
/**
 * partition_format() init all things
 *
 * @param part target partition
 * @return
 *****************************************************************************/

static void partition_format(struct partition *part)
{
    if (part->sec_cnt <= 0) {
        return;
    }
    ASSERT(part->sec_cnt >= 0);
    uint_32 os_boot_record_sz = 1;  // obr size in sector
    uint_32 super_block_sz = 1;     // super block size in sector

    //(files/bits) / (bits/sector) = files/ sector
    uint_32 inode_bitmap_sz =
        DIV_ROUND_UP(MAX_FILES_PER_PARTITION, BITS_PER_SECTOR);
    uint_32 inode_table_sz = DIV_ROUND_UP(
        ((sizeof(struct inode)) * MAX_FILES_PER_PARTITION), SECTOR_SIZE);
    uint_32 used_sector_sz =
        os_boot_record_sz + super_block_sz + inode_bitmap_sz + inode_table_sz;
    uint_32 free_sector_sz = part->sec_cnt - used_sector_sz;

    // Calculate zone bitmap size and zone size
    // zone bitmap size is depended on zone size;
    // Let's say zone bitmap size is A, then zone size is B
    // A+B equals free_sector_sz
    // A = B / U(zone size)
    /* uint_32 zone_bitmap_sz = DIV_ROUND_UP(free_sector_sz, BITS_PER_SECTOR+1);
     */
    /* uint_32 zones_sz = free_sector_sz - zone_bitmap_sz; */
    uint_32 zone_bitmap_sz = DIV_ROUND_UP(free_sector_sz, BITS_PER_SECTOR);
    uint_32 bm_len = free_sector_sz - zone_bitmap_sz;
    zone_bitmap_sz = DIV_ROUND_UP(bm_len, BITS_PER_SECTOR);
    uint_32 zones_sz = free_sector_sz - zone_bitmap_sz;


    // Init super block
    struct super_block sb;
    sb.s_magic = 0x2023B07A;
    sb.s_ninodes =
        DIV_ROUND_UP(inode_table_sz * SECTOR_SIZE, sizeof(struct inode));
    sb.s_nzones = DIV_ROUND_UP(zones_sz, ZONE_SIZE);

    sb.s_imap_lba = part->start_lba + os_boot_record_sz + super_block_sz;
    sb.s_imap_sz = inode_bitmap_sz;

    sb.s_zmap_lba = sb.s_imap_lba + inode_bitmap_sz;
    sb.s_zmap_sz = zone_bitmap_sz;

    sb.s_inode_table_lba = sb.s_zmap_lba + zone_bitmap_sz;
    sb.s_inode_table_sz = inode_table_sz;

    sb.s_data_start_lba = sb.s_inode_table_lba + inode_table_sz;
    sb.root_inode_no = 0;
    sb.dir_entry_size = sizeof(struct dir_entry);
    memset(sb.pad, 0, sizeof(sb.pad));
    // 1. Write super blocks to disk
    // 512bytes
    ide_write(part->my_disk, part->start_lba + 1, &sb, 1);

    uint_32 buf_sz =
        (sb.s_imap_sz >= sb.s_zmap_sz) ? sb.s_imap_sz : sb.s_zmap_sz;
    buf_sz = ((buf_sz >= sb.s_inode_table_sz) ? buf_sz : sb.s_inode_table_sz) *
             SECTOR_SIZE;
    uint_8 *buf = sys_malloc(buf_sz);

    // 2. Write free zone bitmap. Start at 3rd sector
    // (aka part->start_lba+2)
    buf[0] |= 0x1;  // the first zone (block) is root directory

    uint_32 zbm_last_byte = bm_len / 8;
    uint_32 zbm_last_bit = bm_len % 8;
    uint_32 last_sz = SECTOR_SIZE - (zbm_last_byte % SECTOR_SIZE);

    // set unused zone to 0xff
    memset(&buf[zbm_last_byte], 0xff, last_sz);
    uint_8 bit_idx = 0;
    while (bit_idx <= zbm_last_bit) {
        buf[zbm_last_byte] &= ~(1 << bit_idx++);
    }
    ide_write(part->my_disk, sb.s_zmap_lba, buf, sb.s_zmap_sz);

    // 3. Write inode bitmap
    memset(buf, 0, buf_sz);
    buf[0] |= 0x1;  // first inode is root inode
    ide_write(part->my_disk, sb.s_imap_lba, buf, sb.s_imap_sz);

    // 4. Write inode table
    memset(buf, 0, buf_sz);
    struct inode *i = (struct inode *) buf;
    i->i_size = sb.dir_entry_size * 2;  // directory . and ..
    i->i_num = 0;                       // the root directory
    // TODO: need inode mode mask
    //       like exec_mode and access right bits
    i->i_mode = FT_DIRECTORY << 11;
    i->i_zones[0] = sb.s_data_start_lba;
    ide_write(part->my_disk, sb.s_inode_table_lba, buf, sb.s_inode_table_sz);

    // 5. free zone
    // write root directory 2 directory entries
    memset(buf, 0, buf_sz);
    struct dir_entry *d_entry = (struct dir_entry *) buf;
    memcpy(d_entry->filename, ".", 1);
    d_entry->i_no = 0;
    d_entry->f_type = FT_DIRECTORY;
    d_entry++;
    memcpy(d_entry->filename, "..", 2);
    d_entry->i_no = 0;
    d_entry->f_type = FT_DIRECTORY;

    ide_write(part->my_disk, sb.s_data_start_lba, buf, 1);

    sys_free(buf);
}
// Mount file system
// TODO: I want to add buffer.c for mounting.
static bool mount_partition(struct list_head *ele, int arg)
{
    char *target_name = (char *) arg;
    struct partition *part = container_of(ele, struct partition, part_tag);
    // if not the right name then next elements
    if (strcmp(target_name, part->name)) {
        return false;
    }

    struct super_block *sb = sys_malloc(sizeof(struct super_block));
    if (!sb) {
        PAINC("Not enough memory");
    }
    ide_read(part->my_disk, part->start_lba + 1, sb, 1);
    ASSERT(sb->s_magic == 0x2023b07a);
    mounted_part.sb = sb;
    mounted_part.my_disk = part->my_disk;
    mounted_part.start_lba = part->start_lba;
    mounted_part.sec_cnt = part->sec_cnt;

    // 1.create zone  bitmap
    uint_8 *zbm_bits = sys_malloc(sb->s_zmap_sz * SECTOR_SIZE);
    if (!zbm_bits) {
        PAINC("Not enough memory");
    }
    mounted_part.zone_bitmap.map_bytes_length = sb->s_zmap_sz * SECTOR_SIZE;
    mounted_part.zone_bitmap.bits = zbm_bits;
    init_bitmap(&mounted_part.zone_bitmap);
    ide_read(part->my_disk, sb->s_zmap_lba, zbm_bits, sb->s_zmap_sz);

    // 2.create inode bitmap
    uint_8 *ibm_bits = sys_malloc(sb->s_imap_sz * SECTOR_SIZE);
    if (!ibm_bits) {
        PAINC("Not enough memory");
    }
    mounted_part.inode_bitmap.map_bytes_length = sb->s_imap_sz * SECTOR_SIZE;
    mounted_part.inode_bitmap.bits = ibm_bits;
    init_bitmap(&mounted_part.inode_bitmap);
    ide_read(part->my_disk, sb->s_imap_lba, ibm_bits, sb->s_imap_sz);

    // init open inode

    init_list_head(&mounted_part.open_inodes);

    return true;
}

// Go through all partition on disk. if some partition does not have any file
// system then make a file system at this partition (aka use partition_format)
void fs_init(void)
{
    uint_8 channel_no = 0;
    uint_8 dev_no = 0;
    uint_8 part_idx = 0;
    uint_8 *buf = sys_malloc(SECTOR_SIZE);
    if (!buf) {
        PAINC("Not enough memory.");
    }
    // init global file table lock
    lock_init(&g_ft_lock);

    while (channel_no < channel_cnt) {
        dev_no = 0;
        while (dev_no < 2) {
            // TODO: jump master disk
            if (dev_no == 0) {
                dev_no++;
                continue;
            }
            struct disk *hd = &channels[channel_no].devices[dev_no];
            for (int i = 0; i < 12; i++) {
                struct partition part = hd->prim_partition[i];
                if (i >= 4) {
                    int idx = i - 4;
                    part = hd->logic_partition[idx];
                }
                memset(buf, 0, SECTOR_SIZE);

                // super block at 2nd sector of partition
                ide_read(hd, part.start_lba + 1, buf, 1);
                struct super_block *sb = (struct super_block *) buf;
                if (sb->s_magic == 0x2023B07A) {
                    continue;
                } else {
                    partition_format(&part);
                }
            }
            dev_no++;
        }
        channel_no++;
    }
    sys_free(buf);

    // mount partition
    char default_p[8] = "sdb1";
    int len = list_length(&partition_list);
    if (!list_is_empty(&partition_list)) {
        list_walker(&partition_list, mount_partition, (int) default_p);
    }
    // Open root directory
    open_root_dir(&mounted_part);

    // Init g_file_table all inode pointer
    for (int i = 0; i < MAX_FILE_OPEN; i++) {
        g_file_table[i].fd_inode = NULL;
    }
}


/**
 * path_peel
 *
 * need pass copy of path
 *
 * divide path into `directory` + `file`
 * e.g ready path: `/home/tom/Desktop/city.img`
 *     break down to parts: `/home/tom/Desktop/` and `city.img`
 * like a reverse version (from tail) `car`
 *
 * @param whole_path path ready to be peeled
 * @param last_name last file(or directory) name
 * @return anther path without last_name
 *****************************************************************************/
static char *path_peel(char *path, char *last_name)
{
    if (strlen(path) == 0)
        return NULL;
    if (strlen(path) == 1 && !strcmp("/", path))
        return path;
    // 1. test if path is valid path
    uint_32 path_len = strlen(path);
    char *cursor = path;
    char *last_slash;
    while (*cursor != '\0') {
        if (*cursor == '/' && cursor - path != path_len - 1) {
            last_slash = cursor;
        }
        cursor++;
    }
    uint_32 res_len = last_slash - path + 1;  // +1 for over '/'
    uint_32 name_len = path_len - res_len;

    memcpy(last_name, path + res_len, name_len);
    last_name[name_len] = '\0';
    path[res_len] = '\0';
    return path;
}


/**
 * path_depth
 * e.g path: `/home/tom/Desktop/city.img`
 *     depth: 4
 *
 * @param path a path
 * @return path depth number
 *****************************************************************************/
int_32 path_depth(const char *path)
{
    if (strlen(path) == 0)
        return 0;
    char name[MAX_FILE_NAME_LEN] = {0};
    char *tmp_path = sys_malloc(strlen(path));
    memcpy(tmp_path, path, strlen(path));
    char *p_path;
    p_path = path_peel(tmp_path, name);
    uint_32 depth = 1;
    while (strcmp(p_path, "/")) {
        p_path = path_peel(p_path, name);
        depth++;
    }
    sys_free(tmp_path);
    return depth;
}

/**
 * Break a path into all level directories without root directory aka ("/")
 *
 * e.g  path : "/zm/Development/C/playground/code/test.txt",
 * e.g  path1: "/zm/Development/C/playground/code/test/",
 *  dirs: {
 *          "zm/",
 *          "Development/",
 *          "C/",
 *          "playground/",
 *          "code/"
 *        }
 * @param path absolute path
 * @param dirs write dir into this address sys_malloc() outside
 * @return 0 when success
 *         -1 when failed
 *****************************************************************************/
static int_32 path_dirs(const char *path, char *dirs)
{
    int_32 len = strlen(path);
    char *tmp_path = sys_malloc(len);
    if (!tmp_path) {
        // TODO:
        // kprint("Not enough memory at path_dir()");
        return -1;
    }
    char *next_slot = dirs;
    memcpy(tmp_path, path, len);

    while (strcmp("/", tmp_path)) {
        char last[MAX_FILE_NAME_LEN] = {0};
        tmp_path = path_peel(tmp_path, last);
        int_32 len = strlen(last);
        // Only take directory
        if (last[len - 1] == '/') {
            memcpy(next_slot, last, len);
            next_slot += MAX_FILE_NAME_LEN;
        }
    }

    sys_free(tmp_path);
    return 0;
}

/**
 * Search file which name is `name` from directory and return inode number
 *
 *
 * @param part mounted partition
 * @param this_dir search at this dir
 * @param name searched file name
 * @return target inode
 *         if failed return -1
 *****************************************************************************/
static int_32 search_file_recur(struct partition *part,
                                struct dir *this_dir,
                                const char *name)
{
    // Read this_dir->inode->i_zones[] for all dir_entry and dig into directory
    // at this_dir
    // 1. read p_dir from disk by p_dir.inode
    uint_32 all_zones[140] = {0};
    // First 12 i_zones[] elements is direct address of data (aka dir_entry)
    for (int i = 0; i < 12; i++) {
        all_zones[i] = this_dir->inode->i_zones[i];
    }
    // the 13th i_zones[] is a in-direct table which size is 512 bytes
    if (this_dir->inode->i_zones[12]) {
        uint_32 *buf = (uint_32 *) sys_malloc(512);
        ide_read(part->my_disk, this_dir->inode->i_zones[12], buf, 1);
        for (int i = 0; i < 512 / 4; i++) {
            all_zones[12 + i] = buf[i];
        }
        sys_free(buf);
    }

    struct dir_entry *entries_buf = sys_malloc(512);
    for (int zones_idx = 0; zones_idx < 140; zones_idx++) {
        ide_read(part->my_disk, all_zones[zones_idx], entries_buf, 1);
        for (int entry_idx = 0; entry_idx < (512 / sizeof(struct dir_entry));
             entry_idx++) {
            struct dir_entry cur_entry = entries_buf[entry_idx];
            // Skip directory '.' and '..'
            if (strcmp(cur_entry.filename, ".") == 0 ||
                strcmp(cur_entry.filename, "..") == 0) {
                continue;
            }
            if (cur_entry.i_no == 0 && cur_entry.f_type == FT_UNKOWN) {
                sys_free(entries_buf);
                return -1;
            }
            if (cur_entry.f_type == FT_DIRECTORY) {
                struct dir *next_d =
                    dir_open(part, entries_buf[entry_idx].i_no);
                int_32 inode_nr = search_file_recur(part, next_d, name);
                dir_close(next_d);
                sys_free(entries_buf);
                return inode_nr;
            } else if (cur_entry.f_type == FT_REGULAR) {
                if (strcmp(cur_entry.filename, name) == 0) {
                    sys_free(entries_buf);
                    return cur_entry.i_no;
                }
            }
        }
    }

    sys_free(entries_buf);
    return -1;
}

/**
 * find file or directory in parent directory
 *
 * @param part partition
 * @return inode if success
 *          -1 if failed
 *****************************************************************************/
static int_32 search_file(struct partition *part,
                          struct dir *this_dir,
                          const char *name)
{
    // Read this_dir->inode->i_zones[] for all dir_entry and dig into directory
    // at this_dir
    // 1. read p_dir from disk by p_dir.inode
    uint_32 all_zones[140] = {0};
    // First 12 i_zones[] elements is direct address of data (aka dir_entry)
    for (int i = 0; i < 12; i++) {
        all_zones[i] = this_dir->inode->i_zones[i];
    }
    // the 13th i_zones[] is a in-direct table which size is 512 bytes
    if (this_dir->inode->i_zones[12]) {
        uint_32 *buf = (uint_32 *) sys_malloc(512);
        ide_read(part->my_disk, this_dir->inode->i_zones[12], buf, 1);
        for (int i = 0; i < 512 / 4; i++) {
            all_zones[12 + i] = buf[i];
        }
        sys_free(buf);
    }

    struct dir_entry *entries_buf = sys_malloc(512);
    for (int zones_idx = 0; zones_idx < 140; zones_idx++) {
        ide_read(part->my_disk, all_zones[zones_idx], entries_buf, 1);
        for (int entry_idx = 0; entry_idx < (512 / sizeof(struct dir_entry));
             entry_idx++) {
            struct dir_entry cur_entry = entries_buf[entry_idx];
            // Skip directory '.' and '..'
            if (strcmp(cur_entry.filename, ".") == 0 ||
                strcmp(cur_entry.filename, "..") == 0) {
                continue;
            }
            if (cur_entry.i_no == 0 && cur_entry.f_type == FT_UNKOWN) {
                sys_free(entries_buf);
                return -1;
            }
            if (strcmp(cur_entry.filename, name) == 0) {
                sys_free(entries_buf);
                return cur_entry.i_no;
            }
        }
    }

    sys_free(entries_buf);
    return -1;
}

/**
 * find file or directory by a absolute path
 *
 * @param part partition
 * @param pathname path in absolute form
 * @param pdir file or dir is at this parent directory
 * @return inode number
 *****************************************************************************/
// /home/zm/Development/test.c
// /home/zm/Development/
static int_32 search_file_with_pathname(struct partition *part,
                                        const char *pathname,
                                        struct dir *pdir)
{
    bool is_file = false;
    if (pathname[strlen(pathname) - 1] != '/') {
        is_file = true;
    }
    char last_name[MAX_FILE_NAME_LEN];
    char *tmp_path = sys_malloc(strlen(pathname));
    if (!tmp_path) {
        // TODO:
        // kprint("Not enough memory at path_dir()");
        return -1;
    }
    memcpy(tmp_path, pathname, strlen(pathname));
    path_peel(tmp_path, last_name);
    sys_free(tmp_path);

    struct dir *prev_d = NULL;
    struct dir *d = &root_dir;
    int_32 depth = path_depth(pathname);
    char *dirs_path = sys_malloc(MAX_FILE_NAME_LEN * depth);
    path_dirs(pathname, dirs_path);

    int_32 dirs_cnt;
    for (dirs_cnt = 0;
         (dirs_path[dirs_cnt]) && dirs_cnt < depth * MAX_FILE_NAME_LEN;
         dirs_cnt += MAX_FILE_NAME_LEN)
        ;
    dirs_cnt /= MAX_FILE_NAME_LEN;
    dirs_cnt = !dirs_cnt ? 1 : dirs_cnt;
    // file at root directory
    if (dirs_cnt == 1) {
        int_32 f_inode = search_file(part, d, last_name);
        if (f_inode != -1) {
            sys_free(dirs_path);
            memcpy(pdir, d, sizeof(struct dir));
            return f_inode;
        } else {
            sys_free(dirs_path);
            return -1;
        }
    } else {
        // 2. test all directories
        //  -1 is skip '/' root directory and last file;
        for (int_32 idx = (dirs_cnt - 1) * MAX_FILE_NAME_LEN; idx >= 0;
             idx -= MAX_FILE_NAME_LEN) {
            char *dir_name = &dirs_path[idx];
            int_32 d_inode_nr = search_file(part, d, dir_name);
            if (d_inode_nr != -1) {
                prev_d = d;
                d = dir_open(part, d_inode_nr);
            } else {
                return -1;
            }
            // when the last turn of this dirs
            if (idx == 0 && !is_file) {
                memcpy(pdir, prev_d, sizeof(struct dir));
                return d_inode_nr;
            }
        }
        sys_free(dirs_path);
        // this time d is the innerest directory
        int_32 file_inode_nr = search_file(part, d, last_name);
        memcpy(pdir, d, sizeof(struct dir));
        return file_inode_nr;
    }
}

/**
 * system fs api
 * sys_open
 *
 * @param pathname path of file (it must be a file but directory)
 * @param flags flags for choosing method which is defined at fs/fcntl.h
 * @return file descriptor if success
 *         return -1 when failed
 *****************************************************************************/
int_32 sys_open(const char *pathname, uint_8 flags)
{
    int_32 fd = -1;
    // 1. test path
    // 2.test if this file is exist at `pathname` at struct dir_entry
    if ('/' == pathname[strlen(pathname) - 1]) {
        return -1;
    }
    char *path = sys_malloc(strlen(pathname));
    char last_name[MAX_FILE_NAME_LEN];
    memcpy(path, pathname, strlen(pathname));
    path_peel(path, last_name);

    struct dir *next_dir = &root_dir;
    char cur_name[MAX_FILE_NAME_LEN] = {0};

    int_32 depth = path_depth(pathname);
    char *dirs_path = sys_malloc(MAX_FILE_NAME_LEN * depth);
    // Open file not at root directory directly
    if (depth > 1) {
        if (path_dirs(pathname, dirs_path)) {
            return -1;
        };

        // 2. test all directories
        //  -2 is skip '/' root directory and last file;
        for (int_32 idx = (depth - 2) * MAX_FILE_NAME_LEN; idx >= 0;
             idx -= MAX_FILE_NAME_LEN) {
            memcpy(cur_name, dirs_path + idx, MAX_FILE_NAME_LEN);
            int_32 d_inode = search_file(&mounted_part, next_dir, cur_name);
            if (d_inode == -1) {
                // TODO:
                // kprint("file not find.");
                dir_close(next_dir);
                return -1;
            }
            dir_close(next_dir);  // dir_close() does not close root directory
            next_dir = dir_open(&mounted_part, d_inode);
            memset(cur_name, 0, MAX_FILE_NAME_LEN);
        }
        sys_free(dirs_path);
    }
    int_32 file_inode = search_file(&mounted_part, next_dir, last_name);
    if ((file_inode == -1) && !(flags & O_CREAT)) {
        // TODO:
        // kprint("File not found and want to read file.");
        dir_close(next_dir);
        return -1;
    } else if ((file_inode != -1) &&
               (flags & O_CREAT)) {  // find and create a file
        // TODO:
        // kprint("File found and want to create same name file.");
        dir_close(next_dir);
        return -1;
    }
    // 3.if it is exist, test if it is directory return -1
    switch (flags & O_CREAT) {
    case O_CREAT:
        fd = file_create(&mounted_part, next_dir, last_name, flags);
        dir_close(next_dir);
        break;
    default:
        /*when flags is O_RDWR, O_RDONLY, O_WRONLY */
        fd = file_open(&mounted_part, file_inode, flags);
        break;
    }

    return fd;
}

/**
 * close file
 *
 * @param param write here param Comments write here
 * @return -1 failed
 *          0 success
 *****************************************************************************/
static uint_32 fd_local2global(uint_32 local_fd)
{
    TCB_t *cur = running_thread();
    int_32 g_fd = cur->fd_table[local_fd];
    ASSERT(g_fd >= 0 && g_fd < MAX_FILE_OPEN);
    return g_fd;
}

int_32 sys_close(int_32 fd)
{
    // fd = 0, 1, 2 is reserved for stdin, stdout, stderr
    int_32 ret = -1;
    if (fd > 2) {
        uint_32 g_fd = fd_local2global(fd);
        ret = file_close(&g_file_table[g_fd]);
        running_thread()->fd_table[fd] = -1;
    }
    return ret;
}

/**
 * system write
 *
 * write a buffer to a file descriptor
 *
 * @param fd file descriptor
 * @param buf buffer
 * @param count buffer count
 * @return -1 if failed
 *****************************************************************************/
int_32 sys_write(int_32 fd, const void *buf, uint_32 count)
{
    if (fd < 0) {
        // TODO
        // kprint("sys_write: fd error");
        return -1;
    }
    if (fd == FD_STDOUT_NO) {
        char tmp[1024] = {0};
        memcpy(tmp, buf, count);
        // TODO:
        // put string to 0xb0000 address
        // put_str(tmp);
        return count;
    }
    uint_32 g_fd = fd_local2global(fd);
    struct file *wr_file = &g_file_table[g_fd];

    if (wr_file->fd_flag & O_WRONLY || wr_file->fd_flag & O_RDWR) {
        uint_32 bytes_written = file_write(&mounted_part, wr_file, buf, count);
        return bytes_written;
    } else {
        // TODO:
        // put string
        // put_str("sys_write: not allowed to write file without flag O_RDWR or
        //_WRONLY\n");
        return -1;
    }
}

/**
 * system read
 *
 * read a file from a file descriptor to a buffer
 *
 * @param fd file descriptor
 * @param buf buffer
 * @param count buffer count
 * @return -1 if failed
 *****************************************************************************/
int_32 sys_read(int_32 fd, void *buf, uint_32 count)
{
    if (fd < 0) {
        // TODO
        // kprint("sys_write: fd error");
        return -1;
    }
    if (fd == FD_STDIN_NO) {
        // TODO:
        // put string to 0xb0000 address
        // put_str(tmp);
        /* return count; */
    }
    uint_32 g_fd = fd_local2global(fd);
    struct file *f = &g_file_table[g_fd];
    file_read(&mounted_part, f, buf, count);
    return 0;
}

/**
 * sys_lseek
 *
 * set a file position
 *
 * @param fd file descriptor
 * @param offset new offset
 * @param whence base offset for seek
 * @return file position if success
 *         -1 if failed
 *****************************************************************************/
int_32 sys_lseek(int_32 fd, int_32 offset, uint_8 whence)
{
    if (fd < 0) {
        // TODO:
        // kprint("sys_lseek: fd error");
        return -1;
    }
    ASSERT(whence > 0 && whence < 4);
    uint_32 g_fd = fd_local2global(fd);
    struct file *f = &g_file_table[g_fd];
    int_32 new_pos = 0;
    int_32 file_size = (int_32) f->fd_inode->i_size;
    if (whence == SEEK_SET) {
        new_pos = offset;
    } else if (whence == SEEK_CUR) {
        new_pos = (int_32) f->fd_pos + offset;
    } else if (whence == SEEK_END) {
        new_pos = file_size + 1 + (offset - 1);
    } else {
        // TODO:
        // kprint("sys_lseek: whence error");
        return -1;
    }
    if (new_pos < 0 || new_pos > file_size) {
        return -1;
    }
    f->fd_pos = new_pos;
    return f->fd_pos;
}
/**
 *ONLY delete file NOT DIRECTORY!
 *
 *  unlink()  deletes a name from the filesystem.  If that name
 *  was the last link to a file and no processes have the  file
 *  open,  the  file  is  deleted and the space it was using is
 *  made available for reuse.
 *
 *  If the name was the last link to a file but  any  processes
 *  still have the file open, the file will remain in existence
 *  until the last file descriptor referring to it is closed.
 *  If the name referred to a symbolic link, the  link  is  re‐
 *  moved.
 *
 *  If the name referred to a socket, FIFO, or device, the name
 *  for it is removed but processes which have the object  open
 *  may continue to use it.
 *
 * @param pathname path
 * @return On success, zero is returned.  On error,  -1  is  returned,
 *         and errno is set appropriately.(no we don't have this now)
 *****************************************************************************/
int_32 sys_unlink(const char *pathname)
{
    /* struct inode_unlink = sea */
    // 1. If that name was the last link to a file and no processes have the
    // file open,  the  file  is  deleted and the space it was using is made
    // available for reuse.
    char *io_buf = sys_malloc(1024);
    if (!io_buf) {
        return -1;
    }
    struct partition *part = &mounted_part;
    struct dir *pdir = sys_malloc(sizeof(struct dir));
    if (!pdir) {
        sys_free(io_buf);
        return -1;
    }
    int_32 inode_nr = search_file_with_pathname(part, pathname, pdir);
    if (inode_nr == -1) {
        sys_free(pdir);
        sys_free(io_buf);
        return -1;
    }
    struct inode *unlinked_inode = inode_open(part, inode_nr);
    uint_32 file_idx = 0;
    while (file_idx < MAX_FILE_OPEN) {
        if (g_file_table[file_idx].fd_inode != NULL &&
            inode_nr == g_file_table[file_idx].fd_inode->i_num) {
            break;
        }
        file_idx++;
    }
    if (file_idx < MAX_FILE_OPEN) {
        // TODO:
        // kprint("some process is use this file.");
        return -1;
    }

    if (unlinked_inode->i_nlinks == 0) {
        delete_dir_entry(part, pdir, inode_nr, io_buf);
        inode_release(part, inode_nr);
        sys_free(io_buf);
        return 0;
    } else {
        // TODO:
        // kprint("some process is use this file.");
        unlinked_inode->i_nlinks -= 1;
        flush_inode(part, unlinked_inode, io_buf);
        sys_free(io_buf);
        return -1;
    }
}
