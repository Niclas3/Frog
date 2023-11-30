#include <debug.h>
#include <device/ide.h>
#include <fs/dir.h>
#include <fs/file.h>
#include <fs/fs.h>
#include <fs/inode.h>
#include <fs/super_block.h>
#include <string.h>
#include <sys/memory.h>

struct dir root_dir;  // global variable for root directory

static void inode_all_zones(struct partition *part,
                            struct inode *inode,
                            uint_32 *all_zones)
{
    // Find first accessible i_zones[i]
    // 1. read all i_zones;
    // First 12 i_zones[] elements is direct address of data (aka dir_entry)
    for (int i = 0; i < 12 && inode->i_zones[i]; i++) {
        all_zones[i] = inode->i_zones[i];
    }
    // the 13th i_zones[] is a in-direct table which size is ZONE_SIZE bytes
    if (inode->i_zones[12]) {
        ide_read(part->my_disk, inode->i_zones[12], all_zones + 12, 1);
    }
}
/**
 * open root directory
 * only open one time.
 *
 * Load root inode to memory
 *
 * @param part mounted_partition
 * @return void
 *****************************************************************************/
void open_root_dir(struct partition *part)
{
    root_dir.inode = inode_open(part, 0);
    root_dir.dir_pos = 0;  // number 1 directory of all directory
    memset(root_dir.dir_buf, 0, sizeof(root_dir.dir_buf));
}


/**
 * Search dir_entry (file or directory or ...) by name at given directory
 *
 * @param part a mounted partition
 * @param name entry name
 * @param d in what directory
 * @param target_entry if find copy to this pointer
 * @return return 0 when success
 *         return !0 when failed
 *****************************************************************************/
int_32 search_dir_entry(struct partition *part,
                        char *name,
                        struct dir *d,
                        struct dir_entry *target_entry)
{
    uint_32 zone_cnt = MAX_SINGLE_INODE_DATA_SIZE;
    // name                          i_zones_entry             size in bytes
    // 12 direct access i_zones[] -> 12 addresses           -> 12 * 4 = 48 bytes
    // 1  1-layer access          -> 512 / 4 = 128 addresses-> 512 bytes
    uint_32 *all_zones = sys_malloc(12 * 4 + 512);
    if (!all_zones) {
        // TODO: printk("");
        PAINC("Not enough memory when search_dir_entry");
        return -1;
    }
    uint_32 z_idx = 0;
    while ((d->inode->i_zones[z_idx] != 0) && z_idx < 12) {
        *(all_zones + z_idx) = d->inode->i_zones[z_idx];
        z_idx++;
    }
    // load 2rd-layer table
    uint_32 second_layer_lba = d->inode->i_zones[12];
    if (z_idx == 13 && second_layer_lba != 0) {
        uint_32 *second_layer_buf = sys_malloc(512);
        ide_read(part->my_disk, second_layer_lba, second_layer_buf, 1);
        for (int i = 0; i < 128; i++) {
            if (second_layer_buf[i] == 0) {
                break;
            }
            memcpy((all_zones + z_idx + i), second_layer_buf, 4);
        }
        sys_free(second_layer_buf);
    }

    // there are 140 lba address
    // Go through all lba address sector by sector. 1 sector has
    // 512/sizeof(struct dir_entry) dir_entries. Testing all dir_entries find
    // entry name equals given name.
    uint_8 *buf = sys_malloc(512);
    for (int i = 0; i < 140; i++) {
        uint_32 d_entry_lba = all_zones[i];
        memset(buf, 0, 512);
        ide_read(part->my_disk, d_entry_lba, buf, 1);
        struct dir_entry *entries = (struct dir_entry *) buf;
        for (int entry_idx = 0; entry_idx < (512 / sizeof(struct dir_entry));
             entry_idx++) {
            struct dir_entry entry = *(entries + entry_idx);
            if (!strncmp(entry.filename, name, strlen(name))) {
                memcpy(target_entry, &entry, sizeof(struct dir_entry));
                sys_free(all_zones);
                return 0;
            } else if (strlen(entry.filename) <= 0) {
                break;
            }
        }
    }
    sys_free(buf);
    sys_free(all_zones);
    return -1;
}

/**
 * Search dir_entry (file or directory or ...) by name at given directory
 *
 * @param part a mounted partition
 * @param inode_nr inode number
 * @param d in what directory
 * @param target_entry if find copy to this pointer
 * @return return target lba when success
 *         return !0 when failed
 *****************************************************************************/
int_32 search_dir_entry_by_inodenr(struct partition *part,
                                   uint_32 inode_nr,
                                   struct dir *d,
                                   struct dir_entry *target_entry)
{
    // name                          i_zones_entry             size in bytes
    // 12 direct access i_zones[] -> 12 addresses           -> 12 * 4 = 48 bytes
    // 1  1-layer access          -> 512 / 4 = 128 addresses-> 512 bytes
    // 140
    uint_32 all_zones[MAX_ZONE_COUNT] = {0};
    inode_all_zones(part, d->inode, all_zones);

