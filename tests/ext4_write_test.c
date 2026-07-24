#include <aukos/block.h>
#include <aukos/ext4.h>
#include <aukos/log.h>
#include <aukos/vfs.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct memory_disk {
    uint8_t *bytes;
    size_t size;
    unsigned flushes;
    int fail_flush;
};

void log_write(enum log_level level, const char *message)
{
    if (level == LOG_ERROR) fprintf(stderr, "%s\n", message);
}

void *kmalloc(size_t size) { return calloc(1u, size); }

static int memory_read(struct block_device *device, uint64_t sector,
                       void *buffer, size_t count)
{
    struct memory_disk *disk = device->private_data;
    uint64_t offset = sector * BLOCK_SECTOR_SIZE;
    uint64_t size = count * BLOCK_SECTOR_SIZE;
    if (sector > device->sector_count || count > device->sector_count - sector ||
        offset > disk->size || size > disk->size - offset) return -1;
    memcpy(buffer, disk->bytes + offset, (size_t)size);
    return 0;
}

static int memory_write(struct block_device *device, uint64_t sector,
                        const void *buffer, size_t count)
{
    struct memory_disk *disk = device->private_data;
    uint64_t offset = sector * BLOCK_SECTOR_SIZE;
    uint64_t size = count * BLOCK_SECTOR_SIZE;
    if (sector > device->sector_count || count > device->sector_count - sector ||
        offset > disk->size || size > disk->size - offset) return -1;
    memcpy(disk->bytes + offset, buffer, (size_t)size);
    return 0;
}

static int memory_flush(struct block_device *device)
{
    struct memory_disk *disk = device->private_data;
    disk->flushes++;
    return disk->fail_flush ? -1 : 0;
}

static int fail(const char *message)
{
    fprintf(stderr, "ext4_write_test: %s\n", message);
    return 1;
}

static int write_all(struct vfs_file *file, const void *data, size_t size)
{
    return vfs_write(file, data, size) == size ? 0 : -1;
}

static int load_image(const char *path, struct memory_disk *disk)
{
    FILE *file = fopen(path, "rb");
    long size;
    if (!file || fseek(file, 0, SEEK_END) || (size = ftell(file)) <= 0 ||
        fseek(file, 0, SEEK_SET)) return -1;
    disk->bytes = malloc((size_t)size);
    disk->size = (size_t)size;
    if (!disk->bytes || fread(disk->bytes, 1u, disk->size, file) != disk->size ||
        fclose(file)) return -1;
    return 0;
}

static int save_image(const char *path, const struct memory_disk *disk)
{
    FILE *file = fopen(path, "wb");
    if (!file || fwrite(disk->bytes, 1u, disk->size, file) != disk->size ||
        fclose(file)) return -1;
    return 0;
}

static uint32_t get32(const uint8_t *bytes, size_t offset)
{
    return (uint32_t)bytes[offset] | ((uint32_t)bytes[offset + 1u] << 8) |
           ((uint32_t)bytes[offset + 2u] << 16) |
           ((uint32_t)bytes[offset + 3u] << 24);
}

static void put32(uint8_t *bytes, size_t offset, uint32_t value)
{
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1u] = (uint8_t)(value >> 8);
    bytes[offset + 2u] = (uint8_t)(value >> 16);
    bytes[offset + 3u] = (uint8_t)(value >> 24);
}

static int reject_corrupt_images(struct memory_disk *disk,
                                 struct block_device *device)
{
    size_t incompat = 1024u + 0x60u;
    uint32_t saved = get32(disk->bytes, incompat);
    uint32_t inode_table = get32(disk->bytes, 4096u + 8u);
    uint32_t block_bitmap = get32(disk->bytes, 4096u);
    uint32_t root_inode = inode_table * 4096u + 256u;
    uint32_t extent_start = get32(disk->bytes, root_inode + 60u);
    uint8_t saved_bitmap;

    put32(disk->bytes, incompat, saved | 0x80000000u);
    if (ext4_mount_device(device, 1)) return -1;
    put32(disk->bytes, incompat, saved);

    put32(disk->bytes, 4096u + 8u, 9000u);
    if (ext4_mount_device(device, 1)) return -1;
    put32(disk->bytes, 4096u + 8u, inode_table);

    saved_bitmap = disk->bytes[block_bitmap * 4096u];
    disk->bytes[block_bitmap * 4096u] ^= 1u;
    if (ext4_mount_device(device, 1)) return -1;
    disk->bytes[block_bitmap * 4096u] = saved_bitmap;

    put32(disk->bytes, root_inode + 60u, 9000u);
    if (ext4_mount_device(device, 1)) return -1;
    put32(disk->bytes, root_inode + 60u, extent_start);

    {
        size_t rec_len = (size_t)extent_start * 4096u + 4u;
        uint8_t low = disk->bytes[rec_len], high = disk->bytes[rec_len + 1u];
        disk->bytes[rec_len] = 6u;
        disk->bytes[rec_len + 1u] = 0u;
        if (ext4_mount_device(device, 1)) return -1;
        disk->bytes[rec_len] = low;
        disk->bytes[rec_len + 1u] = high;
    }
    return 0;
}

