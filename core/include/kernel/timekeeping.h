#ifndef _FROG_KERNEL_TIMEKEEPING_H
#define _FROG_KERNEL_TIMEKEEPING_H

#include <frog/time.h>
#include <frog/types.h>

struct timekeeping_state {
        time_t seconds;
        uint_32 nanoseconds;
        uint_32 pit_remainder;
};

void timekeeping_advance_state(struct timekeeping_state *state);
void timekeeping_advance(void);
int_32 timekeeping_get_monotonic(struct timespec *result);

int_32 sys_clock_gettime(clockid_t clock_id, struct timespec *user_time);
int_32 sys_gettimeofday(struct timeval *user_time, void *timezone);
int_32 sys_settimeofday(struct timeval *user_time, void *timezone);

#ifdef CONFIG_FROG_TEST_TIME
void timekeeping_regression_test(void);
#endif

#endif
