#pragma once
#include <sys/time.h>

uint_32 read_rtc(void);

int sys_gettimeofday(struct timeval *t, void *z);

//TODO: not implment
int sys_settimeofday(struct timeval * t, void *z);
void clock_init(void);
