#include <frog/string.h>
#include <frog/threads.h>
#include <kernel/qemu_test.h>
#include <kernel/system_init_selection_test.h>

#define SYSTEM_INIT_STATUS_LINE_LENGTH 22U

static char observed_line[SYSTEM_INIT_STATUS_LINE_LENGTH + 1U];
static uint_32 observed_length;
static bool observed_complete;
static TCB_t *observed_init;

static bool current_matches(const char *name)
{
        TCB_t *current = running_thread();

        return current != NULL && current->pid == 1 &&
               current->parent_pid == -1 && current->mm != NULL &&
               strcmp(current->name, name) == 0;
}

static bool is_failure_status_line(const char *line)
{
        static const char *const accepted[] = {
            "system-init status=90\n",
            "system-init status=91\n",
            "system-init status=92\n",
            "system-init status=93\n",
            "system-init status=94\n",
            "system-init status=95\n",
            "system-init status=96\n",
        };

        for (uint_32 index = 0;
             index < sizeof(accepted) / sizeof(accepted[0]); index++) {
                if (strcmp(line, accepted[index]) == 0)
                        return true;
        }
        return false;
}

bool system_init_selection_test_identity_ok(void)
{
        return current_matches("init-graphical");
}

void system_init_selection_test_observe_output(char byte)
{
        TCB_t *current = running_thread();

        if (!current_matches("init") || observed_complete)
                return;
        if (observed_length == 0 && byte != 's')
                return;
        if (observed_length >= SYSTEM_INIT_STATUS_LINE_LENGTH) {
                observed_length = 0;
                return;
        }
        observed_line[observed_length++] = byte;
        observed_line[observed_length] = '\0';
        if (byte != '\n')
                return;
        if (is_failure_status_line(observed_line)) {
                observed_complete = true;
                observed_init = current;
        } else {
                observed_length = 0;
        }
}

void system_init_selection_test_observe_wait(const void *user_fds,
                                             uint_32 count,
                                             int_32 timeout_ms)
{
        if (!observed_complete || running_thread() != observed_init)
                return;
        frog_test_case("system-init.failure-pid1",
                       current_matches("init"));
        frog_test_case("system-init.failure-wait2",
                       user_fds == NULL && count == 0 &&
                           timeout_ms == 1000);
        frog_test_sync("system-init-failure-observed");
        frog_test_finish();
}
