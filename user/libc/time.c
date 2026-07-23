#include <stddef.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "aukos/syscall.h"

#define TIMER_HZ 100u

static struct tm static_tm;

int clock_gettime(int clockid, struct timespec *tp)
{
    uint64_t ticks;

    (void)clockid;
    if (!tp) {
        return -1;
    }

    ticks = aukos_syscall1(AUKOS_SYS_TIME, 0);
    tp->tv_sec = (time_t)(ticks / TIMER_HZ);
    tp->tv_nsec = (long)((ticks % TIMER_HZ) * (1000000000u / TIMER_HZ));
    return 0;
}

int gettimeofday(struct timeval *tv, void *tz)
{
    struct timespec ts;

    (void)tz;
    if (!tv || clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        return -1;
    }

    tv->tv_sec = ts.tv_sec;
    tv->tv_usec = ts.tv_nsec / 1000;
    return 0;
}

time_t time(time_t *tloc)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        return (time_t)-1;
    }
    if (tloc) {
        *tloc = ts.tv_sec;
    }
    return ts.tv_sec;
}

int nanosleep(const struct timespec *req, struct timespec *rem)
{
    (void)req;
    if (rem) {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }
    return 0;
}

struct tm *localtime(const time_t *timer)
{
    time_t value = timer ? *timer : 0;

    memset(&static_tm, 0, sizeof(static_tm));
    static_tm.tm_sec = (int)(value % 60);
    static_tm.tm_min = (int)((value / 60) % 60);
    static_tm.tm_hour = (int)((value / 3600) % 24);
    static_tm.tm_mday = 1;
    static_tm.tm_year = 70;
    return &static_tm;
}

size_t strftime(char *s, size_t max, const char *format, const struct tm *tm)
{
    (void)tm;
    if (!s || max == 0) {
        return 0;
    }
    if (!format) {
        s[0] = '\0';
        return 0;
    }

    strncpy(s, format, max - 1);
    s[max - 1] = '\0';
    return strlen(s);
}
