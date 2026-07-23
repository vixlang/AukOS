#ifndef AUKOS_STDLIB_H
#define AUKOS_STDLIB_H

#include <stddef.h>

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);
long strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);
long long strtoll(const char *nptr, char **endptr, int base);
double strtod(const char *nptr, char **endptr);
double atof(const char *nptr);
long double strtold(const char *nptr, char **endptr);
int atoi(const char *nptr);
void exit(int code) __attribute__((noreturn));
void abort(void);
int atexit(void (*func)(void));
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));

char *getenv(const char *name);
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);

#define MB_CUR_MAX 1

int abs(int j);
long labs(long j);
long long llabs(long long j);
int mkstemp(char *template);
char *mkdtemp(char *template);

#endif
