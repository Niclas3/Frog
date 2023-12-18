#include <debug.h>
#include <device/ide.h>
#include <math.h>
#include <protect.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/int.h>
#include <sys/sched.h>

#include <io.h>
// ide registers numbers

#define reg_data(channel) (channel->port_base + 0)
#define reg_error(channel) (channel->port_base + 1)
#define reg_sect_cnt(channel) (channel->port_base + 2)
#define reg_lba_l(channel) (channel->port_base + 3)
#define reg_lba_m(channel) (channel->port_base + 4)
#define reg_lba_h(channel) (channel->port_base + 5)
#define reg_dev(channel) (channel->port_base + 6)
#define reg_status(channel) (channel->port_base + 7)
#define reg_cmd(channel) (reg_status(channel))
#define reg_ctl_base(channel) (channel->port_base + 0x206)
// control base
// control port base 0x3f6  dev0
//                   0x3f7  dev1
#define reg_alt_status(channel) (channel->port_base + 0x206)
#define reg_ctl(channel) reg_ctl_base(channel)

// ATA status in register alt status or reg_status
#define ATA_STATR_BSY 0x80      // disk busy
#define ATA_STATR_DRDY 0x40     // driver ready
#define ATA_STATR_DF 0x20       // drive fault error
#define ATA_STATR_DSC_SRV 0x10  // SRV!! overlapped mode service request
#define ATA_STATR_DRQ \
    0x08  // data trans ready. set when the drive to transfer, or is ready to
          // accept PIO data.
#define ATA_STATR_CORR 0x04  // Corrected data. always set to zero
#define ATA_STATR_IDX 0x02   // index. Always set to zero.
#define ATA_STATR_ERR \
    0x01  // indicates an error occurred. send a new command  to clear
          // it(software reset)

// Device control register
#define ATA_DEVCR_nIEN 0x2

// Bits in register device
#define BIT_DEV_MBS 0xa0  // bit7 and bit5 is 1
#define BIT_DEV_LBA 0x40
#define BIT_DEV_DEV 0x10

// ide command
#define CMD_IDENTIFY 0xec           // identify
#define CMD_READ_SECTOR 0x20        // read sector
#define CMD_READ_SECTORS_EXT 0x24   // read sectors ext
#define CMD_READ_SECTORS_DMA 0xC8   // read sectors ext
#define CMD_WRITE_SECTOR 0x30       // write sector
#define CMD_WRITE_SECTORS_EXT 0x34  // write sectors ext

// MAX LBA supports
//                80M * 1024 * 1024 = 0x5000000
//                0x5000000 / 512 = 163840 (sector_number)
//                163840-1 sector number starts from 0
#define max_lba ((80 * 1024 * 1024 / 512) - 1)

#define IS_NULL_ENTRY(entry) \
    ((entry).end_head == 0) && ((entry).end_sec == 0) && ((entry).end_chs == 0)

// First partition entry
#define FIRST_P_ENTRY 0x1be

// System file type ID
enum dpt_fs_t {
    DPT_FILE_SYSTEM_TYPE_UNKNOW = 0x0,
    DPT_FILE_SYSTEM_TYPE_EXT = 0x5,
    DPT_FILE_SYSTEM_TYPE_LINUX = 0x83
};


uint_8 channel_cnt;
struct ide_channel channels[2];  // 2 different channels

struct list_head partition_list;  // partition list


/*
 * partition_table_entry
 *-----------------------------------------------------------------------------
 *| offset |  data width |               description                           |
 *------------------------------------------------------------------------------
 *|   0    |     1       | active partition mark. value 0x80 or 0x0.if 0x80
 *presents os-loadable.
 *------------------------------------------------------------------------------
 *|   1    |     1       | partition start header number
 *------------------------------------------------------------------------------
 *|   2    |     1       | partition start sector number
 *------------------------------------------------------------------------------
 *|   3    |     1       | partition start cylinder number
 *------------------------------------------------------------------------------
 *|   4    |     1       | file system type id, 0x0 represents unknow file
 *system, 1 for FAT32
 *------------------------------------------------------------------------------
 *|   5    |     1       | partition end head number
 *------------------------------------------------------------------------------
 *|   6    |     1       | partition end sector number
 *------------------------------------------------------------------------------
 *|   7    |     1       | partition end cylinder number
 *------------------------------------------------------------------------------
 *|   8    |     4       | start of offset partition
 *------------------------------------------------------------------------------
 *|   12   |     4       | capacity of sectors
 *-----------------------------------------------------------------------------
 * */
