#include "include/aukos/ext4.h"

#include "include/aukos/block.h"
#include "include/aukos/log.h"
#include "include/aukos/vfs.h"

#include <stddef.h>
#include <stdint.h>

#define EXT4_SUPERBLOCK_OFFSET 1024u
#define EXT4_SUPERBLOCK_SIZE 1024u
#define EXT4_MAGIC 0xef53u
#define EXT4_EXTENT_MAGIC 0xf30au
#define EXT4_ROOT_INO 2u
#define EXT4_MIN_INODE_SIZE 128u
#define EXT4_MAX_INODE_SIZE 4096u
#define EXT4_MAX_BLOCK_SIZE 4096u
#define EXT4_MIN_DESC_SIZE 32u
#define EXT4_MAX_EXTENT_DEPTH 5u
#define EXT4_MAX_MOUNTS 2u
#define EXT4_MAX_VFS_NODES 128u
#define EXT4_INLINE_EXTENTS 4u
#define EXT4_MAX_FILE_BLOCKS (VFS_MAX_FILE_SIZE / 1024u)

#define EXT4_FEATURE_INCOMPAT_FILETYPE 0x0002u
#define EXT4_FEATURE_INCOMPAT_EXTENTS 0x0040u
#define EXT4_SUPPORTED_INCOMPAT (EXT4_FEATURE_INCOMPAT_FILETYPE | EXT4_FEATURE_INCOMPAT_EXTENTS)
#define EXT4_EXTENTS_FL 0x00080000u

#define EXT4_S_IFMT 0170000u
#define EXT4_S_IFREG 0100000u
#define EXT4_S_IFDIR 0040000u
#define EXT4_S_IFLNK 0120000u

#define EXT4_FT_UNKNOWN 0u
#define EXT4_FT_REG_FILE 1u
#define EXT4_FT_DIR 2u

struct ext4_fs {
    struct block_device *device;
    uint32_t block_size;
    uint64_t blocks_count;
    uint32_t inodes_count;
    uint32_t blocks_per_group;
    uint32_t inodes_per_group;
    uint32_t first_data_block;
    uint32_t first_ino;
    uint16_t inode_size;
    uint16_t descriptor_size;
    uint32_t feature_compat;
    uint32_t feature_incompat;
    uint32_t feature_ro_compat;
    uint32_t group_count;
    uint64_t descriptor_table_offset;
    uint32_t block_bitmap;
    uint32_t inode_bitmap;
    uint32_t inode_table;
    uint32_t inode_table_blocks;
    uint32_t free_blocks;
    uint32_t free_inodes;
    uint16_t used_dirs;
    int writable;
    int errored;
};

struct ext4_inode_data {
    uint32_t ino;
    uint16_t mode;
    uint16_t uid;
    uint16_t gid;
    uint16_t links;
    uint64_t size;
    uint32_t flags;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint8_t block[60];
};

struct ext4_dir_record {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t name_len;
    uint8_t file_type;
    char name[256];
};

struct ext4_mount;

struct ext4_vfs_node {
    int used;
    int linked;
    int dirty;
    uint32_t open_count;
    struct ext4_mount *mount;
    struct ext4_inode_data disk_inode;
    struct vfs_inode inode;
};

struct ext4_mount {
    int used;
    struct ext4_fs fs;
    struct vfs_superblock superblock;
    struct ext4_vfs_node nodes[EXT4_MAX_VFS_NODES];
};

struct extent_entry {
    uint32_t logical;
    uint16_t length;
    uint64_t physical;
};

static struct ext4_mount mounts[EXT4_MAX_MOUNTS];
static int ext4_probe_ok;
static struct vfs_file_ops ext4_file_ops;
static int ext4_fsync(struct vfs_file *file);

static uint16_t read_le16(const uint8_t *data, size_t offset)
{
    return (uint16_t)data[offset] | ((uint16_t)data[offset + 1u] << 8);
}

static uint32_t read_le32(const uint8_t *data, size_t offset)
{
    return (uint32_t)data[offset] | ((uint32_t)data[offset + 1u] << 8) |
           ((uint32_t)data[offset + 2u] << 16) |
           ((uint32_t)data[offset + 3u] << 24);
}

static void write_le16(uint8_t *data, size_t offset, uint16_t value)
{
    data[offset] = (uint8_t)value;
    data[offset + 1u] = (uint8_t)(value >> 8);
}

static void write_le32(uint8_t *data, size_t offset, uint32_t value)
{
    data[offset] = (uint8_t)value;
    data[offset + 1u] = (uint8_t)(value >> 8);
    data[offset + 2u] = (uint8_t)(value >> 16);
    data[offset + 3u] = (uint8_t)(value >> 24);
}

static void memory_copy(void *destination, const void *source, size_t size)
{
    uint8_t *dest = destination;
    const uint8_t *src = source;
    for (size_t i = 0; i < size; i++) dest[i] = src[i];
}

static void memory_zero(void *destination, size_t size)
{
    uint8_t *dest = destination;
    for (size_t i = 0; i < size; i++) dest[i] = 0;
}

static int checked_add_u64(uint64_t a, uint64_t b, uint64_t *result)
{
    if (a > UINT64_MAX - b) return -1;
    *result = a + b;
    return 0;
}

static int checked_mul_u64(uint64_t a, uint64_t b, uint64_t *result)
{
    if (a && b > UINT64_MAX / a) return -1;
    *result = a * b;
    return 0;
}

static uint64_t div_up(uint64_t value, uint64_t divisor)
{
    return value / divisor + (value % divisor != 0u);
}

static uint16_t aligned_dir_size(uint8_t name_len)
{
    return (uint16_t)((8u + name_len + 3u) & ~3u);
}

static int valid_name(const char *name)
{
    size_t length = 0;
    if (!name || !*name) return 0;
    while (name[length]) {
        if (name[length] == '/' || ++length >= VFS_MAX_NAME) return 0;
    }
    return !(length == 1u && name[0] == '.') &&
           !(length == 2u && name[0] == '.' && name[1] == '.');
}

static enum vfs_file_type mode_to_type(uint16_t mode)
{
    switch (mode & EXT4_S_IFMT) {
    case EXT4_S_IFREG: return VFS_FILE_REGULAR;
    case EXT4_S_IFDIR: return VFS_FILE_DIRECTORY;
    case EXT4_S_IFLNK: return VFS_FILE_SYMLINK;
    default: return VFS_FILE_UNKNOWN;
    }
}

static void fail_mount(struct ext4_fs *fs)
{
    if (fs && !fs->errored) log_error("ext4: write failure; mount forced read-only");
    if (fs) {
        fs->errored = 1;
        fs->writable = 0;
    }
}

static int read_block(const struct ext4_fs *fs, uint64_t block, void *buffer)
{
    uint64_t offset;
    return fs && block < fs->blocks_count &&
           checked_mul_u64(block, fs->block_size, &offset) == 0 &&
           block_read_bytes(fs->device, offset, buffer, fs->block_size) == 0 ? 0 : -1;
}

static int write_block(struct ext4_fs *fs, uint64_t block, const void *buffer)
{
    uint64_t offset;
    if (!fs || !fs->writable || block >= fs->blocks_count ||
        checked_mul_u64(block, fs->block_size, &offset) != 0 ||
        block_write_bytes(fs->device, offset, buffer, fs->block_size) != 0) {
        log_error("ext4: underlying block write failed");
        fail_mount(fs);
        return -1;
    }
    return 0;
}

