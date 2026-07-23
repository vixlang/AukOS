#ifndef _SYS_SWAP_H
#define _SYS_SWAP_H

#ifdef __cplusplus
extern "C" {
#endif

int swapon(const char *path, int swapflags);
int swapoff(const char *path);

#define SWAP_FLAG_PREFER 0x8000

#ifdef __cplusplus
}
#endif

#endif