struct partition_table_entry {
    uint_8 bootable;  // 0x80 is bootable
    uint_8 start_head;
    uint_8 start_sec;  // sector
    uint_8 start_chs;  // cylinder
    uint_8 fs_type;
    uint_8 end_head;
    uint_8 end_sec;      // sector
    uint_8 end_chs;      // cylinder
    uint_32 offset_lba;  // this offset in sector lba so you need to times 512
                         // to covert to xxx bytes
    uint_32 sec_cnt;     // all sector counts
} __attribute__((packed));

struct boot_sector {
    uint_8 code_area[446];                   // Bootstrap code area
    struct partition_table_entry tables[4];  // primary partition table
    uint_16 signature;                       // 0xaa55
} __attribute__((packed));


static void swap_pairs_bytes(const char *dst, char *buf, uint_32 len)
{
    uint_8 idx;
    for (idx = 0; idx < len; idx += 2) {
        buf[idx + 1] = *dst++;
        buf[idx] = *dst++;
    }
    buf[idx] = '\0';
}


static void select_disk(struct disk *hd)
{
    uint_8 reg_device = BIT_DEV_MBS | BIT_DEV_LBA;
    // if is slave disk set dev = 1
    if (hd->dev_no == 1) {
        reg_device |= BIT_DEV_DEV;
    }
    outb(reg_dev(hd->my_channel), reg_device);
}

static void select_sector(struct disk *hd, uint_32 lba, uint_8 sec_cnt)
{
    ASSERT(lba <= max_lba);
    struct ide_channel *channel = hd->my_channel;
    // if sec_cnt == 0 then write in 256 sectors
    outb(reg_sect_cnt(channel), sec_cnt);

    // Set LBA28 address
    // lower lba
    outb(reg_lba_l(channel), lba);
    // lba address 8~15bits
    outb(reg_lba_m(channel), lba >> 8);
    // lba address 16~23bits
    outb(reg_lba_h(channel), lba >> 16);

    // lba 24~27bits write into device 0 ~ 3bits
    outb(reg_dev(channel), BIT_DEV_MBS | BIT_DEV_LBA |
                               (hd->dev_no == 1 ? BIT_DEV_DEV : 0) | lba >> 24);
}

static void select_sector_ext(struct disk *hd, uint_32 lba, uint_8 sec_cnt)
{
    ASSERT(lba <= max_lba);
    struct ide_channel *channel = hd->my_channel;
    // if sec_cnt == 0 then write in 256 sectors
    // Write high bits first
    outb(reg_sect_cnt(channel), (sec_cnt >> 8));
    // lower lba
    outb(reg_lba_l(channel), (lba >> 24));  // lba 24~31 bits
    // lba address 8~15bits                // not support over 32 bits
    outb(reg_lba_m(channel), 0);
    // lba address 16~23bits
    outb(reg_lba_h(channel), 0);  // not support over 32 bits

    // write lower bits
    outb(reg_sect_cnt(channel), sec_cnt);
    // lower lba
    outb(reg_lba_l(channel), lba);  // lba 24~31 bits
    // lba address 8~15bits                // not support over 32 bits
    outb(reg_lba_m(channel), (lba >> 8));
    // lba address 16~23bits
    outb(reg_lba_h(channel), (lba >> 16));  // not support over 32 bits

    // lba 24~27bits write into device 0 ~ 3bits
    outb(reg_dev(channel),
         BIT_DEV_MBS | BIT_DEV_LBA | (hd->dev_no == 1 ? BIT_DEV_DEV : 0) | 0);
}



static void cmd_out(struct ide_channel *channel, uint_8 cmd)
{
    channel->expecting_intr = true;
    outb(reg_cmd(channel), cmd);
}

static void read_from_sector(struct disk *hd, void *buf, uint_8 sec_cnt)
{
    uint_32 size_in_byte;
    if (sec_cnt == 0) {
        size_in_byte = 256 * 512;
    } else {
        size_in_byte = sec_cnt * 512;
    }
    // Every sector has 512 byte, but we can read a word aka 2 bytes
    // so we only need read size_in_byte/2 times
    insw(reg_data(hd->my_channel), buf, size_in_byte / 2);

    // 400ns delay - Read alternate status register
    for (uint_8 k = 0; k < 4; k++)
        inb(0x3F6);
}