static int ext4_load_geometry(struct block_device *device, struct ext4_fs *fs)
{
    uint8_t sb[EXT4_SUPERBLOCK_SIZE];
    uint8_t descriptor[EXT4_MIN_DESC_SIZE];
    uint32_t log_block_size, revision;
    uint64_t block_groups, inode_groups, fs_size, device_size, descriptor_bytes;

    if (!device || !fs || !device->read ||
        device->sector_size != BLOCK_SECTOR_SIZE ||
        device->sector_count > UINT64_MAX / device->sector_size ||
        block_read_bytes(device, EXT4_SUPERBLOCK_OFFSET, sb, sizeof(sb)) != 0 ||
        read_le16(sb, 0x38u) != EXT4_MAGIC) return -1;
    memory_zero(fs, sizeof(*fs));
    log_block_size = read_le32(sb, 0x18u);
    if (log_block_size > 2u) return -1;
    fs->device = device;
    fs->block_size = 1024u << log_block_size;
    fs->inodes_count = read_le32(sb, 0x00u);
    fs->blocks_count = read_le32(sb, 0x04u);
    fs->free_blocks = read_le32(sb, 0x0cu);
    fs->free_inodes = read_le32(sb, 0x10u);
    fs->first_data_block = read_le32(sb, 0x14u);
    fs->blocks_per_group = read_le32(sb, 0x20u);
    fs->inodes_per_group = read_le32(sb, 0x28u);
    fs->feature_compat = read_le32(sb, 0x5cu);
    fs->feature_incompat = read_le32(sb, 0x60u);
    fs->feature_ro_compat = read_le32(sb, 0x64u);
    revision = read_le32(sb, 0x4cu);
    fs->first_ino = revision == 0u ? 11u : read_le32(sb, 0x54u);
    fs->inode_size = revision == 0u ? EXT4_MIN_INODE_SIZE : read_le16(sb, 0x58u);
    fs->descriptor_size = revision == 0u ? EXT4_MIN_DESC_SIZE : read_le16(sb, 0xfeu);
    if (!fs->descriptor_size) fs->descriptor_size = EXT4_MIN_DESC_SIZE;
    if ((fs->feature_incompat & ~EXT4_SUPPORTED_INCOMPAT) ||
        read_le32(sb, 0x150u) || !fs->inodes_count || !fs->blocks_count ||
        fs->first_data_block >= fs->blocks_count || !fs->blocks_per_group ||
        !fs->inodes_per_group || fs->inode_size < EXT4_MIN_INODE_SIZE ||
        fs->inode_size > EXT4_MAX_INODE_SIZE || fs->inode_size > fs->block_size ||
        (fs->inode_size & 3u) || fs->descriptor_size < EXT4_MIN_DESC_SIZE ||
        fs->descriptor_size > fs->block_size || (fs->descriptor_size & 7u) ||
        fs->first_data_block != (fs->block_size == 1024u ? 1u : 0u) ||
        fs->blocks_per_group > fs->block_size * 8u ||
        fs->inodes_per_group > fs->block_size * 8u) return -1;
    block_groups = div_up(fs->blocks_count - fs->first_data_block,
                          fs->blocks_per_group);
    inode_groups = div_up(fs->inodes_count, fs->inodes_per_group);
    if (!block_groups || block_groups > UINT32_MAX || inode_groups > block_groups)
        return -1;
    fs->group_count = (uint32_t)block_groups;
    if (checked_mul_u64(fs->blocks_count, fs->block_size, &fs_size) ||
        checked_mul_u64(device->sector_count, device->sector_size, &device_size) ||
        fs_size > device_size) return -1;
    fs->descriptor_table_offset = (fs->block_size == 1024u ? 2u : 1u) *
                                  (uint64_t)fs->block_size;
    if (checked_mul_u64(fs->group_count, fs->descriptor_size, &descriptor_bytes) ||
        fs->descriptor_table_offset > fs_size ||
        descriptor_bytes > fs_size - fs->descriptor_table_offset ||
        block_read_bytes(device, fs->descriptor_table_offset, descriptor,
                         sizeof(descriptor)) != 0) return -1;
    fs->block_bitmap = read_le32(descriptor, 0u);
    fs->inode_bitmap = read_le32(descriptor, 4u);
    fs->inode_table = read_le32(descriptor, 8u);
    fs->used_dirs = read_le16(descriptor, 16u);
    fs->inode_table_blocks = (uint32_t)div_up(
        (uint64_t)fs->inodes_per_group * fs->inode_size, fs->block_size);
    return 0;
}

static int inode_offset(const struct ext4_fs *fs, uint32_t ino, uint64_t *offset)
{
    uint8_t descriptor[EXT4_MIN_DESC_SIZE];
    uint32_t group, index, table;
    uint64_t desc_offset, base, index_bytes, fs_size;
    if (!fs || !offset || !ino || ino > fs->inodes_count) return -1;
    group = (ino - 1u) / fs->inodes_per_group;
    index = (ino - 1u) % fs->inodes_per_group;
    desc_offset = fs->descriptor_table_offset + (uint64_t)group * fs->descriptor_size;
    if (group >= fs->group_count ||
        block_read_bytes(fs->device, desc_offset, descriptor, sizeof(descriptor))) return -1;
    table = read_le32(descriptor, 8u);
    if (table < fs->first_data_block || table >= fs->blocks_count ||
        checked_mul_u64(table, fs->block_size, &base) ||
        checked_mul_u64(index, fs->inode_size, &index_bytes) ||
        checked_add_u64(base, index_bytes, offset) ||
        checked_mul_u64(fs->blocks_count, fs->block_size, &fs_size) ||
        *offset > fs_size || fs->inode_size > fs_size - *offset) return -1;
    return 0;
}

static int read_inode(const struct ext4_fs *fs, uint32_t ino,
                      struct ext4_inode_data *inode)
{
    uint8_t raw[160];
    uint64_t offset;
    size_t size;
    if (!inode || inode_offset(fs, ino, &offset)) return -1;
    memory_zero(raw, sizeof(raw));
    size = fs->inode_size < sizeof(raw) ? fs->inode_size : sizeof(raw);
    if (block_read_bytes(fs->device, offset, raw, size)) return -1;
    memory_zero(inode, sizeof(*inode));
    inode->ino = ino;
    inode->mode = read_le16(raw, 0u);
    inode->uid = read_le16(raw, 2u);
    inode->size = read_le32(raw, 4u);
    inode->atime = read_le32(raw, 8u);
    inode->ctime = read_le32(raw, 12u);
    inode->mtime = read_le32(raw, 16u);
    inode->gid = read_le16(raw, 24u);
    inode->links = read_le16(raw, 26u);
    inode->flags = read_le32(raw, 32u);
    if ((inode->mode & EXT4_S_IFMT) == EXT4_S_IFREG)
        inode->size |= (uint64_t)read_le32(raw, 108u) << 32;
    memory_copy(inode->block, raw + 40u, sizeof(inode->block));
    return inode->mode ? 0 : -1;
}

