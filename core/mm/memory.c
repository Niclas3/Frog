#include <const.h>  // for PG_SIZE
#include <debug.h>
#include <panic.h>
#include <string.h>
#include <sys/memory.h>

#include <sys/threads.h>

//Assume that core.o is 70kb aka 0x11800
//and start at 0x80000
//so the end is 0x80000 + 0x11800 = 0x91800
#define K_HEAP_START 0x00092000
// to                      0xa0000
// 14 pages aka 14 * 4kb = 0xe000
// 1 page dir table and 8 page table
#define PDT_COUNT 1
#define PGT_COUNT 8

struct pool kernel_pool;
struct pool user_pool;
struct _virtual_addr kernel_viraddr;

// upper 10 bits pde
#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)
// mid   10 bits pte
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)

static void mem_pool_init(uint_32 all_mem)
{
    // 1 page dir table and 8 page table
    uint_32 page_table_size = PG_SIZE * (PDT_COUNT + PGT_COUNT);
    /*
     *  page table start at 0x100000 (aka 1MB)
     *  used_mem = page_table_size + 1MB
     *  1MB includes 0xa000 and 0xb800
     **/
    /* uint_32 used_mem = page_table_size + 0x100000;  
     * I put page table at 0x0
     * */
    uint_32 used_mem = 0x100000;
    /*
     *  all_mem for now is 32MB
     *  It must be calculate by loader.s before entering protected mode
     * */
    uint_32 free_mem = all_mem - used_mem;
    uint_16 all_free_pages = free_mem / PG_SIZE;

    /* kernel used memory vs user used memory
     * */
    uint_16 kernel_free_page = all_free_pages / 2;
    uint_16 user_free_page = all_free_pages - kernel_free_page;

    // Kernel bitmap length
    // In bitmap 1 bit represents a free page.
    // div 8 for how many one byte
    uint_32 kbm_length = kernel_free_page / 8;

    // user space bitmap length
    uint_32 ubm_length = user_free_page / 8;

    // Kernel pool start
    // First address of free memory
    // kp_start = 0x0100000;  //1M
    uint_32 kp_start = used_mem;
    // User pool start
    // up_start  = 0x1080000;  // 15.5Mb kernel physical memory
    // free_page = 0xf80; // 3968
    uint_32 up_start = kp_start + kernel_free_page * PG_SIZE;


    kernel_pool.phy_addr_start = kp_start;
    user_pool.phy_addr_start = up_start;

    kernel_pool.pool_size = kernel_free_page * PG_SIZE;
    user_pool.pool_size = user_free_page * PG_SIZE;

    kernel_pool.pool_bitmap.map_bytes_length = kbm_length;
    user_pool.pool_bitmap.map_bytes_length = ubm_length;

    // kernel pool bit map fix at MEM_BITMAP_BASE 0x9a00
    kernel_pool.pool_bitmap.bits = (void *) MEM_BITMAP_BASE;
    user_pool.pool_bitmap.bits = (void *) (MEM_BITMAP_BASE + kbm_length);

    // init kernel bitmap
    init_bitmap(&kernel_pool.pool_bitmap);
    // init user bitmap
    init_bitmap(&user_pool.pool_bitmap);
    lock_init(&kernel_pool.lock);
    lock_init(&user_pool.lock);

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
    TCB_t *cur = running_thread();
    if (cur->pgdir == NULL && poolt == MP_KERNEL) {
        start_pos = find_block_bitmap(&kernel_viraddr.vaddr_bitmap, pg_cnt);
        if (start_pos == -1) {
            return NULL;
        }
        for (int i = 0; i < pg_cnt; i++) {
            set_value_bitmap(&kernel_viraddr.vaddr_bitmap, start_pos + i, 1);
        }
        v_start_addr = start_pos * PG_SIZE + kernel_viraddr.vaddr_start;
        return (void *) v_start_addr;
    } else if(cur->pgdir != NULL && poolt == MP_USER){ // TODO: user vaddress pools
        start_pos = find_block_bitmap(&cur->progress_vaddr.vaddr_bitmap, pg_cnt);
        if (start_pos == -1) {
            return NULL;
        }
        for (int i = 0; i < pg_cnt; i++) {
            set_value_bitmap(&cur->progress_vaddr.vaddr_bitmap, start_pos + i, 1);
        }
        v_start_addr = start_pos * PG_SIZE + cur->progress_vaddr.vaddr_start;
        return (void *) v_start_addr;
    } else {
        PAINC("get_free_vaddress: not allow kernel alloc userspace or user alloc kernel space.");
    }
}

// get a free 4k phy memory aka 1 page in pool 
// -> pte
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

uint_32 *pte_ptr(uint_32 vaddr)
{
    // I set last PDE as PDT table address
    // So the No.1023 pde to hex is 0x3ff
    // when vaddress is 0xffc00000
    // It will get pdt table address
    // top    10 bits 0xffc    <--- this is the last PDE point to page aka PDT
    // middle 10 bits (vaddr's top 10 bits which is original pde index)
    uint_32 target_pte = (0xffc00000 +
                          ((vaddr & 0xffc00000) >> 10) +
                          PTE_IDX(vaddr) * 4);
    return (uint_32 *)target_pte;
}

uint_32 *pde_ptr(uint_32 vaddr)
{
    // The last page table address in PDE is
    // 0xfffffxxx
    // top    10 bits 0x3ff   <-- the last entry of PDT
    // middle 10 bits 0x3ff   <-- pointer to self (same PDT) again
    // bottom 12 pde_idx      <-- target entry
    uint_32 *target_pde = (uint_32 *) (0xfffff000 + PDE_IDX(vaddr) * 4);
    return target_pde;
}
uint_32 addr_v2p(uint_32 vaddr){
    uint_32 *pte = pte_ptr(vaddr);
    return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
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
        // TODO:
        // test if pte is exist
        // should re-consider v-address start 
        /* ASSERT(!(*pte & 0x00000001)); */
        if ((!(*pte & 0x00000001))) {
            *pte = (phyaddress | PG_US_U | PG_RW_W | PG_P_SET);
        } else {
            /* panic("pte exists"); */
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

// get free vaddress and paddress and put them together
void *malloc_page(enum mem_pool_type poolt, uint_32 pg_cnt)
{
    ASSERT(pg_cnt > 0 && pg_cnt < 3840);
    void *vaddr_start = get_free_vaddress(poolt, pg_cnt);
    if (vaddr_start == NULL) { return NULL; }
    uint_32 vaddr = (uint_32) vaddr_start;
    struct pool *mem_pool = poolt & MP_KERNEL ? &kernel_pool : &user_pool;

    while (pg_cnt--) {
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
    lock_fetch(&kernel_pool.lock);
    void *vaddr = malloc_page(MP_KERNEL, pg_cnt);
    if (vaddr != NULL) {
        memset(vaddr, 0, pg_cnt * PG_SIZE);
    }
    lock_release(&kernel_pool.lock);
    return vaddr;
}

// Get user mode page from memory
void *get_user_page(uint_32 pg_cnt)
{
    lock_fetch(&user_pool.lock);
    void *vaddr = malloc_page(MP_USER, pg_cnt);
    if (vaddr != NULL) {
        memset(vaddr, 0, pg_cnt * PG_SIZE);
    }
    lock_release(&user_pool.lock);
    return vaddr;
}


