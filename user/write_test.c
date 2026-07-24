#include <stdint.h>

#define SYS_READ 0u
#define SYS_WRITE 1u
#define SYS_OPEN 2u
#define SYS_CLOSE 3u
#define SYS_LSEEK 8u
#define SYS_EXIT 60u

#define O_RDWR 2
#define O_CREAT 0100

#define SEEK_SET 0

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
    const char *path = "/testfile";
    const char *data = "hello from write_test!\n";
    char buf[64];
    uint64_t fd;
    uint64_t ret;

    syscall3(SYS_WRITE, 1, (uint64_t)"[write_test] creating file...\n", 30);

    fd = syscall3(SYS_OPEN, (uint64_t)path, O_CREAT | O_RDWR, 0644);
    if (fd > 10000) {
        syscall3(SYS_WRITE, 1, (uint64_t)"[write_test] FAIL: open returned error\n", 38);
        syscall1(SYS_EXIT, 1);
        return;
    }

    syscall3(SYS_WRITE, 1, (uint64_t)"[write_test] writing data...\n", 28);

    ret = syscall3(SYS_WRITE, fd, (uint64_t)data, 22);
    if (ret != 22) {
        syscall3(SYS_WRITE, 1, (uint64_t)"[write_test] FAIL: write returned unexpected count\n", 49);
        syscall1(SYS_EXIT, 1);
        return;
    }

    syscall3(SYS_WRITE, 1, (uint64_t)"[write_test] seeking back...\n", 28);

    ret = syscall3(SYS_LSEEK, fd, 0, SEEK_SET);
    if (ret != 0) {
        syscall3(SYS_WRITE, 1, (uint64_t)"[write_test] FAIL: lseek failed\n", 30);
        syscall1(SYS_EXIT, 1);
        return;
    }

    syscall3(SYS_WRITE, 1, (uint64_t)"[write_test] reading back: ", 26);

    ret = syscall3(SYS_READ, fd, (uint64_t)buf, 22);
    if (ret != 22) {
        syscall3(SYS_WRITE, 1, (uint64_t)"[write_test] FAIL: read returned unexpected count\n", 49);
        syscall1(SYS_EXIT, 1);
        return;
    }
    buf[22] = '\0';

    syscall3(SYS_WRITE, 1, (uint64_t)buf, 22);

    syscall3(SYS_CLOSE, fd, 0, 0);

    syscall3(SYS_WRITE, 1, (uint64_t)"[write_test] PASS\n", 19);

    for (;;) {
        syscall1(SYS_EXIT, 0);
    }
}
