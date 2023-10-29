#include <ipc.h>
#include <sys/syscall.h>
#include <sys/systask.h>

#include <sys/threads.h>

extern uint_32 ticks;
void task_sys(void)
{
    message msg;
    while (1) {
        sendrec(RECEIVE, ANY_TASK, &msg);
        int src = msg.m_source;

        switch (msg.m_type) {
        case GET_TICKS:
            msg.RETVAL = ticks;
            sendrec(SEND, src, &msg);
            break;
        case GET_PID: {
            msg.RETVAL = src;
            sendrec(SEND, src, &msg);
            break;
        }
        default:
            break;
        }
    }
}
