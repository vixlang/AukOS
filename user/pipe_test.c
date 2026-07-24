#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define PIPE_CAPACITY 4096u
#define TRANSFER_SIZE (PIPE_CAPACITY * 3u)

static unsigned char transfer[TRANSFER_SIZE];

static int write_all(int fd, const void *buffer, size_t size)
{
    const unsigned char *bytes = buffer;
    size_t written = 0;

    while (written < size) {
        ssize_t result = write(fd, bytes + written, size - written);
        if (result <= 0) {
            return -1;
        }
        written += (size_t)result;
    }
    return 0;
}

static int transfer_once(void)
{
    int fds[2];
    pid_t child;
    size_t received = 0;
    int status = 1;

    if (pipe(fds) != 0) return -1;
    child = fork();
    if (child < 0) return -1;
    if (child == 0) {
        close(fds[0]);
        if (write_all(fds[1], transfer, sizeof(transfer)) != 0) _exit(2);
        close(fds[1]);
        _exit(0);
    }
    close(fds[1]);
    while (received < sizeof(transfer)) {
        unsigned char buffer[733];
        ssize_t count = read(fds[0], buffer, sizeof(buffer));

        if (count <= 0) return -1;
        if (memcmp(buffer, transfer + received, (size_t)count) != 0) return -1;
        received += (size_t)count;
    }
    if (read(fds[0], transfer, 1u) != 0) return -1;
    close(fds[0]);
    return waitpid(child, &status, 0) == child && status == 0 ? 0 : -1;
}

int main(void)
{
    int fds[2];
    int duplicate;
    unsigned char buffer[32];

    for (size_t index = 0; index < sizeof(transfer); index++) {
        transfer[index] = (unsigned char)(index * 29u + 7u);
    }
    if (pipe(fds) != 0 || write_all(fds[1], transfer, sizeof(buffer)) != 0 ||
        read(fds[0], buffer, sizeof(buffer)) != (ssize_t)sizeof(buffer) ||
        memcmp(buffer, transfer, sizeof(buffer)) != 0) {
        puts("[pipe_test] FAIL basic");
        return 1;
    }
    duplicate = dup(fds[1]);
    if (duplicate < 0 || dup2(duplicate, 9) != 9) {
        puts("[pipe_test] FAIL dup");
        return 1;
    }
    close(fds[1]);
    close(duplicate);
    if (write_all(9, "x", 1u) != 0 || read(fds[0], buffer, 1u) != 1 ||
        buffer[0] != 'x') {
        puts("[pipe_test] FAIL endpoint refs");
        return 1;
    }
    close(9);
    if (read(fds[0], buffer, 1u) != 0) {
        puts("[pipe_test] FAIL eof");
        return 1;
    }
    close(fds[0]);

    if (pipe(fds) != 0) return 1;
    close(fds[0]);
    if (write(fds[1], "broken", 6u) != -1) {
        puts("[pipe_test] FAIL broken pipe");
        return 1;
    }
    close(fds[1]);

    for (int round = 0; round < 10; round++) {
        if (transfer_once() != 0) {
            puts("[pipe_test] FAIL transfer/reuse");
            return 1;
        }
    }
    puts("[pipe_test] PASS");
    return 0;
}
