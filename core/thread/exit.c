#include <debug.h>
#include <fs/file.h>
#include <fs/fs.h>
#include <fs/pipe.h>
#include <math.h>
#include <sys/exit.h>
#include <sys/threads.h>

extern struct list_head thread_all_list;
extern struct file g_file_table[MAX_FILE_OPEN];

static void release_proc_resource(TCB_t *thread)
{
    // free kernel symbol link page
    uint_32 *pgdir = thread->pgdir;
    uint_16 user_pde_nr = 768;
    uint_16 pde_idx = 0;
    uint_32 pde = 0;
    uint_32 *pde_p = NULL;

    uint_16 user_pte_nr = 1024;
    uint_16 pte_idx = 0;
    uint_32 pte = 0;
    uint_32 *pte_p = NULL;

    uint_32 *first_pte_vaddr_in_pde = NULL;
    uint_32 pg_phy_addr = 0;
    while (pde_idx < user_pde_nr) {
        pde_p = (uint_32) pgdir + pde_idx;
        pde = *pde_p;
        if (pde & 0x00000001) {
            // one page table has 4M size
            first_pte_vaddr_in_pde = pte_ptr(pde_idx * 0x400000);
            pte_idx = 0;
            while (pte_idx < user_pte_nr) {
                pte_p = first_pte_vaddr_in_pde + pte_idx;
                pte = *pte_p;
                if (pte & 0x00000001) {
                    pg_phy_addr = pte & 0xfffff000;
                    free_phy_page(pg_phy_addr);
                }
                pte_idx++;
            }
            pg_phy_addr = pde & 0xfffff000;
            free_phy_page(pg_phy_addr);
        }
        pde_idx++;
    }

    // free bitmap page
    uint_32 bitmap_len = thread->progress_vaddr.vaddr_bitmap.map_bytes_length;
    uint_32 bitmap_pg_cnt = DIV_ROUND_UP(bitmap_len, PG_SIZE);
    uint_8 *u_vaddr_pool_bm = thread->progress_vaddr.vaddr_bitmap.bits;
    mfree_page(MP_KERNEL, u_vaddr_pool_bm, bitmap_pg_cnt);

    // close file descriptor
    for (int fd_idx = 0; fd_idx < MAX_FILES_OPEN_PER_PROC; fd_idx++) {
        if (thread->fd_table[fd_idx] != -1) {
            if (is_pipe(fd_idx)) {
                uint_32 global_fd = fd_local2global(fd_idx);
                if (--g_file_table[global_fd].fd_pos == 0) {
                    // release pipe
                    mfree_page(MP_KERNEL, g_file_table[fd_idx].fd_inode, 1);
                    g_file_table[global_fd].fd_inode = NULL;
                }
            } else {
                sys_close(fd_idx);
            }
        }
    }
}

// list_walk() callback
static bool find_child(struct list_head *ele, pid_t ppid)
{
    TCB_t *cur = container_of(ele, TCB_t, all_list_tag);
    if (cur->parent_pid == ppid) {
        return true;
    }
    return false;
}

// list_walk() callback
static bool find_hanging_child(struct list_head *ele, pid_t ppid)
{
    TCB_t *cur = container_of(ele, TCB_t, all_list_tag);
    if (cur->parent_pid == ppid && cur->status == THREAD_TASK_HANGING) {
        return true;
    }
    return false;
}

// list_walk() callback
static bool proc_init_adopt_a_child(struct list_head *ele, pid_t pid)
{
    TCB_t *cur = container_of(ele, TCB_t, all_list_tag);
    if (cur->parent_pid == pid) {
        cur->parent_pid = 1;
    }
    return false;
}


pid_t sys_wait(int_32 *status_loc)
{
    TCB_t *parent = running_thread();
    while (1) {
        struct list_head *child_node =
            list_walker(&thread_all_list, find_hanging_child, parent->pid);
        if (child_node != NULL) {
            TCB_t *child = container_of(child_node, TCB_t, all_list_tag);
            *status_loc = child->exit_status;

            pid_t child_pid = child->pid;

            thread_exit(child, false);
            return child_pid;
        }
        child_node = list_walker(&thread_all_list, find_child, parent->pid);
        if (child_node == NULL) {
            return -1;
        } else {
            thread_block(THREAD_TASK_WAITING);
        }
    }
}

void sys_exit(int_32 status)
{
    TCB_t *child = running_thread();
    child->exit_status = status;
    if (child->parent_pid == -1) {
        PANIC("sys_exit: child parent is -1\n");
    }
    list_walker(&thread_all_list, proc_init_adopt_a_child, child->pid);

    release_proc_resource(child);

    TCB_t *parent = pid2thread(child->parent_pid);
    if (parent->status == THREAD_TASK_WAITING) {
        thread_unblock(parent);
    }
    thread_block(THREAD_TASK_HANGING);
}
