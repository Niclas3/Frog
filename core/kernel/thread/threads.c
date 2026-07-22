#include <asm/page.h>  // for PAGE_SIZE
#include <frog/fork.h>
#include <frog/irqflags.h>
#include <frog/memory.h>
#include <frog/process.h>
#include <frog/semaphore.h>
#include <frog/string.h>
#include <frog/threads.h>
#include <frog/types.h>

#include <frog/compiler.h>
#include <kernel/assert.h>
#include <kernel/debug.h>
#include <kernel/panic.h>

struct pid_pool {
        struct bitmap pid_bm;
        pid_t pid_start;
        struct lock pid_lock;
};

TCB_t *main_thread;  // kernel_thread()
TCB_t *idle_thread;  // idle()

static struct lock tid_lock;
static struct lock pid_lock;

// TODO: need a max list size
struct list_head thread_ready_list;
struct list_head thread_all_list;
/* struct list_head process_all_list; */
struct pid_pool pid_pool;

// pid bitmap
// max pid is 1024
/* uint_8 pid_bitmap[128] = {0}; */
static uint_8 *pid_bitmap;

static struct list_head *thread_tag;


/* static tid_t allocate_tid(void); */
static pid_t allocate_pid(void);

extern void switch_to(TCB_t *cur, TCB_t *next);

// a thread when os is idle, block itself
static void idle(void *arg)
{
        while (1) {
                /* thread_block(THREAD_TASK_BLOCKED); */
                safe_halt();
        }
}

static void kernel_thread(__routine_ptr_t func_ptr, void *func_arg)
{
        // Looking for threads' status
        // if A thread is finished then checking other threads at a thread-pool
        // if all threads in pool are handled do while loop at this kernel main
        // thread Maybe some time cli for block some thread so we sti for open
        // timer interrupt
        __asm__ volatile("sti");
        func_ptr(func_arg);
        while (1)
                ;
}

/* Get current TCB/PCB
 * can only use at Ring0!!
 * checkout to kernel stack
 * */
TCB_t *running_thread(void)
{
        TCB_t *current;
        __asm__ volatile("andl %%esp, %0;" : "=r"(current) : "0"(~4095UL));
        return current;
}


static void init_pid_bitmap(uint_32 length)
{
        pid_pool.pid_start = 0;
        pid_pool.pid_bm.bits = pid_bitmap;
        pid_pool.pid_bm.map_bytes_length = length;
        lock_init(&pid_pool.pid_lock);
        init_bitmap(&pid_pool.pid_bm);
}

// Allocating process id for each process
// all threads under a process share same process id.
static pid_t allocate_pid(void)
{
        // beacuse main thread is the first process
        lock_fetch(&pid_pool.pid_lock);
        pid_t base = pid_pool.pid_start;
        uint_32 pos = find_block_bitmap(&pid_pool.pid_bm, 1);
        if (pos == (uint_32) -1) {
                lock_release(&pid_pool.pid_lock);
                return (pid_t) -1;
        }
        set_value_bitmap(&pid_pool.pid_bm, pos, 1);
        lock_release(&pid_pool.pid_lock);
        return base + (pid_t) pos;
}

static void release_pid(pid_t pid)
{
        lock_fetch(&pid_pool.pid_lock);
        ASSERT(pid >= pid_pool.pid_start);
        pid_t base = pid_pool.pid_start;
        uint_32 pos = (uint_32) (pid - base);
        set_value_bitmap(&pid_pool.pid_bm, pos, 0);
        lock_release(&pid_pool.pid_lock);
}

pid_t fork_pid(void)
{
        return allocate_pid();
}

void thread_release_pid(pid_t pid)
{
        if (pid != (pid_t) -1)
                release_pid(pid);
}

/* Init TCB
 */
