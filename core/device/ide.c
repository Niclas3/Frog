#include <device/ide.h>
#include <debug.h>
#include <math.h>
#include <protect.h>
#include <stdbool.h>
#include <sys/sched.h>
#include <string.h>

#include <asm/bootpack.h>
#include <io.h>

// ide registers numbers
//
#define reg_data(channel) (channel->port_base + 0)
#define reg_error(channel) (channel->port_base + 1)
#define reg_sect_cnt(channel) (channel->port_base + 2)
#define reg_lba_l(channel) (channel->port_base + 3)
#define reg_lba_m(channel) (channel->port_base + 4)
#define reg_lba_h(channel) (channel->port_base + 5)
#define reg_dev(channel) (channel->port_base + 6)
#define reg_status(channel) (channel->port_base + 7)
#define reg_cmd(channel) (reg_status(channel))
#define reg_alt_status(channel) (channel->port_base + 0x206)
#define reg_ctl(channel) reg_alt_status(channel)

// Bits in register alt status
#define BIT_STAT_BSY 0x80   // disk busy
#define BIT_STAT_DRDY 0x40  // driver ready
#define BIT_STAT_DRQ 0x8    // data trans ready

// Bits in register device
#define BIT_DEV_MBS 0xa0  // bit7 and bit5 is 1
#define BIT_DEV_LBA 0x40
#define BIT_DEV_DEV 0x10

// ide command
#define CMD_IDENTIFY 0xec      // identify
#define CMD_READ_SECTOR 0x20   // read sector
#define CMD_WRITE_SECTOR 0x30  // write sector

#define max_lba ((80 * 1024 * 1024 / 512) - 1)


uint_8 channel_cnt;
struct ide_channel channels[2];  // 2 different channels

int_32 ext_lba_base = 0;
uint_8 p_no = 0;
uint_8 l_no = 0;                 // master and logic number
struct list_head partition_list; // partition list


/*
 * partition_table_entry
 *-----------------------------------------------------------------------------
 *| offset |  data width |               description                           |
 *------------------------------------------------------------------------------
 *|   0    |     1       | active partition mark. value 0x80 or 0x0.if 0x80 presents os-loadable.
 *------------------------------------------------------------------------------
 *|   1    |     1       | partition start header number
 *------------------------------------------------------------------------------
 *|   2    |     1       | partition start sector number
 *------------------------------------------------------------------------------
 *|   3    |     1       | partition start cylinder number
 *------------------------------------------------------------------------------
 *|   4    |     1       | file system type id, 0x0 represents unknow file system, 1 for FAT32
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
struct partition_table_entry{
    uint_8 bootable;        // 0x80 is bootable
    uint_8 start_head;
    uint_8 start_sec;        // sector
    uint_8 start_chs;        // cylinder
    uint_8 fs_type;
    uint_8 end_head;
    uint_8 end_sec;        // sector
    uint_8 end_chs;        // cylinder

    uint_32 start_lba;     // this sector lba address;
    uint_32 sec_cnt;       // all sector counts
}__attribute__((packed));

void ide_init(void)
{

    uint_8 hd_cnt = *((uint_8 *) (0x475));  // get hd numbers
    ASSERT(hd_cnt > 0);
    // 1 channel -> 2 hd
    // channel_cnt * 2 = hd_cnt
    channel_cnt = DIV_ROUND_UP(hd_cnt, 2);
    struct ide_channel *channel;
    uint_8 channel_no = 0;
    while (channel_no < channel_cnt) {
        channel = &channels[channel_no];
        switch (channel_no) {
        case 0:
            channel->port_base = 0x1f0;
            channel->irq_no = 0x20 + 14; //channel primary 0x2e
            break;
        case 1:
            channel->port_base = 0x170;
            channel->irq_no = 0x20 + 15; //channel secondary 0x2f
            break;
        default:
            ASSERT(channel_no == 0 || channel_no == 1);
            break;
        }
        channel->expecting_intr = false;
        lock_init(&channel->lock);

        semaphore_init(&channel->disk_done, 0);
        /* register_ring0_INT(channel->irq_no, intr_hd_handler); */
        channel_no++;
    }
}

