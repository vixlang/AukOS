#ifndef AUKOS_DIRENT_H
#define AUKOS_DIRENT_H

#include <stdint.h>

#define DT_UNKNOWN 0
#define DT_DIR 4
#define DT_CHR 6
#define DT_REG 8
#define DT_LNK 10

struct dirent64 {
    uint64_t d_ino;
    int64_t d_off;
    uint16_t d_reclen;
    uint8_t d_type;
    char d_name[64];
};

#define dirent dirent64
typedef struct dirent64 dirent;

typedef struct DIR DIR;

struct dirent64 *readdir64(DIR *dirp);
struct dirent *readdir(DIR *dirp);
DIR *opendir(const char *name);
DIR *fdopendir(int fd);
int closedir(DIR *dirp);

int getdents64(int fd, struct dirent64 *dirp, unsigned int count);

#endif
