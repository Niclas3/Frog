#ifndef __SYS_MEMORY_H
#define __SYS_MEMORY_H
#include <bitmap.h>
#include <list.h>
#include <ostype.h>
#include <sys/semaphore.h>
// Address Range Descriptor Structure
//
// Offset in Bytes		Name		Description
// 	0	    BaseAddrLow		Low 32 Bits of Base Address
// 	4	    BaseAddrHigh	High 32 Bits of Base Address
// 	8	    LengthLow		Low 32 Bits of Length in Bytes
// 	12	    LengthHigh		High 32 Bits of Length in Bytes
// 	16	    Type		Address type of  this range.
//
// The BaseAddrLow and BaseAddrHigh together are the 64 bit BaseAddress of this
// range. The BaseAddress is the physical address of the start of the range
// being specified.
//
// The LengthLow and LengthHigh together are the 64 bit Length of this range.
// The Length is the physical contiguous length in bytes of a range being
// specified.
//
// The Type field describes the usage of the described address range as defined
// in the table below.
//
// Value	Pneumonic		Description
// 1	AddressRangeMemory	This run is available RAM usable by the
// 				operating system.
// 2	AddressRangeReserved	This run of addresses is in use or reserved
// 				by the system, and must not be used by the
// 				operating system.
// Other	Undefined		Undefined - Reserved for future use.  Any
// 				range of this type must be treated by the
// 				OS as if the type returned was
// 				AddressRangeReserved.
//
// The BIOS can use the AddressRangeReserved address range type to block out
// various addresses as "not suitable" for use by a programmable device.
//
// Some of the reasons a BIOS would do this are:
//
//     The address range contains system ROM.
//     The address range contains RAM in use by the ROM.
//     The address range is in use by a memory mapped system device.
//     The address range is for whatever reason are unsuitable for a standard
//     device to use as a device memory space.
typedef enum {
    ARDS_address_range_memory = 1,
    ARDS_address_range_reserved = 2,
    ARDS_Undefined
} ARDS_t;

struct memory_map_descriptor {
    uint_32 base_addr_low;
    uint_32 base_addr_high;
    uint_32 length_low;
    uint_32 length_high;
    ARDS_t type;
} __attribute__((packed));

typedef struct _virtual_addr {
    struct bitmap vaddr_bitmap;
    uint_32 vaddr_start;
} virtual_addr;

typedef enum mem_pool_type { MP_KERNEL = 1, MP_USER } pool_type;

struct mem_block {
    struct list_head free_elem;
};

// The largest size is 4KB
// There are seven different descriptions
//
// @Attr block_size :
struct mem_block_desc {
    uint_32 block_size;
    uint_32 blocks_per_arena;
    struct list_head free_list;
};

#define DESC_CNT 7  // type counts of memory blocks

/* P bit shows if or not this entry in memory
 * R/W W bit shows read / execute
 * R/W R bit shows read / execute
 */
#define PG_P_SET 1
#define PG_P_CLI 0

#define PG_RW_W 2
#define PG_RW_R 0
#define PG_US_S 0  // supervisor
#define PG_US_U 4  // user

struct pool {
    struct bitmap pool_bitmap;
    struct lock lock;
    uint_32 phy_addr_start;  // pool must at a phy address
    uint_32 pool_size;
};

void mem_init(void);
// alloc any size memory
void *sys_malloc(uint_32 size);
// Free pointed memory
void sys_free(void *ptr);

void mfree_page(enum mem_pool_type poolt, void *_vaddr, uint_32 pg_cnt);
void free_phy_page(pool_type pt, uint_32 phy_addr_page);

// Alloc a page aka (4kb) link to vaddr_start
void *malloc_page_with_vaddr(enum mem_pool_type poolt, uint_32 vaddr_start);

void *malloc_page_with_vaddr_test(enum mem_pool_type poolt,
                                  uint_32 vaddr_start);

// alloc a phyaddr to given virtual address
void *get_phy_free_page_with_vaddr(enum mem_pool_type poolt, uint_32 vaddr);

// init block descriptors
void block_desc_init(struct mem_block_desc *desc_array);

// // get or free 4k phy memory aka 1 page -> pte
// void* get_free_page(struct pool *mpool);
// void free_page(struct pool *mpool, uint_32 phy_addr_page);

// Copy or free 4m phy memory -> dte
// TODO:
// void copy_page_tables();
// void free_page_tables();

// combine v address -> phy address
// v_addr from `get_free_vaddress`
// phy_addr from `get_free_page()`
void put_page(void *v_addr, void *phy_addr);

uint_32 addr_v2p(uint_32 vaddr);
// Get kernel page from memory
void *get_kernel_page(uint_32 pg_cnt);

void *get_user_page(uint_32 pg_cnt);

uint_32 *pde_ptr(uint_32 vaddr);

uint_32 *pte_ptr(uint_32 vaddr);

#endif
