#include "include/aukos/tmpfs.h"

#include "include/aukos/log.h"
#include "include/aukos/memory.h"
#include "include/aukos/vfs.h"

#include <stddef.h>
#include <stdint.h>

#define TMPFS_MAX_NODES 64u
#define TMPFS_MAX_NAME VFS_MAX_NAME
#define TMPFS_MAX_CHILDREN 64u
#define TMPFS_MAX_PAGES (VFS_MAX_FILE_SIZE / PAGE_SIZE)

struct tmpfs_node {
    uint8_t used;
    uint8_t linked;
    uint8_t static_backing;
    uint32_t open_count;
    char name[TMPFS_MAX_NAME];
    enum vfs_file_type type;
    const uint8_t *static_data;
    void *pages[TMPFS_MAX_PAGES];
    size_t size;
    struct vfs_inode inode;
    struct tmpfs_node *children[TMPFS_MAX_CHILDREN];
    size_t child_count;
};

static struct tmpfs_node tmpfs_nodes[TMPFS_MAX_NODES];
static struct vfs_superblock tmpfs_sb;
static struct vfs_file_ops tmpfs_file_ops;

static int string_compare(const char *left, const char *right)
{
    while (*left && *left == *right) {
        left++;
        right++;
    }
    return (unsigned char)*left - (unsigned char)*right;
}

static void string_copy(char *destination, const char *source)
{
    size_t index = 0;

    while (index + 1u < TMPFS_MAX_NAME && source[index]) {
        destination[index] = source[index];
        index++;
    }
    destination[index] = '\0';
}

static void memory_copy(void *destination, const void *source, size_t size)
{
    uint8_t *to = destination;
    const uint8_t *from = source;

    for (size_t index = 0; index < size; index++) {
        to[index] = from[index];
    }
}

static void memory_zero(void *destination, size_t size)
{
    uint8_t *bytes = destination;

    for (size_t index = 0; index < size; index++) {
        bytes[index] = 0;
    }
}

static void node_release_pages(struct tmpfs_node *node)
{
    for (size_t index = 0; index < TMPFS_MAX_PAGES; index++) {
        if (node->pages[index]) {
            pmm_free_page(node->pages[index]);
            node->pages[index] = 0;
        }
    }
}

static void node_reclaim_if_unused(struct tmpfs_node *node)
{
    if (!node || !node->used || node->linked || node->open_count != 0u) {
        return;
    }
    if (!node->static_backing) {
        node_release_pages(node);
    }
    memory_zero(node, sizeof(*node));
}

static struct tmpfs_node *node_allocate(void)
{
    for (size_t index = 0; index < TMPFS_MAX_NODES; index++) {
        if (!tmpfs_nodes[index].used) {
            struct tmpfs_node *node = &tmpfs_nodes[index];

            memory_zero(node, sizeof(*node));
            node->used = 1u;
            node->inode.ino = (uint32_t)index + 1u;
            node->inode.sb = &tmpfs_sb;
            node->inode.private_data = node;
            node->inode.ops = &tmpfs_file_ops;
            return node;
        }
    }
    return 0;
}

static struct tmpfs_node *node_from_inode(struct vfs_inode *inode)
{
    struct tmpfs_node *node;

    if (!inode || inode->sb != &tmpfs_sb) {
        return 0;
    }
    node = inode->private_data;
    return node && node->used && &node->inode == inode ? node : 0;
}

static int child_index(struct tmpfs_node *directory, const char *name,
                       size_t *index_out)
{
    if (!directory || directory->type != VFS_FILE_DIRECTORY) {
        return -1;
    }
    for (size_t index = 0; index < directory->child_count; index++) {
        if (string_compare(directory->children[index]->name, name) == 0) {
            if (index_out) {
                *index_out = index;
            }
            return 0;
        }
    }
    return -1;
}

