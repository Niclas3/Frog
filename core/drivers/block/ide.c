#include <frog/interrupt.h>
#include <frog/irqflags.h>
#include <frog/math.h>
#include <frog/sched.h>
#include <frog/string.h>
#include <frog/types.h>
#include <kernel/assert.h>
#include <kernel/panic.h>
#include <frog/semaphore.h>
#include <stdio.h>

#include <asm/io.h>

#include <kernel/debug.h>
#include <kernel/device.h>
#include <kernel/driver.h>

#include <frog/block.h>
extern struct bus_type isa_bus;

/* -----------------------------------------------------------------------------
 * ide device I/O ports and register
 * -----------------------------------------------------------------------------
 *|           |       I/O ports       |                   |                    |
 *|   Group   |-----------------------|       Read        |        write       |
 *|           |  primary  | secondary |                   |                    |
 *|-----------|-----------|-----------|-------------------|--------------------+
 *|           |   1F0h    |  170h     | Data              | Data               |
 *|           |-----------|-----------|-------------------|--------------------+
  |           |   1F1h    |  171h     | Error             | Features           |
 *|           |-----------|-----------|-------------------|--------------------+
  |           |   1F2h    |  172h     | Sector Count      | Sector Count       |
 *|           |-----------|-----------|-------------------|--------------------+
 *| Command   |   1F3h    |  173h     | LBA Low           | LBA Low            |
 *| Block     |-----------|-----------|-------------------|--------------------+
  | Registers |   1F4h    |  174h     | LBA Middle        | LBA Middle         |
 *|           |-----------|-----------|-------------------|--------------------+
  |           |   1F5h    |  175h     | LBA High          | LBA High           |
 *|           |-----------|-----------|-------------------|--------------------+
 *|           |   1F6h    |  176h     | Device            | Device             |
 *|           |-----------|-----------|-------------------|--------------------+
  |           |   1F7h    |  177h     | Status            | Command            |
 *|-----------|-----------|-----------|-------------------|--------------------+
 *| Control   |           |           | Alternate         | Device             |
 *| Block     |   3F6h    |  376h     |                   |                    |
  | Register  |           |           | status            | Control            |
 *|-----------|-----------------------------------------------------------------
 *
 * Command hex
 * 1. identify     : 0xEC    To identify device
 * 2. read sector  : 0x20    To read sector
 * 3. write sector : 0x30    To write sector
 *----------------------------------------------------------------------------
 * two different registers : Device register/ Status register
 *----------------------------------------------------------------------------
 *   Device register
 *   |-----------|
 * 7 |     1     |
 *   |-----------|
 * 6 |   L MOD   |   LBA mode. this bit selects the mode of operation, 0 for CHS
 mode , 1 for LBA mode (use LBA28 stands)
 *   |-----------|
 * 5 |     1     |
 *   |-----------|
 * 4 |    DEV    |   drive when dev=0, drive 0 (master) is selected , when dev =
 1 , drive 1 (slave) is selected
 *   |-----------|
 * 3 |    HS3    |  \
 *   |-----------|  |   if L = 0, these four bits select the head number.
 * 2 |    HS2    |  |
 *   |-----------|   >  if L = 1, HS0 through HS3 contain bit 24-27 of the LBA
 * 1 |    HS1    |  |
 *   |-----------|  |
 * 0 |    HS0    |  |
 *   |-----------|  /
 *
 *   Status register
 *   |-----------|
 * 7 |    BSY    |  if this bit is 1, it means busy.
 *   |-----------|
 * 6 |   DRDY    |  if this bit is 1, it means device is ready
 *   |-----------|
 * 5 |  DF/SE    |  Device Fault / Stream Error
 *   |-----------|
 * 4 |     #     |  Command dependent. (formerly DSC bit)
 *   |-----------|
 * 3 |    DRQ    |  if this bit is 1, it means data is ready
 *   |-----------|
 * 2 |     -     |  Obsolete
 *   |-----------|
 * 1 |     -     |  Obsolete
 *   |-----------|
 * 0 |    ERR    |  if this bit is 1, it mean error happening. Error message at
 error register
 *   |-----------|
 *
 *
 *   Device Control register
 *   |-----------|
 * 7 |    HOB    |  High Order Byte (defined by 48-bit Address feature set).
 *   |-----------|
 * 6 |     -     |
 *   |-----------|
 * 5 |     -     |
 *   |-----------|
 * 4 |     -     |
 *   |-----------|
 * 3 |     -     |
 *   |-----------|
 * 2 |   SRST    |  Software reset
 *   |-----------|
 * 1 |   -IEN    |  Interrupt Enable
 *   |-----------|
 * 0 |     0     |
 *   |-----------|
 * */
// MBR disk
struct disk {
        char name[8];                    // name of disk
        struct ide_channel *my_channel;  // this disk own channel
        uint_8 dev_no;                   // master :0, slave :1
};

