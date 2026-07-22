#ifndef _ARCH_X86_PAGE_H
#define _ARCH_X86_PAGE_H

// 32bit for now
#define PAGE_SIZE       4096U
#define KPAGE_TABLE_START 0x00100000UL
/* Loader-owned page directory and page tables: physical [1 MiB, 2 MiB). */
#define BOOTSTRAP_PAGING_END 0x00200000UL
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
#define PG_PWT  0x008U
#define PG_PCD  0x010U

#endif