static struct tmpfs_node *remove_child(struct tmpfs_node *directory,
                                       size_t index)
{
    struct tmpfs_node *removed = directory->children[index];

    for (size_t cursor = index + 1u; cursor < directory->child_count; cursor++) {
        directory->children[cursor - 1u] = directory->children[cursor];
    }
    directory->child_count--;
    directory->children[directory->child_count] = 0;
    return removed;
}

static int tmpfs_truncate_inode(struct vfs_inode *inode, uint64_t length)
{
    struct tmpfs_node *node = node_from_inode(inode);
    size_t new_size;

    if (!node || node->type != VFS_FILE_REGULAR || node->static_backing ||
        length > VFS_MAX_FILE_SIZE) {
        return -1;
    }
    new_size = (size_t)length;
    if (new_size < node->size) {
        size_t first_free = (new_size + PAGE_SIZE - 1u) / PAGE_SIZE;

        for (size_t page = first_free; page < TMPFS_MAX_PAGES; page++) {
            if (node->pages[page]) {
                pmm_free_page(node->pages[page]);
                node->pages[page] = 0;
            }
        }
        if ((new_size & (PAGE_SIZE - 1u)) != 0u) {
            size_t page = new_size / PAGE_SIZE;
            size_t offset = new_size & (PAGE_SIZE - 1u);

            if (node->pages[page]) {
                memory_zero((uint8_t *)node->pages[page] + offset,
                            PAGE_SIZE - offset);
            }
        }
    }
    node->size = new_size;
    inode->size = new_size;
    return 0;
}

static int tmpfs_open(struct vfs_inode *inode, struct vfs_file *file)
{
    struct tmpfs_node *node = node_from_inode(inode);
    uint32_t access;

    if (!node || !file) {
        return -1;
    }
    access = file->flags & VFS_O_ACCMODE;
    if ((file->flags & VFS_O_TRUNC) != 0u) {
        if (node->type != VFS_FILE_REGULAR || access == VFS_O_RDONLY ||
            tmpfs_truncate_inode(inode, 0u) != 0) {
            return -1;
        }
    }
    node->open_count++;
    return 0;
}

static int tmpfs_close(struct vfs_file *file)
{
    struct tmpfs_node *node;

    if (!file || !(node = node_from_inode(file->inode)) ||
        node->open_count == 0u) {
        return -1;
    }
    node->open_count--;
    node_reclaim_if_unused(node);
    return 0;
}

static size_t tmpfs_read(struct vfs_file *file, void *buffer, size_t count,
                         size_t offset)
{
    struct tmpfs_node *node;
    uint8_t *output = buffer;
    size_t remaining;

    if (!file || !(node = node_from_inode(file->inode)) ||
        node->type != VFS_FILE_REGULAR || (!buffer && count != 0u)) {
        return SIZE_MAX;
    }
    if (offset >= node->size || count == 0u) {
        return 0u;
    }
    remaining = node->size - offset;
    if (count < remaining) {
        remaining = count;
    }
    count = remaining;
    if (node->static_backing) {
        memory_copy(output, node->static_data + offset, remaining);
        return remaining;
    }
    while (remaining != 0u) {
        size_t page = offset / PAGE_SIZE;
        size_t page_offset = offset & (PAGE_SIZE - 1u);
        size_t chunk = PAGE_SIZE - page_offset;

        if (chunk > remaining) {
            chunk = remaining;
        }
        if (node->pages[page]) {
            memory_copy(output, (uint8_t *)node->pages[page] + page_offset,
                        chunk);
        } else {
            memory_zero(output, chunk);
        }
        output += chunk;
        offset += chunk;
        remaining -= chunk;
    }
    return count;
}

