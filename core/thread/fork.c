#include <bitmap.h>
#include <debug.h>
#include <fs/file.h>
#include <fs/inode.h>
#include <fs/pipe.h>
#include <math.h>
#include <string.h>
#include <sys/fork.h>
#include <sys/int.h>
#include <sys/memory.h>
#include <sys/process.h>
#include <sys/threads.h>

extern void intr_exit(void);
extern struct file g_file_table[MAX_FILE_OPEN];

extern struct list_head thread_ready_list;
extern struct list_head thread_all_list;
/* extern struct list_head process_all_list; */

static int_32 copy_tcb_vaddrbitmap_stack0(TCB_t *child_thread,
                                          TCB_t *parent_thread)
{
    memcpy(child_thread, parent_thread, PG_SIZE);
    child_thread->pid = fork_pid();
    child_thread->elapsed_ticks = 0;
    child_thread->status = THREAD_TASK_READY;
    child_thread->ticks = child_thread->priority;
    child_thread->parent_pid = parent_thread->pid;
    child_thread->general_tag.next = NULL;
    child_thread->general_tag.prev = NULL;
    child_thread->all_list_tag.next = NULL;
    child_thread->all_list_tag.prev = NULL;
    block_desc_init(child_thread->u_block_descs);

    /* uint_32 bitmap_pg_cnt = */
    /*     DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PG_SIZE / 8, PG_SIZE);
     */
    uint_32 bitmap_len =
        DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PG_SIZE, 8);
    uint_32 bitmap_pg_cnt = DIV_ROUND_UP(bitmap_len, PG_SIZE);
    void *vaddr_btmp = get_kernel_page(bitmap_pg_cnt);
    if (vaddr_btmp == NULL) {
        return -1;
    }
    memcpy(vaddr_btmp, child_thread->progress_vaddr.vaddr_bitmap.bits,
           bitmap_pg_cnt * PG_SIZE);
    child_thread->progress_vaddr.vaddr_bitmap.bits = vaddr_btmp;

    ASSERT(strlen(child_thread->name) < 11);
    strcat(child_thread->name, "_fork");
    return 0;
}


// copy full code , data and
static void copy_body_stack3(TCB_t *child_thread,
                             TCB_t *parent_thread,
                             void *buf_page)
{
    uint_8 *vaddr_btmp = parent_thread->progress_vaddr.vaddr_bitmap.bits;
    uint_32 btmp_bytes_len =
        parent_thread->progress_vaddr.vaddr_bitmap.map_bytes_length;
    uint_32 vaddr_start = parent_thread->progress_vaddr.vaddr_start;
    uint_32 idx_byte = 0;
    uint_32 idx_bit = 0;
    uint_32 prog_vaddr = 0;
    while (idx_byte < btmp_bytes_len) {
        if (vaddr_btmp[idx_byte]) {
            idx_bit = 0;
            while (idx_bit < 8) {
                if ((FULL_MASK << idx_bit) & vaddr_btmp[idx_byte]) {
                    prog_vaddr =
                        (idx_byte * 8 + idx_bit) * PG_SIZE + vaddr_start;
                    // copy to kernel memory first
                    memcpy(buf_page, (void *) prog_vaddr, PG_SIZE);
                    page_dir_activate(child_thread);
                    get_phy_free_page_with_vaddr(MP_USER, prog_vaddr, child_thread->pgdir);
                    memcpy((void *) prog_vaddr, buf_page, PG_SIZE);
                    page_dir_activate(parent_thread);
                }
                idx_bit++;
            }
        }
        idx_byte++;
    }
}