int init_thread(TCB_t *thread, const char *name, uint_8 priority)
{
        if (thread == NULL || name == NULL || priority == 0)
                return -1;
        // Set all 0 for thread memory
        memset(thread, 0, sizeof(*thread));
        uint_8 fd_idx = 0;
        while (fd_idx < MAX_FILES_OPEN_PER_PROC) {
                // -1 represents available file description
                thread->fd_table[fd_idx] = -1;
                fd_idx++;
        }

        thread->pid = allocate_pid();
        if (thread->pid == (pid_t) -1)
                return -1;
        strncpy(thread->name, name, TASK_NAME_LEN - 1);
        thread->name[TASK_NAME_LEN - 1] = '\0';
        if (thread == main_thread) {
                thread->status = THREAD_TASK_RUNNING;
        } else {
                thread->status = THREAD_TASK_READY;
        }
        thread->self_kstack = (uint_32 *) ((uint_32) thread + PAGE_SIZE);
        thread->priority = priority;
        thread->ticks = priority;
        thread->elapsed_ticks = 0;
        thread->mm = NULL;
        thread->cwd_inode_nr =
            0;  // current working directory to root_dir default
        thread->parent_pid = -1;  // default parent_pid is -1 -> no parent pid

        INIT_LIST_HEAD(&thread->general_tag);
        INIT_LIST_HEAD(&thread->all_list_tag);
        INIT_LIST_HEAD(&thread->proc_list_tag);

        thread->stack_magic = 0x19900921;
        return 0;
}

/* Set context ready to execute
 */
void create_thread(TCB_t *thread, __routine_t func, void *arg)
{
        uint_32 context_reg_sz = sizeof(struct context_registers);
        uint_32 thread_stack_sz = sizeof(struct thread_stack);
        thread->self_kstack =
            (uint_32 *) ((uint_32) thread->self_kstack - context_reg_sz);
        thread->self_kstack =
            (uint_32 *) ((uint_32) thread->self_kstack - thread_stack_sz);

        struct thread_stack *kthread_stack =
            (struct thread_stack *) thread->self_kstack;
        kthread_stack->ebp = 0;
        kthread_stack->ebx = 0;
        kthread_stack->esi = 0;
        kthread_stack->edi = 0;
        kthread_stack->eip = kernel_thread;
        kthread_stack->function = func;
        kthread_stack->func_arg = arg;
}

int thread_publish(TCB_t *thread)
{
        unsigned long flags;

        if (thread == NULL || thread->status != THREAD_TASK_READY)
                return -1;
        local_irq_save(flags);
        if (task_on_readylist(thread) ||
            list_find_element(&thread->all_list_tag, &thread_all_list)) {
                local_irq_restore(flags);
                return -1;
        }
        list_add_tail(&thread->general_tag, &thread_ready_list);
        list_add_tail(&thread->all_list_tag, &thread_all_list);
        local_irq_restore(flags);
        return 0;
}

TCB_t *thread_start(const char *name, int priority, __routine_t func, void *arg)
{
        TCB_t *thread = get_kernel_page(1);  // alloc only 4096b aka 1 page for
                                             // struct thread
        if (thread == NULL)
                return NULL;
        if (init_thread(thread, name, priority) < 0) {
                free_page(MP_KERNEL, thread, 1);
                return NULL;
        }
        create_thread(thread, func, arg);

        if (thread_publish(thread) < 0) {
                release_pid(thread->pid);
                free_page(MP_KERNEL, thread, 1);
                return NULL;
        }

        return thread;
}

