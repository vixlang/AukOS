#ifndef AUKOS_SYSCALL_H
#define AUKOS_SYSCALL_H

#include <stdint.h>

/* Linux-compatible syscall numbers */
#define SYS_READ 0u
#define SYS_WRITE 1u
#define SYS_OPEN 2u
#define SYS_CLOSE 3u
#define SYS_STAT 4u
#define SYS_FSTAT 5u
#define SYS_LSEEK 8u
#define SYS_MMAP 9u
#define SYS_MPROTECT 10u
#define SYS_MUNMAP 11u
#define SYS_BRK 12u
#define SYS_RT_SIGACTION 13u
#define SYS_RT_SIGPROCMASK 14u
#define SYS_RT_SIGRETURN 15u
#define SYS_IOCTL 16u
#define SYS_PIPE 22u
#define SYS_DUP 32u
#define SYS_DUP2 33u
#define SYS_GETPID 39u
#define SYS_SOCKET 41u
#define SYS_CONNECT 42u
#define SYS_ACCEPT 43u
#define SYS_BIND 49u
#define SYS_LISTEN 50u
#define SYS_SENDTO 44u
#define SYS_RECVFROM 45u
#define SYS_FORK 57u
#define SYS_EXECVE 59u
#define SYS_EXIT 60u
#define SYS_WAIT4 61u
#define SYS_KILL 62u
#define SYS_UNAME 63u
#define SYS_FSYNC 74u
#define SYS_TRUNCATE 76u
#define SYS_FTRUNCATE 77u
#define SYS_CHDIR 80u
#define SYS_RENAME 82u
#define SYS_MKDIR 83u
#define SYS_UNLINK 87u
#define SYS_RT_SIGPENDING 127u
#define SYS_GETDENTS64 217u
#define SYS_READLINKAT 267u
#define SYS_RMDIR 1029u

/* AukOS-specific syscall numbers */
#define SYS_TIME 1024u
#define SYS_RANDOM 1025u
#define SYS_READDIR 1026u
#define SYS_FSINFO 1027u
#define SYS_GETCWD 1028u

struct user_context;

void syscall_init(void);
void syscall_run_selftest(void);
void syscall_run_vfs_selftest(void);
void syscall_dispatch(struct user_context *ctx);

#endif
