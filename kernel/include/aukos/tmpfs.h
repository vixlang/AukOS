#ifndef AUKOS_TMPFS_H
#define AUKOS_TMPFS_H

struct vfs_superblock;

void tmpfs_init(void);
struct vfs_superblock *tmpfs_mount_empty(void);

#endif
