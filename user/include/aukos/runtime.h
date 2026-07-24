#ifndef AUKOS_RUNTIME_H
#define AUKOS_RUNTIME_H

#include <stddef.h>
#include <sys/types.h>

long aukos_vix_write(const char *text, size_t length);
long aukos_vix_write_str(const char *text);
void aukos_vix_exit(int code) __attribute__((noreturn));

void *aukos_vix_malloc(size_t size);
void *aukos_vix_calloc(size_t count, size_t size);
void *aukos_vix_realloc(void *pointer, size_t size);
void aukos_vix_free(void *pointer);
size_t aukos_vix_strlen(const char *text);
void *aukos_vix_memcpy(void *destination, const void *source, size_t size);
void *aukos_vix_memset(void *destination, int value, size_t size);
int aukos_vix_strcmp(const char *left, const char *right);
int aukos_vix_streq(const char *left, const char *right);

int aukos_vix_open(const char *path, int flags, int mode);
ssize_t aukos_vix_read(int fd, void *buffer, size_t size);
ssize_t aukos_vix_write_fd(int fd, const void *buffer, size_t size);
int aukos_vix_close(int fd);
off_t aukos_vix_lseek(int fd, off_t offset, int whence);
int aukos_vix_fsync(int fd);
int aukos_vix_unlink(const char *path);
int aukos_vix_rename(const char *old_path, const char *new_path);
int aukos_vix_isatty(int fd);
int aukos_vix_fork(void);
int aukos_vix_execve(const char *path, const char **argv, const char **envp);
int aukos_vix_waitpid(int pid, int *status, int options);
int aukos_vix_pipe(int *read_fd, int *write_fd);
int aukos_vix_dup2(int oldfd, int newfd);
int aukos_vix_chdir(const char *path);
int aukos_vix_getcwd(char *buf, size_t size);
int aukos_vix_tty_begin_raw(void);
int aukos_vix_tty_end_raw(void);
int aukos_vix_tty_size(int *rows, int *columns);

void *aukos_vix_fopen(const char *path, const char *mode);
int aukos_vix_fclose(void *stream);
size_t aukos_vix_fread(void *buffer, size_t size, size_t count, void *stream);
size_t aukos_vix_fwrite(const void *buffer, size_t size, size_t count,
                        void *stream);
int aukos_vix_fputs(const char *text, void *stream);
int aukos_vix_fseek(void *stream, long offset, int whence);
long aukos_vix_ftell(void *stream);
int aukos_vix_remove(const char *path);

#endif
