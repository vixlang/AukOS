#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int errno;

static int stat_result;
static mode_t stat_mode;

int aukos_access_stat(const char *path, struct stat *st)
{
    assert(path != 0);
    if (stat_result != 0) {
        errno = ENOENT;
        return -1;
    }
    memset(st, 0, sizeof(*st));
    st->st_mode = stat_mode;
    return 0;
}

int main(void)
{
    stat_result = 0;
    stat_mode = S_IFREG | 0644u;
    assert(access("/work/source.asm", F_OK) == 0);
    assert(access("/work/source.asm", R_OK | W_OK) == 0);
    assert(access("/work/source.asm", X_OK) == -1 && errno == EACCES);

    stat_mode = S_IFREG | 0111u;
    assert(access("/work/tool", X_OK) == 0);
    assert(access("/work/tool", R_OK) == -1 && errno == EACCES);

    assert(access("/work/tool", 8) == -1 && errno == EINVAL);
    assert(access(0, F_OK) == -1 && errno == EINVAL);

    stat_result = -1;
    assert(access("/work/missing", F_OK) == -1 && errno == ENOENT);
    return 0;
}
