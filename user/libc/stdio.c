#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct FILE {
    int fd;
    int error;
    int eof;
    int in_use;
    int ungot;
    int has_ungot;
};

#define FILE_POOL_SIZE 16u

static FILE stdin_file = {0, 0, 0, 1, 0, 0};
static FILE stdout_file = {1, 0, 0, 1, 0, 0};
static FILE stderr_file = {2, 0, 0, 1, 0, 0};
static FILE file_pool[FILE_POOL_SIZE];

FILE *stdin = &stdin_file;
FILE *stdout = &stdout_file;
FILE *stderr = &stderr_file;

static int stream_is_known(FILE *stream)
{
    if (stream == &stdin_file || stream == &stdout_file ||
        stream == &stderr_file) {
        return 1;
    }
    for (size_t index = 0; index < FILE_POOL_SIZE; index++) {
        if (stream == &file_pool[index]) {
            return 1;
        }
    }
    return 0;
}

static int stream_is_open(FILE *stream)
{
    return stream && stream_is_known(stream) && stream->in_use;
}

static FILE *allocate_stream(int fd)
{
    for (size_t index = 0; index < FILE_POOL_SIZE; index++) {
        FILE *stream = &file_pool[index];

        if (!stream->in_use) {
            stream->fd = fd;
            stream->error = 0;
            stream->eof = 0;
            stream->ungot = 0;
            stream->has_ungot = 0;
            stream->in_use = 1;
            return stream;
        }
    }
    errno = EMFILE;
    return 0;
}

static int parse_mode(const char *mode, int *flags)
{
    int plus = 0;
    int binary = 0;
    int text = 0;
    int exclusive = 0;
    int close_on_exec = 0;
    char operation;

    if (!mode || !mode[0] || !flags) {
        errno = EINVAL;
        return -1;
    }
    operation = *mode++;
    if (operation != 'r' && operation != 'w' && operation != 'a') {
        errno = EINVAL;
        return -1;
    }
    while (*mode) {
        if (*mode == '+' && !plus) {
            plus = 1;
        } else if (*mode == 'b' && !binary) {
            binary = 1;
        } else if (*mode == 't' && !text) {
            text = 1;
        } else if (*mode == 'x' && !exclusive && operation == 'w') {
            exclusive = 1;
        } else if (*mode == 'e' && !close_on_exec) {
            close_on_exec = 1;
        } else {
            errno = EINVAL;
            return -1;
        }
        mode++;
    }

    *flags = plus ? O_RDWR : (operation == 'r' ? O_RDONLY : O_WRONLY);
    if (operation == 'w') {
        *flags |= O_CREAT | O_TRUNC;
    } else if (operation == 'a') {
        *flags |= O_CREAT | O_APPEND;
    }
    if (exclusive) {
        *flags |= O_EXCL;
    }
    if (close_on_exec) {
        *flags |= O_CLOEXEC;
    }
    return 0;
}

static size_t stream_write_bytes(FILE *stream, const void *buffer, size_t size)
{
    const unsigned char *bytes = buffer;
    size_t written = 0;

    if (!stream_is_open(stream) || (!buffer && size != 0u)) {
        errno = !stream_is_open(stream) ? EBADF : EINVAL;
        if (stream && stream_is_known(stream)) {
            stream->error = 1;
        }
        return 0;
    }
    while (written < size) {
        ssize_t result = write(stream->fd, bytes + written, size - written);

        if (result < 0) {
            stream->error = 1;
            break;
        }
        if (result == 0) {
            errno = EIO;
            stream->error = 1;
            break;
        }
        written += (size_t)result;
    }
    return written;
}

static int append_char(char *str, size_t size, size_t *offset, int c)
{
    if (str && *offset + 1 < size) str[*offset] = (char)c;
    (*offset)++;
    return 1;
}

static int append_string(char *str, size_t size, size_t *offset, const char *value)
{
    int count = 0;
    if (!value) value = "(null)";
    while (*value) { count += append_char(str, size, offset, *value); value++; }
    return count;
}

static size_t string_length_limit(const char *value, int precision)
{
    size_t length = 0;

    while (value[length] && (precision < 0 || length < (size_t)precision)) {
        length++;
    }
    return length;
}

static void append_padding(char *str, size_t size, size_t *offset, int count, int value)
{
    while (count-- > 0) {
        append_char(str, size, offset, value);
    }
}