static size_t tmpfs_write(struct vfs_file *file, const void *buffer,
                          size_t count, size_t offset)
{
    struct tmpfs_node *node;
    const uint8_t *input = buffer;
    size_t end;
    size_t first_page;
    size_t last_page;
    size_t allocated_indices[TMPFS_MAX_PAGES];
    size_t allocated_count = 0u;
    size_t remaining = count;

    if (!file || !(node = node_from_inode(file->inode)) ||
        node->type != VFS_FILE_REGULAR || node->static_backing ||
        (!buffer && count != 0u) || offset > VFS_MAX_FILE_SIZE ||
        count > VFS_MAX_FILE_SIZE - offset) {
        return SIZE_MAX;
    }
    if (count == 0u) {
        return 0u;
    }
    end = offset + count;
    first_page = offset / PAGE_SIZE;
    last_page = (end - 1u) / PAGE_SIZE;
    for (size_t page = first_page; page <= last_page; page++) {
        if (!node->pages[page]) {
            void *allocation = pmm_alloc_page();

            if (!allocation) {
                for (size_t index = 0; index < allocated_count; index++) {
                    size_t rollback_page = allocated_indices[index];

                    pmm_free_page(node->pages[rollback_page]);
                    node->pages[rollback_page] = 0;
                }
                return SIZE_MAX;
            }
            memory_zero(allocation, PAGE_SIZE);
            node->pages[page] = allocation;
            allocated_indices[allocated_count++] = page;
        }
    }
    while (remaining != 0u) {
        size_t page = offset / PAGE_SIZE;
        size_t page_offset = offset & (PAGE_SIZE - 1u);
        size_t chunk = PAGE_SIZE - page_offset;

        if (chunk > remaining) {
            chunk = remaining;
        }
        memory_copy((uint8_t *)node->pages[page] + page_offset, input, chunk);
        input += chunk;
        offset += chunk;
        remaining -= chunk;
    }
    if (end > node->size) {
        node->size = end;
        node->inode.size = end;
    }
    return count;
}

static int tmpfs_readdir(struct vfs_file *file, void *buffer, size_t count)
{
    struct tmpfs_node *node;
    struct vfs_dirent *entries = buffer;
    size_t maximum = count / sizeof(*entries);
    size_t start;
    size_t result;

    if (!file || !(node = node_from_inode(file->inode)) ||
        node->type != VFS_FILE_DIRECTORY) {
        return -1;
    }
    start = file->offset;
    if (!entries || maximum == 0u) {
        return start < node->child_count ? (int)(node->child_count - start) : 0;
    }
    if (start >= node->child_count) {
        return 0;
    }
    result = node->child_count - start;
    if (result > maximum) {
        result = maximum;
    }
    for (size_t index = 0; index < result; index++) {
        struct tmpfs_node *child = node->children[start + index];

        string_copy(entries[index].name, child->name);
        entries[index].type = child->type;
        entries[index].size = child->size;
    }
    file->offset += result;
    return (int)result;
}

static int tmpfs_lookup(struct vfs_inode *directory, const char *name,
                        struct vfs_inode **out_inode)
{
    struct tmpfs_node *node = node_from_inode(directory);
    size_t index;

    if (!out_inode || child_index(node, name, &index) != 0) {
        return -1;
    }
    *out_inode = &node->children[index]->inode;
    return 0;
}

static int initialize_node(struct tmpfs_node *node, const char *name,
                           enum vfs_file_type type, uint32_t mode)
{
    if (!node || !name) {
        return -1;
    }
    string_copy(node->name, name);
    node->linked = 1u;
    node->type = type;
    node->inode.type = type;
    node->inode.mode = mode & 0777u;
    return 0;
}

static int tmpfs_create(struct vfs_inode *directory, const char *name,
                        uint32_t mode, struct vfs_inode **out_inode)
{
    struct tmpfs_node *parent = node_from_inode(directory);
    struct tmpfs_node *node;

    if (!parent || parent->type != VFS_FILE_DIRECTORY || !out_inode ||
        parent->child_count >= TMPFS_MAX_CHILDREN ||
        child_index(parent, name, 0) == 0 || !(node = node_allocate())) {
        return -1;
    }
    initialize_node(node, name, VFS_FILE_REGULAR, mode);
    parent->children[parent->child_count++] = node;
    *out_inode = &node->inode;
    return 0;
}

