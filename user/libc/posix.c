#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <poll.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "aukos/syscall.h"
#define UINT64_MAX_VALUE 0xffffffffffffffffull

#define VFS_FILE_REGULAR 1u
#define VFS_FILE_DIRECTORY 2u
#define VFS_FILE_SYMLINK 3u
#define VFS_FILE_DEVICE 4u

int errno;

struct aukos_vfs_stat {
    uint64_t ino;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint64_t size;
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
    uint32_t type;
};

#define LIBC_FD_PATH_MAX 64
static char libc_fd_paths[LIBC_FD_PATH_MAX][256];
static unsigned char libc_fd_path_valid[LIBC_FD_PATH_MAX];

static int copy_path(char *out, size_t capacity, const char *path)
{
    size_t length = strlen(path);
    if (length + 1u > capacity) return -1;
    memcpy(out, path, length + 1u);
    return 0;
}

static int path_from_dirfd(int dirfd, const char *path, char *out,
                           size_t capacity)
{
    char cwd[256];
    const char *base = 0;
    size_t base_length;
    size_t path_length;

    if (!path || !out || !capacity) return -1;
    if (path[0] == '/') return copy_path(out, capacity, path);
    if (dirfd >= 0 && dirfd < LIBC_FD_PATH_MAX && libc_fd_path_valid[dirfd]) {
        base = libc_fd_paths[dirfd];
    } else {
        if (!getcwd(cwd, sizeof(cwd))) return -1;
        base = cwd;
    }
    base_length = strlen(base);
    path_length = strlen(path);
    if (base_length + 1u + path_length + 1u > capacity) return -1;
    memcpy(out, base, base_length);
    if (base_length == 0u || out[base_length - 1u] != '/') out[base_length++] = '/';
    memcpy(out + base_length, path, path_length + 1u);
    return 0;
}

static void remember_fd_path(int fd, const char *path)
{
    char resolved[256];
    if (fd < 0 || fd >= LIBC_FD_PATH_MAX ||
        path_from_dirfd(AT_FDCWD, path, resolved, sizeof(resolved)) != 0) return;
    if (copy_path(libc_fd_paths[fd], sizeof(libc_fd_paths[fd]), resolved) == 0)
        libc_fd_path_valid[fd] = 1u;
}

static ssize_t syscall_result(uint64_t result)
{
    if (result == UINT64_MAX_VALUE) {
        errno = ENOSYS;
        return -1;
    }

    return (ssize_t)result;
}

void *mmap(void *address, size_t length, int protection, int flags, int fd,
           off_t offset)
{
    uint64_t result = aukos_syscall6(AUKOS_SYS_MMAP, (uint64_t)address,
                                     length, (uint64_t)protection,
                                     (uint64_t)flags, (uint64_t)fd,
                                     (uint64_t)offset);

    if (result == UINT64_MAX_VALUE) {
        errno = EINVAL;
        return MAP_FAILED;
    }
    return (void *)result;
}