static int write_inode(struct ext4_fs *fs, const struct ext4_inode_data *inode)
{
    uint8_t raw[EXT4_MAX_INODE_SIZE];
    uint64_t offset;
    uint32_t sectors = 0;
    if (!fs || !fs->writable || !inode || inode_offset(fs, inode->ino, &offset) ||
        block_read_bytes(fs->device, offset, raw, fs->inode_size)) {
        fail_mount(fs);
        return -1;
    }
    if (read_le16(inode->block, 0u) == EXT4_EXTENT_MAGIC) {
        uint16_t entries = read_le16(inode->block, 2u);
        for (uint16_t i = 0; i < entries; i++)
            sectors += (read_le16(inode->block, 16u + (size_t)i * 12u) & 0x7fffu) *
                       (fs->block_size / BLOCK_SECTOR_SIZE);
    }
    write_le16(raw, 0u, inode->mode);
    write_le16(raw, 2u, inode->uid);
    write_le32(raw, 4u, (uint32_t)inode->size);
    write_le32(raw, 8u, inode->atime);
    write_le32(raw, 12u, inode->ctime);
    write_le32(raw, 16u, inode->mtime);
    write_le32(raw, 20u, inode->links ? 0u : 1700000000u);
    write_le16(raw, 24u, inode->gid);
    write_le16(raw, 26u, inode->links);
    write_le32(raw, 28u, sectors);
    write_le32(raw, 32u, inode->flags);
    memory_copy(raw + 40u, inode->block, sizeof(inode->block));
    write_le32(raw, 108u, (uint32_t)(inode->size >> 32));
    if (block_write_bytes(fs->device, offset, raw, fs->inode_size)) {
        fail_mount(fs);
        return -1;
    }
    return 0;
}

static int parse_inline_extents(const struct ext4_inode_data *inode,
                                struct extent_entry *entries, uint16_t *count)
{
    uint16_t n, maximum, depth;
    if (!inode || !entries || !count ||
        read_le16(inode->block, 0u) != EXT4_EXTENT_MAGIC) return -1;
    n = read_le16(inode->block, 2u);
    maximum = read_le16(inode->block, 4u);
    depth = read_le16(inode->block, 6u);
    if (depth != 0u || !maximum || maximum > EXT4_INLINE_EXTENTS || n > maximum)
        return -1;
    for (uint16_t i = 0; i < n; i++) {
        size_t off = 12u + (size_t)i * 12u;
        uint16_t raw_length = read_le16(inode->block, off + 4u);
        entries[i].logical = read_le32(inode->block, off);
        entries[i].length = raw_length & 0x7fffu;
        entries[i].physical = ((uint64_t)read_le16(inode->block, off + 6u) << 32) |
                              read_le32(inode->block, off + 8u);
        if (!entries[i].length || (raw_length & 0x8000u)) return -1;
    }
    *count = n;
    return 0;
}

static void store_inline_extents(struct ext4_inode_data *inode,
                                 const struct extent_entry *entries,
                                 uint16_t count)
{
    memory_zero(inode->block, sizeof(inode->block));
    write_le16(inode->block, 0u, EXT4_EXTENT_MAGIC);
    write_le16(inode->block, 2u, count);
    write_le16(inode->block, 4u, EXT4_INLINE_EXTENTS);
    for (uint16_t i = 0; i < count; i++) {
        size_t off = 12u + (size_t)i * 12u;
        write_le32(inode->block, off, entries[i].logical);
        write_le16(inode->block, off + 4u, entries[i].length);
        write_le16(inode->block, off + 6u, (uint16_t)(entries[i].physical >> 32));
        write_le32(inode->block, off + 8u, (uint32_t)entries[i].physical);
    }
}

/* Returns zero for mapped, one for a hole, and -1 for corrupt metadata. */
static int map_extent(const struct ext4_fs *fs, const struct ext4_inode_data *inode,
                      uint32_t logical, uint64_t *physical)
{
    uint8_t block[EXT4_MAX_BLOCK_SIZE];
    const uint8_t *node = inode->block;
    size_t node_size = sizeof(inode->block);
    uint16_t expected_depth;
    if (!fs || !inode || !physical || !(inode->flags & EXT4_EXTENTS_FL) ||
        fs->block_size > sizeof(block)) return -1;
    expected_depth = read_le16(node, 6u);
    if (expected_depth > EXT4_MAX_EXTENT_DEPTH) return -1;
    for (;;) {
        uint16_t count, maximum, depth;
        size_t capacity;
        if (node_size < 12u || read_le16(node, 0u) != EXT4_EXTENT_MAGIC) return -1;
        count = read_le16(node, 2u);
        maximum = read_le16(node, 4u);
        depth = read_le16(node, 6u);
        capacity = (node_size - 12u) / 12u;
        if (count > maximum || !maximum || maximum > capacity ||
            depth != expected_depth) return -1;
        if (!depth) {
            uint64_t previous_end = 0;
            for (uint16_t i = 0; i < count; i++) {
                size_t off = 12u + (size_t)i * 12u;
                uint32_t start_logical = read_le32(node, off);
                uint16_t raw_length = read_le16(node, off + 4u);
                uint32_t length = raw_length & 0x7fffu;
                uint64_t start = ((uint64_t)read_le16(node, off + 6u) << 32) |
                                 read_le32(node, off + 8u);
                uint64_t logical_end = (uint64_t)start_logical + length;
                if (!length || (raw_length & 0x8000u) ||
                    (i && start_logical < previous_end) ||
                    start < fs->first_data_block || start + length > fs->blocks_count)
                    return -1;
                previous_end = logical_end;
                if (logical >= start_logical && logical < logical_end) {
                    *physical = start + logical - start_logical;
                    return 0;
                }
            }
            return 1;
        }
        {
            int selected = -1;
            uint32_t previous = 0;
            for (uint16_t i = 0; i < count; i++) {
                size_t off = 12u + (size_t)i * 12u;
                uint32_t key = read_le32(node, off);
                if (i && key <= previous) return -1;
                previous = key;
                if (key <= logical) selected = i;
            }
            if (selected < 0) return 1;
            size_t off = 12u + (size_t)selected * 12u;
            uint64_t child = ((uint64_t)read_le16(node, off + 8u) << 32) |
                             read_le32(node, off + 4u);
            if (read_block(fs, child, block)) return -1;
        }
        node = block;
        node_size = fs->block_size;
        expected_depth--;
    }
}

static int read_inode_data(const struct ext4_fs *fs,
                           const struct ext4_inode_data *inode, uint64_t offset,
                           void *buffer, size_t count, size_t *bytes_read)
{
    uint8_t *out = buffer;
    uint64_t remaining, position = offset;
    if (!fs || !inode || !bytes_read || (!buffer && count)) return -1;
    *bytes_read = 0;
    if (offset >= inode->size || !count) return 0;
    remaining = inode->size - offset;
    if (remaining > count) remaining = count;
    while (remaining) {
        uint64_t logical64 = position / fs->block_size, physical;
        size_t in_block = position % fs->block_size;
        size_t chunk = fs->block_size - in_block;
        int mapped;
        if (logical64 > UINT32_MAX) return -1;
        if (chunk > remaining) chunk = (size_t)remaining;
        mapped = map_extent(fs, inode, (uint32_t)logical64, &physical);
        if (mapped < 0) return -1;
        if (mapped == 1) memory_zero(out, chunk);
        else {
            uint64_t disk_offset = physical * (uint64_t)fs->block_size + in_block;
            if (block_read_bytes(fs->device, disk_offset, out, chunk)) return -1;
        }
        out += chunk;
        position += chunk;
        remaining -= chunk;
        *bytes_read += chunk;
    }
    return 0;
}