void make_main_thread(void)
{
        // 2 page size
        uintptr_t main_tcb = (K_STACK_START & ~0xFFFUL) - 0x1000UL;
        TCB_t *current = running_thread();
        uint_32 main_stack_pg_count = 1;
        memcpy((void *) main_tcb, current, PAGE_SIZE * main_stack_pg_count);
        main_thread = (TCB_t *) main_tcb;
        if (init_thread(main_thread, "main", 42) < 0)
                PANIC("cannot initialize main thread");

        /* ASSERT(!list_find_element(&main_thread->proc_list_tag,
         * &process_all_list)); */
        /* list_add_tail(&main_thread->proc_list_tag, &process_all_list); */

        ASSERT(
            !list_find_element(&main_thread->all_list_tag, &thread_all_list));
        list_add_tail(&main_thread->all_list_tag, &thread_all_list);

        uintptr_t esp;
        __asm__ volatile("movl %%esp, %0" : "=r"(esp) : :);
        uintptr_t new_esp = main_tcb | (esp & 0xFFFUL);

        uintptr_t ebp;
        __asm__ volatile("movl %%ebp, %0" : "=r"(ebp) : :);
        uintptr_t new_ebp = main_tcb | (ebp & 0xFFFUL);
        uintptr_t caller_ebp = *(uintptr_t *) ebp;

        ASSERT((caller_ebp & ~0xFFFUL) == (uintptr_t) current);
        *(uintptr_t *) new_ebp = main_tcb | (caller_ebp & 0xFFFUL);

        __asm__ volatile("movl %0, %%esp" : : "r"(new_esp) : "%esp");
        __asm__ volatile("movl %0, %%ebp" : : "r"(new_ebp) : "%esp");
}

static inline void append_readylist(TCB_t *cur)
{
        if (cur == idle_thread)
                return;
        if (cur->status == THREAD_TASK_RUNNING) {
                ASSERT(
                    !list_find_element(&cur->general_tag, &thread_ready_list));
                list_add_tail(&cur->general_tag, &thread_ready_list);
                cur->ticks = cur->priority;
                cur->status = THREAD_TASK_READY;
        }
}

void schedule(void)
{
        TCB_t *cur = running_thread();
        cur->need_schedule = false;
        if (list_is_empty(&thread_ready_list)) {
                if (cur->status == THREAD_TASK_RUNNING)
                        return;
                if (cur == idle_thread)
                        return;
                idle_thread->status = THREAD_TASK_RUNNING;
                process_activate(idle_thread);
                switch_to(cur, idle_thread);
        } else {
                thread_tag = list_pop(&thread_ready_list);
                TCB_t *next = container_of(thread_tag, TCB_t, general_tag);
                append_readylist(cur);

                if (next == cur) {
                        ASSERT(!task_on_readylist(next));
                        next->status = THREAD_TASK_RUNNING;
                        return;
                }
                if (cur == idle_thread)
                        cur->status = THREAD_TASK_BLOCKED;
                next->status = THREAD_TASK_RUNNING;
                process_activate(next);
                switch_to(cur, next);
        }
}



// Auth_block
// Block self and set self status to status
// If thread status is
/* THREAD_TASK_HANDING;
   THREAD_TASK_WAITING;
   THREAD_TASK_BLOCKED; */
// call this function.
// remove current thread from thread_ready_list
void thread_auth_block(TCB_t *task, task_status_t status)
{
        ASSERT((status == THREAD_TASK_HANGING) ||
               (status == THREAD_TASK_WAITING) ||
               (status == THREAD_TASK_BLOCKED));
        ASSERT(task);
        ASSERT(task == running_thread());
        unsigned long flags;
        local_irq_save(flags);
        task->status = status;
        // This blocked thread is already pop from thread_ready_list
        // Just call schedule() switch to next thread at thread_ready_list
        // Don't need delete current thread from thread_ready list
        schedule();
        local_irq_restore(flags);
}

// Block self and set self status to status
// If thread status is
/* THREAD_TASK_HANGING;
   THREAD_TASK_WAITING;
   THREAD_TASK_BLOCKED; */
// call this function.
// remove current thread from thread_ready_list
void thread_block(task_status_t status)
{
        ASSERT((status == THREAD_TASK_HANGING) ||
               (status == THREAD_TASK_WAITING) ||
               (status == THREAD_TASK_BLOCKED));
        unsigned long flags;
        local_irq_save(flags);
        TCB_t *cur = running_thread();
        cur->status = status;
        // This blocked thread is already pop from thread_ready_list
        // Just call schedule() switch to next thread at thread_ready_list
        // Don't need delete current thread from thread_ready list
        schedule();
        local_irq_restore(flags);
}

