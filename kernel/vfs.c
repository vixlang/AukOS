#include "include/aukos/vfs.h"

#include "include/aukos/log.h"
#include "include/aukos/memory.h"

#define MAX_FILESYSTEMS 8
#define MAX_MOUNTS 8

struct filesystem {
    char name[VFS_MAX_NAME];
    struct vfs_superblock *(*mount)(void);
};

static struct filesystem filesystems[MAX_FILESYSTEMS];
static uint32_t filesystem_count = 0;

static struct vfs_mount *mount_list = NULL;
static struct vfs_file open_files[VFS_MAX_OPEN_FILES];

static void memory_zero(void *pointer, size_t size)
{
    uint8_t *bytes = pointer;

    for (size_t index = 0; index < size; index++) {
        bytes[index] = 0;
    }
}

static struct vfs_file *open_file_allocate(void)
{
    for (size_t index = 0; index < VFS_MAX_OPEN_FILES; index++) {
        if (!open_files[index].allocated) {
            memory_zero(&open_files[index], sizeof(open_files[index]));
            open_files[index].allocated = 1u;
            return &open_files[index];
        }
    }
    return 0;
}

static void open_file_release(struct vfs_file *file)
{
    if (file >= open_files && file < open_files + VFS_MAX_OPEN_FILES &&
        file->allocated) {
        memory_zero(file, sizeof(*file));
    }
}

static int string_equals(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        if (*left != *right) {
            return 0;
        }
        left++;
        right++;
    }
    return *left == *right;
}

