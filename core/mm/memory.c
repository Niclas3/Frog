#include <const.h>  // for PG_SIZE
#include <debug.h>
#include <panic.h>
#include <string.h>
#include <sys/memory.h>

#include <sys/graphic.h>

#define K_HEAP_START 0xc0100000

struct pool kernel_pool;
struct pool user_pool;
struct virtual_addr kernel_viraddr;

// upper 10 bits pde
#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)
// mid   10 bits pte
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)

static void mem_pool_init(uint_32 all_mem)
{
    // 1 page dir table and 8 page table
    uint_32 page_table_size = PG_SIZE * 256;
    uint_32 used_mem = page_table_size + 0x100000;
    uint_32 free_mem = all_mem - used_mem;
    uint_16 all_free_pages = free_mem / PG_SIZE;

    draw_hex(0xa0000, 320, COL8_00FFFF, 0, 0, page_table_size);
    draw_hex(0xa0000, 320, COL8_00FFFF, 0, 16, used_mem);
    draw_hex(0xa0000, 320, COL8_00FFFF, 0, 32, all_mem);
    draw_hex(0xa0000, 320, COL8_00FFFF, 0, 32+16, free_mem);
    draw_hex(0xa0000, 320, COL8_00FFFF, 0, 32+16+16, all_free_pages);

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
    uint_32 mem_bytes_total = 0x2000000;//32M //(*(uint_32 *) (0xb00));
    mem_pool_init(mem_bytes_total);
}

// Picking a pool return a free v_address
static void *get_free_vaddress(pool_type poolt, uint_32 pg_cnt)
{
    int_32 v_start_addr = 0;
    int_32 start_pos = -1;
    if (poolt == MP_KERNEL) {
        start_pos = find_block_bitmap(&kernel_viraddr.vaddr_bitmap, pg_cnt);
        if (start_pos == -1) {
            return NULL;
        }
        for (int i = 0; i < pg_cnt; i++) {
            set_value_bitmap(&kernel_viraddr.vaddr_bitmap, start_pos + i, 1);
        }
        v_start_addr = start_pos * PG_SIZE + kernel_viraddr.vaddr_start;
        return (void *) v_start_addr;
    } else {
        // TODO: user vaddress pools
        return (void *) NULL;
    }
}

// get or free 4k phy memory aka 1 page -> pte
void *get_free_page(struct pool *mpool)
{
    int_32 start_pos = -1;
    start_pos = find_block_bitmap(&mpool->pool_bitmap, 1);
    if (start_pos == -1) {
        return NULL;
    }
    set_value_bitmap(&mpool->pool_bitmap, start_pos, 1);
    return (void *) (start_pos * PG_SIZE + mpool->phy_addr_start);
}

void free_page(struct pool *mpool, uint_32 phy_addr_page)
{
    if (phy_addr_page < mpool->phy_addr_start)
        panic("free bad phy address");
    int pos = (phy_addr_page - mpool->phy_addr_start) / PG_SIZE;
    if (pos > mpool->pool_bitmap.map_bytes_length)
        panic("free bad address over length");
    set_value_bitmap(&mpool->pool_bitmap, pos, 0);
    return;
}

//
uint_32 *pte_ptr(uint_32 vaddr)
{
    // I set last PDE as PDT table address
    // So the No.1023 pde to hex is 0x3ff
    // when vaddress is 0xffc00000
    // It will get pdt table address
    // top    10 bits 0xffc    <--- this is the last PDE point to page aka PDT
    // middle 10 bits (vaddr's top 10 bits which is original pde index)
    uint_32 *target_pde = (uint_32 *) 0xffc00000 +
                          ((vaddr & 0xffc00000) >> 10) + +PTE_IDX(vaddr) * 4;
    return target_pde;
}

uint_32 *pde_ptr(uint_32 vaddr)
{
    // The last page table address in PDE is
    // 0xfffffxxx
    // top    10 bits 0x3ff   <-- the last entry of PDT
    // middle 10 bits 0x3ff   <-- pointer to self (same PDT) again
    // bottom 12 pde_idx      <-- target entry
    uint_32 *target_pte = (uint_32 *) (0xfffff000 + PDE_IDX(vaddr) * 4);
    return target_pte;
}
// Combine v address -> phy address
void put_page(void *v_addr, void *phy_addr)
{
    uint_32 vaddress = (uint_32) v_addr;
    uint_32 phyaddress = (uint_32) phy_addr;
    uint_32 *pde = pde_ptr(vaddress);
    uint_32 *pte = pte_ptr(vaddress);

    // test P bit of vaddress
    if (*pde & 0x00000001) {
        ASSERT(!(*pte & 0x00000001));
        if ((!(*pte & 0x00000001))) {
            *pte = (phyaddress | PG_US_U | PG_RW_W | PG_P_SET);
        } else {
            panic("pte exists");
            *pte = (phyaddress | PG_US_U | PG_RW_W | PG_P_SET);
        }
    } else {
        // if there is no pde , let's create it.
        // Create phyaddr at kernel pool
        uint_32 pde_phyaddr = (uint_32) get_free_page(&kernel_pool);
        *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_SET);
        // Clear pte target address 1 page 4kb
        // top 10 ->
        memset((void *) ((int) pte & 0xfffff000), 0, PG_SIZE);
        ASSERT(!(*pte & 0x00000001));
        *pte = (phyaddress | PG_US_U | PG_RW_W | PG_P_SET);
    }
}
void *malloc_page(enum mem_pool_type poolt, uint_32 pg_cnt)
{
    ASSERT(pg_cnt > 0 && pg_cnt < 3840);
    void *vaddr_start = get_free_vaddress(poolt, pg_cnt);
    if (vaddr_start == NULL) {
        return NULL;
    }
    uint_32 vaddr = (uint_32) vaddr_start;
    uint_32 cnt = pg_cnt;

    struct pool *mem_pool = poolt & MP_KERNEL ? &kernel_pool : &user_pool;

    while (cnt--) {
        void *phyaddrs = get_free_page(mem_pool);
        if (phyaddrs == NULL) {
            return NULL;
        }
        put_page((void *) vaddr, phyaddrs);
        vaddr += PG_SIZE;
    }
    return vaddr_start;
}

// Get kernel page from memory
void *get_kernel_page(uint_32 pg_cnt)
{
    void *vaddr = malloc_page(MP_KERNEL, pg_cnt);
    if (vaddr != NULL) {
        memset(vaddr, 0, pg_cnt * PG_SIZE);
    }
    return vaddr;
}