int main(int argc, char **argv)
{
    struct memory_disk disk = {0};
    struct block_device device;
    struct vfs_superblock *sb;
    struct vfs_file *file, *old, *replacement, *directory;
    struct vfs_dirent entries[8];
    uint8_t input[9000], output[9000];
    uint64_t offset;
    int entry_count, found_child = 0;

    if (argc != 3 || load_image(argv[1], &disk)) return fail("load image");
    device = (struct block_device){
        .name = "memory-work", .sector_count = disk.size / BLOCK_SECTOR_SIZE,
        .sector_size = BLOCK_SECTOR_SIZE, .private_data = &disk,
        .read = memory_read, .write = memory_write, .flush = memory_flush,
    };
    block_init();
    vfs_init();
    if (reject_corrupt_images(&disk, &device) ||
        !(sb = ext4_mount_device(&device, 1)) ||
        vfs_mount_superblock("/work", sb)) return fail("mount writable image");

    for (size_t i = 0; i < sizeof(input); i++) input[i] = (uint8_t)(i * 17u + 3u);
    if (vfs_open("/work/data", VFS_O_CREAT | VFS_O_EXCL | VFS_O_RDWR, 0644u, &file) ||
        write_all(file, input, sizeof(input)) || vfs_fsync(file) ||
        vfs_seek(file, 0, 0, &offset) ||
        vfs_read(file, output, sizeof(output)) != sizeof(output) ||
        memcmp(input, output, sizeof(input)) || vfs_close(file))
        return fail("cross-block create/write/read/fsync");
    if (vfs_open("/work/data", VFS_O_CREAT | VFS_O_EXCL | VFS_O_RDWR, 0644u, &file) == 0)
        return fail("O_EXCL");

    if (vfs_open("/work/hole", VFS_O_CREAT | VFS_O_RDWR, 0644u, &file) ||
        vfs_seek(file, 8190, 0, &offset) || write_all(file, "tail", 4u) ||
        vfs_seek(file, 0, 0, &offset) || vfs_read(file, output, 16u) != 16u)
        return fail("hole setup");
    for (size_t i = 0; i < 16u; i++) if (output[i]) return fail("hole zero fill");
    if (vfs_ftruncate(file, 4097u) || vfs_ftruncate(file, 12000u) ||
        vfs_seek(file, 4097, 0, &offset) || vfs_read(file, output, 32u) != 32u)
        return fail("truncate grow/shrink");
    for (size_t i = 0; i < 32u; i++) if (output[i]) return fail("truncate zero fill");
    if (vfs_close(file)) return fail("truncate close");

    if (vfs_mkdir("/work/dir", 0755u) ||
        vfs_open("/work/dir/child", VFS_O_CREAT | VFS_O_RDWR, 0600u, &file) ||
        write_all(file, "child", 5u) || vfs_close(file) ||
        vfs_open("/work/dir", VFS_O_RDONLY, 0u, &directory)) return fail("mkdir/create");
    entry_count = vfs_readdir(directory, entries, sizeof(entries));
    if (entry_count < 1) return fail("readdir count");
    for (int i = 0; i < entry_count; i++)
        if (!strcmp(entries[i].name, "child")) found_child = 1;
    if (!found_child || vfs_close(directory)) return fail("readdir child");

    if (vfs_open("/work/open", VFS_O_CREAT | VFS_O_RDWR, 0644u, &old) ||
        write_all(old, "old", 3u) || vfs_unlink("/work/open") ||
        vfs_open("/work/open", VFS_O_CREAT | VFS_O_EXCL | VFS_O_RDWR, 0644u,
                 &replacement) || write_all(replacement, "new", 3u) ||
        vfs_seek(old, 0, 0, &offset) || vfs_read(old, output, 3u) != 3u ||
        memcmp(output, "old", 3u) || vfs_close(old) || vfs_close(replacement))
        return fail("unlink-open-file");

    if (vfs_open("/work/source", VFS_O_CREAT | VFS_O_RDWR, 0644u, &file) ||
        write_all(file, "source", 6u) || vfs_close(file) ||
        vfs_open("/work/target", VFS_O_CREAT | VFS_O_RDWR, 0644u, &old) ||
        write_all(old, "target", 6u) ||
        vfs_rename("/work/source", "/work/target") ||
        vfs_open("/work/target", VFS_O_RDONLY, 0u, &replacement) ||
        vfs_seek(old, 0, 0, &offset) || vfs_read(old, output, 6u) != 6u ||
        memcmp(output, "target", 6u) ||
        vfs_read(replacement, output, 6u) != 6u || memcmp(output, "source", 6u) ||
        vfs_close(old) || vfs_close(replacement)) return fail("rename replacement lifetime");

    for (int round = 0; round < 128; round++) {
        if (vfs_open("/work/reuse", VFS_O_CREAT | VFS_O_EXCL | VFS_O_RDWR,
                     0644u, &file) || write_all(file, &round, sizeof(round)) ||
            vfs_rename("/work/reuse", "/work/reused") ||
            vfs_unlink("/work/reused") || vfs_close(file))
            return fail("128-round reuse");
    }
    if (disk.flushes < 2u) return fail("flush accounting");
    if (vfs_open("/work/data", VFS_O_RDWR, 0u, &file)) return fail("flush failure open");
    disk.fail_flush = 1;
    if (vfs_fsync(file) == 0 || vfs_write(file, "x", 1u) != SIZE_MAX)
        return fail("flush failure forces read-only");
    disk.fail_flush = 0;
    if (vfs_close(file) || save_image(argv[2], &disk)) return fail("save image");
    printf("ext4_write_test: 29 checks passed, %u flushes\n", disk.flushes);
    free(disk.bytes);
    return 0;
}
