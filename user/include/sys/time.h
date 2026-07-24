#ifndef AUKOS_SYS_TIME_H
#define AUKOS_SYS_TIME_H

#include <sys/types.h>

struct timeval {
    time_t tv_sec;
    long tv_usec;
};

int gettimeofday(struct timeval *tv, void *tz);

#endif
