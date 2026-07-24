#ifndef AUKOS_EXT4_H
#define AUKOS_EXT4_H

#include "block.h"

struct vfs_superblock;

int ext4_probe(struct block_device *device);
int ext4_available(void);
struct vfs_superblock *ext4_mount_device(struct block_device *device,
                                         int request_writable);
void ext4_run_selftest(void);

#endif
