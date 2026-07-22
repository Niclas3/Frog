#include <frog/exit.h>
#include <frog/fork.h>
#include <frog/syscall.h>
#include <frog/threads.h>
#include <frog/types.h>
#include <kernel/process_regression.h>
#include <kernel/qemu_test.h>

void process_regression_run_kernel(void)
{
#ifdef CONFIG_FROG_TEST_PROCESS
        TCB_t *current = running_thread();

        thread_yield();
        frog_test_case("scheduler.yield.single",
                       running_thread() == current &&
                       current->status == THREAD_TASK_RUNNING &&
                       !task_on_readylist(current));
#endif
}

void process_regression_run_user(void)
{
#ifdef CONFIG_FROG_TEST_PROCESS
        volatile int private_value = 7;
        pid_t child_pid = fork();

        if (child_pid == 0) {
                private_value = 19;
                exit(37);
                for (;;) {
                }
        }

        frog_test_case("process.fork.parent-result",
                       child_pid != -1 && child_pid != 0);
        if (child_pid == -1)
                return;

        int_32 status = -1;
        pid_t waited_pid = wait(&status);
        frog_test_case("process.wait.pid", waited_pid == child_pid);
        frog_test_case("process.wait.status", status == 37);
        frog_test_case("process.fork.address-space", private_value == 7);
        frog_test_case("process.wait.no-child",
                       wait(NULL) == (pid_t) -1);
#endif
}
