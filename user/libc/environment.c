#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ENV_MAX_ENTRIES 64u
#define ENV_PATH_MAX 256u

#ifdef AUKOS_ENV_TEST_ALLOCATORS
void *env_test_malloc(size_t size);
void *env_test_realloc(void *pointer, size_t size);
void env_test_free(void *pointer);
#define ENV_MALLOC env_test_malloc
#define ENV_REALLOC env_test_realloc
#define ENV_FREE env_test_free
#else
#define ENV_MALLOC malloc
#define ENV_REALLOC realloc
#define ENV_FREE free
#endif

char **environ;
static char **owned_environ;
static size_t environ_count;

static int valid_name(const char *name)
{
    if (!name || name[0] == '\0') {
        return 0;
    }
    for (size_t index = 0; name[index] != '\0'; index++) {
        if (name[index] == '=') {
            return 0;
        }
    }
    return 1;
}

static int entry_matches(const char *entry, const char *name)
{
    size_t index = 0;

    while (name[index] != '\0' && entry[index] == name[index]) {
        index++;
    }
    return name[index] == '\0' && entry[index] == '=';
}

static int ensure_owned(void)
{
    char **copy;
    size_t count = 0;

    if (owned_environ) {
        return 0;
    }
    if (environ) {
        while (count < ENV_MAX_ENTRIES && environ[count]) {
            count++;
        }
        if (count == ENV_MAX_ENTRIES && environ[count]) {
            errno = ENOMEM;
            return -1;
        }
    }
    copy = ENV_MALLOC((count + 1u) * sizeof(char *));
    if (!copy) {
        errno = ENOMEM;
        return -1;
    }
    for (size_t index = 0; index < count; index++) {
        size_t length = strlen(environ[index]) + 1u;

        copy[index] = ENV_MALLOC(length);
        if (!copy[index]) {
            while (index != 0u) {
                ENV_FREE(copy[--index]);
            }
            ENV_FREE(copy);
            errno = ENOMEM;
            return -1;
        }
        memcpy(copy[index], environ[index], length);
    }
    copy[count] = 0;
    owned_environ = copy;
    environ = copy;
    environ_count = count;
    return 0;
}

char *getenv(const char *name)
{
    if (!valid_name(name) || !environ) {
        return 0;
    }
    for (size_t index = 0; environ[index]; index++) {
        if (entry_matches(environ[index], name)) {
            char *equals = strchr(environ[index], '=');
            return equals ? equals + 1 : 0;
        }
    }
    return 0;
}

int setenv(const char *name, const char *value, int overwrite)
{
    char *entry;
    size_t name_length;
    size_t value_length;

    if (!valid_name(name) || !value) {
        errno = EINVAL;
        return -1;
    }
    if (!overwrite && getenv(name)) {
        return 0;
    }
    name_length = strlen(name);
    value_length = strlen(value);
    if (name_length > (size_t)-1 - value_length - 2u) {
        errno = ENOMEM;
        return -1;
    }
    entry = ENV_MALLOC(name_length + value_length + 2u);
    if (!entry) {
        errno = ENOMEM;
        return -1;
    }
    memcpy(entry, name, name_length);
    entry[name_length] = '=';
    memcpy(entry + name_length + 1u, value, value_length + 1u);

    if (ensure_owned() != 0) {
        ENV_FREE(entry);
        return -1;
    }
    for (size_t index = 0; index < environ_count; index++) {
        if (entry_matches(environ[index], name)) {
            ENV_FREE(environ[index]);
            environ[index] = entry;
            return 0;
        }
    }
    if (environ_count == ENV_MAX_ENTRIES) {
        ENV_FREE(entry);
        errno = ENOMEM;
        return -1;
    }
    {
        char **grown = ENV_REALLOC(owned_environ,
                                   (environ_count + 2u) * sizeof(char *));
        if (!grown) {
            ENV_FREE(entry);
            errno = ENOMEM;
            return -1;
        }
        owned_environ = grown;
        environ = grown;
    }
    environ[environ_count++] = entry;
    environ[environ_count] = 0;
    return 0;
}

int unsetenv(const char *name)
{
    size_t index = 0;

    if (!valid_name(name)) {
        errno = EINVAL;
        return -1;
    }
    if (ensure_owned() != 0) {
        return -1;
    }
    while (index < environ_count) {
        if (!entry_matches(environ[index], name)) {
            index++;
            continue;
        }
        ENV_FREE(environ[index]);
        for (size_t move = index + 1u; move <= environ_count; move++) {
            environ[move - 1u] = environ[move];
        }
        environ_count--;
    }
    return 0;
}

int execv(const char *path, char *const argv[])
{
    return execve(path, argv, environ);
}

int execvp(const char *file, char *const argv[])
{
    const char *path;
    size_t cursor = 0;

    if (!file || file[0] == '\0') {
        errno = ENOENT;
        return -1;
    }
    if (strchr(file, '/')) {
        return execve(file, argv, environ);
    }
    path = getenv("PATH");
    if (!path) {
        path = "/bin";
    }
    for (;;) {
        char candidate[ENV_PATH_MAX];
        size_t component = 0;
        size_t file_length = strlen(file);

        while (path[cursor + component] != '\0' &&
               path[cursor + component] != ':') {
            component++;
        }
        if (component + 1u + file_length + 1u <= sizeof(candidate)) {
            size_t out = 0;
            if (component == 0u) {
                candidate[out++] = '.';
            } else {
                memcpy(candidate, path + cursor, component);
                out = component;
            }
            if (candidate[out - 1u] != '/') {
                candidate[out++] = '/';
            }
            memcpy(candidate + out, file, file_length + 1u);
            execve(candidate, argv, environ);
        }
        cursor += component;
        if (path[cursor] == '\0') {
            break;
        }
        cursor++;
    }
    errno = ENOENT;
    return -1;
}
