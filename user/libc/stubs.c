#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <mntent.h>
#include <netdb.h>
#include <pwd.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <locale.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/personality.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <sys/xattr.h>
#include <sys/inotify.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <wchar.h>
#include <wctype.h>

#define EAI_NONAME 8

int abs(int j) { return j < 0 ? -j : j; }
long labs(long j) { return j < 0 ? -j : j; }
long long llabs(long long j) { return j < 0 ? -j : j; }


char *strndup(const char *s, size_t n)
{
    size_t len = 0;
    while (len < n && s[len]) len++;
    char *copy = malloc(len + 1);
    if (copy) { memcpy(copy, s, len); copy[len] = '\0'; }
    return copy;
}

char *strcat(char *dest, const char *src) { char *out = dest + strlen(dest); while ((*out++ = *src++)) {} return dest; }
size_t strcspn(const char *s, const char *reject) { size_t n = 0; while (s[n] && !strchr(reject, s[n])) n++; return n; }
size_t strspn(const char *s, const char *accept) { size_t n = 0; while (s[n] && strchr(accept, s[n])) n++; return n; }
void *memchr(const void *s, int c, size_t n) { const unsigned char *p = s; for (size_t i = 0; i < n; i++) if (p[i] == (unsigned char)c) return (void *)(p + i); return 0; }

mode_t umask(mode_t mask) { (void)mask; return 0; }
unsigned int sleep(unsigned int seconds) { (void)seconds; return 0; }
ssize_t readlink(const char *path, char *buf, size_t bufsize) { (void)path; (void)buf; (void)bufsize; errno = ENOSYS; return -1; }
pid_t setsid(void) { return getpid(); }
char *crypt(const char *key, const char *salt) { (void)key; (void)salt; return "*"; }

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *))
{
    char *arr = base;
    char tmp[256];
    if (size > sizeof(tmp)) return;
    for (size_t i = 0; i < nmemb - 1; i++) {
        for (size_t j = i + 1; j < nmemb; j++) {
            if (compar(arr + i * size, arr + j * size) > 0) {
                memcpy(tmp, arr + i * size, size);
                memcpy(arr + i * size, arr + j * size, size);
                memcpy(arr + j * size, tmp, size);
            }
        }
    }
}

int sscanf(const char *str, const char *format, ...)
{
    (void)str;
    (void)format;
    return 0;
}

int dprintf(int fd, const char *format, ...)
{
    va_list ap;
    char buf[512];
    int len;
    va_start(ap, format);
    len = vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    write(fd, buf, strlen(buf));
    return len;
}

ssize_t getdelim(char **lineptr, size_t *n, int delim, FILE *stream)
{
    (void)lineptr; (void)n; (void)delim; (void)stream;
    errno = ENOSYS;
    return -1;
}

ssize_t getline(char **lineptr, size_t *n, FILE *stream)
{
    return getdelim(lineptr, n, '\n', stream);
}

int setvbuf(FILE *stream, char *buf, int mode, size_t size) { (void)stream; (void)buf; (void)mode; (void)size; return 0; }
char *mkdtemp(char *template) { (void)template; errno = ENOSYS; return 0; }
int chroot(const char *path) { (void)path; errno = ENOSYS; return -1; }
int initgroups(const char *user, gid_t group) { (void)user; (void)group; return 0; }

locale_t newlocale(int category_mask, const char *locale, locale_t base) { (void)category_mask; (void)locale; (void)base; return 0; }
locale_t uselocale(locale_t locale) { (void)locale; return 0; }
void freelocale(locale_t locale) { (void)locale; }

int tcflush(int fd, int queue_selector) { (void)fd; (void)queue_selector; return 0; }
int cfsetspeed(struct termios *termios_p, speed_t speed) { (void)termios_p; (void)speed; return 0; }
void cfmakeraw(struct termios *termios_p) { if (termios_p) termios_p->c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN); }

int sigsuspend(const sigset_t *mask) { (void)mask; errno = ENOSYS; return -1; }
int killpg(int pgrp, int sig) { (void)pgrp; (void)sig; errno = ENOSYS; return -1; }

time_t mktime(struct tm *tm) { (void)tm; errno = ENOSYS; return (time_t)-1; }
char *strptime(const char *s, const char *format, struct tm *tm) { (void)s; (void)format; if (tm) memset(tm, 0, sizeof(*tm)); return 0; }
void tzset(void) {}
char *ctime(const time_t *timer) { (void)timer; return "Thu Jan  1 00:00:00 1970\n"; }