static void write_to_sector(struct disk *hd, void *buf, uint_8 sec_cnt)
{
    uint_32 size_in_byte;
    if (sec_cnt == 0) {
        size_in_byte = 256 * 512;
    } else {
        size_in_byte = sec_cnt * 512;
    }
    outsw(reg_data(hd->my_channel), buf, size_in_byte / 2);
}

// waiting 30s
static bool busy_wait(struct disk *hd)
{
    struct ide_channel *channel = hd->my_channel;
    uint_16 time_limit = 30 * 1000;
    while (time_limit -= 10 >= 0) {
        if (!(inb(reg_status(channel)) & ATA_STATR_BSY)) {
            return (inb(reg_status(channel)) & ATA_STATR_DRQ);
        } else {
            mtime_sleep(10);
        }
    }
    return false;
}

// read 1 sector from ide to buf
static void ide_read_sector(struct disk *hd, uint_32 lba, void *buf)
{
    lock_fetch(&hd->my_channel->lock);
    // 1. Select disk
    select_disk(hd);
    // 2. wirte lba and start sector number
    select_sector(hd, lba, 1);
    // 3. execute by reg_cmd
    cmd_out(hd->my_channel, CMD_READ_SECTOR);  // ready to read data
    // lock self waiting intr unblock this thread
    semaphore_down(&hd->my_channel->disk_done);
    // 4. check disk status is readable or not
    if (!busy_wait(hd)) {
        char error[64] = "read sector failed";
        // TODO: need sprintf()!
        /* PANIC(error); */
        lock_release(&hd->my_channel->lock);
        return;
    }
    // 5. read data from buffer
    read_from_sector(hd, (void *) buf, 1);
    lock_release(&hd->my_channel->lock);
    return;
}

void ide_read(struct disk *hd, uint_32 lba, void *buf, uint_32 sec_cnt)
{
    ASSERT(lba <= max_lba);
    ASSERT(sec_cnt > 0);
    uint_32 cur_lba = lba;
    uint_32 cur_cnt = sec_cnt;
    while (cur_cnt) {
        ide_read_sector(hd, cur_lba, buf);
        lba++;
        cur_cnt--;
    }
    return;
}

// FIXME:
// read sec_cnt sector from ide to buf
// this function only support bochs not qemu
// I don't know why
void ide_read_v2(struct disk *hd, uint_32 lba, void *buf, uint_32 sec_cnt)
{
    ASSERT(lba <= max_lba);
    ASSERT(sec_cnt > 0);
    lock_fetch(&hd->my_channel->lock);
    // 1. Select disk
    select_disk(hd);
    uint_32 secs_op;        // operation of sector one run
    uint_32 secs_done = 0;  // sector has done
    while (secs_done < sec_cnt) {
        if ((secs_done + 256) <= sec_cnt) {
            secs_op = 256;
        } else {
            secs_op = sec_cnt - secs_done;
        }
        // 2. wirte lba and start sector number
        select_sector(hd, lba + secs_done, secs_op);
        // 3. execute by reg_cmd
        cmd_out(hd->my_channel, CMD_READ_SECTOR);  // ready to read data
        // lock self waiting intr unblock this thread
        semaphore_down(&hd->my_channel->disk_done);
        // 4. check disk status is readable or not
        if (!busy_wait(hd)) {
            char error[64] = "read sector failed";
            // TODO: need sprintf()!
            /* PANIC(error); */
            lock_release(&hd->my_channel->lock);
            return;
        }
        // 5. read data from buffer
        read_from_sector(hd, (void *) ((uint_32) buf + secs_done * 512),
                         secs_op);
        secs_done += secs_op;
    }
    lock_release(&hd->my_channel->lock);
    return;
}

// FIXME:
// read sec_cnt sector from ide to buf
// this function only support bochs not qemu
// I don't know why
// read sec_cnt sector from ide to buf
void ide_read_ext(struct disk *hd, uint_32 lba, void *buf, uint_32 sec_cnt)
{
    ASSERT(lba <= max_lba);
    ASSERT(sec_cnt > 0);
    lock_fetch(&hd->my_channel->lock);
    // 1. Select disk
    select_disk(hd);
    uint_32 secs_op;        // operation of sector one run
    uint_32 secs_done = 0;  // sector has done
    while (secs_done < sec_cnt) {
        if ((secs_done + 256) <= sec_cnt) {
            secs_op = 256;
        } else {
            secs_op = sec_cnt - secs_done;
        }
        // 2. wirte lba and start sector number
        select_sector_ext(hd, lba + secs_done, secs_op);
        // 3. execute by reg_cmd
        cmd_out(hd->my_channel, CMD_READ_SECTORS_EXT);  // ready to read data
        // lock self waiting intr unblock this thread
        semaphore_down(&hd->my_channel->disk_done);

        // 4. check disk status is readable or not
        if (!busy_wait(hd)) {
            char error[64] = "read sector failed";
            // TODO: need sprintf()!
            /* PANIC(error); */
            lock_release(&hd->my_channel->lock);
            return;
        }
        // 5. read data from buffer
        read_from_sector(hd, (void *) ((uint_32) buf + secs_done * 512),
                         secs_op);
        secs_done += secs_op;
    }
    lock_release(&hd->my_channel->lock);
    return;
}