static int tmpfs_unlink(struct vfs_inode *directory, const char *name)
{
    struct tmpfs_node *parent = node_from_inode(directory);
    struct tmpfs_node *node;
    size_t index;

    if (child_index(parent, name, &index) != 0 ||
        parent->children[index]->type == VFS_FILE_DIRECTORY) {
        return -1;
    }
    node = remove_child(parent, index);
    node->linked = 0u;
    node->name[0] = '\0';
    node_reclaim_if_unused(node);
    return 0;
}

static int tmpfs_mkdir(struct vfs_inode *directory, const char *name,
                       uint32_t mode, struct vfs_inode **out_inode)
{
    struct tmpfs_node *parent = node_from_inode(directory);
    struct tmpfs_node *node;

    if (!parent || parent->type != VFS_FILE_DIRECTORY || !out_inode ||
        parent->child_count >= TMPFS_MAX_CHILDREN ||
        child_index(parent, name, 0) == 0 || !(node = node_allocate())) {
        return -1;
    }
    initialize_node(node, name, VFS_FILE_DIRECTORY, mode);
    parent->children[parent->child_count++] = node;
    *out_inode = &node->inode;
    return 0;
}

static int tmpfs_rename(struct vfs_inode *old_directory, const char *old_name,
                        struct vfs_inode *new_directory, const char *new_name)
{
    struct tmpfs_node *old_parent = node_from_inode(old_directory);
    struct tmpfs_node *new_parent = node_from_inode(new_directory);
    struct tmpfs_node *source;
    struct tmpfs_node *destination = 0;
    size_t old_index;
    size_t new_index = 0u;
    int destination_exists;

    if (!old_parent || !new_parent || old_parent->type != VFS_FILE_DIRECTORY ||
        new_parent->type != VFS_FILE_DIRECTORY ||
        child_index(old_parent, old_name, &old_index) != 0) {
        return -1;
    }
    source = old_parent->children[old_index];
    if (source->type != VFS_FILE_REGULAR) {
        return -1;
    }
    if (old_parent == new_parent && string_compare(old_name, new_name) == 0) {
        return 0;
    }
    destination_exists = child_index(new_parent, new_name, &new_index) == 0;
    if (destination_exists) {
        destination = new_parent->children[new_index];
        if (destination->type != VFS_FILE_REGULAR) {
            return -1;
        }
    } else if (old_parent != new_parent &&
               new_parent->child_count >= TMPFS_MAX_CHILDREN) {
        return -1;
    }

    if (old_parent == new_parent) {
        if (destination_exists) {
            destination = remove_child(new_parent, new_index);
            if (new_index < old_index) {
                old_index--;
            }
            destination->linked = 0u;
            destination->name[0] = '\0';
        }
        string_copy(source->name, new_name);
    } else {
        remove_child(old_parent, old_index);
        if (destination_exists) {
            new_parent->children[new_index] = source;
            destination->linked = 0u;
            destination->name[0] = '\0';
        } else {
            new_parent->children[new_parent->child_count++] = source;
        }
        string_copy(source->name, new_name);
    }
    node_reclaim_if_unused(destination);
    return 0;
}

static int tmpfs_fsync(struct vfs_file *file)
{
    struct tmpfs_node *node;

    if (!file || !(node = node_from_inode(file->inode)) ||
        node->type != VFS_FILE_REGULAR) {
        return -1;
    }
    return 0;
}

static struct vfs_file_ops tmpfs_file_ops = {
    .open = tmpfs_open,
    .close = tmpfs_close,
    .read = tmpfs_read,
    .write = tmpfs_write,
    .readdir = tmpfs_readdir,
    .lookup = tmpfs_lookup,
    .create = tmpfs_create,
    .unlink = tmpfs_unlink,
    .mkdir = tmpfs_mkdir,
    .truncate = tmpfs_truncate_inode,
    .rename = tmpfs_rename,
    .fsync = tmpfs_fsync,
};

struct vfs_superblock *tmpfs_mount_empty(void)
{
    struct tmpfs_node *root;