static int append_integer(char *str, size_t size, size_t *offset,
                          unsigned long long value, unsigned base,
                          int uppercase, int width, int precision,
                          int left_aligned, int zero_padded, int negative,
                          int show_sign)
{
    char buffer[32];
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    int digit_count = 0;
    int zero_count;
    int prefix_count = negative || show_sign;
    int space_count;
    int start = (int)*offset;

    do {
        buffer[digit_count++] = digits[value % base];
        value /= base;
    } while (value != 0);

    zero_count = precision > digit_count ? precision - digit_count : 0;
    if (zero_padded && !left_aligned && precision < 0 &&
        width > digit_count + prefix_count) {
        zero_count = width - digit_count - prefix_count;
    }
    space_count = width - digit_count - zero_count - prefix_count;
    if (!left_aligned) {
        append_padding(str, size, offset, space_count, ' ');
    }
    if (negative) {
        append_char(str, size, offset, '-');
    } else if (show_sign) {
        append_char(str, size, offset, '+');
    }
    append_padding(str, size, offset, zero_count, '0');
    while (digit_count != 0) {
        append_char(str, size, offset, buffer[--digit_count]);
    }
    if (left_aligned) {
        append_padding(str, size, offset, space_count, ' ');
    }
    return (int)*offset - start;
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
    size_t offset = 0;

    while (*format) {
        if (*format != '%') {
            append_char(str, size, &offset, *format++);
            continue;
        }
        format++;
        {
            int left_aligned = 0;
            int show_sign = 0;
            int zero_padded = 0;
            int width = 0;
            int precision = -1;
            int length = 0;
            char conversion;

            for (;;) {
                if (*format == '-') left_aligned = 1;
                else if (*format == '+') show_sign = 1;
                else if (*format == '0') zero_padded = 1;
                else if (*format == ' ' || *format == '#' || *format == '\'') {
                    /* Accepted for compatibility; these flags are not needed yet. */
                } else break;
                format++;
            }
            if (*format == '*') {
                width = va_arg(ap, int);
                format++;
                if (width < 0) {
                    left_aligned = 1;
                    width = -width;
                }
            } else {
                while (*format >= '0' && *format <= '9') {
                    width = width * 10 + *format++ - '0';
                }
            }
            if (*format == '.') {
                format++;
                precision = 0;
                if (*format == '*') {
                    precision = va_arg(ap, int);
                    format++;
                } else {
                    while (*format >= '0' && *format <= '9') {
                        precision = precision * 10 + *format++ - '0';
                    }
                }
            }
            if (*format == 'l') {
                length = 1;
                format++;
                if (*format == 'l') {
                    length = 2;
                    format++;
                }
            } else if (*format == 'L') {
                length = 3;
                format++;
            }
            conversion = *format;
            if (*format) format++;

            if (conversion == 's') {
                const char *value = va_arg(ap, const char *);
                size_t value_length;
                int padding;

                if (!value) value = "(null)";
                value_length = string_length_limit(value, precision);
                padding = width - (int)value_length;
                if (!left_aligned) append_padding(str, size, &offset, padding, ' ');
                for (size_t index = 0; index < value_length; index++) {
                    append_char(str, size, &offset, value[index]);
                }
                if (left_aligned) append_padding(str, size, &offset, padding, ' ');
            } else if (conversion == 'c') {
                int padding = width - 1;
                if (!left_aligned) append_padding(str, size, &offset, padding, ' ');
                append_char(str, size, &offset, va_arg(ap, int));
                if (left_aligned) append_padding(str, size, &offset, padding, ' ');
            } else if (conversion == 'd' || conversion == 'i') {
                long long value;
                unsigned long long magnitude;

                if (length == 2) value = va_arg(ap, long long);
                else if (length == 1) value = va_arg(ap, long);
                else value = va_arg(ap, int);
                magnitude = value < 0 ? 0ull - (unsigned long long)value
                                      : (unsigned long long)value;
                append_integer(str, size, &offset, magnitude, 10, 0, width,
                               precision, left_aligned, zero_padded,
                               value < 0, show_sign);
            } else if (conversion == 'u' || conversion == 'o' ||
                       conversion == 'x' || conversion == 'X') {
                unsigned long long value;
                unsigned base = conversion == 'o' ? 8u :
                                (conversion == 'u' ? 10u : 16u);

                if (length == 2) value = va_arg(ap, unsigned long long);
                else if (length == 1) value = va_arg(ap, unsigned long);
                else value = va_arg(ap, unsigned int);
                append_integer(str, size, &offset, value, base,
                               conversion == 'X', width, precision,
                               left_aligned, zero_padded, 0, 0);
            } else if (conversion == 'p') {
                unsigned long long value = (unsigned long long)(uintptr_t)va_arg(ap, void *);
                append_string(str, size, &offset, "0x");
                append_integer(str, size, &offset, value, 16, 0,
                               width > 2 ? width - 2 : 0, precision,
                               left_aligned, zero_padded, 0, 0);
            } else if (conversion == '%') {
                append_char(str, size, &offset, '%');
            } else {
                append_char(str, size, &offset, conversion);
            }
        }
    }

    if (str && size) str[offset < size ? offset : size - 1] = '\0';
    return (int)offset;
}

