#ifndef __DEV_IDE_H__
#define __DEV_IDE_H__
#include <ostype.h>
#include <sys/semaphore.h>
#include <list.h>
#include <bitmap.h>

struct partition {
    uint_32 start_lba;             // start of sector
    uint_32 sec_cnt;               // all sector count of this partition
    struct disk* my_disk;          // disk of this partition
    struct list_head part_tag;     // list mark
    char name[8];                  // name of this partition
//below attributes only at memory
    struct super_block* sb;        // super block of this partition
    struct bitmap zone_bitmap;     // bitmap of block (aka zone)
    struct bitmap inode_bitmap;    // bitmap of inode
    struct list_head open_inodes;  // list of open inode 
};

struct disk {
    char name[8];                        // name of disk
    struct ide_channel* my_channel;      // this disk own channel
    uint_8 dev_no;                       // master :0, slave :1
    struct partition prim_partition[4];  // max number of primary partition is 4
    struct partition logic_partition[8]; // only support 8 logic partitions
};

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
 * 6 |   L MOD   |   LBA mode. this bit selects the mode of operation, 0 for CHS mode , 1 for LBA mode (use LBA28 stands)
 *   |-----------|
 * 5 |     1     |
 *   |-----------|
 * 4 |    DEV    |   drive when dev=0, drive 0 (master) is selected , when dev = 1 , drive 1 (slave) is selected
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
 * 0 |    ERR    |  if this bit is 1, it mean error happening. Error message at error register
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

struct ide_channel {
    char name[8];               // name of ata channel
    uint_16 port_base;          // this channel start port
    uint_8 irq_no;              // this channel irq number
    struct lock lock;           // channel lock
    bool expecting_intr;        // if waiting disk interrupt or not
    struct semaphore disk_done; // To block or awake driver
    struct disk devices[2];     // represent master or slave disk
};
void ide_init(void);
void identify_disk(struct disk* hd);

// read sec_cnt sector from ide to buf
void ide_read(struct disk *hd, uint_32 lba, void *buf, uint_32 sec_cnt);

void ide_read_ext(struct disk *hd, uint_32 lba, void *buf, uint_32 sec_cnt);

void ide_read_DMA(struct disk *hd, uint_32 lba, void *buf, uint_32 sec_cnt);

void ide_write(struct disk *hd, uint_32 lba, void *buf, uint_32 sec_cnt);

void ide_write_ext(struct disk *hd, uint_32 lba, void *buf, uint_32 sec_cnt);

void scan_partitions(struct disk *hd);

bool partitions_info(struct list_head *p_list, int arg );

// hd interrupt handler
void intr_hd_handler(uint_8 irq_no);

#endif