    memory_zero(tmpfs_nodes, sizeof(tmpfs_nodes));
    memory_zero(&tmpfs_sb, sizeof(tmpfs_sb));
    root = node_allocate();
    if (!root) {
        return 0;
    }
    initialize_node(root, "", VFS_FILE_DIRECTORY, 0755u);
    tmpfs_sb.magic = 0x746d7066u;
    tmpfs_sb.block_size = PAGE_SIZE;
    tmpfs_sb.root_inode = &root->inode;
    log_info("tmpfs: mounted root filesystem");
    return &tmpfs_sb;
}

#ifndef AUKOS_HOST_TEST
static struct tmpfs_node *create_directory(struct tmpfs_node *parent,
                                           const char *name)
{
    struct tmpfs_node *node = node_allocate();

    if (!node || (parent && parent->child_count >= TMPFS_MAX_CHILDREN)) {
        node_reclaim_if_unused(node);
        return 0;
    }
    initialize_node(node, name, VFS_FILE_DIRECTORY, 0755u);
    if (parent) {
        parent->children[parent->child_count++] = node;
    }
    return node;
}

static struct tmpfs_node *create_static_file(struct tmpfs_node *parent,
                                             const char *name,
                                             const void *data, size_t size,
                                             uint32_t mode)
{
    struct tmpfs_node *node = node_allocate();

    if (!node || !parent || parent->child_count >= TMPFS_MAX_CHILDREN) {
        node_reclaim_if_unused(node);
        return 0;
    }
    initialize_node(node, name, VFS_FILE_REGULAR, mode);
    node->static_backing = 1u;
    node->static_data = data;
    node->size = size;
    node->inode.size = size;
    parent->children[parent->child_count++] = node;
    return node;
}

