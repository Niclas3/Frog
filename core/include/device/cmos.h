#pragma once
#include <sys/time.h>

uint_32 read_rtc(void);

int gettimeofday(struct timeval *t, void *z);

//TODO: not implment
int settimeofday(struct timeval * t, void *z);
void clock_init(void);
