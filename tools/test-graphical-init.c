#include <frog/graphical_startup.h>

#ifndef EXPECTED_COMPOSITOR_PATH
#define EXPECTED_COMPOSITOR_PATH "/test/compositor"
#endif

#ifndef EXPECTED_DESKTOP_PATH
#define EXPECTED_DESKTOP_PATH "/test/desktop"
#endif

struct wait_result {
        pid_t pid;
        int_32 status;
};

static pid_t spawn_results[2];
static struct wait_result waits[3];
static uint_32 spawn_count;
static uint_32 wait_count;
static uint_32 mismatch;
static char reported_status[32];
static uint_32 reported_size;

static bool string_equal(const char *left, const char *right)
{
        while (*left && *right) {
                if (*left++ != *right++)
                        return false;
        }
        return *left == *right;
}

static pid_t mock_spawn(enum graphical_child child, const char *path)
{
        static const char *const paths[2] = {
            EXPECTED_COMPOSITOR_PATH, EXPECTED_DESKTOP_PATH,
        };
        uint_32 index = spawn_count++;

        if (index >= 2 || child != (enum graphical_child) (index + 1) ||
            !string_equal(path, paths[index])) {
                mismatch = 1;
                return -1;
        }
        return spawn_results[index];
}

static pid_t mock_wait(int_32 *status)
{
        struct wait_result result = waits[wait_count++];

        *status = result.status;
        return result.pid;
}

static void reset(pid_t compositor, pid_t desktop)
{
        uint_32 index;

        spawn_results[0] = compositor;
        spawn_results[1] = desktop;
        spawn_count = 0;
        wait_count = 0;
        mismatch = 0;
        for (index = 0; index < 3; ++index) {
                waits[index].pid = -1;
                waits[index].status = 0;
        }
}

static int run(void)
{
        static const struct graphical_init_ops ops = {
            .spawn = mock_spawn,
            .wait = mock_wait,
        };

        return graphical_init_supervise(&ops);
}

static void capture_status(char byte, void *context)
{
        char *output = context;

        if (reported_size < sizeof(reported_status) - 1U)
                output[reported_size++] = byte;
        output[reported_size] = '\0';
}

static int status_reporting(void)
{
        reported_size = 0;
        graphical_init_report_status(83, capture_status, reported_status);
        if (!string_equal(reported_status, "graphical-init status=83\n"))
                return 20;
        reported_size = 0;
        graphical_init_report_status(-1, capture_status, reported_status);
        if (!string_equal(reported_status, "graphical-init status=99\n"))
                return 21;
        reported_size = 0;
        graphical_init_report_status(100, capture_status, reported_status);
        if (!string_equal(reported_status, "graphical-init status=99\n"))
                return 22;
        graphical_init_report_status(0, NULL, NULL);
        return 0;
}

static int launch_failures(void)
{
        reset(-1, 12);
        if (run() != FROG_GRAPHICAL_EXIT_COMPOSITOR_FORK ||
            spawn_count != 1 || wait_count != 0 || mismatch)
                return 1;

        reset(11, -1);
        waits[0] = (struct wait_result) {11, 0};
        if (run() != FROG_GRAPHICAL_EXIT_DESKTOP_FORK ||
            spawn_count != 2 || wait_count != 1 || mismatch)
                return 2;

        reset(11, 12);
        waits[0] = (struct wait_result) {
            11, FROG_GRAPHICAL_EXIT_COMPOSITOR_EXEC,
        };
        waits[1] = (struct wait_result) {
            12, FROG_DESKTOP_EXIT_CONNECT_TIMEOUT,
        };
        if (run() != FROG_GRAPHICAL_EXIT_COMPOSITOR_EXEC ||
            wait_count != 2 || mismatch)
                return 3;

        reset(11, 12);
        waits[0] = (struct wait_result) {
            12, FROG_GRAPHICAL_EXIT_DESKTOP_EXEC,
        };
        waits[1] = (struct wait_result) {11, 0};
        if (run() != FROG_GRAPHICAL_EXIT_DESKTOP_EXEC ||
            wait_count != 2 || mismatch)
                return 4;
        return 0;
}

static int lifecycle(void)
{
        reset(11, 12);
        waits[0] = (struct wait_result) {11, 0};
        waits[1] = (struct wait_result) {
            12, FROG_DESKTOP_EXIT_COMPOSITOR_HUP,
        };
        if (run() != 0 || wait_count != 2 || mismatch)
                return 10;

        reset(11, 12);
        waits[0] = (struct wait_result) {
            12, FROG_DESKTOP_EXIT_COMPOSITOR_HUP,
        };
        waits[1] = (struct wait_result) {11, 0};
        if (run() != 0 ||
            wait_count != 2 || mismatch)
                return 11;

        reset(11, 12);
        waits[0] = (struct wait_result) {11, 9};
        waits[1] = (struct wait_result) {
            12, FROG_DESKTOP_EXIT_COMPOSITOR_HUP,
        };
        if (run() != FROG_GRAPHICAL_EXIT_COMPOSITOR_RUN || mismatch)
                return 12;

        reset(11, 12);
        waits[0] = (struct wait_result) {
            12, FROG_DESKTOP_EXIT_RUNTIME_FAILURE,
        };
        waits[1] = (struct wait_result) {11, 0};
        if (run() != FROG_GRAPHICAL_EXIT_DESKTOP_RUN ||
            wait_count != 2 || mismatch)
                return 13;

        reset(11, 12);
        waits[0] = (struct wait_result) {-1, 0};
        if (run() != FROG_GRAPHICAL_EXIT_WAIT || wait_count != 1 || mismatch)
                return 14;
        return 0;
}

int main(void)
{
        int result = launch_failures();

        if (result != 0)
                return result;
        result = lifecycle();
        return result != 0 ? result : status_reporting();
}
