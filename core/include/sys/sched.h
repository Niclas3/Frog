#ifndef __SYS_SCHED_H_
#define __SYS_SCHED_H_
#include <ostype.h>

void init_timer_manager(void);
/* int 0x20;
 * Interrupt handler for inner Clock
 **/
void inthandler20(void);

void mtime_sleep(uint_32 m_seconds);
void set_timer(uint_32 timeout, void* queue, uint_8 mark);

#endif
