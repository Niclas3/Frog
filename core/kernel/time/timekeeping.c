#include <asm/i8253.h>
#include <frog/errno.h>
#include <frog/irqflags.h>
#include <frog/uaccess.h>
#include <kernel/qemu_test.h>
#include <kernel/timekeeping.h>

#define NSEC_PER_SEC 1000000000U
#define NSEC_PER_USEC 1000U
#define PIT_TICK_NUMERATOR \
        ((uint_64) PIT_COUNTER0_DIVISOR * (uint_64) NSEC_PER_SEC)
#define PIT_TICK_NSEC_QUOTIENT \
        ((uint_32) (PIT_TICK_NUMERATOR / PIT_INPUT_FREQUENCY))
#define PIT_TICK_NSEC_REMAINDER \
        ((uint_32) (PIT_TICK_NUMERATOR % PIT_INPUT_FREQUENCY))

static struct timekeeping_state monotonic_time;
static struct timekeeping_state realtime_origin;
static bool realtime_available;

void timekeeping_advance_state(struct timekeeping_state *state)
{
        state->nanoseconds += PIT_TICK_NSEC_QUOTIENT;
        state->pit_remainder += PIT_TICK_NSEC_REMAINDER;

        if (state->pit_remainder >= PIT_INPUT_FREQUENCY) {
                state->pit_remainder -= PIT_INPUT_FREQUENCY;
                state->nanoseconds++;
        }
        if (state->nanoseconds >= NSEC_PER_SEC) {
                state->nanoseconds -= NSEC_PER_SEC;
                state->seconds++;
        }
}

void timekeeping_advance(void)
{
        timekeeping_advance_state(&monotonic_time);
}

static int_32 timekeeping_snapshot(clockid_t clock_id, struct timespec *result)
{
        struct timekeeping_state monotonic;
        struct timekeeping_state origin;
        bool have_realtime;
        unsigned long flags;

        if (clock_id != CLOCK_MONOTONIC && clock_id != CLOCK_REALTIME)
                return -EINVAL;

        local_irq_save(flags);
        monotonic = monotonic_time;
        origin = realtime_origin;
        have_realtime = realtime_available;
        local_irq_restore(flags);

        if (clock_id == CLOCK_REALTIME) {
                if (!have_realtime)
                        return -ENODATA;
                monotonic.seconds += origin.seconds;
                monotonic.nanoseconds += origin.nanoseconds;
                if (monotonic.nanoseconds >= NSEC_PER_SEC) {
                        monotonic.nanoseconds -= NSEC_PER_SEC;
                        monotonic.seconds++;
                }
        }

        result->tv_sec = monotonic.seconds;
        result->tv_nsec = (int_32) monotonic.nanoseconds;
        return 0;
}

static int_32 do_clock_gettime(clockid_t clock_id,
                               struct timespec *user_time)
{
        struct timespec result;
        int_32 status = timekeeping_snapshot(clock_id, &result);

        if (status != 0)
                return status;
        return copy_to_user(user_time, &result, sizeof(result));
}

int_32 sys_clock_gettime(clockid_t clock_id, struct timespec *user_time)
{
        unsigned long entry_flags;
        int_32 status;

        local_irq_save(entry_flags);
        local_irq_enable();
        status = do_clock_gettime(clock_id, user_time);
        local_irq_restore(entry_flags);
        return status;
}

static int_32 do_gettimeofday(struct timeval *user_time, void *timezone)
{
        struct timespec realtime;
        struct timeval result;
        int_32 status;

        (void) timezone;
        status = timekeeping_snapshot(CLOCK_REALTIME, &realtime);
        if (status != 0)
                return status;

        result.tv_sec = realtime.tv_sec;
        result.tv_usec = realtime.tv_nsec / NSEC_PER_USEC;
        return copy_to_user(user_time, &result, sizeof(result));
}

int_32 sys_gettimeofday(struct timeval *user_time, void *timezone)
{
        unsigned long entry_flags;
        int_32 status;

        local_irq_save(entry_flags);
        local_irq_enable();
        status = do_gettimeofday(user_time, timezone);
        local_irq_restore(entry_flags);
        return status;
}

int_32 sys_settimeofday(struct timeval *user_time, void *timezone)
{
        (void) user_time;
        (void) timezone;
        return -ENOSYS;
}

#ifdef CONFIG_FROG_TEST_TIME
void timekeeping_regression_test(void)
{
        struct timekeeping_state state = {0};

        frog_test_case("time.accumulator.constants",
                       PIT_TICK_NSEC_QUOTIENT == 999849U &&
                           PIT_TICK_NSEC_REMAINDER == 170180U);

        timekeeping_advance_state(&state);
        frog_test_case("time.accumulator.quotient-remainder",
                       state.seconds == 0 && state.nanoseconds == 999849U &&
                           state.pit_remainder == 170180U);

        state.seconds = 0;
        state.nanoseconds = 0;
        state.pit_remainder = 1023000U;
        timekeeping_advance_state(&state);
        frog_test_case("time.accumulator.remainder-carry",
                       state.seconds == 0 && state.nanoseconds == 999850U &&
                           state.pit_remainder == 0);

        state.seconds = 7;
        state.nanoseconds = 999500151U;
        state.pit_remainder = 0;
        timekeeping_advance_state(&state);
        frog_test_case("time.accumulator.normalized",
                       state.seconds == 8 && state.nanoseconds == 500000U &&
                           state.pit_remainder == 170180U);
}
#endif
