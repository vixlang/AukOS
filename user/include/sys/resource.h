#ifndef AUKOS_SYS_RESOURCE_H
#define AUKOS_SYS_RESOURCE_H

#include <sys/time.h>
#include <sys/types.h>

#define RUSAGE_SELF 0
#define RLIMIT_NOFILE 7
#define RLIM_INFINITY ((unsigned long)-1)

typedef unsigned long rlim_t;

struct rlimit {
    rlim_t rlim_cur;
    rlim_t rlim_max;
};

struct rusage {
    struct timeval ru_utime;
    struct timeval ru_stime;
};

int getrlimit(int resource, struct rlimit *rlim);
int setrlimit(int resource, const struct rlimit *rlim);
int getrusage(int who, struct rusage *usage);

#endif
