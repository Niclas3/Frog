#include <const.h>  // for PG_SIZE
#include <sys/memory.h>

#define K_HEAP_START 0xc0100000


struct pool kernel_pool;
struct pool user_pool;
struct virtual_addr kernel_viraddr;

static void mem_pool_init(uint_32 all_mem)
{
    uint_32 page_table_size = PG_SIZE * 256;
    uint_32 used_mem = page_table_size + 0x100000;
    uint_32 free_mem = all_mem - used_mem;
    uint_16 all_free_pages = free_mem / PG_SIZE;

    uint_16 kernel_free_page = all_free_pages / 2;
    uint_16 user_free_page = all_free_pages - kernel_free_page;

    // Kernel bitmap length
    uint_32 kbm_length = kernel_free_page / 8;
    // user space bitmap length
    uint_32 ubm_length = user_free_page / 8;

    // Kernel pool start
    uint_32 kp_start = used_mem;
    // User pool start
    uint_32 up_start = kp_start + kernel_free_page * PG_SIZE;

    kernel_pool.phy_addr_start = kp_start;
    user_pool.phy_addr_start = up_start;

    kernel_pool.pool_size = kernel_free_page * PG_SIZE;
    user_pool.pool_size = user_free_page * PG_SIZE;

    kernel_pool.pool_bitmap.map_bytes_length = kbm_length;
    user_pool.pool_bitmap.map_bytes_length = ubm_length;


    // kernel pool bit map fix at MEM_BITMAP_BASE
    kernel_pool.pool_bitmap.bits = (void *) MEM_BITMAP_BASE;
    user_pool.pool_bitmap.bits = (void *) (MEM_BITMAP_BASE + kbm_length);

    // init kernel bitmap
    init_bitmap(&kernel_pool.pool_bitmap);
    // init user bitmap
    init_bitmap(&user_pool.pool_bitmap);

    kernel_viraddr.vaddr_bitmap.map_bytes_length = kbm_length;
    kernel_viraddr.vaddr_bitmap.bits =
        (void *) (MEM_BITMAP_BASE + kbm_length + ubm_length);
    kernel_viraddr.vaddr_start = K_HEAP_START;
    init_bitmap(&kernel_viraddr.vaddr_bitmap);
}

void mem_init()
{
    uint_32 mem_bytes_tot = (*(uint_32 *) (0xb00));
    mem_pool_init(mem_bytes_tot);
}

// Picking a pool return a free v_address
static void *get_free_vaddress(enum mem_pool_type poolt, uint_32 pg_cnt) {}

// get or free 4k phy memory aka 1 page -> pte
void *get_free_page(struct pool *mpool) {}

void free_page(struct pool *mpool, uint_32 phy_addr_page) {}

// Combine v address -> phy address
void put_page(uint_32 v_addr, uint_32 phy_addr) {}

// Get kernel page from memory
void *get_kernel_page(uint_32 pg_cnt) {}