// Read in DMA
// TODO: impl DMA
void ide_read_DMA(struct disk *hd, uint_32 lba, void *buf, uint_32 sec_cnt)
{
    ASSERT(lba <= max_lba);
    ASSERT(sec_cnt > 0);
    return;
}

// write sec_cnt sector from ide to buf
static void ide_write_sector(struct disk *hd, uint_32 lba, void *buf)
{
    lock_fetch(&hd->my_channel->lock);
    // 1. Select disk
    select_disk(hd);
    // 2. wirte lba and start sector number
    select_sector(hd, lba, 1);
    // 3. execute by reg_cmd
    cmd_out(hd->my_channel, CMD_WRITE_SECTOR);  // ready to write data

    // 4. check disk status is readable or not
    if (!busy_wait(hd)) {
        char error[64] = "read sector failed";
        // TODO: need sprintf()!
        PANIC(error);
    }
    // 5. read data from buffer
    write_to_sector(hd, (void *) ((uint_32) buf ), 1);
    semaphore_down(&hd->my_channel->disk_done);
    lock_release(&hd->my_channel->lock);
}

void ide_write(struct disk *hd, uint_32 lba, void *buf, uint_32 sec_cnt)
{
    ASSERT(lba <= max_lba);
    ASSERT(sec_cnt > 0);
    uint_32 cur_lba = lba;
    uint_32 cur_cnt = sec_cnt;
    while (cur_cnt) {
        ide_write_sector(hd, cur_lba, buf);
        lba++;
        cur_cnt--;
    }
    return;

}

// FIXME:
// read sec_cnt sector from ide to buf
// this function only support bochs not qemu
// I don't know why
// write sec_cnt sector from ide to buf
void ide_write_v2(struct disk *hd, uint_32 lba, void *buf, uint_32 sec_cnt)
{
    ASSERT(lba <= max_lba);
    ASSERT(sec_cnt > 0);
    lock_fetch(&hd->my_channel->lock);

    // 1. Select disk
    select_disk(hd);
    uint_32 secs_op;        // operation of sector one run
    uint_32 secs_done = 0;  // sector has done
    while (secs_done < sec_cnt) {
        if ((secs_done + 256) <= sec_cnt) {
            secs_op = 256;
        } else {
            secs_op = sec_cnt - secs_done;
        }
        // 2. wirte lba and start sector number
        select_sector(hd, lba + secs_done, secs_op);
        // 3. execute by reg_cmd
        cmd_out(hd->my_channel, CMD_WRITE_SECTOR);  // ready to write data

        // 4. check disk status is readable or not
        if (!busy_wait(hd)) {
            char error[64] = "read sector failed";
            // TODO: need sprintf()!
            PANIC(error);
        }
        // 5. read data from buffer
        write_to_sector(hd, (void *) ((uint_32) buf + secs_done * 512),
                        secs_op);
        semaphore_down(&hd->my_channel->disk_done);
        secs_done += secs_op;
    }
    lock_release(&hd->my_channel->lock);
}

