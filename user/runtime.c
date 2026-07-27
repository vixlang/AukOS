#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <aukos/runtime.h>

static struct termios saved_terminal;
static int terminal_saved;

long aukos_vix_write(const char *text, size_t length)
{
    size_t written = 0;

    if (!text && length != 0u) {
        return -1;
    }
    while (written < length) {
        ssize_t result = write(1, text + written, length - written);

        if (result <= 0) {
            return written == 0u ? -1 : (long)written;
        }
        written += (size_t)result;
    }
    return (long)written;
}

long aukos_vix_write_str(const char *text)
{
    return text ? aukos_vix_write(text, strlen(text)) : -1;
}

void aukos_vix_exit(int code)
{
    _exit(code);
}

void *aukos_vix_malloc(size_t size) { return malloc(size); }
void *aukos_vix_calloc(size_t count, size_t size) { return calloc(count, size); }
void *aukos_vix_realloc(void *pointer, size_t size) { return realloc(pointer, size); }
void aukos_vix_free(void *pointer) { free(pointer); }
size_t aukos_vix_strlen(const char *text) { return text ? strlen(text) : 0u; }
void *aukos_vix_memcpy(void *destination, const void *source, size_t size)
{
    return memcpy(destination, source, size);
}
void *aukos_vix_memset(void *destination, int value, size_t size)
{
    return memset(destination, value, size);
}
int aukos_vix_strcmp(const char *left, const char *right)
{
    if (!left || !right) {
        return left == right ? 0 : (left ? 1 : -1);
    }
    return strcmp(left, right);
}
int aukos_vix_streq(const char *left, const char *right)
{
    return aukos_vix_strcmp(left, right) == 0;
}

int aukos_vix_open(const char *path, int flags, int mode)
{
    return open(path, flags, mode);
}
ssize_t aukos_vix_read(int fd, void *buffer, size_t size)
{
    return read(fd, buffer, size);
}
ssize_t aukos_vix_write_fd(int fd, const void *buffer, size_t size)
{
    const unsigned char *bytes = buffer;
    size_t written = 0;

    if (!buffer && size != 0u) {
        return -1;
    }
    while (written < size) {
        ssize_t result = write(fd, bytes + written, size - written);

        if (result <= 0) {
            return written == 0u ? -1 : (ssize_t)written;
        }
        written += (size_t)result;
    }
    return (ssize_t)written;
}
int aukos_vix_close(int fd) { return close(fd); }
off_t aukos_vix_lseek(int fd, off_t offset, int whence)
{
    return lseek(fd, offset, whence);
}
int aukos_vix_fsync(int fd) { return fsync(fd); }
int aukos_vix_unlink(const char *path) { return unlink(path); }
int aukos_vix_rename(const char *old_path, const char *new_path)
{
    return rename(old_path, new_path);
}

int aukos_vix_isatty(int fd) { return isatty(fd); }

int aukos_vix_tty_begin_raw(void)
{
    struct termios raw;

    if (!isatty(0) || tcgetattr(0, &saved_terminal) != 0) {
        return -1;
    }
    raw = saved_terminal;
    raw.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
    if (tcsetattr(0, TCSANOW, &raw) != 0) {
        return -1;
    }
    terminal_saved = 1;
    return 0;
}

int aukos_vix_tty_end_raw(void)
{
    if (!terminal_saved) {
        return 0;
    }
    terminal_saved = 0;
    return tcsetattr(0, TCSANOW, &saved_terminal);
}

int aukos_vix_tty_size(int *rows, int *columns)
{
    struct winsize size;

    if (!rows || !columns || ioctl(1, TIOCGWINSZ, &size) != 0) {
        return -1;
    }
    *rows = size.ws_row;
    *columns = size.ws_col;
    return 0;
}

void *aukos_vix_fopen(const char *path, const char *mode)
{
    return fopen(path, mode);
}
int aukos_vix_fclose(void *stream) { return fclose((FILE *)stream); }
size_t aukos_vix_fread(void *buffer, size_t size, size_t count, void *stream)
{
    return fread(buffer, size, count, (FILE *)stream);
}
size_t aukos_vix_fwrite(const void *buffer, size_t size, size_t count,
                        void *stream)
{
    return fwrite(buffer, size, count, (FILE *)stream);
}
int aukos_vix_fputs(const char *text, void *stream)
{
    return fputs(text, (FILE *)stream);
}
int aukos_vix_fseek(void *stream, long offset, int whence)
{
    return fseek((FILE *)stream, offset, whence);
}
long aukos_vix_ftell(void *stream) { return ftell((FILE *)stream); }
int aukos_vix_remove(const char *path) { return remove(path); }

int aukos_vix_fork(void) { return fork(); }

int aukos_vix_execve(const char *path, const char **argv, const char **envp)
{
    return execve(path, (char *const *)argv, (char *const *)envp);
}

int aukos_vix_waitpid(int pid, int *status, int options)
{
    return waitpid(pid, status, options);
}

int aukos_vix_pipe(int *read_fd, int *write_fd)
{
    int fds[2];

    if (pipe(fds) != 0) { return -1; }
    *read_fd = fds[0];
    *write_fd = fds[1];
    return 0;
}

int aukos_vix_dup2(int oldfd, int newfd) { return dup2(oldfd, newfd); }
int aukos_vix_chdir(const char *path) { return chdir(path); }

int aukos_vix_getcwd(char *buf, size_t size)
{
    return getcwd(buf, size) ? 0 : -1;
}
