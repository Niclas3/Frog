# Memory Management

## MM module features
1. KASAN uses shadow memory and poison to protect kernel heap / user space / shadow memory. 
2. Manage physical memory, split it into two parts, kernel and user space.
3. Manage visual memory in kernel space, which starts at 0xc000_0000.

## API

mem_init()
kmalloc()
kfree()
sys_malloc()
sys_free()

## not implement
sbrk()
brk()
mmap()
munmap()

## MM module structures
```c
typedef struct _virtual_addr {
        struct bitmap vaddr_bitmap;
        uint_32 vaddr_start;
} virtual_addr;

typedef enum mem_pool_type { 
    MP_KERNEL = 1, 
    MP_USER 
} pool_type;

struct mem_block_desc {
        uint_32 block_size;
        uint_32 blocks_per_arena;
        uint_32 redzone_size;
        struct list_head free_list;
};

struct arena {
        struct mem_block_desc *desc;
        uint_32 cnt;
        bool large;  // flag about this arena is over 1024b or not
};

struct pool {
        struct bitmap pool_bitmap;
        struct lock lock;
        uint_32 phy_addr_start;  // pool must at a phy address
        uint_32 pool_size;
};
```

This MM use `struct pool` to manage physical memory in page size. There are 7
types of memory block descriptor, each descriptor has they own free_list to
indicate how many blocks are ready to use. `struct arena` like a large memory
can be split into blocks each `arena` is in page size (4kb).

`struct virtual_add` for managing virtual address, like `struct pool`, there are
also functions free/alloc from it.

## Page table layout


## Physical memory layout
 +-----------------------+------------------+----------------+
 |      address          |   name           |     size       |
 +-----------------------+------------------+----------------+
 |     0x0000_c508       |   GDT            |   100bytes     |
 |     0x0000_c588       |   IDT            |   255 * 8bytes |
 |     0x0006_0000       | MEM_BITMAP_BASE  |                |
 |     0x0007_0000       |   kernel code    |   20 pages     |
 |     0x0007cfcc        |   end    code    |     --         |
 |     0x000a_0000        |   vga            |   x pages      |
 |     0x000b_8000        |   text view      |   x pages      |
 +------------------------------------------+----------------+
 |     0x0010_0000       |                  |                | 
 |                       |   page table     |   3 pages      |
 |                       |                  |                |
 +-----------------------+------------------+----------------+
 physical memory available usage

## Virtual memory layout
 Kernel virtual address space
                         +-------------------------------------+
0xC000_0000              |                                     |
                         |                                     |
                         |                                     |
                         |                                     |
0xC007_0000              +-------------------------------------+ kernel code
                         |                                     |
                         |                                     |
                         |                                     |
0xC007_CFCC              +-------------------------------------+ _end
                         |                                     |
(_end & ~0xfff) + 0x1000 +-------------------------------------+ kernel heap
bottom
                         |                                     |
                         |                                     |
                                     /* ... */
0xFF40_0000              +-------------------------------------+ shadow memory 1 page 4kb
                         |                                     |
                         |     96 pages for                    |
                         |     shadow map 3MB real memory      |
                         |                                     |
0xFF46_0000              |-------------------------------------|
                         |                                     |
0xFF80_0000              +-------------------------------------+ kernel stack pool bottom
                         |                                     |
                         |                                     | reserver by kernel stack  / 1 pagesize 5page
0xFFBF_FFFF              +-------------------------------------+
0xFFC0_0000              +-------------------------------------+ Page table
Directory start
                         |                                     |
                         |                                     |
0xFFFF_FFFF              +-------------------------------------+
                         +-------------------------------------+
0xC000_0000              |                                     |
                         |                                     |
                         |                                     |
                         |                                     |
0xC007_0000              +-------------------------------------+ kernel code
                         |                                     |
                         |                                     |
                         |                                     |
0xC007_CFCC              +-------------------------------------+ _end
                         |                                     |
(_end & ~0xfff) + 0x1000 +-------------------------------------+ kernel heap
bottom
                         |                                     |
                         |                                     |
                                     /* ... */
0xFF40_0000              +-------------------------------------+ shadow memory 1 page 4kb
                         |                                     |
                         |     96 pages for                    |
                         |     shadow map 3MB real memory      |
                         |                                     |
0xFF46_0000              |-------------------------------------|
                         |                                     |
0xFF80_0000              +-------------------------------------+ kernel stack pool bottom
                         |                                     |
                         |                                     | reserver by kernel stack  / 1 pagesize 5page
0xFFBF_FFFF              +-------------------------------------+
0xFFC0_0000              +-------------------------------------+ Page table
Directory start
                         |                                     |
                         |                                     |
0xFFFF_FFFF              +-------------------------------------+

## Page fault process
### lazy allocate
## KASAN
For now, Frog/mm only hook malloc/free functions It will be finished to hook
other memory function.

[] memset
[] memcpy
[] memncpy
[] strlen
[] strcpy

## Some work on allocate fail
Frog does not have any failback like alloc_page_or_die() yet.
