#include <stddef.h>
#include <stdlib.h>
#include <string.h>

void *memcpy(void *dest, const void *src, size_t n)
{
    unsigned char *d = dest;
    const unsigned char *s = src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

void *memmove(void *dest, const void *src, size_t n)
{
    unsigned char *d = dest;
    const unsigned char *s = src;
    if (d < s) for (size_t i = 0; i < n; i++) d[i] = s[i];
    else while (n) { n--; d[n] = s[n]; }
    return dest;
}

void *memset(void *s, int c, size_t n)
{
    unsigned char *p = s;
    for (size_t i = 0; i < n; i++) p[i] = (unsigned char)c;
    return s;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *a = s1;
    const unsigned char *b = s2;
    for (size_t i = 0; i < n; i++) if (a[i] != b[i]) return a[i] - b[i];
    return 0;
}

size_t strlen(const char *s)
{
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

char *strcpy(char *dest, const char *src)
{
    char *out = dest;
    while ((*dest++ = *src++));
    return out;
}

char *stpcpy(char *dest, const char *src)
{
    while ((*dest = *src) != '\0') { dest++; src++; }
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n)
{
    size_t i = 0;
    for (; i < n && src[i]; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = '\0';
    return dest;
}

int strcmp(const char *s1, const char *s2)
{
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i] || !s1[i]) return (unsigned char)s1[i] - (unsigned char)s2[i];
    }
    return 0;
}

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

int strcasecmp(const char *s1, const char *s2)
{
    while (*s1 && lower(*s1) == lower(*s2)) { s1++; s2++; }
    return lower((unsigned char)*s1) - lower((unsigned char)*s2);
}

int strncasecmp(const char *s1, const char *s2, unsigned long n)
{
    for (unsigned long i = 0; i < n; i++) {
        int a = lower((unsigned char)s1[i]);
        int b = lower((unsigned char)s2[i]);
        if (a != b || !s1[i]) return a - b;
    }
    return 0;
}

char *strchr(const char *s, int c)
{
    while (*s) { if (*s == (char)c) return (char *)s; s++; }
    return c == 0 ? (char *)s : 0;
}

char *strrchr(const char *s, int c)
{
    const char *last = 0;
    do { if (*s == (char)c) last = s; } while (*s++);
    return (char *)last;
}

char *strstr(const char *haystack, const char *needle)
{
    size_t needle_len = strlen(needle);
    if (!needle_len) return (char *)haystack;
    for (; *haystack; haystack++) if (strncmp(haystack, needle, needle_len) == 0) return (char *)haystack;
    return 0;
}

char *strpbrk(const char *s, const char *accept)
{
    while (*s) {
        if (strchr(accept, (unsigned char)*s)) {
            return (char *)s;
        }
        s++;
    }
    return 0;
}

char *strdup(const char *s)
{
    size_t size = strlen(s) + 1;
    char *copy = malloc(size);
    if (copy) memcpy(copy, s, size);
    return copy;
}

char *strerror(int errnum)
{
    (void)errnum;
    return "AukOS libc error";
}
