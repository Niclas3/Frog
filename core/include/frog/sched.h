#ifndef _FROG_SCHED_H_
#define _FROG_SCHED_H_
#include <frog/types.h>
#include <frog/kernel.h>
#include <frog/wait.h>

#define	MAX_SCHEDULE_TIMEOUT	LONG_MAX

// Status of thread
typedef enum task_status {
    THREAD_TASK_RUNNING = 0,
    THREAD_TASK_READY   = 1,
    THREAD_TASK_WAITING = 2,
    THREAD_TASK_HANGING = 4,
    THREAD_TASK_BLOCKED = 8,
    THREAD_TASK_DIED = 16,
} task_status_t;

void __wake_up(wait_queue_head_t *q, unsigned int mode, int nr);
void __wake_up_all(wait_queue_head_t *q, unsigned int mode);

#define wake_up(x)			__wake_up((x), THREAD_TASK_WAITING | THREAD_TASK_BLOCKED, 1)
#define wake_up_interruptible(x)	__wake_up((x), THREAD_TASK_WAITING, 1)
#define wake_up_interruptible_all(x) \
        __wake_up_all((x), THREAD_TASK_WAITING)

void init_timer_manager(void);
/* int 0x20;
 * Interrupt handler for inner Clock
 **/
void inthandler20(void);

int_32 schedule_timeout(int_32 timeout);

void mtime_sleep(uint_32 m_seconds);

#endif
