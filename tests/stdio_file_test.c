#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MOCK_CAPACITY 128u

int errno;

static unsigned char mock_data[MOCK_CAPACITY];
static size_t mock_size;
static size_t mock_offset;
static size_t read_chunk;
static size_t write_chunk;
static size_t write_limit;
static int next_fd;
static int open_result;
static int open_calls;
static int close_calls;
static int close_result;
static int last_open_flags;
static int unlink_calls;

static void reset_mock(void)
{
    memset(mock_data, 0, sizeof(mock_data));
    mock_size = 0;
    mock_offset = 0;
    read_chunk = (size_t)-1;
    write_chunk = (size_t)-1;
    write_limit = (size_t)-1;
    next_fd = 10;
    open_result = 0;
    open_calls = 0;
    close_calls = 0;
    close_result = 0;
    last_open_flags = 0;
    unlink_calls = 0;
    errno = 0;
}

int aukos_stdio_open(const char *path, int flags, ...)
{
    assert(path != 0);
    open_calls++;
    last_open_flags = flags;
    if (open_result < 0) {
        errno = EIO;
        return -1;
    }
    if (flags & O_TRUNC) {
        mock_size = 0;
    }
    mock_offset = (flags & O_APPEND) ? mock_size : 0;
    return next_fd++;
}

ssize_t aukos_stdio_read(int fd, void *buffer, size_t count)
{
    size_t available;

    assert(fd >= 0);
    if (mock_offset >= mock_size) {
        return 0;
    }
    available = mock_size - mock_offset;
    if (count > available) {
        count = available;
    }
    if (count > read_chunk) {
        count = read_chunk;
    }
    memcpy(buffer, mock_data + mock_offset, count);
    mock_offset += count;
    return (ssize_t)count;
}

ssize_t aukos_stdio_write(int fd, const void *buffer, size_t count)
{
    assert(fd >= 0);
    if (mock_offset >= write_limit) {
        errno = EIO;
        return -1;
    }
    if (count > write_chunk) {
        count = write_chunk;
    }
    if (count > write_limit - mock_offset) {
        count = write_limit - mock_offset;
    }
    if (count > MOCK_CAPACITY - mock_offset) {
        count = MOCK_CAPACITY - mock_offset;
    }
    if (count == 0u) {
        errno = EIO;
        return -1;
    }
    memcpy(mock_data + mock_offset, buffer, count);
    mock_offset += count;
    if (mock_offset > mock_size) {
        mock_size = mock_offset;
    }
    return (ssize_t)count;
}

off_t aukos_stdio_lseek(int fd, off_t offset, int whence)
{
    off_t base;
    off_t result;

    assert(fd >= 0);
    if (whence == SEEK_SET) {
        base = 0;
    } else if (whence == SEEK_CUR) {
        base = (off_t)mock_offset;
    } else if (whence == SEEK_END) {
        base = (off_t)mock_size;
    } else {
        errno = EINVAL;
        return -1;
    }
    result = base + offset;
    if (result < 0 || (size_t)result > MOCK_CAPACITY) {
        errno = EINVAL;
        return -1;
    }
    mock_offset = (size_t)result;
    return result;
}

