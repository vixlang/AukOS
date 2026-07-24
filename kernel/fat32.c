#include "include/aukos/fat32.h"

#include "include/aukos/block.h"
#include "include/aukos/log.h"

#include <stddef.h>
#include <stdint.h>

#define FAT32_BOOT_SIGNATURE_OFFSET 510u
#define FAT32_BOOT_SIGNATURE 0xaa55u
#define FAT32_BYTES_PER_SECTOR_OFFSET 11u
#define FAT32_SECTORS_PER_CLUSTER_OFFSET 13u
#define FAT32_RESERVED_SECTORS_OFFSET 14u
#define FAT32_FAT_COUNT_OFFSET 16u
#define FAT32_TOTAL_SECTORS_16_OFFSET 19u
#define FAT32_TOTAL_SECTORS_32_OFFSET 32u
#define FAT32_FAT_SIZE_16_OFFSET 22u
#define FAT32_FAT_SIZE_32_OFFSET 36u
#define FAT32_ROOT_CLUSTER_OFFSET 44u
#define FAT32_FS_TYPE_OFFSET 82u

static int fat32_probe_ok;

static uint16_t read_le16(const uint8_t *data, size_t offset)
{
    return (uint16_t)data[offset] | ((uint16_t)data[offset + 1] << 8);
}

static uint32_t read_le32(const uint8_t *data, size_t offset)
{
    return (uint32_t)data[offset] |
           ((uint32_t)data[offset + 1] << 8) |
           ((uint32_t)data[offset + 2] << 16) |
           ((uint32_t)data[offset + 3] << 24);
}

static int memory_equals(const uint8_t *left, const char *right, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        if (left[i] != (uint8_t)right[i]) {
            return 0;
        }
    }
    return 1;
}

static int valid_cluster_size(uint8_t sectors_per_cluster)
{
    return sectors_per_cluster == 1u || sectors_per_cluster == 2u || sectors_per_cluster == 4u ||
           sectors_per_cluster == 8u || sectors_per_cluster == 16u || sectors_per_cluster == 32u ||
           sectors_per_cluster == 64u || sectors_per_cluster == 128u;
}

int fat32_probe(struct block_device *device)
{
    uint8_t boot_sector[BLOCK_SECTOR_SIZE];
    uint16_t signature;
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t total_sectors_16;
    uint16_t fat_size_16;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;
    uint32_t root_cluster;

    if (!device || !device->read || device->sector_size != BLOCK_SECTOR_SIZE) {
        return -1;
    }
    if (device->read(device, 0, boot_sector, 1) != 0) {
        return -1;
    }

    signature = read_le16(boot_sector, FAT32_BOOT_SIGNATURE_OFFSET);
    bytes_per_sector = read_le16(boot_sector, FAT32_BYTES_PER_SECTOR_OFFSET);
    sectors_per_cluster = boot_sector[FAT32_SECTORS_PER_CLUSTER_OFFSET];
    reserved_sectors = read_le16(boot_sector, FAT32_RESERVED_SECTORS_OFFSET);
    fat_count = boot_sector[FAT32_FAT_COUNT_OFFSET];
    total_sectors_16 = read_le16(boot_sector, FAT32_TOTAL_SECTORS_16_OFFSET);
    total_sectors_32 = read_le32(boot_sector, FAT32_TOTAL_SECTORS_32_OFFSET);
    fat_size_16 = read_le16(boot_sector, FAT32_FAT_SIZE_16_OFFSET);
    fat_size_32 = read_le32(boot_sector, FAT32_FAT_SIZE_32_OFFSET);
    root_cluster = read_le32(boot_sector, FAT32_ROOT_CLUSTER_OFFSET);

    if (signature != FAT32_BOOT_SIGNATURE || bytes_per_sector != BLOCK_SECTOR_SIZE) {
        log_error("fat32: boot sector signature mismatch");
        return -1;
    }
    if (!valid_cluster_size(sectors_per_cluster) || reserved_sectors == 0 || fat_count == 0) {
        log_error("fat32: invalid BPB geometry");
        return -1;
    }
    if (total_sectors_16 != 0 || total_sectors_32 == 0 || fat_size_16 != 0 || fat_size_32 == 0 || root_cluster < 2u) {
        log_error("fat32: invalid FAT32-only BPB fields");
        return -1;
    }
    if (!memory_equals(&boot_sector[FAT32_FS_TYPE_OFFSET], "FAT32   ", 8)) {
        log_error("fat32: fs type marker mismatch");
        return -1;
    }

    log_info("fat32: read-only BPB probe passed");
    fat32_probe_ok = 1;
    return 0;
}

int fat32_available(void)
{
    return fat32_probe_ok;
}

void fat32_run_selftest(void)
{
    struct block_device *device = block_find("ram-fat32");

    if (!device) {
        log_error("fat32: self-test skipped, no block device");
        return;
    }

    if (fat32_probe(device) == 0) {
        log_info("fat32: ramdisk probe self-test passed");
    } else {
        log_error("fat32: ramdisk probe self-test failed");
    }
}
