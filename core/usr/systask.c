#include <sys/systask.h>
#include <ipc.h>
#include <sys/syscall.h>

void task_sys(void){
    message msg;
    msg.m_u32.data[0] = 20;
    while(1){
        sendrec(RECEIVE, ANY_TASK, &msg);
        int src = msg.m_source;

        switch(msg.m_type){
            case GET_TICKS:
                msg.m_u16.data[0] = 100;
                sendrec(SEND, src, &msg);
                break;
            default:
                break;
        }
    }
}
