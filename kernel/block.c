#include "include/aukos/block.h"

#include "include/aukos/log.h"

#define MAX_BLOCK_DEVICES 8u

static struct block_device *devices[MAX_BLOCK_DEVICES];
static uint32_t device_count;

static int string_equals(const char *left, const char *right)
{
    if (!left || !right) {
        return 0;
    }

    while (*left != '\0' && *right != '\0') {
        if (*left++ != *right++) {
            return 0;
        }
    }
    return *left == *right;
}

void block_init(void)
{
    device_count = 0;
    log_info("block: initialized");
}

int block_register(struct block_device *device)
{
    if (!device || !device->read || !device->write || device->sector_size == 0 || device_count >= MAX_BLOCK_DEVICES) {
        return -1;
    }

    devices[device_count++] = device;
    log_info("block: registered device");
    return 0;
}

struct block_device *block_get(uint32_t index)
{
    if (index >= device_count) {
        return 0;
    }

    return devices[index];
}

struct block_device *block_find(const char *name)
{
    for (uint32_t i = 0; i < device_count; i++) {
        if (string_equals(devices[i]->name, name)) {
            return devices[i];
        }
    }
    return 0;
}

uint32_t block_count(void)
{
    return device_count;
}

int block_read_bytes(struct block_device *device, uint64_t offset, void *buffer, size_t size)
{
    uint8_t sector_buffer[BLOCK_SECTOR_SIZE];
    uint8_t *output = (uint8_t *)buffer;
    uint64_t device_size;

    if (!device || !device->read || (!buffer && size != 0) ||
        device->sector_size != BLOCK_SECTOR_SIZE ||
        device->sector_count > UINT64_MAX / device->sector_size) {
        return -1;
    }

    device_size = device->sector_count * device->sector_size;
    if (offset > device_size || (uint64_t)size > device_size - offset) {
        return -1;
    }

    while (size != 0) {
        uint64_t sector = offset / device->sector_size;
        size_t sector_offset = (size_t)(offset % device->sector_size);
        size_t chunk = device->sector_size - sector_offset;

        if (chunk > size) {
            chunk = size;
        }
        if (sector_offset == 0 && chunk == device->sector_size) {
            if (device->read(device, sector, output, 1) != 0) {
                return -1;
            }
        } else {
            if (device->read(device, sector, sector_buffer, 1) != 0) {
                return -1;
            }
            for (size_t i = 0; i < chunk; i++) {
                output[i] = sector_buffer[sector_offset + i];
            }
        }

        output += chunk;
        offset += chunk;
        size -= chunk;
    }
    return 0;
}

int block_write_bytes(struct block_device *device, uint64_t offset,
                      const void *buffer, size_t size)
{
    uint8_t sector_buffer[BLOCK_SECTOR_SIZE];
    const uint8_t *input = (const uint8_t *)buffer;
    uint64_t device_size;

    if (!device || !device->read || !device->write || (!buffer && size != 0) ||
        device->sector_size != BLOCK_SECTOR_SIZE ||
        device->sector_count > UINT64_MAX / device->sector_size) {
        return -1;
    }
    device_size = device->sector_count * device->sector_size;
    if (offset > device_size || (uint64_t)size > device_size - offset) {
        return -1;
    }

    while (size != 0) {
        uint64_t sector = offset / device->sector_size;
        size_t sector_offset = (size_t)(offset % device->sector_size);
        size_t chunk;

        if (sector_offset == 0 && size >= device->sector_size) {
            size_t sectors = size / device->sector_size;

            if (sectors > device->sector_count - sector) {
                return -1;
            }
            if (device->write(device, sector, input, sectors) != 0) {
                return -1;
            }
            chunk = sectors * device->sector_size;
        } else {
            chunk = device->sector_size - sector_offset;
            if (chunk > size) {
                chunk = size;
            }
            if (device->read(device, sector, sector_buffer, 1u) != 0) {
                return -1;
            }
            for (size_t i = 0; i < chunk; i++) {
                sector_buffer[sector_offset + i] = input[i];
            }
            if (device->write(device, sector, sector_buffer, 1u) != 0) {
                return -1;
            }
        }
        input += chunk;
        offset += chunk;
        size -= chunk;
    }
    return 0;
}

int block_flush(struct block_device *device)
{
    return device && device->flush ? device->flush(device) : -1;
}

static int memory_equals(const uint8_t *left, const uint8_t *right, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        if (left[i] != right[i]) {
            return 0;
        }
    }
    return 1;
}

void block_run_selftest(void)
{
    struct block_device *device = block_find("ram-scratch");
    uint8_t write_buffer[BLOCK_SECTOR_SIZE];
    uint8_t read_buffer[BLOCK_SECTOR_SIZE];
    uint64_t test_sector;

    if (!device) {
        log_error("block: self-test skipped, no scratch device");
        return;
    }
    if (device->sector_count == 0) {
        log_error("block: self-test skipped, empty device");
        return;
    }

    test_sector = device->sector_count - 1;

    for (size_t i = 0; i < BLOCK_SECTOR_SIZE; i++) {
        write_buffer[i] = (uint8_t)(i ^ 0xa5u);
        read_buffer[i] = 0;
    }

    if (device->write(device, test_sector, write_buffer, 1) == 0 &&
        device->read(device, test_sector, read_buffer, 1) == 0 &&
        memory_equals(write_buffer, read_buffer, BLOCK_SECTOR_SIZE)) {
        log_info("block: ramdisk self-test passed");
    } else {
        log_error("block: ramdisk self-test failed");
    }
}
