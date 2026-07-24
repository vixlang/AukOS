#include <stdint.h>

#define SYS_WRITE 1u
#define SYS_EXIT 60u

static uint64_t syscall3(uint64_t number, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
    uint64_t result;

    __asm__ volatile (
        "syscall"
        : "=a"(result)
        : "a"(number), "D"(arg0), "S"(arg1), "d"(arg2)
        : "rcx", "r11", "memory");

    return result;
}

static uint64_t syscall1(uint64_t number, uint64_t arg0)
{
    uint64_t result;

    __asm__ volatile (
        "syscall"
        : "=a"(result)
        : "a"(number), "D"(arg0)
        : "rcx", "r11", "memory");

    return result;
}

void _start(void)
{
    const char *message = "hello from /bin/hello\n";
    const char *message2 = "welcome to the AukOS!\n";

    syscall3(SYS_WRITE, 1, (uint64_t)message, 22);
    syscall3(SYS_WRITE, 1, (uint64_t)message2, 23);
    for (;;) {
        syscall1(SYS_EXIT, 0);
    }
}