struct ide_channel {
        char name[8];                // name of ata channel
        uint_16 port_base;           // this channel start port
        uint_8 irq_no;               // this channel irq number
        struct lock lock;            // channel lock
        bool expecting_intr;         // if waiting disk interrupt or not
        struct semaphore disk_done;  // To block or awake driver
        struct disk devices[2];      // represent master or slave disk
};


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
        0x08  // data trans ready. set when the drive to transfer, or is ready
              // to accept PIO
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
// TODO::
#define max_lba ((80 * 1024 * 1024 / 512) - 1)

// STEC Compact flash
// clang-format off
struct STEC_CF_identify_data {
        uint_16 signature;             // Word 0: 0x848A
        uint_16 cylinders;             // Word 1
        uint_16 reserved1;             // Word 2
        uint_16 heads;                 // Word 3
        uint_16 retired4;             // Word 4 Do not use this word. Before retirement, was number of unformatted bytes per track 
        uint_16 retired5;             // Word 5 Do not use this word. Before retirement, was number of unformatted bytes per track 
        uint_16 sectors_per_track;     // Word 6
        uint_16 sector_count_cf_lsw;   // Word 7
        uint_16 sector_count_cf_msw;   // Word 8
        uint_16 reserved9;             // Word 9
        uint_16 serial_number[10];     // Words 10–19
        uint_16 retired20;             // Word 20
        uint_16 retired21;             // Word 21
        uint_16 ecc_bytes;             // Word 22  # of ECC bytes passed on Read/Write Long command
        uint_16 firmware_revision[4];  // Words 23–26
        uint_16 model_number[20];      // Words 27–46 Model Number in ASCII (40 characters): STI Flash 8.0.0 <left justified>
        uint_16 rw_multiple_max;       // Word 47 Maximum of 1 sector on Read/Write Multiple command
        uint_16 reserved48;            // Word 48 Double Word not supported 
        uint_16 capabilities;          // Word 49 DMA supported, LBA supported (0200H DMA not supported, LBA supported for part numbers with P)
        uint_16 reserved50;            // Word 50
        uint_16 pio_cycle_timing;      // Word 51 PIO data transfer cycle timing mode 
        uint_16 dma_cycle_timing;      // Word 52 Single word DMA data transfer cycle timing mode (not supported)
        uint_16 reserved53;            // Word 53
        uint_16 current_cylinders;     // Word 54
        uint_16 current_heads;         // Word 55
        uint_16 current_sectors;       // Word 56
        uint_16 current_capacity_lsw;  // Word 57
        uint_16 current_capacity_msw;  // Word 58
        uint_16 block_count_setting;   // Word 59 Current Setting for Block Count=1 for R/W Multiple commands
        uint_16 lba_sector_count_lsw;  // Word 60
        uint_16 lba_sector_count_msw;  // Word 61
        uint_16 single_word_dma;       // Word 62       
        uint_16 multi_word_dma;        // Word 63 Multiword DMA modes supported (0000H Multiword DMA modes not supported for part numbers with P)
        uint_16 advanced_pio_modes;    // Word 64 Advanced PIO modes supported (modes 3 and 4)
        uint_16 min_mwdma_time;        // Word 65 Minimum multiword DMA transfer cycle time per word (ns) (0000H for part numbers with P)
        uint_16 rec_mwdma_time;        // Word 66 Recommended multiword DMA transfer cycle time per word (ns) (0000H for part numbers with P)
        uint_16 min_pio_time_no_flow;  // Word 67 Minimum PIO transfer without flow control
        uint_16 min_pio_time_iordy;    // Word 68 Minimum PIO transfer with IORDY flow control
        uint_16 reserved[256 - 69];    // Words 69–255
} __attribute__((packed));             //
// clang-format on
struct ata_identify_readable_data {
        char serial_number[10];
        char firmware_vision[4];
        char model_number[20];
        uint_32 lba28_sector_count;
        bool support_LBA;
        bool support_DMA;
};
// First partition entry
#define FIRST_P_ENTRY 0x1be

uint_8 channel_cnt;
struct ide_channel channels[2];  // 2 different channels

static inline void swap_pairs_bytes(const char *dst, char *buf, uint_32 len)
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

static bool check_DRQ(struct disk *hd)
{
        struct ide_channel *channel = hd->my_channel;
        if (!(inb(reg_status(channel)) & ATA_STATR_BSY)) {
                return (inb(reg_status(channel)) & ATA_STATR_DRQ);
        }
        return false;
}

static void delay400ns(struct disk *hd)
{
        for (int i = 0; i < 4; i++)
                inb(reg_ctl(hd->my_channel));
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
        while (!check_DRQ(hd)) {
                delay400ns(hd);
        }
        // 5. read data from buffer
        read_from_sector(hd, (void *) buf, 1);
        lock_release(&hd->my_channel->lock);
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
        while (!check_DRQ(hd)) {
                delay400ns(hd);
        }
        // 5. read data from buffer
        write_to_sector(hd, (void *) ((uint_32) buf), 1);
        semaphore_down(&hd->my_channel->disk_done);
        lock_release(&hd->my_channel->lock);
}