int munmap(void *address, size_t length)
{
    uint64_t result = aukos_syscall2(AUKOS_SYS_MUNMAP, (uint64_t)address,
                                     length);

    if (result == UINT64_MAX_VALUE) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

int mprotect(void *address, size_t length, int protection)
{
    uint64_t result = aukos_syscall3(AUKOS_SYS_MPROTECT, (uint64_t)address,
                                     length, (uint64_t)protection);

    if (result == UINT64_MAX_VALUE) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

int brk(void *address)
{
    uint64_t requested = (uint64_t)address;
    uint64_t result = aukos_syscall1(AUKOS_SYS_BRK, requested);

    if (result != requested) {
        errno = ENOMEM;
        return -1;
    }
    return 0;
}

void *sbrk(intptr_t increment)
{
    uintptr_t current = (uintptr_t)aukos_syscall1(AUKOS_SYS_BRK, 0);
    uintptr_t requested;
    uint64_t result;

    if ((increment > 0 && (uintptr_t)increment > (uintptr_t)-1 - current) ||
        (increment < 0 && (uintptr_t)(-(increment + 1)) + 1u > current)) {
        errno = ENOMEM;
        return (void *)-1;
    }
    requested = increment < 0 ? current - ((uintptr_t)(-(increment + 1)) + 1u)
                              : current + (uintptr_t)increment;
    result = aukos_syscall1(AUKOS_SYS_BRK, requested);
    if (result != requested) {
        errno = ENOMEM;
        return (void *)-1;
    }
    return (void *)current;
}

ssize_t read(int fd, void *buffer, size_t count)
{
    return syscall_result(aukos_syscall3(AUKOS_SYS_READ, (uint64_t)fd, (uint64_t)buffer, count));
}

ssize_t write(int fd, const void *buffer, size_t count)
{
    return syscall_result(aukos_syscall3(AUKOS_SYS_WRITE, (uint64_t)fd, (uint64_t)buffer, count));
}

int close(int fd)
{
    int result = (int)syscall_result(aukos_syscall1(AUKOS_SYS_CLOSE, (uint64_t)fd));
    if (result == 0 && fd >= 0 && fd < LIBC_FD_PATH_MAX)
        libc_fd_path_valid[fd] = 0u;
    return result;
}

off_t lseek(int fd, off_t offset, int whence)
{
    return (off_t)syscall_result(aukos_syscall3(AUKOS_SYS_LSEEK, (uint64_t)fd, (uint64_t)offset, (uint64_t)whence));
}

int open(const char *path, int flags, ...)
{
    va_list ap;
    int mode = 0;

    if (flags & O_CREAT) {
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
    }

    int fd = (int)syscall_result(aukos_syscall3(AUKOS_SYS_OPEN, (uint64_t)path, (uint64_t)flags, (uint64_t)mode));
    if (fd >= 0) remember_fd_path(fd, path);
    return fd;
}

int openat(int dirfd, const char *path, int flags, ...)
{
    va_list ap;
    int mode = 0;
    char resolved[256];

    (void)dirfd;

    if (flags & O_CREAT) {
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
    }

    if (path_from_dirfd(dirfd, path, resolved, sizeof(resolved)) != 0) {
        errno = ENAMETOOLONG;
        return -1;
    }
    return open(resolved, flags, mode);
}

int mkdir(const char *path, mode_t mode)
{
    return (int)syscall_result(aukos_syscall2(AUKOS_SYS_MKDIR, (uint64_t)path, (uint64_t)mode));
}

int mkdirat(int dirfd, const char *path, mode_t mode)
{
    char resolved[256];
    if (path_from_dirfd(dirfd, path, resolved, sizeof(resolved)) != 0) {
        errno = ENAMETOOLONG;
        return -1;
    }
    return mkdir(resolved, mode);
}

int unlink(const char *path)
{
    return (int)syscall_result(aukos_syscall1(AUKOS_SYS_UNLINK, (uint64_t)path));
}

int rmdir(const char *path)
{
    return (int)syscall_result(aukos_syscall1(AUKOS_SYS_RMDIR, (uint64_t)path));
}

int unlinkat(int dirfd, const char *path, int flags)
{
    char resolved[256];
    if ((flags & ~AT_REMOVEDIR) != 0 ||
        path_from_dirfd(dirfd, path, resolved, sizeof(resolved)) != 0) {
        errno = EINVAL;
        return -1;
    }
    return (flags & AT_REMOVEDIR) ? rmdir(resolved) : unlink(resolved);
}

int faccessat(int dirfd, const char *path, int mode, int flags)
{
    char resolved[256];
    struct stat st;
    mode_t allowed = 0;
    (void)flags;
    if (path_from_dirfd(dirfd, path, resolved, sizeof(resolved)) != 0) {
        errno = ENAMETOOLONG;
        return -1;
    }
    if (stat(resolved, &st) != 0) return -1;
    if (mode == F_OK) return 0;
    if (mode & ~(R_OK | W_OK | X_OK)) { errno = EINVAL; return -1; }
    if (st.st_mode & 0444u) allowed |= R_OK;
    if (st.st_mode & 0222u) allowed |= W_OK;
    if (st.st_mode & 0111u) allowed |= X_OK;
    if (((mode_t)mode & allowed) != (mode_t)mode) { errno = EACCES; return -1; }
    return 0;
}

int fsync(int fd)
{
    return (int)syscall_result(aukos_syscall1(AUKOS_SYS_FSYNC, (uint64_t)fd));
}

int truncate(const char *path, off_t length)
{
    return (int)syscall_result(aukos_syscall2(AUKOS_SYS_TRUNCATE,
                                               (uint64_t)path,
                                               (uint64_t)length));
}

int ftruncate(int fd, off_t length)
{
    return (int)syscall_result(aukos_syscall2(AUKOS_SYS_FTRUNCATE,
                                               (uint64_t)fd,
                                               (uint64_t)length));
}

int rename(const char *old_path, const char *new_path)
{
    return (int)syscall_result(aukos_syscall2(AUKOS_SYS_RENAME,
                                               (uint64_t)old_path,
                                               (uint64_t)new_path));
}

void _exit(int code)
{
    for (;;) {
        aukos_syscall1(AUKOS_SYS_EXIT, (uint64_t)code);
    }
}

static void convert_stat(const struct aukos_vfs_stat *src, struct stat *dst)
{
    uint32_t type_bits = S_IFREG;

    if (src->type == VFS_FILE_DIRECTORY) {
        type_bits = S_IFDIR;
    } else if (src->type == VFS_FILE_SYMLINK) {
        type_bits = S_IFLNK;
    } else if (src->type == VFS_FILE_DEVICE) {
        type_bits = S_IFCHR;
    }

    dst->st_dev = 0;
    dst->st_ino = src->ino;
    dst->st_mode = type_bits | src->mode;
    dst->st_nlink = 1;
    dst->st_uid = src->uid;
    dst->st_gid = src->gid;
    dst->st_rdev = 0;
    dst->st_size = (off_t)src->size;
    dst->st_blksize = 4096;
    dst->st_blocks = (int64_t)((src->size + 511u) / 512u);
    dst->st_atime = (time_t)src->atime;
    dst->st_mtime = (time_t)src->mtime;
    dst->st_ctime = (time_t)src->ctime;
}

int stat(const char *path, struct stat *buf)
{
    struct aukos_vfs_stat raw;
    if (syscall_result(aukos_syscall2(AUKOS_SYS_STAT, (uint64_t)path, (uint64_t)&raw)) != 0) {
        return -1;
    }
    convert_stat(&raw, buf);
    return 0;
}

int fstatat(int dirfd, const char *path, struct stat *buf, int flags)
{
    (void)flags;
    char resolved[256];
    if (path_from_dirfd(dirfd, path, resolved, sizeof(resolved)) != 0) {
        errno = ENAMETOOLONG;
        return -1;
    }
    return stat(resolved, buf);
}

int fstat(int fd, struct stat *buf)
{
    struct aukos_vfs_stat raw;
    if (syscall_result(aukos_syscall2(AUKOS_SYS_FSTAT, (uint64_t)fd, (uint64_t)&raw)) != 0) {
        return -1;
    }
    convert_stat(&raw, buf);
    return 0;
}

int getdents64(int fd, struct dirent64 *dirp, unsigned int count)
{
    return (int)syscall_result(aukos_syscall3(AUKOS_SYS_GETDENTS64, (uint64_t)fd, (uint64_t)dirp, (uint64_t)count));
}

struct DIR {
    int fd;
    unsigned int index;
    unsigned int count;
    struct dirent64 entries[16];
};

DIR *fdopendir(int fd)
{
    DIR *dir = malloc(sizeof(DIR));

    if (!dir) {
        errno = EINVAL;
        return 0;
    }

    dir->fd = fd;
    dir->index = 0;
    dir->count = 0;
    return dir;
}

DIR *opendir(const char *name)
{
    int fd = open(name, O_RDONLY);

    if (fd < 0) {
        return 0;
    }

    return fdopendir(fd);
}

struct dirent64 *readdir64(DIR *dirp)
{
    int bytes;

    if (!dirp) {
        errno = EBADF;
        return 0;
    }

    if (dirp->index >= dirp->count) {
        bytes = getdents64(dirp->fd, dirp->entries, sizeof(dirp->entries));
        if (bytes <= 0) {
            return 0;
        }
        dirp->index = 0;
        dirp->count = (unsigned int)(bytes / sizeof(struct dirent64));
    }

    return &dirp->entries[dirp->index++];
}

struct dirent *readdir(DIR *dirp)
{
    return (struct dirent *)readdir64(dirp);
}

int closedir(DIR *dirp)
{
    int result;

    if (!dirp) {
        errno = EBADF;
        return -1;
    }

    result = close(dirp->fd);
    free(dirp);
    return result;
}

char *getcwd(char *buffer, size_t size)
{
    int allocated = 0;

    if (!buffer) {
        if (size == 0) {
            size = 2;
        }
        buffer = malloc(size);
        if (!buffer) {
            errno = ENOMEM;
            return 0;
        }
        allocated = 1;
    }
    if (syscall_result(aukos_syscall2(AUKOS_SYS_GETCWD, (uint64_t)buffer, size)) < 0) {
        if (allocated) {
            free(buffer);
        }
        return 0;
    }

    return buffer;
}

int chdir(const char *path)
{
    return (int)syscall_result(aukos_syscall1(AUKOS_SYS_CHDIR,
                                               (uint64_t)path));
}

ssize_t readlinkat(int dirfd, const char *path, char *buffer, size_t size)
{
    return syscall_result(aukos_syscall4(AUKOS_SYS_READLINKAT, (uint64_t)dirfd, (uint64_t)path, (uint64_t)buffer, size));
}

pid_t getpid(void)
{
    return (pid_t)aukos_syscall1(AUKOS_SYS_GETPID, 0);
}

int dup2(int oldfd, int newfd)
{
    int result = (int)syscall_result(aukos_syscall2(AUKOS_SYS_DUP2, (uint64_t)oldfd, (uint64_t)newfd));
    if (result >= 0 && newfd >= 0 && newfd < LIBC_FD_PATH_MAX) {
        if (oldfd >= 0 && oldfd < LIBC_FD_PATH_MAX && libc_fd_path_valid[oldfd]) {
            copy_path(libc_fd_paths[newfd], sizeof(libc_fd_paths[newfd]),
                      libc_fd_paths[oldfd]);
            libc_fd_path_valid[newfd] = 1u;
        } else {
            libc_fd_path_valid[newfd] = 0u;
        }
    }
    return result;
}

int dup(int oldfd)
{
    int result = (int)syscall_result(aukos_syscall1(AUKOS_SYS_DUP, (uint64_t)oldfd));
    if (result >= 0 && result < LIBC_FD_PATH_MAX && oldfd >= 0 &&
        oldfd < LIBC_FD_PATH_MAX && libc_fd_path_valid[oldfd]) {
        copy_path(libc_fd_paths[result], sizeof(libc_fd_paths[result]),
                  libc_fd_paths[oldfd]);
        libc_fd_path_valid[result] = 1u;
    }
    return result;
}

pid_t fork(void)
{
    return (pid_t)syscall_result(aukos_syscall1(AUKOS_SYS_FORK, 0));
}

int execve(const char *path, char *const argv[], char *const envp[])
{
    return (int)syscall_result(aukos_syscall3(AUKOS_SYS_EXECVE, (uint64_t)path, (uint64_t)argv, (uint64_t)envp));
}

pid_t waitpid(pid_t pid, int *status, int options)
{
    return (pid_t)syscall_result(aukos_syscall3(AUKOS_SYS_WAITPID, (uint64_t)pid, (uint64_t)status, (uint64_t)options));
}

int pipe(int pipefd[2])
{
    return (int)syscall_result(aukos_syscall1(AUKOS_SYS_PIPE, (uint64_t)pipefd));
}

int isatty(int fd)
{
    struct winsize size;

    return ioctl(fd, TIOCGWINSZ, &size) == 0;
}

int ioctl(int fd, unsigned long request, ...)
{
    va_list ap;
    void *argument;

    va_start(ap, request);
    argument = va_arg(ap, void *);
    va_end(ap);
    if (!argument) {
        errno = EINVAL;
        return -1;
    }
    return (int)syscall_result(aukos_syscall3(AUKOS_SYS_IOCTL, (uint64_t)fd,
                                               request,
                                               (uint64_t)argument));
}

int tcgetattr(int fd, struct termios *termios_p)
{
    if (!termios_p) {
        errno = EINVAL;
        return -1;
    }
    return ioctl(fd, TCGETS, termios_p);
}

int tcsetattr(int fd, int optional_actions, const struct termios *termios_p)
{
    if (!termios_p || (optional_actions != TCSANOW &&
                       optional_actions != TCSADRAIN &&
                       optional_actions != TCSAFLUSH)) {
        errno = EINVAL;
        return -1;
    }
    return ioctl(fd, TCSETS, (void *)termios_p);
}

int poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    int ready = 0;

    (void)timeout;
    if (!fds && nfds) {
        errno = EINVAL;
        return -1;
    }

    for (nfds_t i = 0; i < nfds; i++) {
        fds[i].revents = 0;
        if (fds[i].fd >= 0) {
            fds[i].revents = fds[i].events & (POLLIN | POLLOUT);
            if (fds[i].revents) ready++;
        }
    }
    return ready;
}

void FD_ZERO(fd_set *set)
{
    if (set) set->bits = 0;
}

void FD_SET(int fd, fd_set *set)
{
    if (set && fd >= 0 && fd < FD_SETSIZE) set->bits |= 1ul << (unsigned)fd;
}

int FD_ISSET(int fd, const fd_set *set)
{
    return set && fd >= 0 && fd < FD_SETSIZE && ((set->bits & (1ul << (unsigned)fd)) != 0);
}

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
    int ready = 0;

    (void)exceptfds;
    (void)timeout;
    for (int fd = 0; fd < nfds; fd++) {
        if (readfds && FD_ISSET(fd, readfds)) ready++;
        if (writefds && FD_ISSET(fd, writefds)) ready++;
    }
    return ready;
}