int mkdirat(int dirfd, const char *path, mode_t mode) { (void)dirfd; (void)path; (void)mode; errno = ENOSYS; return -1; }
int fchmodat(int dirfd, const char *path, mode_t mode, int flags) { (void)dirfd; (void)path; (void)mode; (void)flags; errno = ENOSYS; return -1; }
int fchmod(int fd, mode_t mode) { (void)fd; (void)mode; errno = ENOSYS; return -1; }
int chmod(const char *path, mode_t mode) { (void)path; (void)mode; errno = ENOSYS; return -1; }
int mknod(const char *path, mode_t mode, dev_t dev) { (void)path; (void)mode; (void)dev; errno = ENOSYS; return -1; }
int mknodat(int dirfd, const char *path, mode_t mode, dev_t dev) { (void)dirfd; (void)path; (void)mode; (void)dev; errno = ENOSYS; return -1; }
int mkfifo(const char *path, mode_t mode) { return mknod(path, mode | S_IFIFO, 0); }
int mkfifoat(int dirfd, const char *path, mode_t mode) { return mknodat(dirfd, path, mode | S_IFIFO, 0); }
int lstat(const char *path, struct stat *buf) { return stat(path, buf); }
int chown(const char *path, uid_t owner, gid_t group) { (void)path; (void)owner; (void)group; errno = ENOSYS; return -1; }
int fchownat(int dirfd, const char *path, uid_t owner, gid_t group, int flags) { (void)dirfd; (void)path; (void)owner; (void)group; (void)flags; errno = ENOSYS; return -1; }
int utimensat(int dirfd, const char *path, const struct timespec times[2], int flags) { (void)dirfd; (void)path; (void)times; (void)flags; errno = ENOSYS; return -1; }

int getpwnam_r(const char *name, struct passwd *pwd, char *buf, size_t buflen, struct passwd **result) { (void)name; (void)pwd; (void)buf; (void)buflen; if (result) *result = 0; return 0; }
int getpwuid_r(uid_t uid, struct passwd *pwd, char *buf, size_t buflen, struct passwd **result) { (void)uid; (void)pwd; (void)buf; (void)buflen; if (result) *result = 0; return 0; }
int getgrnam_r(const char *name, struct group *grp, char *buf, size_t buflen, struct group **result) { (void)name; (void)grp; (void)buf; (void)buflen; if (result) *result = 0; return 0; }
int getgrgid_r(gid_t gid, struct group *grp, char *buf, size_t buflen, struct group **result) { (void)gid; (void)grp; (void)buf; (void)buflen; if (result) *result = 0; return 0; }

int rmdir(const char *path) { (void)path; errno = ENOSYS; return -1; }
int fchown(int fd, uid_t owner, gid_t group) { (void)fd; (void)owner; (void)group; errno = ENOSYS; return -1; }
int lchown(const char *path, uid_t owner, gid_t group) { (void)path; (void)owner; (void)group; errno = ENOSYS; return -1; }
int symlink(const char *target, const char *linkpath) { (void)target; (void)linkpath; errno = ENOSYS; return -1; }
int link(const char *oldpath, const char *newpath) { (void)oldpath; (void)newpath; errno = ENOSYS; return -1; }
pid_t getppid(void) { return 0; }
pid_t getpgrp(void) { return getpid(); }
int setpgid(pid_t pid, pid_t pgid) { (void)pid; (void)pgid; errno = ENOSYS; return -1; }
int setuid(uid_t uid) { (void)uid; return 0; }
int setgid(gid_t gid) { (void)gid; return 0; }
uid_t getuid(void) { return 0; }
gid_t getgid(void) { return 0; }
uid_t geteuid(void) { return 0; }
gid_t getegid(void) { return 0; }
unsigned int alarm(unsigned int seconds) { (void)seconds; return 0; }
int nice(int inc) { (void)inc; errno = ENOSYS; return -1; }
int pause(void) { errno = ENOSYS; return -1; }
void sync(void) {}
long sysconf(int name) { (void)name; return -1; }
long pathconf(const char *path, int name) { (void)path; (void)name; return -1; }
int getgroups(int size, gid_t list[]) { (void)size; (void)list; return 0; }
int setgroups(size_t size, const gid_t *list[]) { (void)size; (void)list; errno = ENOSYS; return -1; }
char *getlogin(void) { return "root"; }
int gethostname(char *name, size_t len) { (void)name; (void)len; errno = ENOSYS; return -1; }
int sethostname(const char *name, size_t len) { (void)name; (void)len; errno = ENOSYS; return -1; }
char *ttyname(int fd) { (void)fd; return 0; }
int ttyname_r(int fd, char *buf, size_t buflen) { (void)fd; (void)buf; (void)buflen; errno = ENOSYS; return -1; }
int getpagesize(void) { return 4096; }
int closefrom(int lowfd) { for (int i = lowfd; i < 1024; i++) close(i); return 0; }
ssize_t pread(int fd, void *buf, size_t count, off_t offset) { (void)offset; return read(fd, buf, count); }
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset) { (void)offset; return write(fd, buf, count); }
pid_t wait(int *status) { return waitpid(-1, status, 0); }