int ide_read(struct block_device *bdev, uint_32 lba, uint_32 sec_cnt, void *buf)
{
        struct disk *hd = bdev->bd_disk->private_data;
        ASSERT(hd);
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
        return cur_lba;
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
// pocket386 don't support DMA
void ide_read_DMA(struct disk *hd, uint_32 lba, void *buf, uint_32 sec_cnt)
{
        ASSERT(lba <= max_lba);
        ASSERT(sec_cnt > 0);
        return;
}


int ide_write(struct block_device *bdev,
              uint_32 lba,
              uint_32 sec_cnt,
              void *buf)
{
        struct disk *hd = (struct disk *) bdev->bd_disk->private_data;
        ASSERT(hd);
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
        return cur_cnt;
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


static void identify_disk(struct disk *hd,
                          struct ata_identify_readable_data *data)
{
        select_disk(hd);
        cmd_out(hd->my_channel, CMD_IDENTIFY);

        delay400ns(hd);

        uint_16 identify_data[256] = {0};
        for (int i = 0; i < 256; ++i) {
                insw(reg_data(hd->my_channel), &identify_data[i], 1);
        }
        uint_8 *id = (uint_8 *) &identify_data;

        struct STEC_CF_identify_data *cfid =
            (struct STEC_CF_identify_data *) id;
        uint_32 lba28 =
            (cfid->lba_sector_count_msw << 16) | cfid->lba_sector_count_lsw;

        char *str_mn = (char *) cfid->model_number;
        char *str_sn = (char *) cfid->serial_number;
        char *str_ver = (char *) cfid->firmware_revision;
        char *model_number = kmalloc(20 + 1);
        char *serial_number = kmalloc(10 + 1);
        char *firmware_revision = kmalloc(4 + 1);
        swap_pairs_bytes(str_mn, model_number, 20);
        swap_pairs_bytes(str_sn, serial_number, 10);
        swap_pairs_bytes(str_ver, firmware_revision, 4);
        strncpy(data->model_number, model_number, 20);
        strncpy(data->serial_number, serial_number, 10);
        strncpy(data->firmware_vision, firmware_revision, 4);
        data->model_number[19] = '\0';
        data->serial_number[9] = '\0';
        data->firmware_vision[3] = '\0';
        data->lba28_sector_count = lba28;
        data->support_DMA = cfid->capabilities & 0x0100;
        data->support_LBA = cfid->capabilities & 0x0200;

        return;
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
        /* irq_exit(); */
}

int_32 ide_open(struct block_device *bdev, fmode_t mode)
{
        return 0;
}
int_32 ide_release(struct gendisk *disk, fmode_t mode)
{
        return 0;
}
int_32 ide_ioctl(struct block_device *bdev,
                 fmode_t mode,
                 unsigned cmd,
                 unsigned args)
{
        return 0;
}
static struct block_device_operations ide_block_operations = {
    .open = ide_open,
    .release = ide_release,
    .read = ide_read,
    .write = ide_write,
    .ioctl = ide_ioctl};

static void print_identify_data(struct ata_identify_readable_data *id)
{
        INFO("LBA28 %d M", id->lba28_sector_count * 512 / 1024 / 1024);
        INFO("module number: %s", id->model_number);
        INFO("serial number: %s", id->serial_number);
        INFO("firware revision: %s", id->firmware_vision);
        return;
}

int pata_probe(struct device *dev)
{
        // Get ide info by
        uint_8 hd_cnt = *((uint_8 *) (0x475));  // get hd numbers
        ASSERT(hd_cnt > 0);
        // 1 channel -> 2 hd
        // channel_cnt * 2 = hd_cnt
        channel_cnt = DIV_ROUND_UP(hd_cnt, 2);
        struct ide_channel *channel;
        uint_8 channel_no = 0;
        while (channel_no < channel_cnt) {
                uint_8 dev_no = 0;
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
                register_r0_intr_handler(channel->irq_no, intr_hd_handler);

                while (dev_no < 2) {
                        struct disk *hd = &channel->devices[dev_no];
                        hd->dev_no = dev_no;
                        hd->my_channel = channel;
                        sprintf(hd->name, "sd%c",
                                'a' + channel_no * 2 + dev_no);
                        struct ata_identify_readable_data *id =
                            kmalloc(sizeof(*id));
                        identify_disk(hd, id);
                        if (!id->lba28_sector_count) {
                                dev_no++;
                                continue;
                        }

                        print_identify_data(id);

                        struct gendisk *gdisk = alloc_disk();
                        if (!gdisk) {
                                PANIC("[ide]: not enough memory at gendisk");
                        }
                        gdisk->private_data = hd;
                        gdisk->bdops = &ide_block_operations;
                        gdisk->lba_sectors = id->lba28_sector_count;
                        strncpy(gdisk->name, hd->name, 8);
                        add_disk(gdisk);
                        dev_no++;
                }
                channel_no++;
        }

        return 0;
}



static struct driver ide_driver = {.name = "ata-ide",
                                   .bus = &isa_bus,
                                   .probe = pata_probe};

uint_32 ata_ide_driver_init(void)
{
        register_driver(&ide_driver);
        return 0;
}
