#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "allocator.h"

static struct aukos_allocator process_allocator;
static int process_allocator_initialized;

static void *grow_process_heap(void *context, size_t size)
{
    (void)context;
    return sbrk((intptr_t)size);
}

static struct aukos_allocator *get_process_allocator(void)
{
    if (!process_allocator_initialized) {
        aukos_allocator_init(&process_allocator, grow_process_heap, 0);
        process_allocator_initialized = 1;
    }
    return &process_allocator;
}

void *malloc(size_t size)
{
    return aukos_allocator_malloc(get_process_allocator(), size);
}

void *calloc(size_t nmemb, size_t size)
{
    return aukos_allocator_calloc(get_process_allocator(), nmemb, size);
}

void *realloc(void *ptr, size_t size)
{
    return aukos_allocator_realloc(get_process_allocator(), ptr, size);
}

void free(void *ptr)
{
    aukos_allocator_free(get_process_allocator(), ptr);
}

static int digit_value(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    return -1;
}

long strtol(const char *nptr, char **endptr, int base)
{
    long sign = 1;
    long value = 0;
    while (*nptr == ' ' || (*nptr >= '\t' && *nptr <= '\r')) nptr++;
    if (*nptr == '-') { sign = -1; nptr++; }
    else if (*nptr == '+') nptr++;
    if (base == 0) base = 10;
    while (digit_value(*nptr) >= 0 && digit_value(*nptr) < base) value = value * base + digit_value(*nptr++);
    if (endptr) *endptr = (char *)nptr;
    return value * sign;
}

unsigned long strtoul(const char *nptr, char **endptr, int base)
{
    return (unsigned long)strtol(nptr, endptr, base);
}

long long strtoll(const char *nptr, char **endptr, int base)
{
    return (long long)strtol(nptr, endptr, base);
}

double strtod(const char *nptr, char **endptr)
{
    return (double)strtol(nptr, endptr, 10);
}

double atof(const char *nptr)
{
    return strtod(nptr, 0);
}

long double strtold(const char *nptr, char **endptr)
{
    return (long double)strtod(nptr, endptr);
}

int atoi(const char *nptr)
{
    return (int)strtol(nptr, 0, 10);
}

void exit(int code)
{
    _exit(code);
}
