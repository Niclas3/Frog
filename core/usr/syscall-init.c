#include <debug.h>
#include <ostype.h>
#include <print.h>
#include <stdio.h>
#include <string.h>

#include <sys/syscall-init.h>
#include <sys/syscall.h>

#include <fs/fs.h>  // for sys_write/ sys_open/ sys_close
#include <sys/exec.h>
#include <sys/fork.h>
#include <sys/graphic.h>
#include <sys/threads.h>

#include <ipc.h>

// for test
#include <math.h>
#include <sys/memory.h>

#define syscall_max_nr 32
typedef void *syscall;

syscall syscall_table[syscall_max_nr];

uint_32 sys_getpid(void)
{
    return running_thread()->pid;
}

/**
 * The real producer of system call 'sendrec()'
 *
 * @param func SEND or RECEIVE
 * @param src_dest To/From whom the message is transferred. this is task number
 * @param p_msg pointer to message
 *
 * @return Zero if success
 *****************************************************************************/
uint_32 sys_sendrec(uint_32 func, uint_32 src_dest, message *p_msg)
{
    TCB_t *caller = running_thread();

    ASSERT((src_dest > 0 && src_dest < TASK_MAX) || src_dest == ANY_TASK ||
           src_dest == INTR_TASK);
    int ret = 0;
    p_msg->m_source = caller->pid;
    ASSERT(p_msg->m_source != src_dest);
    /**
     * There are three function about sending and receiving message, SEND,
     * RECIEVE, and BOTH. First two are easy understand.
     * BOTH mean it is transformed into a SEND followed by a RECIEVE
     *
     *****************************************************************************/
    if (func == SEND) {
        ret = msg_send(caller, src_dest, p_msg);
        if (ret != 0) {
            return ret;
        }
    } else if (func == RECEIVE) {
        ret = msg_receive(caller, src_dest, p_msg);
        if (ret != 0) {
            return ret;
        }
    } else if (func == BOTH) {
        ret = msg_send(caller, src_dest, p_msg);
        if (ret == 0)
            ret = msg_receive(caller, src_dest, p_msg);
    } else {
        char error[60];
        sprintf(error,
                "sys_sendrec invalid function: %d (SEND:%d, RECEIVE:%d).", func,
                SEND, RECEIVE);
        PANIC(error);
    }

    return 0;
}

void sys_putc(char c)
{
    put_char(c);
}

extern struct pool user_pool;
extern struct pool kernel_pool;
extern struct _virtual_addr kernel_viraddr;
/* static void *malloc_a_user_page(struct pool *phy_pool) */
static void *malloc_a_user_page(uint_32 va, uint_32 pa)
{
    // get free v_address
    TCB_t *cur = running_thread();
    struct bitmap *v_bitmap = &cur->progress_vaddr.vaddr_bitmap;
    uint_32 v_pos = find_block_bitmap(v_bitmap, 1);
    set_value_bitmap(v_bitmap, v_pos, 1);
    uint_32 v_addr = cur->progress_vaddr.vaddr_start + v_pos * 4096;
    /* uint_32 v_addr = 0x1003000; */

    // get free p_address
    struct bitmap *bmap = &user_pool.pool_bitmap;
    uint_32 p_pos = find_block_bitmap(bmap, 1);
    set_value_bitmap(bmap, p_pos, 1);
    uint_32 p_addr = user_pool.phy_addr_start + p_pos * 4096;
    /* uint_32 p_addr = 0x40c5000; */

    /* put_page(va, pa); */
    /* *(char *) va= 0xdd; */
    /* return (void *) va; */
    put_page(v_addr, p_addr);
    return (void *) v_addr;
}

static void *malloc_user_page_with_va(uint_32 va)
{
    // get free p_address
    struct bitmap *bmap = &user_pool.pool_bitmap;
    uint_32 p_pos = find_block_bitmap(bmap, 1);
    set_value_bitmap(bmap, p_pos, 1);
    uint_32 p_addr = user_pool.phy_addr_start + p_pos * 4096;
    /* uint_32 p_addr = 0x40c5000; */

    put_page(va, p_addr);
    return (void *) va;
}


