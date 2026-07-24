#ifndef AUKOS_SYS_SELECT_H
#define AUKOS_SYS_SELECT_H

#include <sys/time.h>

#define FD_SETSIZE 64

typedef struct {
    unsigned long bits;
} fd_set;

void FD_ZERO(fd_set *set);
void FD_SET(int fd, fd_set *set);
int FD_ISSET(int fd, const fd_set *set);
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);

#endif