// FIXME:
// read sec_cnt sector from ide to buf
// this function only support bochs not qemu
// I don't know why
// read sec_cnt sector from ide to buf
void ide_write_ext(struct disk *hd, uint_32 lba, void *buf, uint_32 sec_cnt)
{
    ASSERT(lba <= max_lba);
    ASSERT(sec_cnt > 0);
    lock_fetch(&hd->my_channel->lock);

    // 1. Select disk
    select_disk(hd);
    uint_32 secs_op;        // operation of sector one run
    uint_32 secs_done = 0;  // sector has done
    while (secs_done < sec_cnt) {
        if ((secs_done + 256) <= sec_cnt) {
            secs_op = 256;
        } else {
            secs_op = sec_cnt - secs_done;
        }
        // 2. wirte lba and start sector number
        select_sector_ext(hd, lba + secs_done, secs_op);
        // 3. execute by reg_cmd
        cmd_out(hd->my_channel, CMD_WRITE_SECTORS_EXT);  // ready to write data

        // 4. check disk status is readable or not
        if (!busy_wait(hd)) {
            char error[64] = "read sector failed";
            // TODO: need sprintf()!
            PANIC(error);
        }
        // 5. read data from buffer
        write_to_sector(hd, (void *) ((uint_32) buf + secs_done * 512),
                        secs_op);
        semaphore_down(&hd->my_channel->disk_done);
        secs_done += secs_op;
    }
    lock_release(&hd->my_channel->lock);
}
// get hd infomation
void identify_disk(struct disk *hd)
{
    uint_16 id_info[512];
    select_disk(hd);
    cmd_out(hd->my_channel, CMD_IDENTIFY);

    semaphore_down(&hd->my_channel->disk_done);

    if (!busy_wait(hd)) {
        char error[64];
        // TODO: need sprintf()!
        PANIC("Error identify_disk");
    }
    read_from_sector(hd, id_info, 1);
    char buf[64];
    uint_8 sn_start = 10 * 2;
    uint_8 sn_len = 20;
    uint_8 md_start = 27 * 2;
    uint_8 md_len = 40;
    swap_pairs_bytes(&id_info[sn_start], buf, sn_len);
    memset(buf, 0, sizeof(buf));
    uint_32 sectors = *(uint_32 *) &id_info[60 * 2];
}

static void copy_entry(struct partition_table_entry *des,
                       struct partition_table_entry *src)
{
    des->start_head = src->start_head;
    des->start_sec = src->start_sec;
    des->start_chs = src->start_chs;
    des->end_head = src->end_head;
    des->end_sec = src->end_sec;
    des->end_chs = src->end_chs;
    des->fs_type = src->fs_type;
    des->bootable = src->bootable;
    des->sec_cnt = src->sec_cnt;
    des->offset_lba = src->offset_lba;
}
static void copy_4_entries(struct partition_table_entry *des,
                           struct partition_table_entry *src)
{
    for (int i = 0; i < 4; i++) {
        copy_entry(&des[i], &src[i]);
    }
}
static void move_4_entries(struct partition_table_entry *des,
                           struct partition_table_entry *src)
{
    copy_4_entries(des, src);
    memset(src, 0, sizeof(struct partition_table_entry) * 4);
}
/**
 * get disk partition table
 *
 * @param hd disk pointer for read it
 * @param offset_lba offset in lba
 * @param entries entries for return
 * @return 0 success
 *****************************************************************************/
static uint_32 get_dpt(struct disk *hd,
                       uint_32 offset_lba,
                       struct partition_table_entry *entries)
{
    struct boot_sector sector;
    /*
     * Read the first sector of given hard disk, it contains MBR(master boot
     * record)layout is here[https://en.wikipedia.org/wiki/Master_boot_record]
     * The first partition entry at 0x01be, size 16 bytes. There are 4 same
     * partition entries
     * * */
    ide_read(hd, offset_lba, &sector, 1);
    copy_4_entries(entries, (struct partition_table_entry *) &sector.tables);
    ASSERT(sector.signature == 0xaa55);
    return 0;
}

/**
 * Calculate next logic partition table from given entries
 *
 * @param entries from extension partition (only 2)
 * @param next return next partition table to this pointer
 * @return return 0 represent the last entry
 *****************************************************************************/

static uint_32 g_ext_base_offset = 0;  // in lba type
static uint_32 next_dpt(struct disk *hd,
                        struct partition_table_entry *entries,
                        struct partition_table_entry *next)
{
    for (uint_32 i = 0; i < 4; i++) {
        struct partition_table_entry entry = entries[i];
        if (IS_NULL_ENTRY(entry)) {
            continue;
        }
        // 1. test (entries->fs_type) if it is 0x5 which means next dpt
        if (entry.fs_type == DPT_FILE_SYSTEM_TYPE_EXT) {
            get_dpt(hd, g_ext_base_offset + entry.offset_lba, next);
            return 1;
        } else {
            // ignore other type disk partition table
        }
    }
    return 0;
}

/**
 * sacn all disk partition table to
 *                                   hd->prim_partition
 *                                   hd->logic_partition
 *
 * @param hd disk
 * @param ext_lba first is 0
 * @return
 *****************************************************************************/