static int read_dir_record(const struct ext4_fs *fs,
                           const struct ext4_inode_data *directory,
                           uint64_t offset, struct ext4_dir_record *record)
{
    uint8_t header[8];
    size_t got;
    uint64_t block_remaining;
    if (!fs || !directory || !record || mode_to_type(directory->mode) != VFS_FILE_DIRECTORY ||
        offset >= directory->size ||
        read_inode_data(fs, directory, offset, header, sizeof(header), &got) ||
        got != sizeof(header)) return -1;
    record->inode = read_le32(header, 0u);
    record->rec_len = read_le16(header, 4u);
    record->name_len = header[6u];
    record->file_type = header[7u];
    block_remaining = fs->block_size - offset % fs->block_size;
    if (record->rec_len < 8u || (record->rec_len & 3u) ||
        record->rec_len > block_remaining || record->rec_len > directory->size - offset ||
        record->name_len > record->rec_len - 8u || record->file_type > 7u ||
        record->inode > fs->inodes_count) return -1;
    if (record->name_len &&
        (read_inode_data(fs, directory, offset + 8u, record->name,
                         record->name_len, &got) || got != record->name_len)) return -1;
    record->name[record->name_len] = '\0';
    return 0;
}

static int names_equal(const char *name, const struct ext4_dir_record *record)
{
    size_t i = 0;
    while (name[i] && i < record->name_len) {
        if (name[i] != record->name[i]) return 0;
        i++;
    }
    return !name[i] && i == record->name_len;
}

static int find_in_directory(const struct ext4_fs *fs,
                             const struct ext4_inode_data *directory,
                             const char *name, uint32_t *ino)
{
    struct ext4_dir_record record;
    uint64_t offset = 0;
    if (!name || !ino) return -1;
    while (offset < directory->size) {
        if (read_dir_record(fs, directory, offset, &record)) return -1;
        if (record.inode && names_equal(name, &record)) {
            *ino = record.inode;
            return 0;
        }
        offset += record.rec_len;
    }
    return -1;
}

static void sync_vfs_inode(struct ext4_vfs_node *node)
{
    node->inode.ino = node->disk_inode.ino;
    node->inode.type = mode_to_type(node->disk_inode.mode);
    node->inode.mode = node->disk_inode.mode;
    node->inode.uid = node->disk_inode.uid;
    node->inode.gid = node->disk_inode.gid;
    node->inode.size = node->disk_inode.size;
    node->inode.atime = node->disk_inode.atime;
    node->inode.mtime = node->disk_inode.mtime;
    node->inode.ctime = node->disk_inode.ctime;
}

static struct ext4_vfs_node *get_node(struct ext4_mount *mount, uint32_t ino)
{
    struct ext4_vfs_node *free_node = 0;
    if (!mount) return 0;
    for (size_t i = 0; i < EXT4_MAX_VFS_NODES; i++) {
        if (mount->nodes[i].used && mount->nodes[i].disk_inode.ino == ino)
            return &mount->nodes[i];
        if (!mount->nodes[i].used && !free_node) free_node = &mount->nodes[i];
    }
    if (!free_node || read_inode(&mount->fs, ino, &free_node->disk_inode)) return 0;
    free_node->used = 1;
    free_node->linked = !mount->fs.writable || free_node->disk_inode.links != 0u;
    free_node->mount = mount;
    free_node->inode.sb = &mount->superblock;
    free_node->inode.private_data = free_node;
    free_node->inode.ops = &ext4_file_ops;
    sync_vfs_inode(free_node);
    if (free_node->inode.type == VFS_FILE_UNKNOWN) {
        memory_zero(free_node, sizeof(*free_node));
        return 0;
    }
    return free_node;
}

static int bitmap_bit(const uint8_t *bitmap, uint32_t bit)
{
    return (bitmap[bit / 8u] >> (bit % 8u)) & 1u;
}

static void bitmap_set(uint8_t *bitmap, uint32_t bit, int value)
{
    uint8_t mask = (uint8_t)(1u << (bit % 8u));
    if (value) bitmap[bit / 8u] |= mask;
    else bitmap[bit / 8u] &= (uint8_t)~mask;
}

static int write_free_counts(struct ext4_fs *fs)
{
    uint8_t sb[EXT4_SUPERBLOCK_SIZE], descriptor[EXT4_MIN_DESC_SIZE];
    if (block_read_bytes(fs->device, EXT4_SUPERBLOCK_OFFSET, sb, sizeof(sb)) ||
        block_read_bytes(fs->device, fs->descriptor_table_offset, descriptor,
                         sizeof(descriptor))) {
        fail_mount(fs);
        return -1;
    }
    write_le32(sb, 0x0cu, fs->free_blocks);
    write_le32(sb, 0x10u, fs->free_inodes);
    write_le16(descriptor, 12u, (uint16_t)fs->free_blocks);
    write_le16(descriptor, 14u, (uint16_t)fs->free_inodes);
    write_le16(descriptor, 16u, fs->used_dirs);
    if (block_write_bytes(fs->device, EXT4_SUPERBLOCK_OFFSET, sb, sizeof(sb)) ||
        block_write_bytes(fs->device, fs->descriptor_table_offset, descriptor,
                          sizeof(descriptor))) {
        fail_mount(fs);
        return -1;
    }
    return 0;
}

static int validate_writable_geometry(struct ext4_fs *fs)
{
    uint8_t block_bitmap[EXT4_MAX_BLOCK_SIZE];
    uint8_t inode_bitmap[EXT4_MAX_BLOCK_SIZE];
    uint32_t free_blocks = 0, free_inodes = 0;
    uint64_t table_end;
    if (fs->block_size != 4096u || fs->group_count != 1u ||
        fs->blocks_count > fs->blocks_per_group ||
        fs->inodes_count > fs->inodes_per_group || fs->feature_compat != 0u ||
        fs->feature_incompat != EXT4_SUPPORTED_INCOMPAT || fs->feature_ro_compat != 0u ||
        fs->first_ino < 11u || fs->first_ino > fs->inodes_count ||
        fs->block_bitmap >= fs->blocks_count || fs->inode_bitmap >= fs->blocks_count ||
        fs->inode_table >= fs->blocks_count ||
        checked_add_u64(fs->inode_table, fs->inode_table_blocks, &table_end) ||
        table_end > fs->blocks_count || !fs->device->write || !fs->device->flush ||
        read_block(fs, fs->block_bitmap, block_bitmap) ||
        read_block(fs, fs->inode_bitmap, inode_bitmap)) return -1;
    for (uint32_t block = 0; block < fs->blocks_count; block++)
        if (!bitmap_bit(block_bitmap, block)) free_blocks++;
    for (uint32_t ino = 1; ino <= fs->inodes_count; ino++)
        if (!bitmap_bit(inode_bitmap, ino - 1u)) free_inodes++;
    if (free_blocks != fs->free_blocks || free_inodes != fs->free_inodes ||
        !bitmap_bit(block_bitmap, fs->block_bitmap) ||
        !bitmap_bit(block_bitmap, fs->inode_bitmap)) return -1;
    for (uint32_t block = fs->inode_table; block < table_end; block++)
        if (!bitmap_bit(block_bitmap, block)) return -1;
    for (uint32_t ino = 1; ino < fs->first_ino; ino++)
        if (!bitmap_bit(inode_bitmap, ino - 1u)) return -1;
    return 0;
}

static int allocate_inode_number(struct ext4_fs *fs, uint32_t *ino)
{
    uint8_t bitmap[EXT4_MAX_BLOCK_SIZE];
    if (!fs->writable || !fs->free_inodes || read_block(fs, fs->inode_bitmap, bitmap))
        return -1;
    for (uint32_t candidate = fs->first_ino; candidate <= fs->inodes_count; candidate++) {
        if (!bitmap_bit(bitmap, candidate - 1u)) {
            bitmap_set(bitmap, candidate - 1u, 1);
            if (write_block(fs, fs->inode_bitmap, bitmap)) return -1;
            fs->free_inodes--;
            if (write_free_counts(fs)) return -1;
            *ino = candidate;
            return 0;
        }
    }
    return -1;
}

