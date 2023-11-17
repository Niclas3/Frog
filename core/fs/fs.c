#include <fs/dir.h>
#include <fs/fs.h>
#include <fs/inode.h>
#include <fs/super_block.h>

#include <device/ide.h>
#include <fs/file.h>
#include <math.h>
#include <string.h>
#include <sys/memory.h>

#include <debug.h>

// global variable define at ide.c
extern struct ide_channel channels[2];  // 2 different channels
extern uint_8 channel_cnt;
extern struct list_head partition_list;  // partition list
extern struct file g_file_table[MAX_FILE_OPEN];

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
    /* mounted_part.start_lba = part->start_lba; */
    /* mounted_part.sec_cnt = part->sec_cnt; */

    // 1.create zone  bitmap
    uint_8 *zbm_bits = sys_malloc(sb->s_zmap_sz * SECTOR_SIZE);
    if (!zbm_bits) {
        PAINC("Not enough memory");
    }
    ide_read(part->my_disk, sb->s_zmap_lba, zbm_bits, sb->s_zmap_sz);
    init_bitmap(&mounted_part.zone_bitmap);
    mounted_part.zone_bitmap.bits = zbm_bits;
    mounted_part.zone_bitmap.map_bytes_length = sb->s_zmap_lba * SECTOR_SIZE;

    // 2.create inode bitmap
    uint_8 *ibm_bits = sys_malloc(sb->s_imap_sz * SECTOR_SIZE);
    if (!ibm_bits) {
        PAINC("Not enough memory");
    }
    ide_read(part->my_disk, sb->s_imap_lba, ibm_bits, sb->s_imap_sz);
    init_bitmap(&mounted_part.inode_bitmap);
    mounted_part.inode_bitmap.bits = ibm_bits;
    mounted_part.inode_bitmap.map_bytes_length = sb->s_imap_sz * SECTOR_SIZE;

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
                ide_read(hd, part.start_lba, buf, 1);
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
int_32 path_depth(char *path)
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
 * Search file from directory and return inode number
 *
 * @param this_dir search at this dir
 * @param name searched file name
 * @return target inode
 *         if failed return -1
 *****************************************************************************/
int_32 search_file(struct partition *part,
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
                struct dir *sub_d = dir_open(part, entries_buf[entry_idx].i_no);
                int_32 inode_nr = search_file(part, sub_d, name);
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
