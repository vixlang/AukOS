#ifndef AUKOS_BLOCK_H
#define AUKOS_BLOCK_H

#include <stddef.h>
#include <stdint.h>

#define BLOCK_SECTOR_SIZE 512u

struct block_device {
    const char *name;
    uint64_t sector_count;
    uint32_t sector_size;
    void *private_data;
    int (*read)(struct block_device *device, uint64_t sector, void *buffer, size_t sector_count);
    int (*write)(struct block_device *device, uint64_t sector, const void *buffer, size_t sector_count);
    int (*flush)(struct block_device *device);
};

void block_init(void);
int block_register(struct block_device *device);
struct block_device *block_get(uint32_t index);
struct block_device *block_find(const char *name);
uint32_t block_count(void);
int block_read_bytes(struct block_device *device, uint64_t offset, void *buffer, size_t size);
int block_write_bytes(struct block_device *device, uint64_t offset,
                      const void *buffer, size_t size);
int block_flush(struct block_device *device);
void block_run_selftest(void);

#endif
