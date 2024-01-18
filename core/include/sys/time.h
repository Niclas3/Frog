#ifndef __SYS_TIME_H
#define __SYS_TIME_H
#include <ostype.h>

struct timeval {
	time_t      tv_sec;     /* seconds */
	suseconds_t tv_usec;    /* microseconds */
};

struct timezone {
	int tz_minuteswest;     /* minutes west of Greenwich */
	int tz_dsttime;         /* type of DST correction */
};


//impl at cmos.c
extern int gettimeofday(struct timeval *p, void *z);

//TODO: not implment
//impl at cmos.c
extern int settimeofday(struct timeval *p, void *z);


#endif