void scan_partitions(struct disk *hd)
{
    // TODO: maybe should use mm/sys_malloc()
    struct partition_table_entry entries[4] = {0};
    struct partition_table_entry next[4] = {0};
    struct partition_table_entry *p_entries = entries;
    struct partition_table_entry *p_next = next;

    uint_8 lp_idx = 0;  // logic_partition idex max is 8
    get_dpt(hd, 0, p_entries);
    // 1. hd->prim_partition
    for (int i = 0; i < 4; i++) {
        hd->prim_partition[i].sec_cnt = entries[i].sec_cnt;
        hd->prim_partition[i].start_lba = entries[i].offset_lba;
        hd->prim_partition[i].my_disk = hd;
        // main partition number start at no.1
        sprintf(hd->prim_partition[i].name, "%s%d", hd->name, i + 1);
        list_add_tail(&hd->prim_partition[i].part_tag, &partition_list);
    }

    // set ext partition offset
    uint_32 ext_base = entries[3].offset_lba;
    uint_32 pre_offset = 0;  // previous offset
    // 2. hd->logic_partition
    while (next_dpt(hd, p_entries, p_next)) {
        if (g_ext_base_offset == 0) {
            g_ext_base_offset = ext_base;
        }
        hd->logic_partition[lp_idx].my_disk = hd;
        hd->logic_partition[lp_idx].sec_cnt = next[0].sec_cnt;
        // logic partition index starts at no.5
        sprintf(hd->logic_partition[lp_idx].name, "%s%d", hd->name, lp_idx + 5);
        // g_ext_partition_offset is set at next_dpt() when the get first logic
        // partition
        hd->logic_partition[lp_idx].start_lba =
            next[0].offset_lba + g_ext_base_offset + pre_offset;
        list_add_tail(&hd->logic_partition[lp_idx].part_tag, &partition_list);

        // save offset to previous offset
        pre_offset = next[1].offset_lba;
        lp_idx++;
        copy_4_entries(p_entries, p_next);
    }
}
bool partitions_info(struct list_head *p_list, int arg)
{
    struct partition *part = container_of(p_list, struct partition, part_tag);
    printf("partition: %s capacity:%x start:%x\n", part->name, part->sec_cnt,
           part->start_lba);
    return false;
}

void ide_init(void)
{
    uint_8 hd_cnt = *((uint_8 *) (0x475));  // get hd numbers
    ASSERT(hd_cnt > 0);
    // 1 channel -> 2 hd
    // channel_cnt * 2 = hd_cnt
    channel_cnt = DIV_ROUND_UP(hd_cnt, 2);
    struct ide_channel *channel;
    uint_8 channel_no = 0;
    uint_8 dev_no = 0;
    while (channel_no < channel_cnt) {
        channel = &channels[channel_no];
        switch (channel_no) {
        case 0:
            channel->port_base = 0x1f0;
            channel->irq_no = 0x20 + 14;  // channel primary 0x2e
            break;
        case 1:
            channel->port_base = 0x170;
            channel->irq_no = 0x20 + 15;  // channel secondary 0x2f
            break;
        default:
            ASSERT(channel_no == 0 || channel_no == 1);
            break;
        }
        channel->expecting_intr = false;
        lock_init(&channel->lock);

        semaphore_init(&channel->disk_done, 0);
        init_list_head(&partition_list);
        register_r0_intr_handler(channel->irq_no, intr_hd_handler);

        while (dev_no < 2) {
            if (dev_no != 0) {
                struct disk *hd = &channel->devices[dev_no];
                hd->dev_no = dev_no;
                hd->my_channel = channel;
                sprintf(hd->name, "sd%c", 'a' + channel_no * 2 + dev_no);
                scan_partitions(hd);
            } else {
                dev_no++;
                continue;
            }
            dev_no++;
        }
        channel_no++;
    }
}

// interrupt of disk
void intr_hd_handler(uint_8 irq_no)
{
    ASSERT(irq_no == 0x2e || irq_no == 0x2f);
    uint_8 ch_no = irq_no - 0x2e;
    struct ide_channel *channel = &channels[ch_no];
    uint_8 status = inb(reg_status(channel));
    ASSERT(channel->irq_no == irq_no);
    if (channel->expecting_intr) {
        channel->expecting_intr = false;
        semaphore_up(&channel->disk_done);
        // ack disk to clear interrupt flag
        /* inb(reg_status(channel)); */
    }
}
