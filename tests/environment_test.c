#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char **environ;
int errno;
static int allocation_countdown = -1;

void *env_test_malloc(size_t size)
{
    if (allocation_countdown == 0) return NULL;
    if (allocation_countdown > 0) allocation_countdown--;
    return malloc(size);
}

void *env_test_realloc(void *pointer, size_t size)
{
    if (allocation_countdown == 0) return NULL;
    if (allocation_countdown > 0) allocation_countdown--;
    return realloc(pointer, size);
}

void env_test_free(void *pointer)
{
    free(pointer);
}

int execve(const char *path, char *const argv[], char *const envp[])
{
    (void)path;
    (void)argv;
    (void)envp;
    return -1;
}

int main(void)
{
    char *initial[] = {"PATH=/bin", "HOME=/", "REMOVE=one", 0};

    environ = initial;
    assert(strcmp(getenv("PATH"), "/bin") == 0);
    assert(getenv("MISSING") == NULL);
    assert(setenv("HOME", "/work", 0) == 0);
    assert(strcmp(getenv("HOME"), "/") == 0);
    assert(setenv("HOME", "/work", 1) == 0);
    assert(strcmp(getenv("HOME"), "/work") == 0);
    assert(setenv("NEW", "value", 1) == 0);
    assert(strcmp(getenv("NEW"), "value") == 0);
    allocation_countdown = 1;
    assert(setenv("ALLOC_FAIL", "preserve", 1) == -1);
    allocation_countdown = -1;
    assert(getenv("ALLOC_FAIL") == NULL);
    assert(strcmp(getenv("HOME"), "/work") == 0);
    assert(unsetenv("REMOVE") == 0);
    assert(getenv("REMOVE") == NULL);
    assert(unsetenv("MISSING") == 0);
    assert(setenv("BAD=NAME", "x", 1) == -1);
    assert(unsetenv("") == -1);
    for (int index = 0; index < 61; index++) {
        char name[] = "C00";

        name[1] = (char)('0' + index / 10);
        name[2] = (char)('0' + index % 10);
        assert(setenv(name, "capacity", 1) == 0);
    }
    assert(setenv("OVER_CAPACITY", "no", 1) == -1);
    assert(strcmp(getenv("HOME"), "/work") == 0);

    puts("environment_test: PASS");
    return 0;
}
