#ifndef AUKOS_SYS_WAIT_H
#define AUKOS_SYS_WAIT_H

#include <sys/types.h>

#define WNOHANG 1
#define WUNTRACED 2
#define WIFEXITED(status) (((status) & 0xff) == 0)
#define WEXITSTATUS(status) (((status) >> 8) & 0xff)
#define WTERMSIG(status) (((status) & 0x7f))
#define WIFSIGNALED(status) (((status) & 0x7f) != 0)

pid_t waitpid(pid_t pid, int *status, int options);
pid_t wait(int *status);

#endif