static int allocate_block_number(struct ext4_fs *fs, uint32_t *result)
{
    uint8_t bitmap[EXT4_MAX_BLOCK_SIZE];
    uint8_t zero[EXT4_MAX_BLOCK_SIZE];
    if (!fs->writable || !fs->free_blocks || read_block(fs, fs->block_bitmap, bitmap))
        return -1;
    for (uint32_t block = fs->first_data_block; block < fs->blocks_count; block++) {
        if (!bitmap_bit(bitmap, block)) {
            memory_zero(zero, fs->block_size);
            if (write_block(fs, block, zero)) {
                fail_mount(fs);
                return -1;
            }
            bitmap_set(bitmap, block, 1);
            if (write_block(fs, fs->block_bitmap, bitmap)) return -1;
            fs->free_blocks--;
            if (write_free_counts(fs)) return -1;
            *result = block;
            return 0;
        }
    }
    return -1;
}

static int free_block_number(struct ext4_fs *fs, uint32_t block)
{
    uint8_t bitmap[EXT4_MAX_BLOCK_SIZE];
    if (block >= fs->blocks_count || read_block(fs, fs->block_bitmap, bitmap) ||
        !bitmap_bit(bitmap, block)) return -1;
    bitmap_set(bitmap, block, 0);
    if (write_block(fs, fs->block_bitmap, bitmap)) return -1;
    fs->free_blocks++;
    return write_free_counts(fs);
}

static int free_inode_number(struct ext4_fs *fs, uint32_t ino)
{
    uint8_t bitmap[EXT4_MAX_BLOCK_SIZE];
    if (ino < fs->first_ino || ino > fs->inodes_count ||
        read_block(fs, fs->inode_bitmap, bitmap) ||
        !bitmap_bit(bitmap, ino - 1u)) return -1;
    bitmap_set(bitmap, ino - 1u, 0);
    if (write_block(fs, fs->inode_bitmap, bitmap)) return -1;
    fs->free_inodes++;
    return write_free_counts(fs);
}

static int append_extent_mapping(struct ext4_inode_data *inode, uint32_t logical,
                                 uint32_t physical)
{
    struct extent_entry extents[EXT4_INLINE_EXTENTS];
    struct extent_entry expanded[EXT4_INLINE_EXTENTS + 1u];
    struct extent_entry merged[EXT4_INLINE_EXTENTS + 1u];
    uint16_t count, out = 0, inserted = 0;
    if (parse_inline_extents(inode, extents, &count)) return -1;
    for (uint16_t i = 0; i < count; i++) {
        if (logical >= extents[i].logical &&
            logical < extents[i].logical + extents[i].length) return -1;
        if (!inserted && logical < extents[i].logical) {
            expanded[out++] = (struct extent_entry){logical, 1u, physical};
            inserted = 1;
        }
        expanded[out++] = extents[i];
    }
    if (!inserted) expanded[out++] = (struct extent_entry){logical, 1u, physical};
    count = out;
    out = 0;
    for (uint16_t i = 0; i < count; i++) {
        if (out && merged[out - 1u].logical + merged[out - 1u].length == expanded[i].logical &&
            merged[out - 1u].physical + merged[out - 1u].length == expanded[i].physical &&
            merged[out - 1u].length < 0x7fffu) {
            merged[out - 1u].length++;
        } else {
            merged[out++] = expanded[i];
        }
    }
    if (out > EXT4_INLINE_EXTENTS) return -1;
    store_inline_extents(inode, merged, out);
    return 0;
}

static int directory_block(struct ext4_fs *fs, const struct ext4_inode_data *dir,
                           uint64_t *physical)
{
    if (dir->size != fs->block_size || map_extent(fs, dir, 0u, physical) != 0)
        return -1;
    return 0;
}

static int dir_find_buffer(const uint8_t *buffer, uint32_t block_size,
                           const char *name, size_t *offset_out,
                           uint32_t *ino_out)
{
    size_t offset = 0;
    while (offset < block_size) {
        uint16_t rec_len = read_le16(buffer, offset + 4u);
        uint8_t name_len = buffer[offset + 6u];
        if (rec_len < 8u || (rec_len & 3u) || rec_len > block_size - offset ||
            name_len > rec_len - 8u) return -1;
        if (read_le32(buffer, offset) && name_len) {
            size_t i = 0;
            while (name[i] && i < name_len && name[i] == (char)buffer[offset + 8u + i]) i++;
            if (!name[i] && i == name_len) {
                if (offset_out) *offset_out = offset;
                if (ino_out) *ino_out = read_le32(buffer, offset);
                return 0;
            }
        }
        offset += rec_len;
    }
    return -1;
}

static void fill_dirent(uint8_t *buffer, size_t offset, uint16_t rec_len,
                        uint32_t ino, uint8_t type, const char *name)
{
    uint8_t length = 0;
    while (name[length]) length++;
    memory_zero(buffer + offset, rec_len);
    write_le32(buffer, offset, ino);
    write_le16(buffer, offset + 4u, rec_len);
    buffer[offset + 6u] = length;
    buffer[offset + 7u] = type;
    memory_copy(buffer + offset + 8u, name, length);
}

static int dir_insert_buffer(uint8_t *buffer, uint32_t block_size, const char *name,
                             uint32_t ino, uint8_t type)
{
    size_t offset = 0;
    uint8_t name_len = 0;
    while (name[name_len]) name_len++;
    uint16_t need = aligned_dir_size(name_len);
    if (dir_find_buffer(buffer, block_size, name, 0, 0) == 0) return -1;
    while (offset < block_size) {
        uint32_t existing = read_le32(buffer, offset);
        uint16_t rec_len = read_le16(buffer, offset + 4u);
        uint8_t existing_len = buffer[offset + 6u];
        uint16_t used = aligned_dir_size(existing_len);
        if (rec_len < 8u || rec_len > block_size - offset) return -1;
        if (!existing && rec_len >= need) {
            fill_dirent(buffer, offset, rec_len, ino, type, name);
            return 0;
        }
        if (existing && rec_len >= used + need) {
            write_le16(buffer, offset + 4u, used);
            fill_dirent(buffer, offset + used, rec_len - used, ino, type, name);
            return 0;
        }
        offset += rec_len;
    }
    return -1;
}

static int dir_remove_buffer(uint8_t *buffer, uint32_t block_size,
                             const char *name, uint32_t *removed)
{
    size_t offset, previous = SIZE_MAX, cursor = 0;
    if (dir_find_buffer(buffer, block_size, name, &offset, removed)) return -1;
    while (cursor < offset) {
        previous = cursor;
        cursor += read_le16(buffer, cursor + 4u);
    }
    if (previous != SIZE_MAX) {
        uint16_t combined = read_le16(buffer, previous + 4u) +
                            read_le16(buffer, offset + 4u);
        write_le16(buffer, previous + 4u, combined);
    } else {
        write_le32(buffer, offset, 0u);
    }
    return 0;
}

static int add_directory_entry(struct ext4_vfs_node *dir, const char *name,
                               uint32_t ino, uint8_t type)
{
    uint8_t buffer[EXT4_MAX_BLOCK_SIZE];
    uint64_t physical;
    struct ext4_fs *fs = &dir->mount->fs;
    if (directory_block(fs, &dir->disk_inode, &physical) ||
        read_block(fs, physical, buffer) ||
        dir_insert_buffer(buffer, fs->block_size, name, ino, type) ||
        write_block(fs, physical, buffer)) return -1;
    return 0;
}

