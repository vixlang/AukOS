#include <stdint.h>

#include "../include/aukos/syscall.h"

extern int main(int argc, char **argv, char **envp);
extern char **environ;

void _start(uint64_t argc, char **argv, char **envp)
{
    environ = envp;
    int code = main((int)argc, argv, envp);

    for (;;) {
        aukos_syscall1(AUKOS_SYS_EXIT, (uint64_t)code);
    }
}
