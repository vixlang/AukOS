#include "include/aukos/ramdisk.h"

#include "include/aukos/block.h"
#include "include/aukos/log.h"

#define SCRATCH_SECTORS 2u
#define FAT32_SECTORS 16u
#define EXT4_SECTORS 64u
#define EXT4_BLOCK_SIZE 1024u
#define EXT4_SUPERBLOCK_OFFSET 1024u
#define EXT4_INODE_SIZE 256u
#define EXT4_ROOT_INODE 2u
#define EXT4_HELLO_INODE 12u
#define EXT4_INODE_TABLE_BLOCK 5u
#define EXT4_ROOT_DATA_BLOCK 9u
#define EXT4_HELLO_DATA_BLOCK 10u

struct ramdisk_data {
    uint8_t *storage;
    uint64_t sector_count;
};

static uint8_t scratch_storage[SCRATCH_SECTORS * BLOCK_SECTOR_SIZE];
static uint8_t fat32_storage[FAT32_SECTORS * BLOCK_SECTOR_SIZE];
static uint8_t ext4_storage[EXT4_SECTORS * BLOCK_SECTOR_SIZE];
static struct ramdisk_data ramdisk_data[3];
static struct block_device ramdisk_devices[3];

static const char ext4_hello_contents[] = "Hello from AukOS ext4!\n";

static void write_le16(uint8_t *storage, size_t offset, uint16_t value)
{
    storage[offset] = (uint8_t)value;
    storage[offset + 1] = (uint8_t)(value >> 8);
}

static void write_le32(uint8_t *storage, size_t offset, uint32_t value)
{
    storage[offset] = (uint8_t)value;
    storage[offset + 1] = (uint8_t)(value >> 8);
    storage[offset + 2] = (uint8_t)(value >> 16);
    storage[offset + 3] = (uint8_t)(value >> 24);
}

static void write_bytes(uint8_t *storage, size_t offset, const char *value, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        storage[offset + i] = (uint8_t)value[i];
    }
}

static void ramdisk_seed_fat32_fixture(void)
{
    fat32_storage[0] = 0xebu;
    fat32_storage[1] = 0x58u;
    fat32_storage[2] = 0x90u;
    write_bytes(fat32_storage, 3u, "AUKOSFS ", 8u);
    write_le16(fat32_storage, 11u, BLOCK_SECTOR_SIZE);
    fat32_storage[13] = 1u;
    write_le16(fat32_storage, 14u, 1u);
    fat32_storage[16] = 1u;
    write_le16(fat32_storage, 17u, 0u);
    write_le16(fat32_storage, 19u, 0u);
    fat32_storage[21] = 0xf8u;
    write_le16(fat32_storage, 22u, 0u);
    write_le16(fat32_storage, 24u, 1u);
    write_le16(fat32_storage, 26u, 1u);
    write_le32(fat32_storage, 28u, 0u);
    write_le32(fat32_storage, 32u, FAT32_SECTORS);
    write_le32(fat32_storage, 36u, 1u);
    write_le16(fat32_storage, 40u, 0u);
    write_le16(fat32_storage, 42u, 0u);
    write_le32(fat32_storage, 44u, 2u);
    write_le16(fat32_storage, 48u, 1u);
    write_le16(fat32_storage, 50u, 6u);
    fat32_storage[64] = 0x80u;
    fat32_storage[66] = 0x29u;
    write_le32(fat32_storage, 67u, 0x41554b31u);
    write_bytes(fat32_storage, 71u, "AUKOS RAM  ", 11u);
    write_bytes(fat32_storage, 82u, "FAT32   ", 8u);
    write_le16(fat32_storage, 510u, 0xaa55u);
}

static void seed_extent_inode(size_t inode_number, uint16_t mode, uint32_t size, uint32_t data_block)
{
    size_t inode = EXT4_INODE_TABLE_BLOCK * EXT4_BLOCK_SIZE +
                   (inode_number - 1u) * EXT4_INODE_SIZE;
    size_t extent = inode + 40u;

    write_le16(ext4_storage, inode + 0u, mode);
    write_le32(ext4_storage, inode + 4u, size);
    write_le32(ext4_storage, inode + 28u, 2u);
    write_le32(ext4_storage, inode + 32u, 0x00080000u);

    write_le16(ext4_storage, extent + 0u, 0xf30au);
    write_le16(ext4_storage, extent + 2u, 1u);
    write_le16(ext4_storage, extent + 4u, 4u);
    write_le16(ext4_storage, extent + 6u, 0u);
    write_le32(ext4_storage, extent + 12u, 0u);
    write_le16(ext4_storage, extent + 16u, 1u);
    write_le16(ext4_storage, extent + 18u, 0u);
    write_le32(ext4_storage, extent + 20u, data_block);
}

static void seed_dirent(size_t offset, uint32_t inode, uint16_t rec_len,
                        uint8_t name_len, uint8_t type, const char *name)
{
    write_le32(ext4_storage, offset, inode);
    write_le16(ext4_storage, offset + 4u, rec_len);
    ext4_storage[offset + 6u] = name_len;
    ext4_storage[offset + 7u] = type;
    write_bytes(ext4_storage, offset + 8u, name, name_len);
}

