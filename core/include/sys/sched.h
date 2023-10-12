#ifndef __SYS_SCHED_H_
#define __SYS_SCHED_H_
#include <ostype.h>
/* int 0x20;
 * Interrupt handler for inner Clock
 **/
void inthandler20(void);

void mtime_sleep(uint_32 m_seconds);

#endif
