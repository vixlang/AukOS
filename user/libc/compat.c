#include <errno.h>
#include <math.h>
#include <regex.h>
#include <sched.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <syslog.h>
#include <wchar.h>
#include <wctype.h>
#include <arpa/inet.h>

static uint16_t bswap16(uint16_t value)
{
    return (uint16_t)((value >> 8) | (value << 8));
}

static uint32_t bswap32(uint32_t value)
{
    return ((value & 0x000000ffu) << 24) |
           ((value & 0x0000ff00u) << 8) |
           ((value & 0x00ff0000u) >> 8) |
           ((value & 0xff000000u) >> 24);
}

int regcomp(regex_t *preg, const char *regex, int cflags)
{
    (void)regex;
    (void)cflags;
    if (preg) preg->re_nsub = 0;
    return 0;
}

int regexec(const regex_t *preg, const char *string, size_t nmatch, regmatch_t pmatch[], int eflags)
{
    (void)preg;
    (void)string;
    (void)nmatch;
    (void)pmatch;
    (void)eflags;
    return REG_NOMATCH;
}

size_t regerror(int errcode, const regex_t *preg, char *errbuf, size_t errbuf_size)
{
    const char *message = errcode == REG_NOMATCH ? "no match" : "regex error";
    (void)preg;
    if (errbuf && errbuf_size) {
        strncpy(errbuf, message, errbuf_size - 1);
        errbuf[errbuf_size - 1] = '\0';
    }
    return strlen(message) + 1;
}

void regfree(regex_t *preg)
{
    (void)preg;
}

double fabs(double x) { return x < 0 ? -x : x; }
double floor(double x) { long v = (long)x; return (double)(v > x ? v - 1 : v); }
double ceil(double x) { long v = (long)x; return (double)(v < x ? v + 1 : v); }
double log(double x) { (void)x; return 0.0; }
double log10(double x) { (void)x; return 0.0; }
double pow(double x, double y) { (void)y; return x; }

int sched_yield(void)
{
    return 0;
}

int getrlimit(int resource, struct rlimit *rlim)
{
    (void)resource;
    if (!rlim) {
        errno = EINVAL;
        return -1;
    }
    rlim->rlim_cur = 32;
    rlim->rlim_max = 32;
    return 0;
}

int setrlimit(int resource, const struct rlimit *rlim)
{
    (void)resource;
    (void)rlim;
    return 0;
}

int getrusage(int who, struct rusage *usage)
{
    (void)who;
    if (!usage) {
        errno = EINVAL;
        return -1;
    }
    memset(usage, 0, sizeof(*usage));
    return 0;
}

int statfs(const char *path, struct statfs *buf)
{
    (void)path;
    if (!buf) {
        errno = EINVAL;
        return -1;
    }
    memset(buf, 0, sizeof(*buf));
    buf->f_bsize = 4096;
    buf->f_frsize = 4096;
    return 0;
}

int fstatfs(int fd, struct statfs *buf)
{
    (void)fd;
    return statfs("/", buf);
}

int statvfs(const char *path, struct statvfs *buf)
{
    struct statfs sfs;
    if (statfs(path, &sfs) != 0) return -1;
    memset(buf, 0, sizeof(*buf));
    buf->f_bsize = (unsigned long)sfs.f_bsize;
    buf->f_frsize = (unsigned long)sfs.f_frsize;
    return 0;
}

int fstatvfs(int fd, struct statvfs *buf)
{
    (void)fd;
    return statvfs("/", buf);
}

void openlog(const char *ident, int option, int facility)
{
    (void)ident;
    (void)option;
    (void)facility;
}

void syslog(int priority, const char *format, ...)
{
    va_list ap;
    (void)priority;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
}

void closelog(void) {}

int setjmp(jmp_buf env)
{
    (void)env;
    return 0;
}

void longjmp(jmp_buf env, int value)
{
    (void)env;
    (void)value;
    for (;;) {}
}

int sigsetjmp(sigjmp_buf env, int savesigs)
{
    (void)savesigs;
    return setjmp(env);
}

void siglongjmp(sigjmp_buf env, int value)
{
    longjmp(env, value);
}

uint16_t htons(uint16_t hostshort) { return bswap16(hostshort); }
uint16_t ntohs(uint16_t netshort) { return bswap16(netshort); }
uint32_t htonl(uint32_t hostlong) { return bswap32(hostlong); }
uint32_t ntohl(uint32_t netlong) { return bswap32(netlong); }

int inet_pton(int af, const char *src, void *dst)
{
    (void)af;
    (void)src;
    (void)dst;
    errno = ENOSYS;
    return -1;
}

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
    (void)af;
    (void)src;
    if (!dst || size == 0) return 0;
    dst[0] = '0';
    if (size > 1) dst[1] = '\0';
    return dst;
}

int wcwidth(wchar_t wc)
{
    return wc == 0 ? 0 : 1;
}

size_t wcrtomb(char *s, wchar_t wc, mbstate_t *ps)
{
    (void)ps;
    if (!s) return 1;
    if (wc < 0 || wc > 127) wc = '?';
    s[0] = (char)wc;
    return 1;
}

int iswspace(wint_t wc) { return wc == ' ' || (wc >= '\t' && wc <= '\r'); }
int iswprint(wint_t wc) { return wc >= ' ' && wc < 127; }
int iswpunct(wint_t wc) { return wc > ' ' && wc < 127 && !((wc >= '0' && wc <= '9') || (wc >= 'A' && wc <= 'Z') || (wc >= 'a' && wc <= 'z')); }
