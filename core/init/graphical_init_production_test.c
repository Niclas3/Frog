#include <frog/errno.h>
#include <frog/string.h>
#include <frog/threads.h>
#include <frog/test.h>
#include <kernel/graphical_init_production_test.h>
#include <kernel/qemu_test.h>

#define GRAPHICAL_STATUS_LINE_LENGTH 25U

static char observed_line[GRAPHICAL_STATUS_LINE_LENGTH + 1U];
static uint_32 observed_length;
static bool observed_complete;
static bool compositor_ok;
static bool desktop_ok;
static pid_t compositor_pid = -1;
static pid_t desktop_pid = -1;
static TCB_t *observed_init;

static bool current_is_init_graphical(void)
{
        TCB_t *current = running_thread();

        return current != NULL && current->pid == 1 &&
               current->parent_pid == -1 && current->mm != NULL &&
               strcmp(current->name, "init-graphical") == 0;
}

static bool current_is_child(const char *name)
{
        TCB_t *current = running_thread();

        return current != NULL && current->pid > 1 &&
               current->parent_pid == 1 && current->mm != NULL &&
               strcmp(current->name, name) == 0;
}

int_32 graphical_init_production_test_child_report(uint_32 id,
                                                   int_32 passed)
{
        TCB_t *current = running_thread();
        bool valid;

        if (id == FROG_TEST_GRAPHICAL_COMPOSITOR_RELEASE) {
                if (passed == 0 || !compositor_ok || current == NULL ||
                    current->pid != compositor_pid ||
                    !current_is_child("compositor"))
                        return -EUCLEAN;
                return desktop_ok ? 1 : 0;
        }
        if (id == FROG_TEST_GRAPHICAL_COMPOSITOR_IDENTITY) {
                valid = !compositor_ok && passed != 0 &&
                        current_is_child("compositor");
                frog_test_case("graphical-init.compositor-identity", valid);
                if (valid) {
                        compositor_ok = true;
                        compositor_pid = current->pid;
                        frog_test_sync("graphical-init-compositor-observed");
                }
                return valid ? 0 : -EUCLEAN;
        }
        if (id == FROG_TEST_GRAPHICAL_DESKTOP_IDENTITY) {
                valid = !desktop_ok && passed != 0 &&
                        current_is_child("desktop");
                frog_test_case("graphical-init.desktop-identity", valid);
                if (valid) {
                        desktop_ok = true;
                        desktop_pid = current->pid;
                        frog_test_sync("graphical-init-desktop-observed");
                }
                return valid ? 0 : -EUCLEAN;
        }
        return -EINVAL;
}

void graphical_init_production_test_observe_output(char byte)
{
        static const char expected[] = "graphical-init status=00\n";

        if (!current_is_init_graphical() || observed_complete)
                return;
        if (observed_length == 0 && byte != expected[0])
                return;
        if (observed_length >= GRAPHICAL_STATUS_LINE_LENGTH ||
            byte != expected[observed_length]) {
                observed_length = byte == expected[0] ? 1U : 0U;
                observed_line[0] = byte;
                observed_line[observed_length] = '\0';
                return;
        }
        observed_line[observed_length++] = byte;
        observed_line[observed_length] = '\0';
        if (observed_length == GRAPHICAL_STATUS_LINE_LENGTH &&
            strcmp(observed_line, expected) == 0 && compositor_ok &&
            desktop_ok) {
                observed_complete = true;
                observed_init = running_thread();
        }
}

void graphical_init_production_test_observe_wait(const void *user_fds,
                                                 uint_32 count,
                                                 int_32 timeout_ms)
{
        if (!observed_complete || running_thread() != observed_init)
                return;
        frog_test_case("graphical-init.status-exact", true);
        frog_test_case("graphical-init.children-reaped",
                       compositor_pid > 1 && desktop_pid > 1 &&
                           compositor_pid != desktop_pid &&
                           pid2thread(compositor_pid) == NULL &&
                           pid2thread(desktop_pid) == NULL);
        frog_test_case("graphical-init.pid1-wait2",
                       current_is_init_graphical() && user_fds == NULL &&
                           count == 0 && timeout_ms == 1000);
        frog_test_sync("graphical-init-lifecycle-observed");
        frog_test_sync("graphical-init-production-observed");
        frog_test_finish();
}