static bool test_inside(uint_32 vaddr_page)
{
    if (NULL == malloc_page_with_vaddr(MP_USER, vaddr_page)) {
        return false;
    }
    /* if (NULL == malloc_user_page_with_va(vaddr_page)) { */
    /*     return false; */
    /* } */

    // 0x1002000
    /* char *testa = sys_malloc(512); */
    /* sys_free(testa); */

    uint_32 v_a = 0x1004000;
    uint_32 p_a = 0x40c8000;
    uint_32 v_addr = malloc_a_user_page(v_a, p_a);

    *(char *) v_addr = 0xcc;
    if (*(char *) v_addr != 0xcc) {
        // alloc next address
        v_addr = malloc_a_user_page(v_a, p_a);
    }
    *(char *) v_addr = 0xbb;
    return true;
}

#include <elf.h>
static int_32 testoutside(const char *pathname)
{
    uint_8 *buf = sys_malloc(sizeof(Elf32_Ehdr));
    Elf32_Phdr *ph_buf = sys_malloc(32);
    test_inside(0x8048000);

    return 0;
}


static void remove_page(void *v_addr)
{
    uint_32 vaddress = (uint_32) v_addr;
    uint_32 *pde = pde_ptr(vaddress);
    uint_32 *pte = pte_ptr(vaddress);
    if (*pde & 0x00000001) {      // test pde if exist
        if (*pte & 0x00000001) {  // test pde if exist or not
            *pte &= 0x11111110;   // PG_P_CLI;
        } else {                  // pte is not exists
            PANIC("Free twice");
            // Still make pde is unexist
            *pte &= 0x11111110;  // PG_P_CLI;
        }
        __asm__ volatile("invlpg %0" ::"m"(v_addr) : "memory");
    }
}

static bool load_code(int_32 fd, uint_32 virtaddr, uint_32 offset, uint_32 size)
{
    uint_32 vaddr_first_page = virtaddr & 0xfffff000;
    uint_32 size_fpage = PG_SIZE - (virtaddr & 0x00000fff);
    uint_32 occupy_page_cnt = 0;  // in page
    if (size > size_fpage) {
        uint_32 left_size = size - size_fpage;
        occupy_page_cnt = DIV_ROUND_UP(left_size, PG_SIZE + 1);
    } else {
        occupy_page_cnt = 1;
    }

    uint_32 page_idx = 0;
    uint_32 vaddr_page = vaddr_first_page;
    while (page_idx < occupy_page_cnt) {
        uint_32 *pde = pde_ptr(vaddr_page);
        uint_32 *pte = pte_ptr(vaddr_page);

        if (!(*pde & PG_P_SET) || !(*pte & PG_P_SET)) {
            /* // 0x1002000 */
            /* char *test = sys_malloc(512); */
            /* sys_free(test); */

            /* if (NULL == malloc_page_with_vaddr_test(MP_USER, vaddr_page)) {
             */
            if (NULL == malloc_page_with_vaddr(MP_USER, vaddr_page)) {
                return false;
            }
            // 0x1002000
            /* char *testa = sys_malloc(512); */
            /* sys_free(testa); */

            uint_32 v_a = 0x1004000;
            uint_32 p_a = 0x40c8000;
            uint_32 v_addr = malloc_a_user_page(v_a, p_a);

            *(char *) v_addr = 0xcc;
            if (*(char *) v_addr != 0xcc) {
                // alloc next address
                v_addr = malloc_a_user_page(v_a, p_a);
            }
            *(char *) v_addr = 0xbb;
        }
        vaddr_page += PG_SIZE;
        page_idx++;
    }
    sys_lseek(fd, offset, SEEK_SET);
    sys_read(fd, (void *) virtaddr, size);
    return true;
}

