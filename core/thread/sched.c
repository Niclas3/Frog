#include <sys/sched.h>

#include <sys/pic.h>
#include <asm/bootpack.h>

#include <sys/threads.h>

#include <debug.h>

#include <sys/graphic.h>

/**
 * Total ticks count since open timer interrupt
 */
uint_32 ticks = 0;
extern void schedule(void);

/* int 0x20;
 * Interrupt handler for inner Clock
 **/
void inthandler20(void){
    TCB_t *cur_thread = running_thread();
    ASSERT(cur_thread->stack_magic == 0x19900921);
    cur_thread->elapsed_ticks++;
    ticks++;
    if(cur_thread->ticks == 0){
        schedule();
    }else{
        cur_thread->ticks--;
    }

    /* _io_out8(PIC0_OCW2, PIC_EOI_IRQ0); */
    return;
}