static int reclaim_node(struct ext4_vfs_node *node)
{
    struct ext4_fs *fs;
    struct extent_entry extents[EXT4_INLINE_EXTENTS];
    uint16_t count;
    if (!node || node->linked || node->open_count) return 0;
    fs = &node->mount->fs;
    if (parse_inline_extents(&node->disk_inode, extents, &count)) return -1;
    node->disk_inode.links = 0;
    node->disk_inode.size = 0;
    store_inline_extents(&node->disk_inode, extents, 0u);
    if (write_inode(fs, &node->disk_inode)) return -1;
    for (uint16_t i = 0; i < count; i++)
        for (uint16_t b = 0; b < extents[i].length; b++)
            if (free_block_number(fs, (uint32_t)(extents[i].physical + b))) return -1;
    if (free_inode_number(fs, node->disk_inode.ino)) return -1;
    memory_zero(node, sizeof(*node));
    return 0;
}

static int ext4_truncate_inode(struct vfs_inode *inode, uint64_t length)
{
    struct ext4_vfs_node *node = inode ? inode->private_data : 0;
    struct ext4_fs *fs;
    struct extent_entry old[EXT4_INLINE_EXTENTS], kept[EXT4_INLINE_EXTENTS];
    uint16_t old_count, kept_count = 0;
    uint32_t keep_blocks;
    if (!node || inode->type != VFS_FILE_REGULAR || length > VFS_MAX_FILE_SIZE ||
        !(fs = &node->mount->fs)->writable ||
        parse_inline_extents(&node->disk_inode, old, &old_count)) return -1;
    if (length >= node->disk_inode.size) {
        node->disk_inode.size = length;
        if (write_inode(fs, &node->disk_inode)) return -1;
        node->dirty = 0;
        sync_vfs_inode(node);
        return 0;
    }
    keep_blocks = (uint32_t)div_up(length, fs->block_size);
    for (uint16_t i = 0; i < old_count; i++) {
        struct extent_entry e = old[i];
        if (e.logical >= keep_blocks) continue;
        if ((uint64_t)e.logical + e.length > keep_blocks)
            e.length = (uint16_t)(keep_blocks - e.logical);
        kept[kept_count++] = e;
    }
    if (length && (length % fs->block_size)) {
        uint64_t physical;
        if (map_extent(fs, &node->disk_inode, keep_blocks - 1u, &physical) == 0) {
            uint8_t buffer[EXT4_MAX_BLOCK_SIZE];
            size_t tail = length % fs->block_size;
            if (read_block(fs, physical, buffer)) return -1;
            memory_zero(buffer + tail, fs->block_size - tail);
            if (write_block(fs, physical, buffer)) return -1;
        }
    }
    node->disk_inode.size = length;
    store_inline_extents(&node->disk_inode, kept, kept_count);
    if (write_inode(fs, &node->disk_inode) || block_flush(fs->device)) {
        fail_mount(fs);
        return -1;
    }
    for (uint16_t i = 0; i < old_count; i++) {
        uint32_t first = old[i].logical < keep_blocks ? keep_blocks - old[i].logical : 0u;
        if (first > old[i].length) first = old[i].length;
        for (uint32_t b = first; b < old[i].length; b++)
            if (free_block_number(fs, (uint32_t)(old[i].physical + b))) return -1;
    }
    sync_vfs_inode(node);
    node->dirty = 0;
    return 0;
}

static int ext4_open_file(struct vfs_inode *inode, struct vfs_file *file)
{
    struct ext4_vfs_node *node = inode ? inode->private_data : 0;
    uint32_t access;
    if (!node || !file) return -1;
    access = file->flags & VFS_O_ACCMODE;
    if (!node->mount->fs.writable &&
        (access != VFS_O_RDONLY ||
         (file->flags & (VFS_O_CREAT | VFS_O_TRUNC | VFS_O_APPEND)))) return -1;
    if ((file->flags & VFS_O_TRUNC) &&
        (access == VFS_O_RDONLY || inode->type != VFS_FILE_REGULAR ||
         ext4_truncate_inode(inode, 0u))) return -1;
    node->open_count++;
    return 0;
}

static int ext4_close_file(struct vfs_file *file)
{
    struct ext4_vfs_node *node = file && file->inode ? file->inode->private_data : 0;
    if (!node || !node->open_count) return -1;
    if (node->mount->fs.writable && node->linked && node->dirty &&
        ext4_fsync(file)) return -1;
    node->open_count--;
    return node->mount->fs.writable ? reclaim_node(node) : 0;
}

static size_t ext4_read_file(struct vfs_file *file, void *buffer, size_t count,
                             size_t offset)
{
    struct ext4_vfs_node *node = file && file->inode ? file->inode->private_data : 0;
    size_t got;
    if (!node || file->inode->type != VFS_FILE_REGULAR ||
        read_inode_data(&node->mount->fs, &node->disk_inode, offset,
                        buffer, count, &got)) return SIZE_MAX;
    return got;
}

static size_t ext4_write_file(struct vfs_file *file, const void *buffer,
                              size_t count, size_t offset)
{
    struct ext4_vfs_node *node = file && file->inode ? file->inode->private_data : 0;
    struct ext4_fs *fs;
    struct ext4_inode_data next;
    const uint8_t *input = buffer;
    size_t remaining = count;
    uint64_t position = offset, end;
    if (!node || file->inode->type != VFS_FILE_REGULAR || (!buffer && count) ||
        !(fs = &node->mount->fs)->writable || offset > VFS_MAX_FILE_SIZE ||
        count > VFS_MAX_FILE_SIZE - offset ||
        parse_inline_extents(&node->disk_inode,
                             (struct extent_entry[EXT4_INLINE_EXTENTS]){{0}},
                             &(uint16_t){0})) return SIZE_MAX;
    if (!count) return 0;
    end = (uint64_t)offset + count;
    next = node->disk_inode;
    while (remaining) {
        uint32_t logical = position / fs->block_size;
        size_t in_block = position % fs->block_size;
        size_t chunk = fs->block_size - in_block;
        uint64_t physical;
        uint8_t data[EXT4_MAX_BLOCK_SIZE];
        int mapped = map_extent(fs, &next, logical, &physical);
        if (chunk > remaining) chunk = remaining;
        if (mapped < 0) return SIZE_MAX;
        if (mapped == 1) {
            uint32_t allocated;
            if (allocate_block_number(fs, &allocated) ||
                append_extent_mapping(&next, logical, allocated)) {
                log_error("ext4: extent allocation/update failed");
                fail_mount(fs);
                return SIZE_MAX;
            }
            physical = allocated;
            memory_zero(data, fs->block_size);
        } else if (read_block(fs, physical, data)) return SIZE_MAX;
        memory_copy(data + in_block, input, chunk);
        if (write_block(fs, physical, data)) return SIZE_MAX;
        input += chunk;
        position += chunk;
        remaining -= chunk;
    }
    if (end > next.size) next.size = end;
    node->disk_inode = next;
    node->dirty = 1;
    sync_vfs_inode(node);
    return count;
}

static int ext4_lookup_vfs(struct vfs_inode *directory, const char *name,
                           struct vfs_inode **out_inode)
{
    struct ext4_vfs_node *dir = directory ? directory->private_data : 0;
    struct ext4_vfs_node *result;
    uint32_t ino;
    if (!dir || directory->type != VFS_FILE_DIRECTORY || !out_inode ||
        find_in_directory(&dir->mount->fs, &dir->disk_inode, name, &ino) ||
        !(result = get_node(dir->mount, ino))) return -1;
    *out_inode = &result->inode;
    return 0;
}