static int_32 load_elf_file(const char *pathname)
{
    /* test_inside(0x8084000); */
    /* load_code(3, 0x8084000, 0x1000, 0xc); */
    int_32 ret = -1;
    int_32 fd = sys_open(pathname, O_RDONLY);
    if (fd == -1) {
        ret = -1;
        return ret;
    }
    // 1. read elf header
    Elf32_Off phoff;
    Elf32_Half phsz;
    Elf32_Half phnum;


    // it seem that malloc memory first
    // the a bug happens
    /* uint_32 v_a = 0x1003000; */
    /* uint_32 p_a = 0x40c5000; */
    /* uint_32 v_addr = malloc_a_user_page(v_a, p_a); */
    /* *(char *) v_addr = 0xaa; */
    /* uint_8 *v_buf = sys_malloc(sizeof(Elf32_Ehdr)); */
    /* load_code(1, 0x8048000, 0x1000, 12); */


    // 0x1000000
    uint_8 *buf = sys_malloc(sizeof(Elf32_Ehdr));
    if (buf == NULL) {
        ret = -1;
        return ret;
    }
    sys_read(fd, buf, sizeof(Elf32_Ehdr));
    Elf32_Ehdr *elf_header = (Elf32_Ehdr *) buf;
    if (elf_header->e_ident[0] == ELFMAG0 &&
        elf_header->e_ident[1] == ELFMAG1 &&
        elf_header->e_ident[2] == ELFMAG2 &&
        elf_header->e_ident[3] == ELFMAG3) {
        phoff = elf_header->e_phoff;     // offset of file
        phsz = elf_header->e_phentsize;  // program header entry size
        phnum = elf_header->e_phnum;     // program header number
        // read program header

        // 0x1001000
        Elf32_Phdr *ph_buf = sys_malloc(phsz);
        for (uint_32 idx = 0; idx < phnum; idx++) {
            memset(ph_buf, 0, phsz);
            sys_lseek(fd, phoff, SEEK_SET);

            // 0x1003000
            if (sys_read(fd, ph_buf, phsz) != phsz) {
                ret = -1;
                goto done;
            }
            if (PT_LOAD == ph_buf->p_type) {
                if (!load_code(fd, ph_buf->p_vaddr, ph_buf->p_offset,
                               ph_buf->p_filesz)) {
                    ret = -1;
                    goto done;
                }
            }
            phoff += phsz;
        }

        sys_free(ph_buf);

    } else {
        ret = -1;
        return ret;
    }
    ret = elf_header->e_entry;
    goto done;
done:
    sys_close(fd);
    sys_free(buf);
    return ret;
}

// Get a free 4k phy memory aka 1 page in pool
// -> pte
static void *get_free_page(struct pool *mpool)
{
    int_32 start_pos = -1;
    start_pos = find_block_bitmap(&mpool->pool_bitmap, 1);
    if (start_pos == -1) {
        return NULL;
    }
    set_value_bitmap(&mpool->pool_bitmap, start_pos, 1);
    return (void *) (start_pos * PG_SIZE + mpool->phy_addr_start);
}

// Picking a pool return a free v_address
static void *get_free_vaddress(pool_type poolt, uint_32 pg_cnt)
{
    uint_32 v_start_addr = 0;
    uint_32 start_pos = -1;
    TCB_t *cur = running_thread();
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
    } else if (poolt == MP_USER) {
        start_pos =
            find_block_bitmap(&cur->progress_vaddr.vaddr_bitmap, pg_cnt);
        if (start_pos == -1) {
            return NULL;
        }
        for (int i = 0; i < pg_cnt; i++) {
            set_value_bitmap(&cur->progress_vaddr.vaddr_bitmap, start_pos + i,
                             1);
        }
        v_start_addr = start_pos * PG_SIZE + cur->progress_vaddr.vaddr_start;
        return (void *) v_start_addr;
    } else {
        PANIC(
            "get_free_vaddress: not allow kernel alloc userspace or user alloc "
            "kernel space.");
        return NULL;
    }
}