int aukos_stdio_close(int fd)
{
    assert(fd >= 0);
    close_calls++;
    if (close_result < 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}

int aukos_stdio_unlink(const char *path)
{
    assert(strcmp(path, "/work/remove-me") == 0);
    unlink_calls++;
    return 0;
}

static void expect_mode(const char *mode, int expected_flags)
{
    FILE *stream = fopen("/work/file", mode);

    assert(stream != 0);
    assert(last_open_flags == expected_flags);
    assert(fclose(stream) == 0);
}

int main(void)
{
    FILE *stream;
    FILE *streams[16];
    unsigned char buffer[16];

    reset_mock();
    expect_mode("r", O_RDONLY);
    expect_mode("rt", O_RDONLY);
    expect_mode("r+", O_RDWR);
    expect_mode("wb", O_WRONLY | O_CREAT | O_TRUNC);
    expect_mode("wt", O_WRONLY | O_CREAT | O_TRUNC);
    expect_mode("a+", O_RDWR | O_CREAT | O_APPEND);
    expect_mode("w+xe", O_RDWR | O_CREAT | O_TRUNC | O_EXCL | O_CLOEXEC);
    assert(fopen("/work/file", "") == 0 && errno == EINVAL);
    assert(fopen("/work/file", "rx") == 0 && errno == EINVAL);

    open_result = -1;
    assert(fopen("/work/missing", "r") == 0);
    assert(close_calls == 7);
    open_result = 0;

    for (size_t index = 0; index < 16u; index++) {
        streams[index] = fopen("/work/file", "r");
        assert(streams[index] != 0);
    }
    assert(fopen("/work/file", "r") == 0 && errno == EMFILE);
    assert(close_calls == 8);
    for (size_t index = 0; index < 16u; index++) {
        assert(fclose(streams[index]) == 0);
    }

    memcpy(mock_data, "abcdefgh", 8u);
    mock_size = 8u;
    read_chunk = 3u;
    stream = fopen("/work/input", "r");
    assert(stream != 0);
    assert(fread(buffer, 4u, 2u, stream) == 2u);
    assert(memcmp(buffer, "abcdefgh", 8u) == 0);
    assert(ftell(stream) == 8);
    assert(fread(buffer, 1u, 1u, stream) == 0u);
    assert(feof(stream) != 0);
    assert(fseek(stream, 2, SEEK_SET) == 0);
    assert(feof(stream) == 0);
    assert(fgets((char *)buffer, 4, stream) == (char *)buffer);
    assert(strcmp((char *)buffer, "cde") == 0);
    assert(fgets((char *)buffer, 0, stream) == 0 && errno == EINVAL);
    assert(fseek(stream, 2, SEEK_SET) == 0);
    assert(getc(stream) == 'c');
    assert(ungetc('Z', stream) == 'Z');
    assert(getc(stream) == 'Z');
    rewind(stream);
    assert(ftell(stream) == 0);
    assert(fclose(stream) == 0);
    assert(fclose(stream) == EOF && errno == EBADF);
    assert(fread(buffer, 1u, 1u, stream) == 0u && errno == EBADF);
    assert(ferror(stream) != 0);

    write_chunk = 3u;
    stream = fopen("/work/output", "w");
    assert(stream != 0);
    assert(fwrite("12345678", 4u, 2u, stream) == 2u);
    assert(mock_size == 8u && memcmp(mock_data, "12345678", 8u) == 0);
    assert(fputs("90", stream) == 0);
    assert(mock_size == 10u && memcmp(mock_data, "1234567890", 10u) == 0);
    assert(fileno(stream) >= 0);
    assert(fclose(stream) == 0);

    write_limit = 5u;
    stream = fopen("/work/partial", "w");
    assert(stream != 0);
    assert(fwrite("abcdefgh", 4u, 2u, stream) == 1u);
    assert(ferror(stream) != 0);
    clearerr(stream);
    assert(ferror(stream) == 0);
    close_result = -1;
    assert(fclose(stream) == EOF);
    close_result = 0;
    stream = fopen("/work/reused", "r");
    assert(stream != 0);
    assert(fclose(stream) == 0);

    assert(remove("/work/remove-me") == 0);
    assert(unlink_calls == 1);

    mock_offset = 0;
    mock_size = 0;
    write_limit = (size_t)-1;
    errno = ENOENT;
    perror("nasm");
    {
        char expected[64];
        int expected_size = snprintf(expected, sizeof(expected), "nasm: %s\n",
                                     strerror(ENOENT));
        assert(expected_size > 0);
        assert(mock_size == (size_t)expected_size);
        assert(memcmp(mock_data, expected, mock_size) == 0);
    }
    return 0;
}