// Unblock giving thread
// add thread to head of tread_ready_list
void thread_unblock(TCB_t *thread)
{
        /* DEBUG("%s: %d", thread->name, thread->status); */
        ASSERT((thread->status == THREAD_TASK_HANGING) ||
               (thread->status == THREAD_TASK_WAITING) ||
               (thread->status == THREAD_TASK_BLOCKED));
        unsigned long flags;
        local_irq_save(flags);
        if (thread->status != THREAD_TASK_READY) {
                ASSERT(!list_find_element(&thread->general_tag,
                                          &thread_ready_list));
                if (list_find_element(&thread->general_tag,
                                      &thread_ready_list)) {
                        PANIC("The blocked thread at ready list!!?");
                }
                list_add(&thread->general_tag, &thread_ready_list);
                thread->status = THREAD_TASK_READY;
        }
        local_irq_restore(flags);
}

// Yield self for other thread
void thread_yield(void)
{
        TCB_t *cur = running_thread();
        unsigned long flags;
        local_irq_save(flags);
        ASSERT(!list_find_element(&cur->general_tag, &thread_ready_list));
        list_add_tail(&cur->general_tag, &thread_ready_list);
        cur->status = THREAD_TASK_READY;
        schedule();
        local_irq_restore(flags);
}

void thread_exit(TCB_t *discard_thread, bool need_schedule)
{
        unsigned long flags;
        local_irq_save(flags);
        pid_t discard_pid = discard_thread->pid;
        discard_thread->status = THREAD_TASK_DIED;
        struct list_head *discard_node = &discard_thread->general_tag;
        if (list_find_element(discard_node, &thread_ready_list)) {
                list_del_init(discard_node);
        }
        // remove from all_thread_list
        list_del_init(&discard_thread->all_list_tag);
        release_pid(discard_pid);

        if (discard_thread != running_thread() && discard_thread != main_thread) {
                free_page(MP_KERNEL, discard_thread, 1);
        }

        if (need_schedule) {
                schedule();
                PANIC("waiting for next!");
        }
        local_irq_restore(flags);
}

static bool find_pid(struct list_head *ele, pid_t pid)
{
        TCB_t *cur = container_of(ele, TCB_t, all_list_tag);
        if (cur->pid == pid) {
                return true;
        }
        return false;
}
TCB_t *pid2thread(pid_t pid)
{
        unsigned long flags;
        local_irq_save(flags);
        struct list_head *node = list_walker(&thread_all_list, find_pid, pid);
        if (node == NULL) {
                local_irq_restore(flags);
                return NULL;
        }
        TCB_t *thread = container_of(node, TCB_t, all_list_tag);
        local_irq_restore(flags);
        return thread;
}

// enter into idle mode
void cpu_idle(void)
{
        // block this boot_init create by bootloader.s
        /* thread_block(THREAD_TASK_BLOCKED); */
        /* idle((void *) 0); */
}

/* Init all things that thread need
 * before all things start
 * */
void thread_init(void)
{
        init_timer_manager();
        INIT_LIST_HEAD(&thread_ready_list);
        INIT_LIST_HEAD(&thread_all_list);
        /* INIT_LIST_HEAD(&process_all_list); */

        // alloc pid_bitmap space
        pid_bitmap = get_kernel_page(1);
        if (pid_bitmap == NULL) {
                PANIC("thread init error when alloc pid bitmap");
        }
        init_pid_bitmap(4096);
        // first kernel thread pid = 2
        /* make_main_thread();  // maybe main thread not start here */
        // idle thread pid = 0
        idle_thread = thread_start("idle", 10, idle, 0);
        if (idle_thread == NULL)
                PANIC("cannot initialize idle thread");
        list_del_init(&idle_thread->general_tag);
        idle_thread->status = THREAD_TASK_BLOCKED;
}
