#include <debug.h>
#include <math.h>
#include <sys/pic.h>
#include <sys/sched.h>
#include <sys/threads.h>

#define mil_seconds_per_intr DIV_ROUND_UP(1000, IRQ0_FREQUENCY)

/**
 * Total ticks count since open timer interrupt
 */
uint_32 ticks = 0;


extern void schedule(void);

/* int 0x20;
 * Interrupt handler for inner Clock
 **/
void inthandler20(void)
{
    TCB_t *cur_thread = running_thread();
    ASSERT(cur_thread->stack_magic == 0x19900921);
    cur_thread->elapsed_ticks++;
    ticks++;
    if (cur_thread->ticks == 0) {
        schedule();
    } else {
        cur_thread->ticks--;
    }

    return;
}

/*
 * Test this function entry timing (tick) with current ticks, if less than
 * target ticks number aka sleep_ticks, then yield current thread once.
 * */
static void tick_to_sleep(uint_32 sleep_ticks)
{
    uint_32 start_tick = ticks;
    while (ticks - start_tick < sleep_ticks) {
        thread_yield();
    }
}

void mtime_sleep(uint_32 m_seconds)
{
    ASSERT(mil_seconds_per_intr != 0);
    uint_32 sleep_ticks = DIV_ROUND_UP(m_seconds, mil_seconds_per_intr);
    ASSERT(sleep_ticks > 0);
    tick_to_sleep(sleep_ticks);
}