static int ext4_readdir_vfs(struct vfs_file *file, void *buffer, size_t count)
{
    struct ext4_vfs_node *dir = file && file->inode ? file->inode->private_data : 0;
    struct vfs_dirent *entries = buffer;
    size_t maximum = count / sizeof(*entries), visible = 0, emitted = 0;
    uint64_t offset = 0;
    if (!dir || file->inode->type != VFS_FILE_DIRECTORY || !entries || !maximum)
        return -1;
    while (offset < dir->disk_inode.size) {
        struct ext4_dir_record record;
        struct ext4_vfs_node *child;
        if (read_dir_record(&dir->mount->fs, &dir->disk_inode, offset, &record)) return -1;
        offset += record.rec_len;
        if (!record.inode || (record.name_len == 1u && record.name[0] == '.') ||
            (record.name_len == 2u && record.name[0] == '.' && record.name[1] == '.'))
            continue;
        if (visible++ < file->offset) continue;
        if (emitted == maximum) break;
        child = get_node(dir->mount, record.inode);
        if (!child || record.name_len >= VFS_MAX_NAME) return -1;
        memory_copy(entries[emitted].name, record.name, record.name_len + 1u);
        entries[emitted].type = child->inode.type;
        entries[emitted].size = child->inode.size;
        emitted++;
    }
    file->offset += emitted;
    return (int)emitted;
}

static int initialize_new_inode(struct ext4_mount *mount, uint32_t ino,
                                uint16_t type_mode, uint32_t mode,
                                struct ext4_inode_data *inode)
{
    memory_zero(inode, sizeof(*inode));
    inode->ino = ino;
    inode->mode = type_mode | (mode & 0777u);
    inode->links = 1u;
    inode->flags = EXT4_EXTENTS_FL;
    store_inline_extents(inode, (struct extent_entry[1]){{0}}, 0u);
    return write_inode(&mount->fs, inode);
}

static int ext4_create(struct vfs_inode *directory, const char *name,
                       uint32_t mode, struct vfs_inode **out_inode)
{
    struct ext4_vfs_node *dir = directory ? directory->private_data : 0;
    struct ext4_vfs_node *node;
    struct ext4_inode_data inode;
    uint32_t ino, ignored;
    if (!dir || directory->type != VFS_FILE_DIRECTORY || !out_inode ||
        !valid_name(name) || !dir->mount->fs.writable ||
        find_in_directory(&dir->mount->fs, &dir->disk_inode, name, &ignored) == 0 ||
        allocate_inode_number(&dir->mount->fs, &ino) ||
        initialize_new_inode(dir->mount, ino, EXT4_S_IFREG, mode, &inode) ||
        add_directory_entry(dir, name, ino, EXT4_FT_REG_FILE) ||
        !(node = get_node(dir->mount, ino))) return -1;
    *out_inode = &node->inode;
    return 0;
}

static int ext4_mkdir(struct vfs_inode *directory, const char *name,
                      uint32_t mode, struct vfs_inode **out_inode)
{
    struct ext4_vfs_node *dir = directory ? directory->private_data : 0;
    struct ext4_inode_data inode;
    struct ext4_vfs_node *node;
    uint8_t block[EXT4_MAX_BLOCK_SIZE];
    uint32_t ino, data, ignored;
    if (!dir || directory->type != VFS_FILE_DIRECTORY || !out_inode ||
        !valid_name(name) || !dir->mount->fs.writable ||
        find_in_directory(&dir->mount->fs, &dir->disk_inode, name, &ignored) == 0 ||
        allocate_inode_number(&dir->mount->fs, &ino) ||
        allocate_block_number(&dir->mount->fs, &data) ||
        initialize_new_inode(dir->mount, ino, EXT4_S_IFDIR, mode, &inode)) return -1;
    inode.size = dir->mount->fs.block_size;
    inode.links = 2u;
    if (append_extent_mapping(&inode, 0u, data)) return -1;
    memory_zero(block, sizeof(block));
    fill_dirent(block, 0u, 12u, ino, EXT4_FT_DIR, ".");
    fill_dirent(block, 12u, (uint16_t)(dir->mount->fs.block_size - 12u),
                dir->disk_inode.ino, EXT4_FT_DIR, "..");
    if (write_block(&dir->mount->fs, data, block) ||
        block_flush(dir->mount->fs.device) ||
        write_inode(&dir->mount->fs, &inode) ||
        add_directory_entry(dir, name, ino, EXT4_FT_DIR)) return -1;
    dir->disk_inode.links++;
    if (write_inode(&dir->mount->fs, &dir->disk_inode)) return -1;
    dir->mount->fs.used_dirs++;
    if (write_free_counts(&dir->mount->fs) || !(node = get_node(dir->mount, ino)))
        return -1;
    *out_inode = &node->inode;
    return 0;
}

static int ext4_unlink(struct vfs_inode *directory, const char *name)
{
    struct ext4_vfs_node *dir = directory ? directory->private_data : 0;
    struct ext4_vfs_node *node;
    struct ext4_fs *fs;
    uint8_t block[EXT4_MAX_BLOCK_SIZE];
    uint64_t physical;
    uint32_t ino;
    if (!dir || directory->type != VFS_FILE_DIRECTORY || !valid_name(name) ||
        !(fs = &dir->mount->fs)->writable ||
        find_in_directory(fs, &dir->disk_inode, name, &ino) ||
        !(node = get_node(dir->mount, ino)) || node->inode.type == VFS_FILE_DIRECTORY ||
        directory_block(fs, &dir->disk_inode, &physical) || read_block(fs, physical, block) ||
        dir_remove_buffer(block, fs->block_size, name, &ino) ||
        write_block(fs, physical, block)) return -1;
    node->linked = 0;
    node->disk_inode.links = 0;
    return reclaim_node(node);
}

