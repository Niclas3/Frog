#include <frog/string.h>
#include <frog/test.h>
#include <frog/threads.h>
#include <frog/printk.h>
#include <frog/irqflags.h>
#include <kernel/desktop_production_test.h>
#include <kernel/qemu_test.h>

static pid_t compositor_pid = -1;
static pid_t desktop_pid = -1;

static int current_is_graphical_init(void)
{
        TCB_t *current = running_thread();

        return current && current->pid == 1 && current->parent_pid == -1 &&
               current->mm && strcmp(current->name, "init-graphical") == 0;
}

static int child_is_fork_of_init(pid_t pid)
{
        TCB_t *child = pid2thread(pid);

        return pid > 1 && child && child->parent_pid == 1 && child->mm;
}

static void report_visible(const char *name, int passed)
{
        frog_test_case(name, passed);
        if (passed)
                printk("FROGTEST CASE %s PASS\n", name);
}

void desktop_production_test_observe_fork(pid_t child_pid)
{
        int init_ok = current_is_graphical_init();

        if (compositor_pid < 0) {
                int passed = init_ok && child_is_fork_of_init(child_pid);

                report_visible("desktop.production-init-chain", init_ok);
                report_visible("desktop.init-compositor-fork", passed);
                if (passed)
                        compositor_pid = child_pid;
                return;
        }
        if (desktop_pid < 0) {
                int passed = init_ok && child_pid != compositor_pid &&
                             child_is_fork_of_init(child_pid);

                report_visible("desktop.init-desktop-fork", passed);
                if (passed)
                        desktop_pid = child_pid;
                return;
        }
        report_visible("desktop.unexpected-extra-fork", 0);
}

int desktop_production_test_validate_exec(uint_32 id, int passed)
{
        TCB_t *current = running_thread();
        pid_t expected_pid;
        const char *expected_name;

        if (id == FROG_TEST_DESKTOP_COMPOSITOR_EXEC) {
                expected_pid = compositor_pid;
                expected_name = "compositor";
        } else if (id == FROG_TEST_DESKTOP_CLIENT_EXEC) {
                expected_pid = desktop_pid;
                expected_name = "desktop";
        } else {
                return passed;
        }
        return passed && current && current->pid == expected_pid &&
               current->parent_pid == 1 && current->mm &&
               strcmp(current->name, expected_name) == 0;
}

static int process_is_live(pid_t pid, const char *name)
{
        TCB_t *thread = pid2thread(pid);

        return pid > 1 && thread && thread->pid == pid &&
               thread->parent_pid == 1 && thread->mm &&
               thread->status != THREAD_TASK_HANGING &&
               thread->status != THREAD_TASK_DIED &&
               strcmp(thread->name, name) == 0;
}

int desktop_production_test_validate_liveness(void)
{
        unsigned long flags;
        int passed;

        /* Keep the pid lookup and TCB inspection in one scheduler snapshot. */
        local_irq_save(flags);
        passed = process_is_live(compositor_pid, "compositor") &&
                 process_is_live(desktop_pid, "desktop");
        local_irq_restore(flags);
        return passed;
}