static size_t string_length(const char *str)
{
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

static int string_copy(char *dest, const char *src, size_t dest_size)
{
    size_t i;
    for (i = 0; i < dest_size - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
    return 0;
}

int vfs_resolve_path(const char *cwd, const char *path, char *out,
                     size_t out_size)
{
    char combined[VFS_MAX_PATH * 2u];
    size_t combined_length = 0;
    size_t out_len = 1;
    size_t cursor = 0;

    if (!cwd || !path || !out || out_size < 2u || path[0] == '\0' ||
        cwd[0] != '/') {
        return -1;
    }
    if (path[0] != '/') {
        while (cwd[combined_length] != '\0') {
            if (combined_length + 1u >= sizeof(combined)) {
                return -1;
            }
            combined[combined_length] = cwd[combined_length];
            combined_length++;
        }
        if (combined_length == 0u || combined[combined_length - 1u] != '/') {
            combined[combined_length++] = '/';
        }
    }
    for (size_t index = 0; path[index] != '\0'; index++) {
        if (combined_length + 1u >= sizeof(combined)) {
            return -1;
        }
        combined[combined_length++] = path[index];
    }
    combined[combined_length] = '\0';
    out[0] = '/';

    while (cursor < combined_length) {
        size_t component_start;
        size_t component_length;

        while (cursor < combined_length && combined[cursor] == '/') {
            cursor++;
        }
        component_start = cursor;
        while (cursor < combined_length && combined[cursor] != '/') {
            cursor++;
        }
        component_length = cursor - component_start;
        if (component_length == 0u ||
            (component_length == 1u && combined[component_start] == '.')) {
            continue;
        }
        if (component_length == 2u && combined[component_start] == '.' &&
            combined[component_start + 1u] == '.') {
            while (out_len > 1u && out[out_len - 1u] != '/') {
                out_len--;
            }
            if (out_len > 1u) {
                out_len--;
            }
            continue;
        }
        if (component_length >= VFS_MAX_NAME ||
            out_len + (out_len > 1u ? 1u : 0u) + component_length >= out_size) {
            return -1;
        }
        if (out_len > 1u) {
            out[out_len++] = '/';
        }
        for (size_t index = 0; index < component_length; index++) {
            out[out_len++] = combined[component_start + index];
        }
    }
    out[out_len] = '\0';
    return 0;
}

static int path_normalize(const char *path, char *out, size_t out_size)
{
    return vfs_resolve_path("/", path, out, out_size);
}

void vfs_init(void)
{
    filesystem_count = 0;
    mount_list = NULL;
    memory_zero(open_files, sizeof(open_files));
    log_info("vfs: initialized");
}

int vfs_register_filesystem(const char *name, struct vfs_superblock *(*mount)(void))
{
    if (filesystem_count >= MAX_FILESYSTEMS) {
        log_error("vfs: too many filesystems");
        return -1;
    }
    
    struct filesystem *fs = &filesystems[filesystem_count];
    string_copy(fs->name, name, VFS_MAX_NAME);
    fs->mount = mount;
    filesystem_count++;
    
    log_info("vfs: registered filesystem");
    return 0;
}

int vfs_mount(const char *path, const char *fs_name)
{
    struct filesystem *fs = NULL;
    for (uint32_t i = 0; i < filesystem_count; i++) {
        if (string_equals(filesystems[i].name, fs_name)) {
            fs = &filesystems[i];
            break;
        }
    }
    
    if (!fs) {
        log_error("vfs: filesystem not found");
        return -1;
    }
    
    struct vfs_superblock *sb = fs->mount();
    if (!sb) {
        log_error("vfs: failed to mount filesystem");
        return -1;
    }
    
    return vfs_mount_superblock(path, sb);
}

int vfs_mount_superblock(const char *path, struct vfs_superblock *sb)
{
    struct vfs_mount *mount;

    if (!path || !sb || !sb->root_inode) {
        return -1;
    }
    mount = kmalloc(sizeof(struct vfs_mount));
    if (!mount) {
        log_error("vfs: out of memory");
        return -1;
    }
    
    path_normalize(path, mount->path, VFS_MAX_PATH);
    mount->sb = sb;
    mount->next = mount_list;
    mount_list = mount;
    
    log_info("vfs: mounted filesystem");
    return 0;
}

int vfs_lookup(const char *path, struct vfs_inode **out_inode)
{
    char normalized[VFS_MAX_PATH];
    if (path_normalize(path, normalized, VFS_MAX_PATH) != 0) {
        return -1;
    }
    
    // Find the mount point that is the longest prefix of the path
    struct vfs_mount *best_mount = NULL;
    size_t best_len = 0;
    struct vfs_mount *mount = mount_list;
    while (mount) {
        size_t mount_len = string_length(mount->path);
        if (mount_len <= string_length(normalized)) {
            int match = 1;
            for (size_t i = 0; i < mount_len; i++) {
                if (mount->path[i] != normalized[i]) {
                    match = 0;
                    break;
                }
            }
            if (match) {
                // Root mount "/" matches any path starting with "/"
                // Non-root mounts must be followed by "/" or end
                int is_root = (mount_len == 1 && mount->path[0] == '/');
                if (is_root || normalized[mount_len] == '/' || normalized[mount_len] == '\0') {
                    if (mount_len > best_len) {
                        best_mount = mount;
                        best_len = mount_len;
                    }
                }
            }
        }
        mount = mount->next;
    }
    
    if (!best_mount) {
        return -1;
    }
    
    struct vfs_inode *current = best_mount->sb->root_inode;
    
    // If the path is exactly the mount point, return root inode
    if (normalized[best_len] == '\0' || 
        (best_len == 1 && normalized[1] == '\0')) {
        *out_inode = current;
        return 0;
    }
    
    // Walk path components
    const char *p = normalized + best_len;
    if (*p == '/') p++;
    
    char component[VFS_MAX_NAME];
    while (*p != '\0') {
        // Extract next component
        size_t comp_len = 0;
        while (p[comp_len] != '/' && p[comp_len] != '\0' && comp_len < VFS_MAX_NAME - 1) {
            comp_len++;
        }
        if (comp_len == 0) break;
        
        for (size_t i = 0; i < comp_len; i++) {
            component[i] = p[i];
        }
        component[comp_len] = '\0';
        
        // Skip past this component
        p += comp_len;
        if (*p == '/') p++;
        
        // Lookup component in current directory
        if (!current->ops || !current->ops->lookup) {
            return -1;
        }
        
        struct vfs_inode *next;
        if (current->ops->lookup(current, component, &next) != 0) {
            return -1;
        }
        current = next;
    }
    
    *out_inode = current;
    return 0;
}

static int path_parent_and_name(const char *path, char *parent_path, size_t parent_size, char *name, size_t name_size)
{
    size_t len = string_length(path);
    size_t last_slash = 0;
    size_t name_start;

    if (len == 0) return -1;

    for (size_t i = 0; i < len; i++) {
        if (path[i] == '/') last_slash = i;
    }

    if (last_slash == 0) {
        parent_path[0] = '/';
        parent_path[1] = '\0';
        name_start = 1;
    } else {
        size_t parent_len = last_slash;
        if (parent_len >= parent_size) return -1;
        for (size_t i = 0; i < parent_len; i++) parent_path[i] = path[i];
        parent_path[parent_len] = '\0';
        name_start = last_slash + 1;
    }

    size_t name_len = len - name_start;
    if (name_len == 0 || name_len >= name_size) return -1;
    for (size_t i = 0; i < name_len; i++) name[i] = path[name_start + i];
    name[name_len] = '\0';

    return 0;
}

int vfs_open(const char *path, uint32_t flags, uint32_t mode, struct vfs_file **out_file)
{
    char normalized[VFS_MAX_PATH];
    struct vfs_inode *inode;
    int lookup_result;
    int created = 0;
    struct vfs_file *file;

    if (!path || !out_file || (flags & VFS_O_ACCMODE) == VFS_O_ACCMODE ||
        path_normalize(path, normalized, sizeof(normalized)) != 0) {
        return -1;
    }

    file = open_file_allocate();
    if (!file) {
        return -1;
    }

    lookup_result = vfs_lookup(normalized, &inode);

    if (lookup_result != 0) {
        if (flags & VFS_O_CREAT) {
            char parent_path[VFS_MAX_PATH];
            char filename[VFS_MAX_NAME];
            struct vfs_inode *parent_dir;

            if (path_parent_and_name(normalized, parent_path, VFS_MAX_PATH, filename, VFS_MAX_NAME) != 0) {
                goto fail;
            }

            if (vfs_lookup(parent_path, &parent_dir) != 0) {
                goto fail;
            }

            if (!parent_dir->ops || !parent_dir->ops->create) {
                goto fail;
            }

            if (parent_dir->ops->create(parent_dir, filename, mode, &inode) != 0) {
                goto fail;
            }
            created = 1;
        } else {
            goto fail;
        }
    } else if ((flags & (VFS_O_CREAT | VFS_O_EXCL)) ==
               (VFS_O_CREAT | VFS_O_EXCL)) {
        goto fail;
    }

    file->inode = inode;
    file->created = (uint8_t)created;
    file->offset = 0;
    file->flags = flags;
    file->private_data = NULL;
    
    if (inode->ops && inode->ops->open) {
        if (inode->ops->open(inode, file) != 0) {
            goto fail_created;
        }
    }

    if (flags & VFS_O_APPEND) {
        file->offset = inode->size;
    }
    
    *out_file = file;
    return 0;

fail_created:
    if (created) {
        (void)vfs_unlink(normalized);
    }
fail:
    open_file_release(file);
    return -1;
}

int vfs_close(struct vfs_file *file)
{
    if (!file) {
        return -1;
    }
    
    if (!file->allocated) {
        return -1;
    }
    if (file->inode && file->inode->ops && file->inode->ops->close &&
        file->inode->ops->close(file) != 0) {
        return -1;
    }
    open_file_release(file);
    return 0;
}

size_t vfs_read(struct vfs_file *file, void *buf, size_t count)
{
    size_t bytes_read;

    if (!file || !file->allocated || !file->inode || !file->inode->ops ||
        !file->inode->ops->read ||
        (file->flags & VFS_O_ACCMODE) == VFS_O_WRONLY) {
        return SIZE_MAX;
    }
    bytes_read = file->inode->ops->read(file, buf, count, file->offset);
    if (bytes_read == SIZE_MAX || bytes_read > count ||
        file->offset > SIZE_MAX - bytes_read) {
        return SIZE_MAX;
    }
    file->offset += bytes_read;
    return bytes_read;
}

size_t vfs_read_at(struct vfs_file *file, void *buf, size_t count,
                   uint64_t offset)
{
    if (!file || !file->allocated || !file->inode || !file->inode->ops ||
        !file->inode->ops->read || offset > SIZE_MAX ||
        (file->flags & VFS_O_ACCMODE) == VFS_O_WRONLY) {
        return SIZE_MAX;
    }
    {
        size_t result = file->inode->ops->read(file, buf, count,
                                               (size_t)offset);
        return result <= count ? result : SIZE_MAX;
    }
}

size_t vfs_write(struct vfs_file *file, const void *buf, size_t count)
{
    size_t bytes_written;

    if (!file || !file->allocated || !file->inode || !file->inode->ops ||
        !file->inode->ops->write ||
        (file->flags & VFS_O_ACCMODE) == VFS_O_RDONLY) {
        return SIZE_MAX;
    }
    
    if (file->flags & VFS_O_APPEND) {
        file->offset = file->inode->size;
    }

    if (file->offset > SIZE_MAX - count) {
        return SIZE_MAX;
    }
    bytes_written = file->inode->ops->write(file, buf, count, file->offset);
    if (bytes_written == SIZE_MAX || bytes_written > count) {
        return SIZE_MAX;
    }
    file->offset += bytes_written;
    return bytes_written;
}

int vfs_unlink(const char *path)
{
    char normalized[VFS_MAX_PATH];
    char parent_path[VFS_MAX_PATH];
    char filename[VFS_MAX_NAME];
    struct vfs_inode *parent_dir;

    if (!path || path_normalize(path, normalized, sizeof(normalized)) != 0 ||
        path_parent_and_name(normalized, parent_path, sizeof(parent_path), filename, sizeof(filename)) != 0) {
        return -1;
    }

    if (vfs_lookup(parent_path, &parent_dir) != 0 || !parent_dir->ops || !parent_dir->ops->unlink) {
        return -1;
    }

    return parent_dir->ops->unlink(parent_dir, filename);
}

int vfs_mkdir(const char *path, uint32_t mode)
{
    char normalized[VFS_MAX_PATH];
    char parent_path[VFS_MAX_PATH];
    char dirname[VFS_MAX_NAME];
    struct vfs_inode *parent_dir;
    struct vfs_inode *new_dir;

    if (!path || path_normalize(path, normalized, sizeof(normalized)) != 0 ||
        path_parent_and_name(normalized, parent_path, sizeof(parent_path), dirname, sizeof(dirname)) != 0) {
        return -1;
    }

    if (vfs_lookup(parent_path, &parent_dir) != 0 || !parent_dir->ops || !parent_dir->ops->mkdir) {
        return -1;
    }

    return parent_dir->ops->mkdir(parent_dir, dirname, mode, &new_dir);
}

int vfs_seek(struct vfs_file *file, int64_t offset, uint32_t whence, uint64_t *new_offset)
{
    uint64_t base;

    if (!file || !file->allocated || !file->inode || !new_offset ||
        file->inode->type != VFS_FILE_REGULAR) {
        return -1;
    }

    switch (whence) {
    case 0:
        base = 0;
        break;
    case 1:
        base = file->offset;
        break;
    case 2:
        base = file->inode ? file->inode->size : 0;
        break;
    default:
        return -1;
    }

    if (offset < 0) {
        uint64_t magnitude = (uint64_t)(-(offset + 1)) + 1u;

        if (magnitude > base) {
            return -1;
        }
    }

    if (offset > 0 && base > UINT64_MAX - (uint64_t)offset) {
        return -1;
    }
    file->offset = offset < 0 ? base - (uint64_t)(-(offset + 1)) - 1u
                              : base + (uint64_t)offset;
    *new_offset = file->offset;
    return 0;
}

int vfs_stat_inode(struct vfs_inode *inode, struct vfs_stat *stat)
{
    if (!inode || !stat) {
        return -1;
    }

    stat->ino = inode->ino;
    stat->mode = inode->mode;
    stat->uid = inode->uid;
    stat->gid = inode->gid;
    stat->size = inode->size;
    stat->atime = inode->atime;
    stat->mtime = inode->mtime;
    stat->ctime = inode->ctime;
    stat->type = inode->type;
    return 0;
}

int vfs_stat_path(const char *path, struct vfs_stat *stat)
{
    struct vfs_inode *inode;

    if (vfs_lookup(path, &inode) != 0) {
        return -1;
    }

    return vfs_stat_inode(inode, stat);
}

int vfs_readdir(struct vfs_file *file, void *buf, size_t count)
{
    if (!file || !file->inode || !file->inode->ops || !file->inode->ops->readdir) {
        return -1;
    }
    
    return file->inode->ops->readdir(file, buf, count);
}

int vfs_truncate(const char *path, uint64_t length)
{
    struct vfs_inode *inode;

    if (!path || length > VFS_MAX_FILE_SIZE ||
        vfs_lookup(path, &inode) != 0 || inode->type != VFS_FILE_REGULAR ||
        !inode->ops || !inode->ops->truncate) {
        return -1;
    }
    return inode->ops->truncate(inode, length);
}

int vfs_ftruncate(struct vfs_file *file, uint64_t length)
{
    if (!file || !file->allocated || !file->inode ||
        file->inode->type != VFS_FILE_REGULAR ||
        (file->flags & VFS_O_ACCMODE) == VFS_O_RDONLY ||
        length > VFS_MAX_FILE_SIZE || !file->inode->ops ||
        !file->inode->ops->truncate) {
        return -1;
    }
    return file->inode->ops->truncate(file->inode, length);
}

int vfs_rename(const char *old_path, const char *new_path)
{
    char old_normalized[VFS_MAX_PATH];
    char new_normalized[VFS_MAX_PATH];
    char old_parent_path[VFS_MAX_PATH];
    char new_parent_path[VFS_MAX_PATH];
    char old_name[VFS_MAX_NAME];
    char new_name[VFS_MAX_NAME];
    struct vfs_inode *old_parent;
    struct vfs_inode *new_parent;

    if (!old_path || !new_path ||
        path_normalize(old_path, old_normalized, sizeof(old_normalized)) != 0 ||
        path_normalize(new_path, new_normalized, sizeof(new_normalized)) != 0 ||
        path_parent_and_name(old_normalized, old_parent_path,
                             sizeof(old_parent_path), old_name,
                             sizeof(old_name)) != 0 ||
        path_parent_and_name(new_normalized, new_parent_path,
                             sizeof(new_parent_path), new_name,
                             sizeof(new_name)) != 0 ||
        vfs_lookup(old_parent_path, &old_parent) != 0 ||
        vfs_lookup(new_parent_path, &new_parent) != 0 ||
        old_parent->sb != new_parent->sb || !old_parent->ops ||
        old_parent->ops != new_parent->ops || !old_parent->ops->rename) {
        return -1;
    }
    return old_parent->ops->rename(old_parent, old_name, new_parent, new_name);
}

int vfs_fsync(struct vfs_file *file)
{
    if (!file || !file->allocated || !file->inode || !file->inode->ops ||
        !file->inode->ops->fsync) {
        return -1;
    }
    return file->inode->ops->fsync(file);
}

size_t vfs_open_file_count(void)
{
    size_t count = 0;

    for (size_t index = 0; index < VFS_MAX_OPEN_FILES; index++) {
        if (open_files[index].allocated) {
            count++;
        }
    }
    return count;
}
