#include <aukos/log.h>
#include <aukos/memory.h>
#include <aukos/vfs.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void log_write(enum log_level level, const char *message)
{
    (void)level;
    (void)message;
}

void *kmalloc(size_t size)
{
    return malloc(size);
}

int main(void)
{
    char path[VFS_MAX_PATH];
    char long_name[VFS_MAX_NAME + 2u];

    assert(vfs_resolve_path("/work", "file", path, sizeof(path)) == 0);
    assert(strcmp(path, "/work/file") == 0);
    assert(vfs_resolve_path("/work/sub", ".././file", path, sizeof(path)) == 0);
    assert(strcmp(path, "/work/file") == 0);
    assert(vfs_resolve_path("/work", "../../tmp///x", path, sizeof(path)) == 0);
    assert(strcmp(path, "/tmp/x") == 0);
    assert(vfs_resolve_path("/work", "/bin/../tmp", path, sizeof(path)) == 0);
    assert(strcmp(path, "/tmp") == 0);
    memset(long_name, 'a', sizeof(long_name));
    long_name[sizeof(long_name) - 1u] = '\0';
    assert(vfs_resolve_path("/", long_name, path, sizeof(path)) == -1);
    assert(vfs_resolve_path("/", "", path, sizeof(path)) == -1);

    puts("path_test: PASS");
    return 0;
}
