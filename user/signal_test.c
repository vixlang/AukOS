#include <errno.h>
#include <signal.h>
#include <stdint.h>

#include "aukos/syscall.h"

int errno;

static volatile int handled_usr1;
static volatile int handled_usr2;
static volatile int handler_masked_usr1;

static uint64_t string_length(const char *value)
{
    uint64_t length = 0;

    while (value[length] != '\0') {
        length++;
    }
    return length;
}

static void write_string(const char *value)
{
    aukos_syscall3(AUKOS_SYS_WRITE, 1, (uint64_t)value, string_length(value));
}

static void usr1_handler(int signal)
{
    sigset_t mask = 0;

    handled_usr1 += signal == SIGUSR1;
    if (sigprocmask(SIG_SETMASK, 0, &mask) == 0) {
        handler_masked_usr1 = sigismember(&mask, SIGUSR1);
    }
}

static void usr2_handler(int signal)
{
    handled_usr2 += signal == SIGUSR2;
}

static int run_test(void)
{
    struct sigaction action;
    sigset_t set;
    sigset_t pending;
    sigset_t current;

    action.sa_handler = usr1_handler;
    action.sa_mask = 0;
    action.sa_flags = 0;
    action.sa_restorer = 0;
    if (sigaction(SIGUSR1, &action, 0) != 0 ||
        sigemptyset(&set) != 0 || sigaddset(&set, SIGUSR1) != 0 ||
        sigprocmask(SIG_BLOCK, &set, 0) != 0 || raise(SIGUSR1) != 0) {
        return -1;
    }

    if (handled_usr1 != 0 || sigpending(&pending) != 0 ||
        sigismember(&pending, SIGUSR1) != 1) {
        return -1;
    }

    if (sigprocmask(SIG_UNBLOCK, &set, 0) != 0 || handled_usr1 != 1 ||
        handler_masked_usr1 != 1) {
        return -1;
    }

    if (sigprocmask(SIG_SETMASK, 0, &current) != 0 ||
        sigismember(&current, SIGUSR1) != 0) {
        return -1;
    }

    if (signal(SIGUSR2, usr2_handler) == SIG_ERR ||
        raise(SIGUSR2) != 0 || handled_usr2 != 1 ||
        signal(SIGTERM, SIG_IGN) == SIG_ERR || raise(SIGTERM) != 0) {
        return -1;
    }

    return 0;
}

void _start(void)
{
    int result = run_test();

    if (result == 0) {
        write_string("[signal_test] PASS\n");
    } else {
        write_string("[signal_test] FAIL\n");
    }

    for (;;) {
        aukos_syscall1(AUKOS_SYS_EXIT, result == 0 ? 0 : 1);
    }
}
