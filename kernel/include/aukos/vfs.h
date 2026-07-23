#ifndef AUKOS_VFS_H
#define AUKOS_VFS_H

#include <stddef.h>
#include <stdint.h>

#define VFS_MAX_PATH 256
#define VFS_MAX_NAME 64
#define VFS_MAX_FD 32
#define VFS_MAX_OPEN_FILES 64u
#define VFS_MAX_FILE_SIZE (1024u * 1024u)

#define VFS_O_ACCMODE 0003u
#define VFS_O_RDONLY 0000u
#define VFS_O_WRONLY 0001u
#define VFS_O_RDWR 0002u
#define VFS_O_CREAT 0100u
#define VFS_O_EXCL 0200u
#define VFS_O_TRUNC 01000u
#define VFS_O_APPEND 02000u

enum vfs_file_type {
    VFS_FILE_UNKNOWN = 0,
    VFS_FILE_REGULAR,
    VFS_FILE_DIRECTORY,
    VFS_FILE_SYMLINK,
    VFS_FILE_DEVICE,
};

struct vfs_dirent {
    char name[VFS_MAX_NAME];
    enum vfs_file_type type;
    uint64_t size;
};

struct vfs_stat {
    uint64_t ino;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint64_t size;
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
    enum vfs_file_type type;
};

struct vfs_inode;
struct vfs_file_ops;

struct vfs_file {
    struct vfs_inode *inode;
    size_t offset;
    uint32_t flags;
    void *private_data;
    uint8_t allocated;
    uint8_t created;
};

struct vfs_inode {
    uint32_t ino;
    enum vfs_file_type type;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint64_t size;
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
    struct vfs_superblock *sb;
    void *private_data;
    struct vfs_file_ops *ops;
};

struct vfs_file_ops {
    int (*open)(struct vfs_inode *inode, struct vfs_file *file);
    int (*close)(struct vfs_file *file);
    size_t (*read)(struct vfs_file *file, void *buf, size_t count, size_t offset);
    size_t (*write)(struct vfs_file *file, const void *buf, size_t count, size_t offset);
    int (*readdir)(struct vfs_file *file, void *buf, size_t count);
    int (*lookup)(struct vfs_inode *dir, const char *name, struct vfs_inode **out_inode);
    int (*create)(struct vfs_inode *dir, const char *name, uint32_t mode, struct vfs_inode **out_inode);
    int (*unlink)(struct vfs_inode *dir, const char *name);
    int (*mkdir)(struct vfs_inode *dir, const char *name, uint32_t mode, struct vfs_inode **out_inode);
    int (*truncate)(struct vfs_inode *inode, uint64_t length);
    int (*rename)(struct vfs_inode *old_dir, const char *old_name,
                  struct vfs_inode *new_dir, const char *new_name);
    int (*fsync)(struct vfs_file *file);
};

struct vfs_superblock {
    uint32_t magic;
    uint32_t block_size;
    uint64_t total_blocks;
    uint64_t free_blocks;
    struct vfs_inode *root_inode;
    void *private_data;
};

struct vfs_mount {
    char path[VFS_MAX_PATH];
    struct vfs_superblock *sb;
    struct vfs_mount *next;
};

struct vfs_fd_table {
    struct vfs_file *files[VFS_MAX_FD];
    uint32_t count;
};

void vfs_init(void);
int vfs_resolve_path(const char *cwd, const char *path, char *out,
                     size_t out_size);
int vfs_register_filesystem(const char *name, struct vfs_superblock *(*mount)(void));
int vfs_mount(const char *path, const char *fs_name);
int vfs_mount_superblock(const char *path, struct vfs_superblock *sb);
int vfs_open(const char *path, uint32_t flags, uint32_t mode, struct vfs_file **out_file);
int vfs_close(struct vfs_file *file);
size_t vfs_read(struct vfs_file *file, void *buf, size_t count);
size_t vfs_read_at(struct vfs_file *file, void *buf, size_t count,
                   uint64_t offset);
size_t vfs_write(struct vfs_file *file, const void *buf, size_t count);
int vfs_seek(struct vfs_file *file, int64_t offset, uint32_t whence, uint64_t *new_offset);
int vfs_stat_inode(struct vfs_inode *inode, struct vfs_stat *stat);
int vfs_stat_path(const char *path, struct vfs_stat *stat);
int vfs_readdir(struct vfs_file *file, void *buf, size_t count);
int vfs_lookup(const char *path, struct vfs_inode **out_inode);
int vfs_unlink(const char *path);
int vfs_mkdir(const char *path, uint32_t mode);
int vfs_truncate(const char *path, uint64_t length);
int vfs_ftruncate(struct vfs_file *file, uint64_t length);
int vfs_rename(const char *old_path, const char *new_path);
int vfs_fsync(struct vfs_file *file);
size_t vfs_open_file_count(void);

#endif