static void ramdisk_seed_ext4_fixture(void)
{
    size_t sb = EXT4_SUPERBLOCK_OFFSET;
    size_t directory = EXT4_ROOT_DATA_BLOCK * EXT4_BLOCK_SIZE;

    write_le32(ext4_storage, sb + 0x00u, 16u);
    write_le32(ext4_storage, sb + 0x04u, EXT4_SECTORS * BLOCK_SECTOR_SIZE / EXT4_BLOCK_SIZE);
    write_le32(ext4_storage, sb + 0x14u, 1u);
    write_le32(ext4_storage, sb + 0x18u, 0u);
    write_le32(ext4_storage, sb + 0x20u, 32u);
    write_le32(ext4_storage, sb + 0x28u, 16u);
    write_le16(ext4_storage, sb + 0x38u, 0xef53u);
    write_le32(ext4_storage, sb + 0x4cu, 1u);
    write_le32(ext4_storage, sb + 0x54u, 11u);
    write_le16(ext4_storage, sb + 0x58u, EXT4_INODE_SIZE);
    write_le32(ext4_storage, sb + 0x60u, 0x42u);
    write_le16(ext4_storage, sb + 0xfeu, 32u);

    write_le32(ext4_storage, 2u * EXT4_BLOCK_SIZE + 8u, EXT4_INODE_TABLE_BLOCK);

    seed_extent_inode(EXT4_ROOT_INODE, 0040755u, EXT4_BLOCK_SIZE, EXT4_ROOT_DATA_BLOCK);
    seed_extent_inode(EXT4_HELLO_INODE, 0100644u,
                      (uint32_t)(sizeof(ext4_hello_contents) - 1u), EXT4_HELLO_DATA_BLOCK);

    seed_dirent(directory, EXT4_ROOT_INODE, 12u, 1u, 2u, ".");
    seed_dirent(directory + 12u, EXT4_ROOT_INODE, 12u, 2u, 2u, "..");
    seed_dirent(directory + 24u, EXT4_HELLO_INODE, EXT4_BLOCK_SIZE - 24u,
                9u, 1u, "hello.txt");
    write_bytes(ext4_storage, EXT4_HELLO_DATA_BLOCK * EXT4_BLOCK_SIZE,
                ext4_hello_contents, sizeof(ext4_hello_contents) - 1u);
}

static void memory_copy(void *dest, const void *src, size_t size)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    for (size_t i = 0; i < size; i++) {
        d[i] = s[i];
    }
}

static int ramdisk_transfer_bounds(uint64_t sector, size_t sector_count)
{
    return sector <= UINT64_MAX - sector_count;
}

static int ramdisk_read(struct block_device *device, uint64_t sector, void *buffer, size_t sector_count)
{
    struct ramdisk_data *data = (struct ramdisk_data *)device->private_data;

    if (!data || !buffer || !ramdisk_transfer_bounds(sector, sector_count) ||
        sector > data->sector_count || sector_count > data->sector_count - sector) {
        return -1;
    }

    memory_copy(buffer, &data->storage[sector * BLOCK_SECTOR_SIZE], sector_count * BLOCK_SECTOR_SIZE);
    return 0;
}

static int ramdisk_write(struct block_device *device, uint64_t sector, const void *buffer, size_t sector_count)
{
    struct ramdisk_data *data = (struct ramdisk_data *)device->private_data;

    if (!data || !buffer || !ramdisk_transfer_bounds(sector, sector_count) ||
        sector > data->sector_count || sector_count > data->sector_count - sector) {
        return -1;
    }

    memory_copy(&data->storage[sector * BLOCK_SECTOR_SIZE], buffer, sector_count * BLOCK_SECTOR_SIZE);
    return 0;
}

void ramdisk_init(void)
{
    static const char *names[3] = {"ram-scratch", "ram-fat32", "ram-ext4"};
    static uint8_t *storages[3] = {scratch_storage, fat32_storage, ext4_storage};
    static const uint64_t sector_counts[3] = {SCRATCH_SECTORS, FAT32_SECTORS, EXT4_SECTORS};

    ramdisk_seed_fat32_fixture();
    ramdisk_seed_ext4_fixture();

    for (size_t i = 0; i < 3u; i++) {
        ramdisk_data[i].storage = storages[i];
        ramdisk_data[i].sector_count = sector_counts[i];
        ramdisk_devices[i].name = names[i];
        ramdisk_devices[i].sector_count = sector_counts[i];
        ramdisk_devices[i].sector_size = BLOCK_SECTOR_SIZE;
        ramdisk_devices[i].private_data = &ramdisk_data[i];
        ramdisk_devices[i].read = ramdisk_read;
        ramdisk_devices[i].write = ramdisk_write;

        if (block_register(&ramdisk_devices[i]) != 0) {
            log_error("ramdisk: failed to register fixture device");
            return;
        }
    }
    log_info("ramdisk: scratch, FAT32, and ext4 fixtures ready");
}