int socket(int domain, int type, int protocol)
{
    return (int)syscall_result(aukos_syscall3(AUKOS_SYS_SOCKET, (uint64_t)domain, (uint64_t)type, (uint64_t)protocol));
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    return (int)syscall_result(aukos_syscall3(AUKOS_SYS_CONNECT, (uint64_t)sockfd, (uint64_t)addr, (uint64_t)addrlen));
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    return (int)syscall_result(aukos_syscall3(AUKOS_SYS_BIND, (uint64_t)sockfd, (uint64_t)addr, (uint64_t)addrlen));
}

int listen(int sockfd, int backlog)
{
    return (int)syscall_result(aukos_syscall2(AUKOS_SYS_LISTEN, (uint64_t)sockfd, (uint64_t)backlog));
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    return (int)syscall_result(aukos_syscall3(AUKOS_SYS_ACCEPT, (uint64_t)sockfd, (uint64_t)addr, (uint64_t)addrlen));
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
    return syscall_result(aukos_syscall6(AUKOS_SYS_SENDTO, (uint64_t)sockfd,
                          (uint64_t)buf, (uint64_t)len, (uint64_t)flags,
                          (uint64_t)dest_addr, (uint64_t)addrlen));
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{
    return syscall_result(aukos_syscall6(AUKOS_SYS_RECVFROM, (uint64_t)sockfd,
                          (uint64_t)buf, (uint64_t)len, (uint64_t)flags,
                          (uint64_t)src_addr, (uint64_t)addrlen));
}
