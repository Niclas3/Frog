#include <fs/select.h>
#include <fs/file.h>
#include <sys/poll.h>
#include <sys/threads.h>
#include <sys/sched.h>
#include <sys/fork.h>

uint_32 sys_wait2(int n, int_32 *fds, struct timeval *tvp){
    TCB_t *cur = running_thread();

    cur->status = THREAD_TASK_WAITING;
    schedule_timeout(299);

    return 0;
}
