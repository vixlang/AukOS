#ifndef _MNTENT_H
#define _MNTENT_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct mntent {
    char *mnt_fsname;
    char *mnt_dir;
    char *mnt_type;
    char *mnt_opts;
    int mnt_freq;
    int mnt_passno;
};

FILE *setmntent(const char *filep, const char *type);
struct mntent *getmntent(FILE *stream);
int endmntent(FILE *streamp);
char *hasmntopt(const struct mntent *mnt, const char *opt);

#ifdef __cplusplus
}
#endif

#endif