int snprintf(char *str, size_t size, const char *format, ...)
{
    va_list ap;
    int result;
    va_start(ap, format);
    result = vsnprintf(str, size, format, ap);
    va_end(ap);
    return result;
}

int sprintf(char *str, const char *format, ...)
{
    va_list ap;
    int result;
    va_start(ap, format);
    result = vsnprintf(str, (size_t)-1, format, ap);
    va_end(ap);
    return result;
}

int vfprintf(FILE *stream, const char *format, va_list ap)
{
    char buffer[512];
    int result = vsnprintf(buffer, sizeof(buffer), format, ap);
    size_t length = strlen(buffer);

    if (stream_write_bytes(stream, buffer, length) != length) {
        return EOF;
    }
    return result;
}

int fprintf(FILE *stream, const char *format, ...)
{
    va_list ap;
    int result;
    va_start(ap, format);
    result = vfprintf(stream, format, ap);
    va_end(ap);
    return result;
}

int vprintf(const char *format, va_list ap)
{
    return vfprintf(stdout, format, ap);
}

int printf(const char *format, ...)
{
    va_list ap;
    int result;
    va_start(ap, format);
    result = vprintf(format, ap);
    va_end(ap);
    return result;
}

int putchar(int c)
{
    return fputc(c, stdout);
}

int puts(const char *s)
{
    return fputs(s, stdout) == EOF || fputc('\n', stdout) == EOF ? EOF : 0;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t total;

    if (size == 0u || nmemb == 0u) {
        return 0;
    }
    if (nmemb > (size_t)-1 / size) {
        errno = EOVERFLOW;
        if (stream && stream_is_known(stream)) {
            stream->error = 1;
        }
        return 0;
    }
    total = size * nmemb;
    return stream_write_bytes(stream, ptr, total) / size;
}

int fputc(int c, FILE *stream)
{
    unsigned char value = (unsigned char)c;

    return stream_write_bytes(stream, &value, 1u) == 1u ? value : EOF;
}

int fputs(const char *s, FILE *stream)
{
    size_t length;

    if (!s) {
        errno = EINVAL;
        if (stream && stream_is_known(stream)) {
            stream->error = 1;
        }
        return EOF;
    }
    length = strlen(s);
    return stream_write_bytes(stream, s, length) == length ? 0 : EOF;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    unsigned char *bytes = ptr;
    size_t total;
    size_t received = 0;

    if (size == 0u || nmemb == 0u) {
        return 0;
    }
    if (!stream_is_open(stream) || !ptr) {
        errno = !stream_is_open(stream) ? EBADF : EINVAL;
        if (stream && stream_is_known(stream)) {
            stream->error = 1;
        }
        return 0;
    }
    if (nmemb > (size_t)-1 / size) {
        errno = EOVERFLOW;
        stream->error = 1;
        return 0;
    }
    total = size * nmemb;
    if (stream->has_ungot) {
        bytes[received++] = (unsigned char)stream->ungot;
        stream->has_ungot = 0;
    }
    while (received < total) {
        ssize_t result = read(stream->fd, bytes + received, total - received);

        if (result < 0) {
            stream->error = 1;
            break;
        }
        if (result == 0) {
            stream->eof = 1;
            break;
        }
        received += (size_t)result;
    }
    return received / size;
}

int getc(FILE *stream)
{
    unsigned char value;

    return fread(&value, 1u, 1u, stream) == 1u ? value : EOF;
}

int getchar(void)
{
    return getc(stdin);
}

int fgetc(FILE *stream)
{
    return getc(stream);
}

