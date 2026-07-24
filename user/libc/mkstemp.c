#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef AUKOS_MKSTEMP_TEST
#include "aukos/syscall.h"
#endif

#define MKSTEMP_SUFFIX_LENGTH 6u
#define MKSTEMP_ATTEMPTS 64u

#ifdef AUKOS_MKSTEMP_TEST
int aukos_mkstemp_open(const char *path, int flags, int mode);
uint64_t aukos_mkstemp_random(void);
#else
static int aukos_mkstemp_open(const char *path, int flags, int mode)
{
    return open(path, flags, mode);
}

static uint64_t aukos_mkstemp_random(void)
{
    return aukos_syscall1(AUKOS_SYS_RANDOM, 0u);
}
#endif

int mkstemp(char *template)
{
    static uint64_t sequence;
    static const char alphabet[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    size_t length = 0u;
    size_t suffix;
    uint64_t seed;

    if (!template) {
        errno = EINVAL;
        return -1;
    }
    while (template[length]) {
        length++;
    }
    if (length < MKSTEMP_SUFFIX_LENGTH) {
        errno = EINVAL;
        return -1;
    }
    suffix = length - MKSTEMP_SUFFIX_LENGTH;
    for (size_t index = suffix; index < length; index++) {
        if (template[index] != 'X') {
            errno = EINVAL;
            return -1;
        }
    }

    seed = aukos_mkstemp_random() ^ ++sequence;
    for (size_t attempt = 0; attempt < MKSTEMP_ATTEMPTS; attempt++) {
        uint64_t value = seed + attempt * 0x9e3779b97f4a7c15ull;
        int fd;

        for (size_t index = 0; index < MKSTEMP_SUFFIX_LENGTH; index++) {
            template[suffix + index] = alphabet[value % 62u];
            value = value / 62u + 17u;
        }
        fd = aukos_mkstemp_open(template, O_CREAT | O_EXCL | O_RDWR, 0600);
        if (fd >= 0) {
            return fd;
        }
    }
    for (size_t index = suffix; index < length; index++) {
        template[index] = 'X';
    }
    errno = EEXIST;
    return -1;
}
