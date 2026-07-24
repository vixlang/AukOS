#include <libgen.h>

static char current_directory[] = ".";

char *basename(char *path)
{
    char *end;
    char *base;

    if (!path || !*path) {
        return current_directory;
    }

    end = path;
    while (*end) {
        end++;
    }
    while (end > path + 1 && end[-1] == '/') {
        *--end = '\0';
    }

    base = end;
    while (base > path && base[-1] != '/') {
        base--;
    }
    return base;
}

char *dirname(char *path)
{
    char *end;
    char *slash;

    if (!path || !*path) {
        return current_directory;
    }

    end = path;
    while (*end) {
        end++;
    }
    while (end > path + 1 && end[-1] == '/') {
        *--end = '\0';
    }

    slash = end;
    while (slash > path && slash[-1] != '/') {
        slash--;
    }
    if (slash == path) {
        return current_directory;
    }

    slash--;
    while (slash > path && slash[-1] == '/') {
        slash--;
    }
    if (slash == path) {
        path[1] = '\0';
        return path;
    }
    *slash = '\0';
    return path;
}