void tmpfs_init(void)
{
    extern const unsigned char _binary_build_user_hello_elf_start[];
    extern const unsigned char _binary_build_user_hello_elf_end[];
    extern const unsigned char _binary_build_user_aush_elf_start[];
    extern const unsigned char _binary_build_user_aush_elf_end[];
    extern const unsigned char _binary_build_user_write_test_elf_start[];
    extern const unsigned char _binary_build_user_write_test_elf_end[];
    extern const unsigned char _binary_build_user_signal_test_elf_start[];
    extern const unsigned char _binary_build_user_signal_test_elf_end[];
    extern const unsigned char _binary_build_user_udp_test_elf_start[];
    extern const unsigned char _binary_build_user_udp_test_elf_end[];
    extern const unsigned char _binary_build_user_dev_memory_test_elf_start[];
    extern const unsigned char _binary_build_user_dev_memory_test_elf_end[];
    extern const unsigned char _binary_build_user_pipe_test_elf_start[];
    extern const unsigned char _binary_build_user_pipe_test_elf_end[];
    extern const unsigned char _binary_build_user_process_env_test_elf_start[];
    extern const unsigned char _binary_build_user_process_env_test_elf_end[];
    extern const unsigned char _binary_build_user_process_stack_test_elf_start[];
    extern const unsigned char _binary_build_user_process_stack_test_elf_end[];
    extern const unsigned char _binary_build_user_file_api_test_elf_start[];
    extern const unsigned char _binary_build_user_file_api_test_elf_end[];
    extern const unsigned char _binary_build_user_language_hello_elf_start[];
    extern const unsigned char _binary_build_user_language_hello_elf_end[];
    extern const unsigned char _binary_build_user_language_runtime_test_elf_start[];
    extern const unsigned char _binary_build_user_language_runtime_test_elf_end[];
    extern const unsigned char _binary_build_user_ed_elf_start[];
    extern const unsigned char _binary_build_user_ed_elf_end[];
    extern const unsigned char _binary_build_user_touch_elf_start[];
    extern const unsigned char _binary_build_user_touch_elf_end[];
    extern const unsigned char _binary_build_user_vixc_elf_start[];
    extern const unsigned char _binary_build_user_vixc_elf_end[];
    extern const unsigned char _binary_build_user_vixc_test_elf_start[];
    extern const unsigned char _binary_build_user_vixc_test_elf_end[];
    extern const unsigned char _binary_build_user_persistence_test_elf_start[];
    extern const unsigned char _binary_build_user_persistence_test_elf_end[];
    extern const unsigned char _binary_build_user_nasm_test_elf_start[];
    extern const unsigned char _binary_build_user_nasm_test_elf_end[];
    extern const unsigned char _binary_build_nasm_aukos_nasm_elf_start[];
    extern const unsigned char _binary_build_nasm_aukos_nasm_elf_end[];
    extern const unsigned char _binary_build_vix_runtime_runtime_o_start[];
    extern const unsigned char _binary_build_vix_runtime_runtime_o_end[];
    extern const unsigned char _binary_build_toybox_aukos_toybox_elf_start[];
    extern const unsigned char _binary_build_toybox_aukos_toybox_elf_end[];
    struct tmpfs_node *root;
    struct tmpfs_node *bin;
    struct tmpfs_node *lib;
    struct tmpfs_node *vix_lib;

#define EMBED_EXEC(parent, name, symbol)                                        \
    create_static_file((parent), (name), _binary_##symbol##_start,             \
                       (size_t)(_binary_##symbol##_end - _binary_##symbol##_start), \
                       0555u)

    vfs_register_filesystem("tmpfs", tmpfs_mount_empty);
    if (vfs_mount("/", "tmpfs") != 0 ||
        vfs_lookup("/", &tmpfs_sb.root_inode) != 0) {
        log_error("tmpfs: failed to mount root");
        return;
    }
    root = node_from_inode(tmpfs_sb.root_inode);
    bin = create_directory(root, "bin");
    lib = create_directory(root, "lib");
    vix_lib = create_directory(lib, "vix");
    create_directory(root, "work");
    create_directory(root, "tmp");
    EMBED_EXEC(bin, "hello", build_user_hello_elf);
    EMBED_EXEC(bin, "sh", build_user_aush_elf);
    EMBED_EXEC(bin, "aush", build_user_aush_elf);
    EMBED_EXEC(bin, "write_test", build_user_write_test_elf);
    EMBED_EXEC(bin, "signal_test", build_user_signal_test_elf);
    EMBED_EXEC(bin, "udp_test", build_user_udp_test_elf);
    EMBED_EXEC(bin, "dev_memory_test", build_user_dev_memory_test_elf);
    EMBED_EXEC(bin, "pipe_test", build_user_pipe_test_elf);
    EMBED_EXEC(bin, "process_env_test", build_user_process_env_test_elf);
    EMBED_EXEC(bin, "process_stack_test", build_user_process_stack_test_elf);
    EMBED_EXEC(bin, "file_api_test", build_user_file_api_test_elf);
    EMBED_EXEC(bin, "vix_hello", build_user_language_hello_elf);
    EMBED_EXEC(bin, "vix_runtime_test", build_user_language_runtime_test_elf);
    EMBED_EXEC(bin, "ed", build_user_ed_elf);
    EMBED_EXEC(bin, "touch", build_user_touch_elf);
    EMBED_EXEC(bin, "vixc", build_user_vixc_elf);
    EMBED_EXEC(bin, "vixc_test", build_user_vixc_test_elf);
    EMBED_EXEC(bin, "persistence_test", build_user_persistence_test_elf);
    EMBED_EXEC(bin, "nasm_test", build_user_nasm_test_elf);
    EMBED_EXEC(bin, "nasm", build_nasm_aukos_nasm_elf);
    EMBED_EXEC(bin, "toybox", build_toybox_aukos_toybox_elf);
    create_static_file(vix_lib, "runtime.o", _binary_build_vix_runtime_runtime_o_start,
                       (size_t)(_binary_build_vix_runtime_runtime_o_end -
                                _binary_build_vix_runtime_runtime_o_start),
                       0444u);
    create_static_file(root, "motd", "AukOS tmpfs is online\n", 22u, 0444u);
    log_info("tmpfs: initialized with user programs and /bin/toybox");
#undef EMBED_EXEC
}
#endif
