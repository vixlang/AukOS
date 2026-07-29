#ifndef AUKOS_USER_SYSCALL_H
#define AUKOS_USER_SYSCALL_H

#include <stdint.h>

#define AUKOS_SYS_READ 0u
#define AUKOS_SYS_WRITE 1u
#define AUKOS_SYS_OPEN 2u
#define AUKOS_SYS_CLOSE 3u
#define AUKOS_SYS_STAT 4u
#define AUKOS_SYS_FSTAT 5u
#define AUKOS_SYS_LSEEK 8u
#define AUKOS_SYS_MMAP 9u
#define AUKOS_SYS_MPROTECT 10u
#define AUKOS_SYS_MUNMAP 11u
#define AUKOS_SYS_BRK 12u
#define AUKOS_SYS_RT_SIGACTION 13u
#define AUKOS_SYS_RT_SIGPROCMASK 14u
#define AUKOS_SYS_RT_SIGRETURN 15u
#define AUKOS_SYS_IOCTL 16u
#define AUKOS_SYS_PIPE 22u
#define AUKOS_SYS_DUP2 33u
#define AUKOS_SYS_DUP 32u
#define AUKOS_SYS_GETPID 39u
#define AUKOS_SYS_SOCKET 41u
#define AUKOS_SYS_CONNECT 42u
#define AUKOS_SYS_ACCEPT 43u
#define AUKOS_SYS_SENDTO 44u
#define AUKOS_SYS_RECVFROM 45u
#define AUKOS_SYS_BIND 49u
#define AUKOS_SYS_LISTEN 50u
#define AUKOS_SYS_FORK 57u
#define AUKOS_SYS_EXECVE 59u
#define AUKOS_SYS_EXIT 60u
#define AUKOS_SYS_WAITPID 61u
#define AUKOS_SYS_KILL 62u
#define AUKOS_SYS_UNAME 63u
#define AUKOS_SYS_FSYNC 74u
#define AUKOS_SYS_TRUNCATE 76u
#define AUKOS_SYS_FTRUNCATE 77u
#define AUKOS_SYS_CHDIR 80u
#define AUKOS_SYS_RENAME 82u
#define AUKOS_SYS_MKDIR 83u
#define AUKOS_SYS_UNLINK 87u
#define AUKOS_SYS_RT_SIGPENDING 127u
#define AUKOS_SYS_GETDENTS64 217u
#define AUKOS_SYS_READLINKAT 267u
#define AUKOS_SYS_RMDIR 1029u
#define AUKOS_SYS_TIME 1024u
#define AUKOS_SYS_RANDOM 1025u
#define AUKOS_SYS_READDIR 1026u
#define AUKOS_SYS_FSINFO 1027u
#define AUKOS_SYS_GETCWD 1028u
#define AUKOS_SYS_ICMP_ECHO 1030u

static inline uint64_t aukos_syscall1(uint64_t number, uint64_t arg0)
{
    uint64_t result;
    __asm__ volatile ("syscall" : "=a"(result) : "a"(number), "D"(arg0) : "rcx", "r11", "memory");
    return result;
}

static inline uint64_t aukos_syscall2(uint64_t number, uint64_t arg0, uint64_t arg1)
{
    uint64_t result;
    __asm__ volatile ("syscall" : "=a"(result) : "a"(number), "D"(arg0), "S"(arg1) : "rcx", "r11", "memory");
    return result;
}

static inline uint64_t aukos_syscall3(uint64_t number, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
    uint64_t result;
    __asm__ volatile ("syscall" : "=a"(result) : "a"(number), "D"(arg0), "S"(arg1), "d"(arg2) : "rcx", "r11", "memory");
    return result;
}

static inline uint64_t aukos_syscall4(uint64_t number, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
    uint64_t result;
    register uint64_t r10 __asm__("r10") = arg3;
    __asm__ volatile ("syscall" : "=a"(result) : "a"(number), "D"(arg0), "S"(arg1), "d"(arg2), "r"(r10) : "rcx", "r11", "memory");
    return result;
}

static inline uint64_t aukos_syscall5(uint64_t number, uint64_t arg0,
                                      uint64_t arg1, uint64_t arg2,
                                      uint64_t arg3, uint64_t arg4)
{
    uint64_t result;
    register uint64_t r10 __asm__("r10") = arg3;
    register uint64_t r8 __asm__("r8") = arg4;
    __asm__ volatile ("syscall" : "=a"(result) : "a"(number), "D"(arg0),
                      "S"(arg1), "d"(arg2), "r"(r10), "r"(r8) :
                      "rcx", "r11", "memory");
    return result;
}

static inline uint64_t aukos_syscall6(uint64_t number, uint64_t arg0,
                                      uint64_t arg1, uint64_t arg2,
                                      uint64_t arg3, uint64_t arg4,
                                      uint64_t arg5)
{
    uint64_t result;
    register uint64_t r10 __asm__("r10") = arg3;
    register uint64_t r8 __asm__("r8") = arg4;
    register uint64_t r9 __asm__("r9") = arg5;
    __asm__ volatile ("syscall" : "=a"(result) : "a"(number), "D"(arg0),
                      "S"(arg1), "d"(arg2), "r"(r10), "r"(r8), "r"(r9) :
                      "rcx", "r11", "memory");
    return result;
}

#endif