static int_32 build_child_stack(TCB_t *child_thread, TCB_t *parent_thread)
{
    struct context_registers *intr_0_stack =
        (struct context_registers *) ((uint_32) child_thread + PG_SIZE -
                                      sizeof(struct context_registers));

    // 1. change child thread return value
    intr_0_stack->eax = 0;
    // 2. Construct thread_stack
    // in switch.s
    /* ;Now stack top is ret address */
    /* push esi */
    /* push edi */
    /* push ebx */
    /* push ebp */

    uint_32 *ret_addr_in_thread_stack = (uint_32 *) intr_0_stack - 1;
    uint_32 *esi_addr = (uint_32 *) intr_0_stack - 2;
    uint_32 *edi_addr = (uint_32 *) intr_0_stack - 3;
    uint_32 *ebx_addr = (uint_32 *) intr_0_stack - 4;
    uint_32 *ebp_ptr_in_thread_stack = (uint_32 *) intr_0_stack - 5;
    *ret_addr_in_thread_stack = (uint_32) intr_exit;
    child_thread->self_kstack = ebp_ptr_in_thread_stack;

    /* struct thread_stack *t_stack = */
    /*     (struct thread_stack *) ((uint_32) child_thread + PG_SIZE - */
    /*                              sizeof(struct context_registers) - */
    /*                              sizeof(struct thread_stack)); */

    return 0;
}

static void update_inode_open_cnts(TCB_t *thread)
{
    int_32 local_fd = 3;
    int_32 global_fd = 0;
    while (local_fd < MAX_FILES_OPEN_PER_PROC) {
        global_fd = thread->fd_table[local_fd];
        ASSERT(global_fd < MAX_FILE_OPEN);
        if (global_fd != -1) {
            if (is_pipe(local_fd)) {
                g_file_table[global_fd].fd_pos++;
            } else {
                g_file_table[global_fd].fd_inode->i_count++;
            }
        }
        local_fd++;
    }
}


/**
 * Copy parent thread to child thread
 *****************************************************************************/
static uint_32 copy_process(TCB_t *child_thread, TCB_t *parent_thread)
{
    // 1. buffer at kernel memory
    void *buf_page = get_kernel_page(1);
    if (buf_page == NULL) {
        return -1;
    }
    // 2. copy tcb vaddr bit map and stack0 from parent thread
    if (copy_tcb_vaddrbitmap_stack0(child_thread, parent_thread) == -1) {
        return -1;
    }
    // 3.create page table
    child_thread->pgdir = create_page_dir();
    if (child_thread->pgdir == NULL) {
        return -1;
    }
    // 4. copy parent code and data
    copy_body_stack3(child_thread, parent_thread, buf_page);


    // 5. create new child thread
    build_child_stack(child_thread, parent_thread);
    update_inode_open_cnts(child_thread);
    mfree_page(MP_KERNEL, buf_page, 1);
    return 0;
}

uint_32 sys_fork(void)
{
    TCB_t *parent_thread = running_thread();
    TCB_t *child_thread = get_kernel_page(1);

    if (child_thread == NULL) {
        return -1;
    }
    ASSERT((INTR_OFF == intr_get_status()) && parent_thread->pgdir != NULL);
    if (copy_process(child_thread, parent_thread) == -1) {
        return -1;
    }


    ASSERT(!list_find_element(&child_thread->all_list_tag, &thread_all_list));
    list_add_tail(&child_thread->all_list_tag, &thread_all_list);

    ASSERT(!list_find_element(&child_thread->general_tag, &thread_ready_list));
    list_add_tail(&child_thread->general_tag, &thread_ready_list);

    return child_thread->pid;
}


void add_wait_queue(wait_queue_head_t *q, wait_queue_t * wait)
{
    //TODO:
    //Don't think about lock at this time
	/* unsigned long flags; */
        /*  */
	/* wait->flags &= ~WQ_FLAG_EXCLUSIVE; */
	/* wq_write_lock_irqsave(&q->lock, flags); */
        enum intr_status old_status = intr_disable();
	__add_wait_queue(q, wait);
        intr_set_status(old_status);
	/* wq_write_unlock_irqrestore(&q->lock, flags); */
}

void remove_wait_queue(wait_queue_head_t *q, wait_queue_t * wait)
{
    //TODO:
    //Don't think about lock at this time
	/* unsigned long flags; */
        /*  */
	/* wait->flags &= ~WQ_FLAG_EXCLUSIVE; */
	/* wq_write_lock_irqsave(&q->lock, flags); */
        enum intr_status old_status = intr_disable();
        __remove_wait_queue(q, wait);
        intr_set_status(old_status);
	/* wq_write_unlock_irqrestore(&q->lock, flags); */
}
