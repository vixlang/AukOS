#ifndef AUKOS_STDIO_H
#define AUKOS_STDIO_H

#include <stdarg.h>
#include <stddef.h>
#include <sys/types.h>

#define EOF (-1)

typedef struct FILE FILE;
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int putchar(int c);
int puts(const char *s);
int printf(const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);
int sprintf(char *str, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);
int vprintf(const char *format, va_list ap);
int vfprintf(FILE *stream, const char *format, va_list ap);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fputc(int c, FILE *stream);
int fflush(FILE *stream);
int ferror(FILE *stream);
FILE *fopen(const char *path, const char *mode);
FILE *fdopen(int fd, const char *mode);
int fclose(FILE *stream);

int putc(int c, FILE *stream);
#define putc(c, stream) fputc(c, stream)
int getc(FILE *stream);
int ungetc(int c, FILE *stream);
int getchar(void);

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
void rewind(FILE *stream);
int feof(FILE *stream);
void clearerr(FILE *stream);
int fileno(FILE *stream);
int remove(const char *path);
int rename(const char *oldpath, const char *newpath);

int sscanf(const char *str, const char *format, ...);
int vsscanf(const char *str, const char *format, va_list ap);

void perror(const char *s);

ssize_t getdelim(char **lineptr, size_t *n, int delim, FILE *stream);
ssize_t getline(char **lineptr, size_t *n, FILE *stream);
int dprintf(int fd, const char *format, ...);
int vdprintf(int fd, const char *format, va_list ap);
int fputs(const char *s, FILE *stream);

#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2
int setvbuf(FILE *stream, char *buf, int mode, size_t size);

#endif