    // there are 140 lba address
    // Go through all lba address sector by sector. 1 sector has
    // 512/sizeof(struct dir_entry) dir_entries. Testing all dir_entries find
    // entry name equals given name.
    uint_8 *buf = sys_malloc(512);
    for (int i = 0; i < MAX_ZONE_COUNT; i++) {
        uint_32 d_entry_lba = all_zones[i];
        uint_32 zone_link = 0;  // it shows how many entry in this zone.
        memset(buf, 0, 512);
        ide_read(part->my_disk, d_entry_lba, buf, 1);
        struct dir_entry *entries = (struct dir_entry *) buf;
        for (int entry_idx = 0; entry_idx < (512 / sizeof(struct dir_entry));
             entry_idx++) {
            struct dir_entry entry = *(entries + entry_idx);
            // if entry is not . or ..
            if (strcmp(entry.filename, ".") || strcmp(entry.filename, "..")) {
                zone_link++;
            }
            if (entry.i_no == inode_nr) {
                memcpy(target_entry, &entry, sizeof(struct dir_entry));
                sys_free(all_zones);
                return d_entry_lba;
            } else {
                break;
            }
        }
    }
    sys_free(buf);
    sys_free(all_zones);
    return -1;
}
/**
 * open directory
 *
 * @param param write here param Comments write here
 * @return return Comments write here
 *****************************************************************************/
struct dir *dir_open(struct partition *part, uint_32 inode_nr)
{
    struct dir *d = (struct dir *) sys_malloc(sizeof(struct dir));
    d->inode = inode_open(part, inode_nr);
    d->dir_pos = 0;  // for lseek
    return d;
}

/**
 * close directory
 *
 * Root directory cannot be closed
 *****************************************************************************/
void dir_close(struct dir *d)
{
    if (d != &root_dir) {
        inode_close(d->inode);
        sys_free(d);
    }
}

/**
 * create dir entry
 *
 * @param param write here param Comments write here
 * @return return Comments write here
 *****************************************************************************/
void new_dir_entry(char *name,
                   uint_32 inode_nr,
                   enum file_type file_type,
                   struct dir_entry *entry)
{
    uint_32 nlen = strlen(name);
    ASSERT(nlen < MAX_FILE_NAME_LEN);
    memcpy(entry->filename, name, nlen);
    entry->i_no = inode_nr;
    entry->f_type = file_type;
}

/**
 * flush_dir_entry
 *
 * flush new dir entry to disk
 *
 * flush dir_entry to disk under given parent_dir
 * @param p_dir parent directory
 * @param new_entry new entry
 * @param io_buf need a 512 bytes size buffer
 * @return return 0 when success
 *         return !0 when failed
 *
 *****************************************************************************/