int mount(const char *source, const char *target, const char *filesystemtype, unsigned long mountflags, const void *data) { (void)source; (void)target; (void)filesystemtype; (void)mountflags; (void)data; errno = ENOSYS; return -1; }
int umount(const char *target) { (void)target; errno = ENOSYS; return -1; }
int umount2(const char *target, int flags) { (void)target; (void)flags; errno = ENOSYS; return -1; }
int personality(unsigned long persona) { (void)persona; errno = ENOSYS; return -1; }
long syscall(long number, ...) { (void)number; errno = ENOSYS; return -1; }

int inotify_init(void) { errno = ENOSYS; return -1; }
int inotify_init1(int flags) { (void)flags; errno = ENOSYS; return -1; }
int inotify_add_watch(int fd, const char *pathname, uint32_t mask) { (void)fd; (void)pathname; (void)mask; errno = ENOSYS; return -1; }
int inotify_rm_watch(int fd, int wd) { (void)fd; (void)wd; errno = ENOSYS; return -1; }

int swapon(const char *path, int swapflags) { (void)path; (void)swapflags; errno = ENOSYS; return -1; }
int swapoff(const char *path) { (void)path; errno = ENOSYS; return -1; }
int sysinfo(struct sysinfo *info) { (void)info; errno = ENOSYS; return -1; }

ssize_t getxattr(const char *path, const char *name, void *value, size_t size) { (void)path; (void)name; (void)value; (void)size; errno = ENOSYS; return -1; }
ssize_t lgetxattr(const char *path, const char *name, void *value, size_t size) { return getxattr(path, name, value, size); }
ssize_t fgetxattr(int fd, const char *name, void *value, size_t size) { (void)fd; (void)name; (void)value; (void)size; errno = ENOSYS; return -1; }
ssize_t listxattr(const char *path, char *list, size_t size) { (void)path; (void)list; (void)size; errno = ENOSYS; return -1; }
ssize_t llistxattr(const char *path, char *list, size_t size) { return listxattr(path, list, size); }
ssize_t flistxattr(int fd, char *list, size_t size) { (void)fd; (void)list; (void)size; errno = ENOSYS; return -1; }
int setxattr(const char *path, const char *name, const void *value, size_t size, int flags) { (void)path; (void)name; (void)value; (void)size; (void)flags; errno = ENOSYS; return -1; }
int lsetxattr(const char *path, const char *name, const void *value, size_t size, int flags) { return setxattr(path, name, value, size, flags); }
int fsetxattr(int fd, const char *name, const void *value, size_t size, int flags) { (void)fd; (void)name; (void)value; (void)size; (void)flags; errno = ENOSYS; return -1; }

int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res) { (void)node; (void)service; (void)hints; if (res) *res = 0; return EAI_NONAME; }
void freeaddrinfo(struct addrinfo *res) { (void)res; }
const char *gai_strerror(int errcode) { (void)errcode; return "address info error"; }
struct hostent *gethostbyname(const char *name) { (void)name; errno = ENOSYS; return 0; }
const char *hstrerror(int errcode_val) { (void)errcode_val; return "host error"; }

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) { (void)sockfd; (void)level; (void)optname; (void)optval; (void)optlen; errno = ENOSYS; return -1; }
int shutdown(int sockfd, int how) { (void)sockfd; (void)how; errno = ENOSYS; return -1; }

void vsyslog(int priority, const char *format, va_list ap) { (void)priority; vfprintf(stderr, format, ap); }

int isxdigit(int c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
wint_t towlower(wint_t wc) { return isupper((int)wc) ? (wint_t)(wc + 32) : wc; }


struct passwd *getpwnam(const char *name) { (void)name; return 0; }
struct passwd *getpwuid(uid_t uid) { (void)uid; return 0; }
struct group *getgrnam(const char *name) { (void)name; return 0; }
struct group *getgrgid(gid_t gid) { (void)gid; return 0; }

FILE *setmntent(const char *filep, const char *type) { (void)filep; (void)type; return 0; }
struct mntent *getmntent(FILE *stream) { (void)stream; return 0; }
int endmntent(FILE *streamp) { (void)streamp; return 0; }
char *hasmntopt(const struct mntent *mnt, const char *opt) { (void)mnt; (void)opt; return 0; }

int fcntl(int fd, int cmd, ...) { (void)fd; (void)cmd; errno = ENOSYS; return -1; }
