#include <frog/graphical_startup.h>

#ifdef FROG_DESKTOP_SMOKE_TEST
#include <frog/syscall.h>
#include <frog/test.h>

static void graphical_test_report(uint_32 id, bool passed)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(SYS_TEST_REPORT), "b"(id), "c"(passed)
                         : "memory");
        (void) result;
}
#endif

int graphical_init_supervise(const struct graphical_init_ops *ops)
{
        pid_t compositor_pid;
        pid_t desktop_pid;
        int_32 compositor_status = 0;
        int_32 desktop_status = 0;
        bool compositor_done = false;
        bool desktop_done = false;

        if (!ops || !ops->spawn || !ops->wait)
                return FROG_GRAPHICAL_EXIT_WAIT;

        compositor_pid = ops->spawn(GRAPHICAL_CHILD_COMPOSITOR,
                                    FROG_GRAPHICAL_COMPOSITOR_PATH);
#ifdef FROG_DESKTOP_SMOKE_TEST
        graphical_test_report(FROG_TEST_DESKTOP_INIT_COMPOSITOR_FORK,
                              compositor_pid >= 0);
#endif
        if (compositor_pid < 0)
                return FROG_GRAPHICAL_EXIT_COMPOSITOR_FORK;

        desktop_pid = ops->spawn(GRAPHICAL_CHILD_DESKTOP,
                                 FROG_GRAPHICAL_DESKTOP_PATH);
#ifdef FROG_DESKTOP_SMOKE_TEST
        graphical_test_report(FROG_TEST_DESKTOP_INIT_DESKTOP_FORK,
                              desktop_pid >= 0);
#endif
        while (!compositor_done || (desktop_pid >= 0 && !desktop_done)) {
                int_32 status;
                pid_t waited_pid = ops->wait(&status);

                if (waited_pid == compositor_pid && !compositor_done) {
                        compositor_status = status;
                        compositor_done = true;
                } else if (desktop_pid >= 0 && waited_pid == desktop_pid &&
                           !desktop_done) {
                        desktop_status = status;
                        desktop_done = true;
                } else {
                        return FROG_GRAPHICAL_EXIT_WAIT;
                }
        }

        int result = 0;

        if (compositor_status == FROG_GRAPHICAL_EXIT_COMPOSITOR_EXEC)
                result = FROG_GRAPHICAL_EXIT_COMPOSITOR_EXEC;
        else if (desktop_pid >= 0 &&
                 desktop_status == FROG_GRAPHICAL_EXIT_DESKTOP_EXEC)
                result = FROG_GRAPHICAL_EXIT_DESKTOP_EXEC;
        else if (desktop_pid < 0)
                result = FROG_GRAPHICAL_EXIT_DESKTOP_FORK;
        else if (compositor_status != 0)
                result = FROG_GRAPHICAL_EXIT_COMPOSITOR_RUN;
        else if (desktop_status != FROG_DESKTOP_EXIT_COMPOSITOR_HUP)
                result = FROG_GRAPHICAL_EXIT_DESKTOP_RUN;
#ifdef FROG_DESKTOP_SMOKE_TEST
        graphical_test_report(FROG_TEST_DESKTOP_LIFECYCLE, result == 0);
#endif
        return result;
}

void graphical_init_report_status(
    int_32 status, void (*emit)(char byte, void *context), void *context)
{
        char line[] = "graphical-init status=00\n";
        uint_32 value = status < 0 ? 99U : (uint_32) status;
        uint_32 index;

        if (!emit)
                return;
        if (value > 99U)
                value = 99U;
        line[22] = (char) ('0' + value / 10U);
        line[23] = (char) ('0' + value % 10U);
        for (index = 0; index < sizeof(line) - 1U; ++index)
                emit(line[index], context);
}

#ifndef GRAPHICAL_INIT_HOST_TEST
#include <frog/syscall.h>

static pid_t spawn_child(enum graphical_child child, const char *path)
{
        pid_t pid = fork();

        if (pid != 0)
                return pid;
        const char *argv[] = {path, NULL};
        int_32 exec_status = child == GRAPHICAL_CHILD_COMPOSITOR
                                 ? FROG_GRAPHICAL_EXIT_COMPOSITOR_EXEC
                                 : FROG_GRAPHICAL_EXIT_DESKTOP_EXEC;

        (void) execv(path, argv);
        exit(exec_status);
        for (;;)
                ;
}

static pid_t wait_child(int_32 *status)
{
        return wait(status);
}

static void emit_console(char byte, void *context)
{
        (void) context;
        putc(byte);
}

int main(void)
{
        static const struct graphical_init_ops ops = {
            .spawn = spawn_child,
            .wait = wait_child,
        };
        int_32 status = graphical_init_supervise(&ops);

        graphical_init_report_status(status, emit_console, NULL);
        for (;;)
                (void) wait2(NULL, 0, 1000);
}
#endif