static void swap_pairs_bytes(const char* dst, char* buf, uint_32 len){
    uint_8 idx;
    for(idx=0; idx< len; idx+=2){
        buf[idx+1] = *dst++;
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
    _io_out8(reg_dev(hd->my_channel), reg_device);
}

static void select_sector(struct disk *hd, uint_32 lba, uint_8 sec_cnt)
{
    ASSERT(lba <= max_lba);
    struct ide_channel *channel = hd->my_channel;
    // if sec_cnt == 0 then write in 256 sectors
    _io_out8(reg_sect_cnt(channel), sec_cnt);

    // lower lba
    _io_out8(reg_lba_l(channel), lba);
    // lba address 8~15bits
    _io_out8(reg_lba_m(channel), lba >> 8);
    // lba address 16~23bits
    _io_out8(reg_lba_h(channel), lba >> 16);

    // lba 24~27bits write into device 0~3bits
    _io_out8(reg_dev(channel), BIT_DEV_MBS | BIT_DEV_LBA |
                                   (hd->dev_no == 1 ? BIT_DEV_DEV : 0) |
                                   lba >> 24);
}

static void cmd_out(struct ide_channel *channel, uint_8 cmd)
{
    channel->expecting_intr = true;
    _io_out8(reg_cmd(channel), cmd);
}

static void read_from_sector(struct disk *hd, void *buf, uint_8 sec_cnt)
{
    uint_32 size_in_byte;
    if (sec_cnt == 0) {
        size_in_byte = 256 * 512;
    } else {
        size_in_byte = sec_cnt * 512;
    }
    insw(reg_data(hd->my_channel), buf, size_in_byte / 2);
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
        if (!(inb(reg_status(channel)) & BIT_STAT_BSY)) {
            return (inb(reg_status(channel)) & BIT_STAT_DRQ);
        } else {
            mtime_sleep(10);
        }
    }
    return false;
}

// read sec_cnt sector from ide to buf
void ide_read(struct disk *hd, uint_32 lba, void *buf, uint_32 sec_cnt)
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
        //TODO: need sprintf()!
            PAINC(error);
        }
        // 5. read data from buffer
        read_from_sector(hd, (void *) ((uint_32) buf + secs_done * 512),
                         secs_op);
        secs_done += secs_op;
    }
    lock_release(&hd->my_channel->lock);
}

void ide_write(struct disk *hd, uint_32 lba, void *buf, uint_32 sec_cnt)
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
        //TODO: need sprintf()!
            PAINC(error);
        }
        // 5. read data from buffer
        write_to_sector(hd, (void *) ((uint_32) buf + secs_done * 512),
                        secs_op);
        semaphore_down(&hd->my_channel->disk_done);
        secs_done += secs_op;
    }
    lock_release(&hd->my_channel->lock);
}

//get hd infomation
void identify_disk(struct disk* hd){
    char id_info[512];
    select_disk(hd);
    cmd_out(hd->my_channel, CMD_IDENTIFY);

    semaphore_down(&hd->my_channel->disk_done);

    if(!busy_wait(hd)){
        char error[64];
        //TODO: need sprintf()!
        PAINC("Error identify_disk");
    }
    read_from_sector(hd, id_info, 1);
    char buf[64];
    uint_8 sn_start = 10 * 2;
    uint_8 sn_len   = 20;
    uint_8 md_start = 27 * 2;
    uint_8 md_len   = 40;
    swap_pairs_bytes(&id_info[sn_start], buf, sn_len);
    memset(buf, 0, sizeof(buf));
    uint_32 sectors = *(uint_32*)&id_info[60*2];
}

// interrupt of disk

void intr_hd_handler(uint_8 irq_no)
{
    ASSERT(irq_no == 0x2e || irq_no == 0x2f);
    uint_8 ch_no = irq_no - 0x2e;
    struct ide_channel *channel = &channels[ch_no];
    ASSERT(channel->irq_no == irq_no);
    if (channel->expecting_intr) {
        channel->expecting_intr = false;
        semaphore_up(&channel->disk_done);
        // ack disk
        inb(reg_status(channel));
    }
}
