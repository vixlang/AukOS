#include <errno.h>
#include <langinfo.h>
#include <locale.h>
#include <signal.h>
#include <string.h>
#include <sys/utsname.h>

#include "aukos/syscall.h"

#define UINT64_MAX_VALUE 0xffffffffffffffffull

__attribute__((naked, noreturn)) static void signal_trampoline(void)
{
    __asm__ volatile (
        "movq $15, %rax\n\t"
        "syscall\n\t"
        "ud2");
}

int uname(struct utsname *buf)
{
    if (!buf || aukos_syscall1(AUKOS_SYS_UNAME, (uint64_t)buf) == UINT64_MAX_VALUE) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

void (*signal(int signum, void (*handler)(int)))(int)
{
    struct sigaction action;
    struct sigaction old_action;

    action.sa_handler = handler;
    action.sa_mask = 0;
    action.sa_flags = SA_RESTART;
    action.sa_restorer = 0;
    if (sigaction(signum, &action, &old_action) != 0) {
        return SIG_ERR;
    }
    return old_action.sa_handler;
}

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
    struct sigaction kernel_action;
    const struct sigaction *action = act;
    uint64_t result;

    if (act) {
        kernel_action = *act;
        if (act->sa_handler != SIG_DFL && act->sa_handler != SIG_IGN) {
            kernel_action.sa_flags |= SA_RESTORER;
            kernel_action.sa_restorer = signal_trampoline;
        } else {
            kernel_action.sa_restorer = 0;
        }
        action = &kernel_action;
    }

    result = aukos_syscall4(AUKOS_SYS_RT_SIGACTION, (uint64_t)signum,
                            (uint64_t)action, (uint64_t)oldact, sizeof(sigset_t));
    if (result == UINT64_MAX_VALUE) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

int sigemptyset(sigset_t *set)
{
    if (!set) {
        errno = EINVAL;
        return -1;
    }
    *set = 0;
    return 0;
}

int sigaddset(sigset_t *set, int signum)
{
    if (!set || signum <= 0 || signum >= NSIG) {
        errno = EINVAL;
        return -1;
    }
    *set |= 1ul << (unsigned long)signum;
    return 0;
}

int sigfillset(sigset_t *set)
{
    if (!set) {
        errno = EINVAL;
        return -1;
    }
    *set = ~1ul;
    return 0;
}

int sigdelset(sigset_t *set, int signum)
{
    if (!set || signum <= 0 || signum >= NSIG) {
        errno = EINVAL;
        return -1;
    }
    *set &= ~(1ul << (unsigned long)signum);
    return 0;
}

int sigismember(const sigset_t *set, int signum)
{
    if (!set || signum <= 0 || signum >= NSIG) {
        errno = EINVAL;
        return -1;
    }
    return (*set & (1ul << (unsigned long)signum)) != 0;
}

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    uint64_t result = aukos_syscall4(AUKOS_SYS_RT_SIGPROCMASK, (uint64_t)how,
                                     (uint64_t)set, (uint64_t)oldset, sizeof(sigset_t));
    if (result == UINT64_MAX_VALUE) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

int sigpending(sigset_t *set)
{
    uint64_t result;

    if (!set) {
        errno = EINVAL;
        return -1;
    }
    result = aukos_syscall2(AUKOS_SYS_RT_SIGPENDING, (uint64_t)set, sizeof(sigset_t));
    if (result == UINT64_MAX_VALUE) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

int kill(int pid, int sig)
{
    uint64_t result = aukos_syscall2(AUKOS_SYS_KILL, (uint64_t)pid, (uint64_t)sig);

    if (result == UINT64_MAX_VALUE) {
        errno = sig < 0 || sig >= NSIG ? EINVAL : ESRCH;
        return -1;
    }
    return 0;
}

int raise(int sig)
{
    uint64_t pid = aukos_syscall1(AUKOS_SYS_GETPID, 0);

    return kill((int)pid, sig);
}

char *setlocale(int category, const char *locale)
{
    (void)category;
    (void)locale;
    return "C";
}

char *nl_langinfo(nl_item item)
{
    if (item == CODESET) {
        return "UTF-8";
    }
    return "";
}
