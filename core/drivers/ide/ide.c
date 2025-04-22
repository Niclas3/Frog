#include <debug.h>
#include <device/ide.h>
#include <frog/interrupt.h>
#include <frog/irqflags.h>
#include <frog/sched.h>
#include <frog/string.h>
#include <frog/types.h>
#include <math.h>
#include <stdio.h>

#include <asm/io.h>
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
#define ATA_STATR_DRQ 0x08 // data trans ready. set when the drive to transfer, or is ready to accept PIO
#define ATA_STATR_CORR 0x04  // Corrected data. always set to zero
#define ATA_STATR_IDX 0x02   // index. Always set to zero.
// indicates an error occurred. send a new command  to clear it(software reset)
#define ATA_STATR_ERR 0x01

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


// First partition entry
#define FIRST_P_ENTRY 0x1be

uint_8 channel_cnt;
struct ide_channel channels[2];  // 2 different channels

struct list_head partition_list;  // partition list

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
                                   (hd->dev_no == 1 ? BIT_DEV_DEV : 0) |
                                   lba >> 24);
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
        outb(reg_dev(channel), BIT_DEV_MBS | BIT_DEV_LBA |
                                   (hd->dev_no == 1 ? BIT_DEV_DEV : 0) | 0);
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
        // we use word so `size_in_byte / 2`
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
// 1 sector is 512 bytes
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
        char *r_cursor = (char *) buf;
        while (cur_cnt) {
                ide_read_sector(hd, cur_lba, r_cursor);
                r_cursor += 512;
                cur_lba++;
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
                cmd_out(hd->my_channel,
                        CMD_READ_SECTORS_EXT);  // ready to read data
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
        // 3. execute by reg_cmd / send cmd to ide disk
        cmd_out(hd->my_channel, CMD_WRITE_SECTOR);  // ready to write data

        // 4. check disk status is readable or not
        // check disk status by polling / why don't use interrupt
        if (!busy_wait(hd)) {
                char error[64] = "read sector failed";
                // TODO: need sprintf()!
                PANIC(error);
        }
        // 5. read data from buffer
        write_to_sector(hd, (void *) ((uint_32) buf), 1);
        semaphore_down(&hd->my_channel->disk_done);
        lock_release(&hd->my_channel->lock);
}

void ide_write(struct disk *hd, uint_32 lba, void *buf, uint_32 sec_cnt)
{
        ASSERT(lba <= max_lba);
        ASSERT(sec_cnt > 0);
        uint_32 cur_lba = lba;
        uint_32 cur_cnt = sec_cnt;
        char *w_cursor = (char *) buf;
        while (cur_cnt) {
                ide_write_sector(hd, cur_lba, w_cursor);
                w_cursor += 512;
                cur_lba++;
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
                cmd_out(hd->my_channel,
                        CMD_WRITE_SECTOR);  // ready to write data

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
                cmd_out(hd->my_channel,
                        CMD_WRITE_SECTORS_EXT);  // ready to write data

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

bool partitions_info(struct list_head *p_list, int arg)
{
        struct partition *part =
            container_of(p_list, struct partition, part_tag);
        printf("partition: %s capacity:%x start:%x\n", part->name,
               part->sec_cnt, part->start_lba);
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
                INIT_LIST_HEAD(&partition_list);
                register_r0_intr_handler(channel->irq_no, intr_hd_handler);

                while (dev_no < 2) {
                        struct disk *hd = &channel->devices[dev_no];
                        hd->dev_no = dev_no;
                        hd->my_channel = channel;
                        sprintf(hd->name, "sd%c", 'a' + channel_no * 2 + dev_no);
                        if (dev_no != 0) {
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

        ack(irq_no);
        irq_exit();
}
