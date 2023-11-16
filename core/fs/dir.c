#include <debug.h>
#include <device/ide.h>
#include <fs/dir.h>
#include <fs/inode.h>
#include <string.h>
#include <sys/memory.h>

struct dir root_dir;  // global variable for root directory

/**
 * open root directory
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
}

/**
 * Search dir_entry (file or directory or ...) by name at given directory
 *
 * @param part a mounted partition
 * @param name entry name
 * @param d in what directory
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
 * open directory
 *
 * @param param write here param Comments write here
 * @return return Comments write here
 *****************************************************************************/
struct dir *dir_open(struct partition *part, uint_32 inode_nr)
{
    struct dir *d = (struct dir *) sys_malloc(sizeof(struct dir));
    d->inode = inode_open(part, inode_nr);
    d->dir_pos = 0;  // ????
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
void new_dir_entry(char *name, uint_32 inode_nr, struct dir_entry *entry)
{
    uint_32 nlen = strlen(name);
    ASSERT(nlen < MAX_FILE_NAME_LEN);
    memcpy(entry->filename, name, nlen);
    entry->i_no = inode_nr;
}
