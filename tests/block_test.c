#include <aukos/block.h>
#include <aukos/log.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_SECTORS 4u

struct memory_disk {
    uint8_t bytes[TEST_SECTORS * BLOCK_SECTOR_SIZE];
    int fail_read;
    int fail_write;
    int fail_flush;
    unsigned reads;
    unsigned writes;
    unsigned flushes;
};

void log_write(enum log_level level, const char *message)
{
    (void)level;
    (void)message;
}

static int memory_read(struct block_device *device, uint64_t sector,
                       void *buffer, size_t count)
{
    struct memory_disk *disk = device->private_data;
    if (disk->fail_read || sector > TEST_SECTORS ||
        count > TEST_SECTORS - sector) return -1;
    disk->reads++;
    memcpy(buffer, disk->bytes + sector * BLOCK_SECTOR_SIZE,
           count * BLOCK_SECTOR_SIZE);
    return 0;
}

static int memory_write(struct block_device *device, uint64_t sector,
                        const void *buffer, size_t count)
{
    struct memory_disk *disk = device->private_data;
    if (disk->fail_write || sector > TEST_SECTORS ||
        count > TEST_SECTORS - sector) return -1;
    disk->writes++;
    memcpy(disk->bytes + sector * BLOCK_SECTOR_SIZE, buffer,
           count * BLOCK_SECTOR_SIZE);
    return 0;
}

static int memory_flush(struct block_device *device)
{
    struct memory_disk *disk = device->private_data;
    disk->flushes++;
    return disk->fail_flush ? -1 : 0;
}

static int expect(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "block_test: %s\n", message);
        return -1;
    }
    return 0;
}

int main(void)
{
    struct memory_disk first = {0};
    struct memory_disk second = {0};
    struct block_device a = {
        .name = "mem0", .sector_count = TEST_SECTORS,
        .sector_size = BLOCK_SECTOR_SIZE, .private_data = &first,
        .read = memory_read, .write = memory_write, .flush = memory_flush,
    };
    struct block_device b = a;
    uint8_t input[BLOCK_SECTOR_SIZE * 2u];
    uint8_t output[sizeof(input)];

    b.name = "mem1";
    b.private_data = &second;
    memset(first.bytes, 0xa5, sizeof(first.bytes));
    for (size_t i = 0; i < sizeof(input); i++) input[i] = (uint8_t)i;

    block_init();
    if (expect(block_register(&a) == 0 && block_register(&b) == 0,
               "device registration") ||
        expect(block_count() == 2 && block_find("mem0") == &a &&
               block_find("mem1") == &b, "deterministic naming") ||
        expect(block_write_bytes(&a, 0, 0, 0) == 0, "zero write") ||
        expect(block_write_bytes(&a, 0, input, sizeof(input)) == 0 &&
               first.writes == 1 && first.reads == 0, "aligned direct write") ||
        expect(!memcmp(first.bytes, input, sizeof(input)), "aligned contents") ||
        expect(block_write_bytes(&a, 510, input, 5) == 0,
               "cross-sector RMW") ||
        expect(first.bytes[509] == input[509] && first.bytes[510] == input[0] &&
               first.bytes[514] == input[4] && first.bytes[515] == input[515],
               "RMW preserves neighbors") ||
        expect(block_write_bytes(&a, sizeof(first.bytes) - 1u, input, 1u) == 0,
               "last byte") ||
        expect(block_write_bytes(&a, sizeof(first.bytes), input, 1u) != 0 &&
               block_write_bytes(&a, UINT64_MAX, input, 2u) != 0,
               "capacity and overflow") ||
        expect(block_read_bytes(&a, 510, output, 5u) == 0 &&
               !memcmp(output, input, 5u), "read after RMW") ||
        expect(second.reads == 0 && second.writes == 0 && second.flushes == 0,
               "device state isolation") ||
        expect(block_flush(&a) == 0 && first.flushes == 1, "flush") ||
        expect((first.fail_flush = 1, block_flush(&a) != 0),
               "flush failure propagation") ||
        expect((first.fail_read = 1,
                block_write_bytes(&a, 1u, input, 1u) != 0),
               "RMW read failure") ||
        expect((first.fail_read = 0, first.fail_write = 1,
                block_write_bytes(&a, 0u, input, BLOCK_SECTOR_SIZE) != 0),
               "write failure")) return 1;

    puts("block_test: 16 checks passed");
    return 0;
}