static int ext4_rename(struct vfs_inode *old_directory, const char *old_name,
                       struct vfs_inode *new_directory, const char *new_name)
{
    struct ext4_vfs_node *old_dir = old_directory ? old_directory->private_data : 0;
    struct ext4_vfs_node *new_dir = new_directory ? new_directory->private_data : 0;
    struct ext4_fs *fs;
    struct ext4_vfs_node *source, *destination = 0;
    uint8_t old_block[EXT4_MAX_BLOCK_SIZE], new_block[EXT4_MAX_BLOCK_SIZE];
    uint64_t old_physical, new_physical;
    uint32_t source_ino, destination_ino;
    int destination_exists;
    if (!old_dir || !new_dir || old_dir->mount != new_dir->mount ||
        !valid_name(old_name) || !valid_name(new_name) ||
        !(fs = &old_dir->mount->fs)->writable ||
        find_in_directory(fs, &old_dir->disk_inode, old_name, &source_ino) ||
        !(source = get_node(old_dir->mount, source_ino)) ||
        source->inode.type != VFS_FILE_REGULAR) return -1;
    if (old_dir == new_dir) {
        size_t old_offset, new_offset;
        if (old_name[0] == new_name[0]) {
            size_t i = 0; while (old_name[i] && old_name[i] == new_name[i]) i++;
            if (!old_name[i] && !new_name[i]) return 0;
        }
        if (directory_block(fs, &old_dir->disk_inode, &old_physical) ||
            read_block(fs, old_physical, old_block) ||
            dir_find_buffer(old_block, fs->block_size, old_name, &old_offset, 0)) return -1;
        destination_exists = dir_find_buffer(old_block, fs->block_size, new_name,
                                             &new_offset, &destination_ino) == 0;
        if (destination_exists) {
            destination = get_node(old_dir->mount, destination_ino);
            if (!destination || destination->inode.type != VFS_FILE_REGULAR) return -1;
            write_le32(old_block, new_offset, source_ino);
            old_block[new_offset + 7u] = EXT4_FT_REG_FILE;
            if (dir_remove_buffer(old_block, fs->block_size, old_name, &source_ino)) return -1;
        } else {
            if (dir_remove_buffer(old_block, fs->block_size, old_name, &source_ino) ||
                dir_insert_buffer(old_block, fs->block_size, new_name, source_ino,
                                  EXT4_FT_REG_FILE)) return -1;
        }
        if (write_block(fs, old_physical, old_block)) return -1;
    } else {
        if (directory_block(fs, &old_dir->disk_inode, &old_physical) ||
            directory_block(fs, &new_dir->disk_inode, &new_physical) ||
            read_block(fs, old_physical, old_block) || read_block(fs, new_physical, new_block))
            return -1;
        destination_exists = dir_find_buffer(new_block, fs->block_size, new_name,
                                             0, &destination_ino) == 0;
        if (destination_exists) {
            size_t destination_offset;
            destination = get_node(old_dir->mount, destination_ino);
            if (!destination || destination->inode.type != VFS_FILE_REGULAR ||
                dir_find_buffer(new_block, fs->block_size, new_name,
                                &destination_offset, 0)) return -1;
            write_le32(new_block, destination_offset, source_ino);
        } else if (dir_insert_buffer(new_block, fs->block_size, new_name, source_ino,
                                     EXT4_FT_REG_FILE)) return -1;
        if (dir_remove_buffer(old_block, fs->block_size, old_name, &source_ino) ||
            write_block(fs, new_physical, new_block) ||
            write_block(fs, old_physical, old_block)) return -1;
    }
    if (destination) {
        destination->linked = 0;
        destination->disk_inode.links = 0;
        if (reclaim_node(destination)) return -1;
    }
    return 0;
}

static int ext4_fsync(struct vfs_file *file)
{
    struct ext4_vfs_node *node = file && file->inode ? file->inode->private_data : 0;
    if (!node || file->inode->type != VFS_FILE_REGULAR) return -1;
    if (!node->mount->fs.writable) return node->mount->fs.errored ? -1 : 0;
    if (block_flush(node->mount->fs.device) ||
        write_inode(&node->mount->fs, &node->disk_inode) ||
        block_flush(node->mount->fs.device)) {
        fail_mount(&node->mount->fs);
        return -1;
    }
    node->dirty = 0;
    return 0;
}

static struct vfs_file_ops ext4_file_ops = {
    .open = ext4_open_file, .close = ext4_close_file,
    .read = ext4_read_file, .write = ext4_write_file,
    .readdir = ext4_readdir_vfs, .lookup = ext4_lookup_vfs,
    .create = ext4_create, .unlink = ext4_unlink, .mkdir = ext4_mkdir,
    .truncate = ext4_truncate_inode, .rename = ext4_rename, .fsync = ext4_fsync,
};

struct vfs_superblock *ext4_mount_device(struct block_device *device,
                                         int request_writable)
{
    struct ext4_mount *mount = 0;
    struct ext4_vfs_node *root;
    struct ext4_dir_record first_record;
    uint64_t root_block;
    for (size_t i = 0; i < EXT4_MAX_MOUNTS; i++)
        if (!mounts[i].used) { mount = &mounts[i]; break; }
    if (!mount) return 0;
    memory_zero(mount, sizeof(*mount));
    if (ext4_load_geometry(device, &mount->fs)) return 0;
    if (request_writable) {
        if (validate_writable_geometry(&mount->fs)) {
            log_error("ext4: work filesystem is not in the supported writable subset");
            return 0;
        }
        mount->fs.writable = 1;
    }
    mount->used = 1;
    mount->superblock.magic = EXT4_MAGIC;
    mount->superblock.block_size = mount->fs.block_size;
    mount->superblock.total_blocks = mount->fs.blocks_count;
    mount->superblock.free_blocks = mount->fs.free_blocks;
    mount->superblock.private_data = mount;
    root = get_node(mount, EXT4_ROOT_INO);
    if (!root || root->inode.type != VFS_FILE_DIRECTORY ||
        map_extent(&mount->fs, &root->disk_inode, 0u, &root_block) != 0 ||
        read_dir_record(&mount->fs, &root->disk_inode, 0u, &first_record) != 0) {
        memory_zero(mount, sizeof(*mount));
        return 0;
    }
    mount->superblock.root_inode = &root->inode;
    return &mount->superblock;
}

int ext4_probe(struct block_device *device)
{
    struct ext4_fs fs;
    if (ext4_load_geometry(device, &fs)) return -1;
    ext4_probe_ok = 1;
    return 0;
}

int ext4_available(void) { return ext4_probe_ok; }

static int fixture_vfs_selftest(void)
{
    static const char expected[] = "Hello from AukOS ext4!\n";
    struct vfs_file *file;
    char contents[sizeof(expected) - 1u];
    if (vfs_open("/ext4/hello.txt", VFS_O_RDONLY, 0u, &file) ||
        vfs_read(file, contents, sizeof(contents)) != sizeof(contents) ||
        vfs_close(file)) return -1;
    for (size_t i = 0; i < sizeof(contents); i++)
        if (contents[i] != expected[i]) return -1;
    return 0;
}

static int work_device_selftest(struct block_device *device,
                                const struct vfs_superblock *sb)
{
    uint8_t original[BLOCK_SECTOR_SIZE], pattern[BLOCK_SECTOR_SIZE], readback[BLOCK_SECTOR_SIZE];
    uint64_t fs_sectors = sb->total_blocks * (sb->block_size / BLOCK_SECTOR_SIZE);
    uint64_t sector = device->sector_count - 1u;
    if (fs_sectors >= device->sector_count || device->read(device, sector, original, 1u)) return -1;
    for (size_t i = 0; i < sizeof(pattern); i++) pattern[i] = (uint8_t)(i ^ 0x6du);
    if (device->write(device, sector, pattern, 1u) || block_flush(device) ||
        device->read(device, sector, readback, 1u)) return -1;
    for (size_t i = 0; i < sizeof(pattern); i++)
        if (pattern[i] != readback[i]) return -1;
    return device->write(device, sector, original, 1u) || block_flush(device) ? -1 : 0;
}

void ext4_run_selftest(void)
{
    struct block_device *ram = block_find("ram-ext4");
    struct block_device *fixture = block_find("virtio0");
    struct block_device *work = block_find("virtio1");
    struct vfs_superblock *fixture_sb, *work_sb;
    ext4_probe_ok = 0;
    if (!ram || ext4_probe(ram)) {
        log_error("ext4: RAM parser regression failed");
        return;
    }
    fixture_sb = ext4_mount_device(fixture, 0);
    if (!fixture_sb || vfs_mount_superblock("/ext4", fixture_sb) ||
        fixture_vfs_selftest()) {
        log_error("ext4: virtio0 read-only mount regression failed");
        return;
    }
    log_info("ext4: mounted /ext4 read-only from virtio0");
    work_sb = ext4_mount_device(work, 1);
    if (!work_sb || work_device_selftest(work, work_sb) ||
        vfs_mount_superblock("/work", work_sb)) {
        log_error("ext4: writable virtio1 work mount failed");
        return;
    }
    ext4_probe_ok = 1;
    log_info("ext4: mounted /work read-write from virtio1 with flush");
}
