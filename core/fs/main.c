#include <sys/fstask.h>
#include <ipc.h>
#include <sys/syscall.h>

void task_fs(void){
    message msg;
    while(1){
        sendrec(RECEIVE, ANY_TASK, &msg);
        int src = msg.m_source;

        switch(msg.m_type){
            case OPEN_FILE:
                msg.m_u16.data[0] = 400;
                sendrec(SEND, src, &msg);
                break;
            default:
                break;
        }
    }
}
