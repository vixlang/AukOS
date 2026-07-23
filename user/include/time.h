#ifndef AUKOS_TIME_H
#define AUKOS_TIME_H

#include <stddef.h>
#include <sys/types.h>
#include <sys/time.h>

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1

typedef int clockid_t;
typedef int timer_t;

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

struct itimerspec {
    struct timespec it_interval;
    struct timespec it_value;
};

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

int clock_gettime(int clockid, struct timespec *tp);
int nanosleep(const struct timespec *req, struct timespec *rem);
time_t time(time_t *tloc);
struct tm *localtime(const time_t *timer);
struct tm *gmtime(const time_t *timer);
struct tm *localtime_r(const time_t *timer, struct tm *result);
time_t mktime(struct tm *tm);
size_t strftime(char *s, size_t max, const char *format, const struct tm *tm);
char *strptime(const char *s, const char *format, struct tm *tm);
void tzset(void);
char *ctime(const time_t *timer);

#endif
