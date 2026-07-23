#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int errno;
static int failures_before_success;
static int open_calls;

int aukos_mkstemp_open(const char *path, int flags, int mode)
{
    assert(path);
    assert(flags == (O_CREAT | O_EXCL | O_RDWR));
    assert(mode == 0600);
    open_calls++;
    if (failures_before_success != 0) {
        failures_before_success--;
        return -1;
    }
    return 7;
}

uint64_t aukos_mkstemp_random(void)
{
    return 0x123456789abcdef0ull;
}

int main(void)
{
    char valid[] = "/work/tempXXXXXX";
    char exhausted[] = "/tmp/failXXXXXX";
    char invalid[] = "/tmp/badXXXXX";

    failures_before_success = 2;
    assert(mkstemp(valid) == 7);
    assert(open_calls == 3);
    assert(strcmp(valid, "/work/tempXXXXXX") != 0);
    assert(strlen(valid) == strlen("/work/tempXXXXXX"));

    open_calls = 0;
    failures_before_success = 100;
    assert(mkstemp(exhausted) == -1);
    assert(errno == EEXIST);
    assert(open_calls == 64);
    assert(strcmp(exhausted, "/tmp/failXXXXXX") == 0);

    open_calls = 0;
    assert(mkstemp(invalid) == -1);
    assert(errno == EINVAL);
    assert(open_calls == 0);
    assert(strcmp(invalid, "/tmp/badXXXXX") == 0);
    puts("mkstemp_test: PASS");
    return 0;
}
