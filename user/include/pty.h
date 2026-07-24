#ifndef _PTY_H
#define _PTY_H

#include <termios.h>
#include <sys/ioctl.h>

#ifdef __cplusplus
extern "C" {
#endif

int openpty(int *amaster, int *aslave, char *name,
            const struct termios *termp,
            const struct winsize *winp);
int forkpty(int *amaster, char *name,
            const struct termios *termp,
            const struct winsize *winp);
int login_tty(int fd);

#ifdef __cplusplus
}
#endif

#endif