int_32 flush_dir_entry(struct partition *part,
                       struct dir *p_dir,
                       struct dir_entry *new_entry,
                       void *io_buf)
{
    // 1. read p_dir from disk by p_dir.inode
    uint_32 all_zones[140] = {0};
    // First 12 i_zones[] elements is direct address of data (aka dir_entry)
    for (int i = 0; i < 12; i++) {
        all_zones[i] = p_dir->inode->i_zones[i];
    }
    // the 13th i_zones[] is a in-direct table which size is 512 bytes
    if (p_dir->inode->i_zones[12]) {
        uint_32 *buf = (uint_32 *) io_buf;
        ide_read(part->my_disk, p_dir->inode->i_zones[12], buf, 1);
        for (int i = 0; i < 512 / 4; i++) {
            all_zones[12 + i] = buf[i];
        }
    }

    // 2. Go through all_zones address find the last entry
    //  2.1
    memset(io_buf, 0, 512);
    int entry_idx;
    int zones_idx;
    uint_32 zone_lba = -1;

    struct dir_entry *entries_buf = io_buf;
    for (int zones_idx = 0; (zones_idx < 140); zones_idx++) {
        // No find accessible entry, need to allocate new zone for new entry
        if (all_zones[zones_idx] == 0) {
            // allocate a zone at bitmap
            zone_lba = zone_bitmap_alloc(part);
            if (zone_lba == -1) {
                // TODO:
                // kprint("Error at flush_dir_entry");
                ASSERT(zone_lba != -1);
                return -1;
            }
            // Flush allocated zone to disk
            flush_bitmap(part, ZONE_BITMAP,
                         zone_lba - part->sb->s_data_start_lba);
            if (zones_idx < 12) {  // direct table
                p_dir->inode->i_zones[zones_idx] = all_zones[zones_idx] =
                    zone_lba;
            } else if (zone_lba == 12) {  // in-direct table
                p_dir->inode->i_zones[12] = zone_lba;
                zone_lba = -1;
                zone_lba = zone_bitmap_alloc(part);
                if (zone_lba == -1) {
                    // unset zone bitmap
                    uint_32 bitmap_idx =
                        p_dir->inode->i_zones[12] - part->sb->s_data_start_lba;
                    set_value_bitmap(&part->zone_bitmap, bitmap_idx, 0);
                    p_dir->inode->i_zones[12] = 0;
                    // TODO:
                    // kprint("Error at flush_dir_entry");
                    ASSERT(zone_lba != -1);
                    return -1;
                } else {
                    uint_32 bitmap_idx =
                        p_dir->inode->i_zones[12] - part->sb->s_data_start_lba;
                    flush_bitmap(part, ZONE_BITMAP, bitmap_idx);
                    all_zones[12] = zone_lba;
                    ide_write(part->my_disk, p_dir->inode->i_zones[12],
                              all_zones + 12, 1);
                }
                memset(io_buf, 0, 512);
                memcpy(io_buf, new_entry, sizeof(struct dir_entry));
                ide_write(part->my_disk, all_zones[zones_idx], io_buf, 1);
                p_dir->inode->i_size += sizeof(struct dir_entry);
                return 0;
            }
        } else {
            ide_read(part->my_disk, all_zones[zones_idx], entries_buf, 1);
            for (entry_idx = 0; entry_idx < (512 / sizeof(struct dir_entry));
                 entry_idx++) {
                if ((strlen(entries_buf[entry_idx].filename) == 0) &&
                    entries_buf[entry_idx].f_type == FT_UNKOWN) {
                    // Find accessible entry index at entry_idx
                    memcpy(&entries_buf[entry_idx], new_entry,
                           sizeof(struct dir_entry));
                    ide_write(part->my_disk, all_zones[zones_idx], entries_buf,
                              1);
                    p_dir->inode->i_size += sizeof(struct dir_entry);
                    return 0;
                }
            }
        }
    }

    return -1;
}

/**
 * Delete dir entry from parent directory
 *
 * @param part mounted partition
 * @param pdir deleted entry from this parent directory
 * @param inode_nr deleted entry's inode number
 * @param io_buf io buffer
 * @return
 *****************************************************************************/
void delete_dir_entry(struct partition *part,
                      struct dir *pdir,
                      uint_32 inode_nr,
                      void *io_buf)
{
    struct dir_entry *target_entry = {0};
    // name                          i_zones_entry             size in bytes
    // 12 direct access i_zones[] -> 12 addresses           -> 12 * 4 = 48 bytes
    // 1  1-layer access          -> 512 / 4 = 128 addresses-> 512 bytes
    // 140
    uint_32 all_zones[MAX_ZONE_COUNT] = {0};
    inode_all_zones(part, pdir->inode, all_zones);

    // there are 140 lba address
    // Go through all lba address sector by sector. 1 sector has
    // 512/sizeof(struct dir_entry) dir_entries. Testing all dir_entries find
    // entry name equals given name.
    uint_8 *buf = io_buf;
    int zone_idx;
    struct dir_entry *found_entry = NULL;
    for (zone_idx = 0; zone_idx < MAX_ZONE_COUNT; zone_idx++) {
        uint_32 d_entry_lba = all_zones[zone_idx];
        uint_32 zone_link = 0;  // it shows how many entry in this zone.
        memset(buf, 0, 512);
        ide_read(part->my_disk, d_entry_lba, buf, 1);
        struct dir_entry *entries = (struct dir_entry *) buf;
        for (int entry_idx = 0; entry_idx < (512 / sizeof(struct dir_entry));
             entry_idx++) {
            struct dir_entry entry = *(entries + entry_idx);
            // if entry is not . or ..
            if (strcmp(entry.filename, ".") && strcmp(entry.filename, "..")) {
                zone_link++;
            }
            if (entry.i_no == inode_nr) {
                memcpy(target_entry, &entry, sizeof(struct dir_entry));
                found_entry = entries + entry_idx;
            }
        }
        if (!found_entry) {
            continue;
        }
        // when find target
        if (found_entry && zone_link == 0) {
            uint_32 zone_bitmap_idx =
                all_zones[zone_idx] - part->sb->s_data_start_lba;
            set_value_bitmap(&part->zone_bitmap, ZONE_BITMAP, 0);
            flush_bitmap(part, ZONE_BITMAP, zone_bitmap_idx);
            if (zone_idx < 12) {
                pdir->inode->i_zones[zone_idx] = 0;
            } else {
                uint_32 indirect_zones_cnt = 0;
                uint_32 indirect_zones_idx = 12;
                for (; indirect_zones_idx < MAX_ZONE_COUNT;
                     indirect_zones_idx++) {
                    if (all_zones[indirect_zones_idx]) {
                        indirect_zones_cnt++;
                    }
                }
                ASSERT(indirect_zones_cnt >= 1);
                // If has other zones in indirect table then not recycle
                // indirect table
                if (indirect_zones_cnt > 1) {
                    all_zones[zone_idx] = 0;
                    ide_write(part->my_disk, pdir->inode->i_zones[12],
                              all_zones + 12, 1);
                } else {  // if only has one (include current entry) then
                          // recycle all indirect table
                    zone_bitmap_idx =
                        pdir->inode->i_zones[12] - part->sb->s_data_start_lba;
                    set_value_bitmap(&part->zone_bitmap, zone_bitmap_idx, 0);
                    flush_bitmap(part, ZONE_BITMAP, zone_bitmap_idx);
                    pdir->inode->i_zones[12] = 0;
                }
            }
        } else if (found_entry) {
            memset(found_entry, 0, sizeof(struct dir_entry));
            ide_write(part->my_disk, all_zones[zone_idx], entries, 1);
        }
        // flush parent directory inode
        pdir->inode->i_size -= sizeof(struct dir_entry);
        memset(io_buf, 0, 512 * 2);
        flush_inode(part, pdir->inode, io_buf);
    }
}

