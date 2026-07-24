#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

int access(const char *path, int mode)
{
    struct stat st;
    mode_t allowed = 0;

    if (!path || (mode & ~(R_OK | W_OK | X_OK)) != 0) {
        errno = EINVAL;
        return -1;
    }
    if (stat(path, &st) != 0) {
        return -1;
    }
    if (mode == F_OK) {
        return 0;
    }
    if (st.st_mode & 0444u) allowed |= R_OK;
    if (st.st_mode & 0222u) allowed |= W_OK;
    if (st.st_mode & 0111u) allowed |= X_OK;
    if (((mode_t)mode & allowed) != (mode_t)mode) {
        errno = EACCES;
        return -1;
    }
    return 0;
}
