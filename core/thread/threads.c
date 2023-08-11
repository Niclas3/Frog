#include <sys/threads.h>
#include <string.h>
#include <const.h> // for PG_SIZE

#include <sys/memory.h>

static void kernel_thread(__routine_ptr_t func_ptr, void* func_arg){
    func_ptr(func_arg);
}

/* Init TCB 
 */
void init_thread(TCB_t* thread, char* name, uint_8 priority){
    memset(thread, 0, sizeof(*thread));
    strcpy(thread->name, name);
    thread->status = SYS_THREAD_TASK_RUNNING;
    thread->priority = priority;

    thread->self_kstack = (uint_32*)((uint_32) thread + PG_SIZE);
    /* thread->stack_magic = 0xFB06D7DD; */
    thread->stack_magic = 0x19900921;
}

/* Set context ready to execute
 */
void create_thread(TCB_t *thread, __routine_t func, void* arg){
    thread->self_kstack -= sizeof(struct context_registers);

    thread->self_kstack -= sizeof(struct thread_stack);
    struct thread_stack* kthread_stack = (struct thread_stack*)thread->self_kstack;
    kthread_stack->eip = kernel_thread ;
    kthread_stack->function = func;
    kthread_stack->func_arg = arg;
    kthread_stack->ebp = 0;
    kthread_stack->ebx = 0;
    kthread_stack->esi = 0;
    kthread_stack->edi = 0;
}

TCB_t* thread_start(char* name,
                   int priority,
                   __routine_t func,
                   void* arg){
    TCB_t *thread = get_kernel_page(1) ;
    init_thread(thread, name, priority);
    create_thread(thread, name, priority);

    __asm__ volatile ("movl %0, %%esp;\
                       pop %%ebp;\
                       pop %%ebx;\
                       pop %%edi;\
                       pop %%esi;\
                       ret"
                       :: "g" (thread->self_kstack):"memory");
    return thread;
}