static void *malloc_page(enum mem_pool_type poolt, uint_32 pg_cnt)
{
    ASSERT(pg_cnt > 0);
    void *vaddr_start = get_free_vaddress(poolt, pg_cnt);
    if (vaddr_start == NULL) {
        return NULL;
    }
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

void sys_testsyscall(int a)
{
    /* char *test = sys_malloc(512); */
    /* sys_free(test); */
    /* testoutside("/ls"); */
    load_elf_file("/ls");
    TCB_t *cur = running_thread();

    int_32 start_pos = -1;
    struct bitmap *bmap = &user_pool.pool_bitmap;
    /* start_pos = find_block_bitmap(&user_pool.pool_bitmap, 1); */
    /* int_32 bit_pos = start_pos; */
    /* int_32 value = 1; */
    /* set_value_bitmap(bmap, start_pos, 1); */

    /* uint_32 v1= malloc_page(MP_USER, 1); */
    /* uint_32 v2= malloc_page(MP_USER, 1); */
    /* uint_32 v1 = sys_malloc(512); */
    /* uint_32 v2 = sys_malloc(23); */

    malloc_page_with_vaddr(MP_USER, 0x08084000);
    /* uint_32 v_addr = malloc_a_user_page(&user_pool); */
    /* uint_32 v_addr = malloc_page(MP_USER, 1); */
    uint_32 v3 = sys_malloc(512);
    *(char *) v3 = 0xff;

    /* *(char *) v_addr = 0xff; */
    int x = 1;
    x++;

    /* struct bitmap *v_bitmap = &cur->progress_vaddr.vaddr_bitmap; */
    /* uint_32 v_pos = find_block_bitmap( v_bitmap,1); */
    /* set_value_bitmap(v_bitmap, v_pos, 1); */
    /* uint_32 v_addr = cur->progress_vaddr.vaddr_start + v_pos*4096; */
    /*  */
    /* uint_32 p_pos = find_block_bitmap(&user_pool.pool_bitmap, 1); */
    /* set_value_bitmap(bmap, p_pos, 1); */
    /* uint_32 p_addr = user_pool.phy_addr_start+p_pos *4096; */
    /* put_page(v_addr, p_addr); */
    /* *(char*)v_addr = 0xff; */

    return;
}

void syscall_init(void)
{
    // Mono-kernel way
    syscall_table[SYS_GETPID] = sys_getpid;
    syscall_table[SYS_MALLOC] = sys_malloc;
    syscall_table[SYS_FREE] = sys_free;

    syscall_table[SYS_FORK] = sys_fork;
    syscall_table[SYS_EXECV] = sys_execv;

    syscall_table[SYS_OPEN] = sys_open;
    syscall_table[SYS_CLOSE] = sys_close;
    syscall_table[SYS_READ] = sys_read;
    syscall_table[SYS_WRITE] = sys_write;
    syscall_table[SYS_SEEK] = sys_lseek;
    syscall_table[SYS_UNLINK] = sys_unlink;
    syscall_table[SYS_MKDIR] = sys_mkdir;
    syscall_table[SYS_OPENDIR] = sys_opendir;
    syscall_table[SYS_CLOSEDIR] = sys_closedir;
    syscall_table[SYS_READDIR] = sys_readdir;
    syscall_table[SYS_REWINDDIR] = sys_rewinddir;
    syscall_table[SYS_RMDIR] = sys_rmdir;

    syscall_table[SYS_GETCWD] = sys_getcwd;
    syscall_table[SYS_CHDIR] = sys_chdir;
    syscall_table[SYS_STAT] = sys_stat;

    syscall_table[SYS_PUTC] = sys_putc;

    // mico-kernel way
    syscall_table[SYS_SENDREC] = sys_sendrec;
    // for test
    syscall_table[SYS_TESTSYSCALL] = sys_testsyscall;
}
