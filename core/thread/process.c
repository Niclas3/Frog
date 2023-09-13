#include <const.h>
#include <string.h>
#include <sys/descriptor.h>
#include <sys/int.h>
#include <sys/process.h>
#include <sys/threads.h>
#include <sys/tss.h>

#include <debug.h>
extern struct list_head thread_ready_list;
extern struct list_head thread_all_list;

#define USER_VADDR_START 0x8048000

#define CELLING(X, STEP) ((X + STEP - 1) / (STEP))


// Use this function jmp code from ring0 to ring3
extern void intr_exit(void);

void start_process(void *filename)
{
    void *function = filename;
    TCB_t *cur = running_thread();
    cur->self_kstack += sizeof(struct thread_stack);
    struct context_registers *proc_stack =
        (struct context_registers *) cur->self_kstack;
    proc_stack->edi = proc_stack->esi = proc_stack->ebp = proc_stack->esp = 0;
    proc_stack->eax = proc_stack->ebx = proc_stack->ecx = proc_stack->edx = 0;
    proc_stack->gs = 0;
    proc_stack->ds = proc_stack->es = proc_stack->fs =
        CREATE_SELECTOR(SEL_IDX_DATA_DPL_3, TI_GDT, RPL3);
    proc_stack->cs = CREATE_SELECTOR(SEL_IDX_CODE_DPL_3, TI_GDT, RPL3);
    proc_stack->eip = function;
    proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_IF_1 | EFLAGS_RESERVED);
    proc_stack->esp_ptr =
        (void *) ((uint_32) malloc_page_with_vaddr(MP_USER, USER_STACK3_VADDR) +
                  PG_SIZE);
    proc_stack->ss = CREATE_SELECTOR(SEL_IDX_DATA_DPL_3, TI_GDT, RPL3);
    __asm__ volatile("movl %0, %%esp; jmp intr_exit" ::"g"(proc_stack)
                     : "memory");
}

void page_dir_activate(TCB_t *thread)
{
    uint_32 pagedir_phy_addr = 0x100000;  // default pagedir address is 0x0
    if (thread->pgdir != NULL) {
/* __asm__ volatile ("xchgw %bx, %bx;"); */
        pagedir_phy_addr = addr_v2p((uint_32) thread->pgdir);
    }
    __asm__ volatile("movl %0, %%cr3;" : : "r"(pagedir_phy_addr) : "memory");
}

void process_activate(TCB_t *thread)
{
    ASSERT(thread != NULL);

    page_dir_activate(thread);
    if (thread->pgdir) {
        update_tss_esp0(thread);
    }
}

uint_32 *create_page_dir(void)
{
    uint_32 *page_dir_vaddr = get_kernel_page(1);
    if (page_dir_vaddr == NULL) {
        return NULL;
    }
    // 1024 = 4 * 256;
    memcpy((uint_32 *) ((uint_32) page_dir_vaddr + 0x300 * 4),
           (uint_32 *) (0xfffff000 + 0x300 * 4), 1024);
    uint_32 new_page_dir_phy_addr = addr_v2p((uint_32) page_dir_vaddr);
    // Add last pde to pd phy_addr
    page_dir_vaddr[1023] = new_page_dir_phy_addr | PG_US_U | PG_RW_W | PG_P_SET;
    return page_dir_vaddr;
}

void create_user_vaddr_bitmap(TCB_t *user_prog)
{
    user_prog->progress_vaddr.vaddr_start = USER_VADDR_START;
    uint_32 bitmap_pg_cnt =
        CELLING((0xc0000000 - USER_VADDR_START) / PG_SIZE / 8, PG_SIZE);
    user_prog->progress_vaddr.vaddr_bitmap.bits =
        get_kernel_page(bitmap_pg_cnt);
    user_prog->progress_vaddr.vaddr_bitmap.map_bytes_length =
        (0xc0000000 - USER_VADDR_START) / PG_SIZE / 8;
    init_bitmap(&user_prog->progress_vaddr.vaddr_bitmap);
}

void process_execute(void *filename, char *name)
{
    TCB_t *thread = get_kernel_page(1);
    init_thread(thread, name, default_priority);
    create_user_vaddr_bitmap(thread);
    create_thread(thread, start_process, filename);
    thread->pgdir = create_page_dir();

    enum intr_status old_status = intr_disable();
    ASSERT(!list_find_element(&thread->general_tag, &thread_ready_list));
    list_add_tail(&thread->general_tag, &thread_ready_list);

    ASSERT(!list_find_element(&thread->all_list_tag, &thread_all_list));
    list_add_tail(&thread->all_list_tag, &thread_all_list);
    intr_set_status(old_status);
}