/**
 * The readdir() function returns a pointer to a dirent structure represent‐
 * ing the next directory entry in the directory stream pointed to by  dirp.
 * It  returns NULL on reaching the end of the directory stream or if an er‐
 * ror occurred.
 *
 * @param dirp directory
 * @return a table about dir entries
 *****************************************************************************/
struct dir_entry *read_dir(struct dir *dirp)
{
    // All directory entries store in i_zones. all size is
    // (MAX_ZONE_COUNT * ZONE_SIZE) aka (140 * 512)
    // read parent directory i_zones
    struct partition *part = &mounted_part;
    uint_32 all_zones[MAX_ZONE_COUNT] = {0};
    uint_8 *buf = sys_malloc(ZONE_SIZE);
    if (!buf) {
        return NULL;
    }
    inode_all_zones(part, dirp->inode, all_zones);
    // directory position things
    // One zone can hold 512 / sizeof(struct dir_entry) directory entries
    // pdir->dir_pos = size_of_de * number;
    int_32 enties_cnt_per_zone = ZONE_SIZE / sizeof(struct dir_entry);
    int_32 zone_idx = 0;
    int_32 entry_idx = 0;
    zone_idx = (dirp->dir_pos / sizeof(struct dir_entry)) / enties_cnt_per_zone;
    entry_idx =
        (dirp->dir_pos / sizeof(struct dir_entry)) % enties_cnt_per_zone;


    for (; all_zones[zone_idx] && zone_idx < MAX_ZONE_COUNT; zone_idx++) {
        ide_read(part->my_disk, all_zones[zone_idx], buf, 1);
        struct dir_entry *dire_buf = (struct dir_entry *) buf;
        for (; entry_idx < 512 / sizeof(struct dir_entry); entry_idx++) {
            struct dir_entry *entry = &dire_buf[entry_idx];
            if (entry->i_no == 0 && strlen(entry->filename) == 0) {
                break;
            }
            memcpy(dirp->dir_buf, entry, sizeof(struct dir_entry));
            dirp->dir_pos += sizeof(struct dir_entry);
            sys_free(buf);
            return (struct dir_entry *) dirp->dir_buf;
        }
    }
    sys_free(buf);
    return NULL;
}

/**
 * test directory if is empty.
 *
 * if this directory dont have . and ..
 *
 *****************************************************************************/
bool dir_is_empty(struct dir *dirp)
{
    return (dirp->inode->i_size == (sizeof(struct dir_entry)) * 2);
}
/**
 * dir_remove() deletes a directory, which must be empty.
 *
 * @param param write here param Comments write here
 * @return  on success return 0
 *          on failed  return -1
 *****************************************************************************/
int_32 dir_remove(struct partition *part,
                  struct dir *parent_dir,
                  struct dir *child_dir)
{
    struct inode *child_dir_inode = child_dir->inode;
    int_32 zone_idx = 1;
    // only i_zones[0] has data
    for (; zone_idx < 13; zone_idx++) {
        ASSERT(child_dir_inode->i_zones[zone_idx] == 0);
    }
    void *io_buf = sys_malloc(SECTOR_SIZE * 2);
    if (!io_buf) {
        // TODO:
        // kprint("dir_remove: not enough memory");
        return -1;
    }
    delete_dir_entry(part, parent_dir, child_dir_inode->i_num, io_buf);
    inode_release(part, child_dir_inode->i_num);
    sys_free(io_buf);
    return 0;
}
