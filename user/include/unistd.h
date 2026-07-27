#ifndef AUKOS_UNISTD_H
#define AUKOS_UNISTD_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

typedef long ssize_t;
typedef long off_t;
typedef int pid_t;

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

ssize_t read(int fd, void *buffer, size_t count);
ssize_t write(int fd, const void *buffer, size_t count);
off_t lseek(int fd, off_t offset, int whence);
int close(int fd);
int isatty(int fd);
pid_t getpid(void);
int dup2(int oldfd, int newfd);
pid_t fork(void);
int execve(const char *path, char *const argv[], char *const envp[]);
int pipe(int pipefd[2]);
int brk(void *address);
void *sbrk(intptr_t increment);
char *getcwd(char *buffer, size_t size);
ssize_t readlinkat(int dirfd, const char *path, char *buffer, size_t size);
void _exit(int code) __attribute__((noreturn));

int chdir(const char *path);
int unlink(const char *path);
int unlinkat(int dirfd, const char *path, int flags);
int rmdir(const char *path);
int faccessat(int dirfd, const char *path, int mode, int flags);
int fchown(int fd, uid_t owner, gid_t group);
int lchown(const char *path, uid_t owner, gid_t group);
mode_t umask(mode_t mask);
int access(const char *path, int mode);
#define R_OK 4
#define W_OK 2
#define X_OK 1
#define F_OK 0
uid_t getuid(void);
gid_t getgid(void);
uid_t geteuid(void);
gid_t getegid(void);
pid_t getppid(void);
pid_t getpgrp(void);
pid_t setsid(void);
int setpgid(pid_t pid, pid_t pgid);
int setuid(uid_t uid);
int setgid(gid_t gid);
int setreuid(uid_t ruid, uid_t euid);
int setregid(uid_t rgid, uid_t egid);
unsigned int alarm(unsigned int seconds);
int nice(int inc);
int pause(void);
void sync(void);
int fsync(int fd);
int truncate(const char *path, off_t length);
int ftruncate(int fd, off_t length);
long sysconf(int name);
long pathconf(const char *path, int name);
int getgroups(int size, gid_t list[]);
int setgroups(size_t size, const gid_t *list[]);
char *getlogin(void);
int gethostname(char *name, size_t len);
int sethostname(const char *name, size_t len);
char *ttyname(int fd);
int ttyname_r(int fd, char *buf, size_t buflen);
int getpagesize(void);
int closefrom(int lowfd);

ssize_t pread(int fd, void *buf, size_t count, off_t offset);
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);
int link(const char *oldpath, const char *newpath);
int symlink(const char *target, const char *linkpath);
ssize_t readlink(const char *path, char *buf, size_t bufsize);
int dup(int oldfd);
pid_t vfork(void);
int execv(const char *path, char *const argv[]);
int execvp(const char *file, char *const argv[]);
int chroot(const char *path);
int initgroups(const char *user, gid_t group);
int utimensat(int dirfd, const char *path, const struct timespec times[2], int flags);
unsigned int sleep(unsigned int seconds);
char *crypt(const char *key, const char *salt);

#endif