char *fgets(char *s, int size, FILE *stream)
{
    int offset = 0;

    if (!s || size <= 0 || !stream_is_open(stream)) {
        errno = !stream_is_open(stream) ? EBADF : EINVAL;
        if (stream && stream_is_known(stream)) {
            stream->error = 1;
        }
        return 0;
    }
    while (offset + 1 < size) {
        int value = getc(stream);

        if (value == EOF) {
            break;
        }
        s[offset++] = (char)value;
        if (value == '\n') {
            break;
        }
    }
    if (offset == 0 && (stream->eof || stream->error)) {
        return 0;
    }
    s[offset] = '\0';
    return s;
}

int ungetc(int c, FILE *stream)
{
    if (c == EOF || !stream_is_open(stream) || stream->has_ungot) {
        errno = !stream_is_open(stream) ? EBADF : EINVAL;
        return EOF;
    }
    stream->ungot = (unsigned char)c;
    stream->has_ungot = 1;
    stream->eof = 0;
    return stream->ungot;
}

int fflush(FILE *stream)
{
    if (!stream) {
        return 0;
    }
    if (!stream_is_open(stream)) {
        errno = EBADF;
        return EOF;
    }
    return 0;
}

int ferror(FILE *stream)
{
    if (!stream_is_open(stream)) {
        errno = EBADF;
        return 1;
    }
    return stream->error;
}

int feof(FILE *stream)
{
    if (!stream_is_open(stream)) {
        errno = EBADF;
        return 0;
    }
    return stream->eof;
}

void clearerr(FILE *stream)
{
    if (stream_is_open(stream)) {
        stream->error = 0;
        stream->eof = 0;
    } else {
        errno = EBADF;
    }
}

FILE *fopen(const char *path, const char *mode)
{
    FILE *stream;
    int flags;
    int fd;

    if (!path || parse_mode(mode, &flags) != 0) {
        if (!path) {
            errno = EINVAL;
        }
        return 0;
    }
    fd = open(path, flags, 0666);
    if (fd < 0) {
        return 0;
    }
    stream = allocate_stream(fd);
    if (!stream) {
        int saved_errno = errno;

        close(fd);
        errno = saved_errno;
    }
    return stream;
}

FILE *fdopen(int fd, const char *mode)
{
    int flags;

    if (fd < 0 || parse_mode(mode, &flags) != 0) {
        if (fd < 0) {
            errno = EBADF;
        }
        return 0;
    }
    (void)flags;
    return allocate_stream(fd);
}

int fclose(FILE *stream)
{
    int fd;
    int result;

    if (!stream_is_open(stream)) {
        errno = EBADF;
        return EOF;
    }
    fd = stream->fd;
    stream->fd = -1;
    stream->in_use = 0;
    stream->error = 0;
    stream->eof = 0;
    stream->has_ungot = 0;
    result = close(fd);
    return result == 0 ? 0 : EOF;
}

int fseek(FILE *stream, long offset, int whence)
{
    if (!stream_is_open(stream)) {
        errno = EBADF;
        return -1;
    }
    if (lseek(stream->fd, (off_t)offset, whence) < 0) {
        stream->error = 1;
        return -1;
    }
    stream->eof = 0;
    stream->has_ungot = 0;
    return 0;
}

long ftell(FILE *stream)
{
    off_t result;

    if (!stream_is_open(stream)) {
        errno = EBADF;
        return -1;
    }
    result = lseek(stream->fd, 0, SEEK_CUR);
    if (result < 0) {
        stream->error = 1;
        return -1;
    }
    return (long)result;
}

int fseeko(FILE *stream, off_t offset, int whence)
{
    if ((off_t)(long)offset != offset) {
        errno = EOVERFLOW;
        return -1;
    }
    return fseek(stream, (long)offset, whence);
}

off_t ftello(FILE *stream)
{
    return (off_t)ftell(stream);
}

void rewind(FILE *stream)
{
    if (fseek(stream, 0, SEEK_SET) == 0) {
        clearerr(stream);
    }
}

int fileno(FILE *stream)
{
    if (!stream_is_open(stream)) {
        errno = EBADF;
        return -1;
    }
    return stream->fd;
}

int remove(const char *path)
{
    return unlink(path);
}

void perror(const char *s)
{
    const char *message = strerror(errno);

    if (s && *s) {
        fprintf(stderr, "%s: %s\n", s, message);
    } else {
        fprintf(stderr, "%s\n", message);
    }
}
