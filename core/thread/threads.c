#include <const.h>  // for PG_SIZE
#include <stdbool.h>
#include <string.h>
#include <sys/int.h>
#include <sys/memory.h>
#include <sys/process.h>
#include <sys/threads.h>

#include <debug.h>

TCB_t *main_thread;  // kernel_thread() tid = 1
TCB_t *idle_thread;  // idle()          tid = 2

struct lock tid_lock;
struct lock pid_lock;
// TODO: need a max list size
struct list_head thread_ready_list;
struct list_head thread_all_list;
struct list_head process_all_list;
static struct list_head *thread_tag;


static tid_t allocate_tid(void);

extern void switch_to(TCB_t *cur, TCB_t *next);

// a thread when os is idle, block itself
static void idle(void *arg)
{
    while (1) {
        thread_block(THREAD_TASK_BLOCKED);
        __asm__ volatile("sti; hlt" : : : "memory");
    }
}
static void kernel_thread(__routine_ptr_t func_ptr, void *func_arg)
{
    // Looking for threads' status
    // if A thread is finished then checking other threads at a thread-pool
    // if all threads in pool are handled do while loop at this kernel main
    // thread Maybe some time cli for block some thread so we sti for open timer
    // interrupt
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
    uint_32 esp;
    __asm__ volatile("mov %%esp, %0" : "=g"(esp));
    return (TCB_t *) (esp & 0xfffff000);
}

/* Init TCB
 */
void init_thread(TCB_t *thread, char *name, uint_8 priority)
{
    // set all 0 for thread memory
    memset(thread, 0, sizeof(*thread));
    thread->tid = allocate_tid();
    strcpy(thread->name, name);
    if (thread == main_thread) {
        thread->status = THREAD_TASK_RUNNING;
    } else {
        thread->status = THREAD_TASK_READY;
    }
    thread->self_kstack = (uint_32 *) ((uint_32) thread + PG_SIZE);
    thread->priority = priority;
    thread->ticks = priority;
    thread->elapsed_ticks = 0;
    thread->pgdir = NULL;

    thread->stack_magic = 0x19900921;
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

TCB_t *thread_start(char *name, int priority, __routine_t func, void *arg)
{
    TCB_t *thread = get_kernel_page(1);
    init_thread(thread, name, priority);
    create_thread(thread, func, arg);

    ASSERT(!list_find_element(&thread->general_tag, &thread_ready_list));
    list_add_tail(&thread->general_tag, &thread_ready_list);

    ASSERT(!list_find_element(&thread->all_list_tag, &thread_all_list));
    list_add_tail(&thread->all_list_tag, &thread_all_list);

    return thread;
}

static void make_main_thread(void)
{
    main_thread = running_thread();
    init_thread(main_thread, "main", 42);

    main_thread->pid = 1;
    ASSERT(!list_find_element(&main_thread->proc_list_tag, &process_all_list));
    list_add_tail(&main_thread->proc_list_tag, &process_all_list);

    ASSERT(!list_find_element(&main_thread->all_list_tag, &thread_all_list));
    list_add_tail(&main_thread->all_list_tag, &thread_all_list);
}

void schedule(void)
{
    TCB_t *cur = running_thread();
    if (cur->status == THREAD_TASK_RUNNING) {
        ASSERT(!list_find_element(&cur->general_tag, &thread_ready_list));
        list_add_tail(&cur->general_tag, &thread_ready_list);
        cur->ticks = cur->priority;
        cur->status = THREAD_TASK_READY;
    } else {
    }
    if (list_is_empty(&thread_ready_list)) {
        thread_unblock(idle_thread);
    }
    thread_tag = NULL;
    thread_tag = list_pop(&thread_ready_list);
    TCB_t *next = container_of(thread_tag, TCB_t, general_tag);
    next->status = THREAD_TASK_RUNNING;
    process_activate(next);
    switch_to(cur, next);
}


/* Init all things that thread need
 * before all things start
 * */
void thread_init(void)
{
    init_list_head(&thread_ready_list);
    init_list_head(&thread_all_list);
    init_list_head(&process_all_list);
    lock_init(&tid_lock);
    lock_init(&pid_lock);
    // main thread tid = 1
    make_main_thread();
    // idle thread tid = 2
    idle_thread = thread_start("idle", 10, idle, 0);
}

// Auth_block
// Block self and set self status to status
// If thread status is
/* THREAD_TASK_HANDING;
   THREAD_TASK_WAITING;
   THREAD_TASK_BLOCKED; */
// call this function.
// remove current thread from thread_ready_list
void thread_auth_block(TCB_t*task, task_status_t status)
{
    ASSERT((status == THREAD_TASK_HANDING) || (status == THREAD_TASK_WAITING) ||
           (status == THREAD_TASK_BLOCKED));
    ASSERT(task);
    enum intr_status old_int_status = intr_disable();
    TCB_t *cur = task;
    cur->status = status;
    // This blocked thread is already pop from thread_ready_list
    // Just call schedule() switch to next thread at thread_ready_list
    // Don't need delete current thread from thread_ready list
    schedule();
    intr_set_status(old_int_status);
}

// Block self and set self status to status
// If thread status is
/* THREAD_TASK_HANDING;
   THREAD_TASK_WAITING;
   THREAD_TASK_BLOCKED; */
// call this function.
// remove current thread from thread_ready_list
void thread_block(task_status_t status)
{
    ASSERT((status == THREAD_TASK_HANDING) || (status == THREAD_TASK_WAITING) ||
           (status == THREAD_TASK_BLOCKED));
    enum intr_status old_int_status = intr_disable();
    TCB_t *cur = running_thread();
    cur->status = status;
    // This blocked thread is already pop from thread_ready_list
    // Just call schedule() switch to next thread at thread_ready_list
    // Don't need delete current thread from thread_ready list
    schedule();
    intr_set_status(old_int_status);
}

// Unblock giving thread
// add thread to head of tread_ready_list
void thread_unblock(TCB_t *thread)
{
    ASSERT((thread->status == THREAD_TASK_HANDING) ||
           (thread->status == THREAD_TASK_WAITING) ||
           (thread->status == THREAD_TASK_BLOCKED));
    enum intr_status old_int_status = intr_disable();
    if (thread->status != THREAD_TASK_READY) {
        ASSERT(!list_find_element(&thread->general_tag, &thread_ready_list));
        if (list_find_element(&thread->general_tag, &thread_ready_list)) {
            PAINC("The blocked thread at ready list!!?");
        }
        list_add(&thread->general_tag, &thread_ready_list);
        thread->status = THREAD_TASK_READY;
    }
    intr_set_status(old_int_status);
}

// Yield self for other thread
void thread_yield(void)
{
    TCB_t *cur = running_thread();
    enum intr_status old_status = intr_disable();
    ASSERT(!list_find_element(&cur->general_tag, &thread_ready_list));
    list_add_tail(&cur->general_tag, &thread_ready_list);
    cur->status = THREAD_TASK_READY;
    schedule();
    intr_set_status(old_status);
}

// Allocating thread id for each thread
static tid_t allocate_tid(void)
{
    static tid_t next_tid = 0;
    lock_fetch(&tid_lock);
    next_tid++;
    lock_release(&tid_lock);
    return next_tid;
}

