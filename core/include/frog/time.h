#ifndef __SYS_TIME_H
#define __SYS_TIME_H

#include <frog/types.h>

#define CLOCK_REALTIME  0
#define CLOCK_MONOTONIC 1

typedef int_32 clockid_t;

struct timespec {
        time_t tv_sec;
        int_32 tv_nsec;
};

struct timeval {
        time_t tv_sec;
        int_32 tv_usec;
};

struct timezone {
        int tz_minuteswest;
        int tz_dsttime;
};

int clock_gettime(clockid_t clock_id, struct timespec *tp);
int gettimeofday(struct timeval *tv, void *timezone);
int settimeofday(struct timeval *tv, void *timezone);

#endif
