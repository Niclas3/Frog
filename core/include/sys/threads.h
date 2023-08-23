#ifndef __SYS_THREAD_H
#define __SYS_THREAD_H

#include <ostype.h>

//Routine type 
typedef void* (*__routine_ptr_t)(void*);
typedef void* __routine_t (void*);

//Status of thread
typedef enum task_status {
    SYS_THREAD_TASK_CREATED=0,
    SYS_THREAD_TASK_RUNNING,
    SYS_THREAD_TASK_READY,
    SYS_THREAD_TASK_DONE,
    SYS_THREAD_TASK_WAITING,
    SYS_THREAD_TASK_HANDING,
    SYS_THREAD_TASK_CANCELLED,
    SYS_THREAD_TASK_REJECTED,
} task_status_t;

/* Save registers context
 * stack
 **/
struct context_registers{
    uint_32 vector_no; // number of interrupt
    uint_32 edi;
    uint_32 esi;
    uint_32 ebp;
    uint_32 esp;
    uint_32 ebx;
    uint_32 edx;
    uint_32 ecx;
    uint_32 eax;
    uint_32 gs;
    uint_32 fs;
    uint_32 es;
    uint_32 ds;
    //ring3->ring0
    uint_32 err_code;
    void (*eip)(void);
    uint_32 ss;
    uint_32 cs;
    uint_32 eflags;
    void* esp_ptr;
};

/* thread stack
 *
 * */
struct thread_stack {
    uint_32 ebp;
    uint_32 ebx;
    uint_32 edi;
    uint_32 esi;
    void (*eip) (__routine_ptr_t func, void* __arg);

    void (*unused_retaddr);
    __routine_ptr_t function; //function name
    void* func_arg;
};

typedef struct thread_control_block {
    uint_32 *self_kstack;
    task_status_t status;
    uint_32       priority;
    char name[16];
    uint_32 stack_magic; // mark the board of stack
} TCB_t;


TCB_t* thread_start(char* name, int priority, __routine_t func, void* arg);

#endif
