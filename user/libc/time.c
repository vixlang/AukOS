#include <stddef.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "aukos/syscall.h"

#define TIMER_HZ 100u

static struct tm static_tm;

static int is_leap_year(int year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static int days_in_year(int year)
{
    return is_leap_year(year) ? 366 : 365;
}

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

struct tm *localtime_r(const time_t *timer, struct tm *result)
{
    static const unsigned char month_days[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    time_t value;
    time_t days;
    time_t seconds;
    int year = 1970;
    int month;

    if (!timer || !result) {
        return 0;
    }
    value = *timer;
    days = value / 86400;
    seconds = value % 86400;
    if (seconds < 0) {
        seconds += 86400;
        days--;
    }

    memset(result, 0, sizeof(*result));
    result->tm_sec = (int)(seconds % 60);
    result->tm_min = (int)((seconds / 60) % 60);
    result->tm_hour = (int)(seconds / 3600);
    result->tm_wday = (int)((days + 4) % 7);
    if (result->tm_wday < 0) {
        result->tm_wday += 7;
    }

    while (days < 0) {
        year--;
        days += days_in_year(year);
    }
    while (days >= days_in_year(year)) {
        days -= days_in_year(year);
        year++;
    }
    result->tm_year = year - 1900;
    result->tm_yday = (int)days;

    for (month = 0; month < 12; month++) {
        int length = month_days[month];

        if (month == 1 && is_leap_year(year)) {
            length++;
        }
        if (days < length) {
            break;
        }
        days -= length;
    }
    result->tm_mon = month;
    result->tm_mday = (int)days + 1;
    return result;
}

struct tm *localtime(const time_t *timer)
{
    return localtime_r(timer, &static_tm);
}

struct tm *gmtime(const time_t *timer)
{
    return localtime_r(timer, &static_tm);
}

static int append_char(char **output, size_t *remaining, char value)
{
    if (*remaining <= 1u) {
        return -1;
    }
    *(*output)++ = value;
    (*remaining)--;
    return 0;
}

static int append_number(char **output, size_t *remaining, int value, int width,
                         char padding)
{
    char digits[12];
    int count = 0;
    unsigned int magnitude;

    if (value < 0) {
        if (append_char(output, remaining, '-') != 0) {
            return -1;
        }
        magnitude = (unsigned int)(-(value + 1)) + 1u;
        width--;
    } else {
        magnitude = (unsigned int)value;
    }
    do {
        digits[count++] = (char)('0' + magnitude % 10u);
        magnitude /= 10u;
    } while (magnitude != 0u);
    while (count < width) {
        if (append_char(output, remaining, padding) != 0) {
            return -1;
        }
        width--;
    }
    while (count > 0) {
        if (append_char(output, remaining, digits[--count]) != 0) {
            return -1;
        }
    }
    return 0;
}

size_t strftime(char *s, size_t max, const char *format, const struct tm *tm)
{
    char *output = s;
    size_t remaining = max;

    if (!s || max == 0u || !format || !tm) {
        return 0u;
    }
    while (*format) {
        int value;
        int width = 2;
        char padding = '0';

        if (*format != '%') {
            if (append_char(&output, &remaining, *format++) != 0) {
                goto overflow;
            }
            continue;
        }
        format++;
        if (*format == 'e') {
            value = tm->tm_mday;
            padding = ' ';
        } else if (*format == 'H') {
            value = tm->tm_hour;
        } else if (*format == 'M') {
            value = tm->tm_min;
        } else if (*format == 'S') {
            value = tm->tm_sec;
        } else if (*format == 'd') {
            value = tm->tm_mday;
        } else if (*format == 'm') {
            value = tm->tm_mon + 1;
        } else if (*format == 'y') {
            value = (tm->tm_year + 1900) % 100;
        } else if (*format == 'Y') {
            value = tm->tm_year + 1900;
            width = 4;
        } else if (*format == 'j') {
            value = tm->tm_yday + 1;
            width = 3;
        } else if (*format == 'w') {
            value = tm->tm_wday;
            width = 1;
        } else if (*format == '%') {
            if (append_char(&output, &remaining, '%') != 0) {
                goto overflow;
            }
            format++;
            continue;
        } else if (*format == '\0') {
            goto overflow;
        } else {
            if (append_char(&output, &remaining, '%') != 0 ||
                append_char(&output, &remaining, *format) != 0) {
                goto overflow;
            }
            format++;
            continue;
        }
        if (append_number(&output, &remaining, value, width, padding) != 0) {
            goto overflow;
        }
        format++;
    }

    *output = '\0';
    return (size_t)(output - s);

overflow:
    s[0] = '\0';
    return 0u;
}
