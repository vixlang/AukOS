#include <stdint.h>

#include <aukos/runtime.h>

extern int main(int argc, char **argv, char **envp);

void _start(uint64_t argc, char **argv, char **envp)
{
    aukos_vix_exit(main((int)argc, argv, envp));
}
