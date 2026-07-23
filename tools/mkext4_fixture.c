#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SECTOR_SIZE 512u
#define SECTOR_COUNT 128u
#define BLOCK_SIZE 1024u
#define EXT4_BLOCKS 32u
#define INODE_SIZE 256u
#define INODE_TABLE_BLOCK 5u
#define ROOT_DATA_BLOCK 9u
#define HELLO_DATA_BLOCK 10u

static uint8_t image[SECTOR_SIZE * SECTOR_COUNT];
static const char hello[] = "Hello from AukOS ext4!\n";

static void put16(size_t offset, uint16_t value)
{
    image[offset] = (uint8_t)value;
    image[offset + 1u] = (uint8_t)(value >> 8);
}

static void put32(size_t offset, uint32_t value)
{
    image[offset] = (uint8_t)value;
    image[offset + 1u] = (uint8_t)(value >> 8);
    image[offset + 2u] = (uint8_t)(value >> 16);
    image[offset + 3u] = (uint8_t)(value >> 24);
}

static void seed_inode(uint32_t inode_number, uint16_t mode, uint32_t size,
                       uint32_t data_block)
{
    size_t inode = INODE_TABLE_BLOCK * BLOCK_SIZE +
                   (size_t)(inode_number - 1u) * INODE_SIZE;
    size_t extent = inode + 40u;

    put16(inode, mode);
    put32(inode + 4u, size);
    put32(inode + 28u, 2u);
    put32(inode + 32u, 0x00080000u);
    put16(extent, 0xf30au);
    put16(extent + 2u, 1u);
    put16(extent + 4u, 4u);
    put32(extent + 12u, 0u);
    put16(extent + 16u, 1u);
    put32(extent + 20u, data_block);
}

static void seed_dirent(size_t offset, uint32_t inode, uint16_t record_length,
                        uint8_t name_length, uint8_t type, const char *name)
{
    put32(offset, inode);
    put16(offset + 4u, record_length);
    image[offset + 6u] = name_length;
    image[offset + 7u] = type;
    memcpy(image + offset + 8u, name, name_length);
}

int main(int argc, char **argv)
{
    FILE *output;
    size_t superblock = 1024u;
    size_t directory = ROOT_DATA_BLOCK * BLOCK_SIZE;

    if (argc != 2) {
        fprintf(stderr, "usage: %s OUTPUT\n", argv[0]);
        return 2;
    }

    put32(superblock + 0x00u, 16u);
    put32(superblock + 0x04u, EXT4_BLOCKS);
    put32(superblock + 0x14u, 1u);
    put32(superblock + 0x18u, 0u);
    put32(superblock + 0x20u, 32u);
    put32(superblock + 0x28u, 16u);
    put16(superblock + 0x38u, 0xef53u);
    put32(superblock + 0x4cu, 1u);
    put32(superblock + 0x54u, 11u);
    put16(superblock + 0x58u, INODE_SIZE);
    put32(superblock + 0x60u, 0x42u);
    put16(superblock + 0xfeu, 32u);
    put32(2u * BLOCK_SIZE + 8u, INODE_TABLE_BLOCK);

    seed_inode(2u, 0040755u, BLOCK_SIZE, ROOT_DATA_BLOCK);
    seed_inode(12u, 0100644u, (uint32_t)(sizeof(hello) - 1u), HELLO_DATA_BLOCK);
    seed_dirent(directory, 2u, 12u, 1u, 2u, ".");
    seed_dirent(directory + 12u, 2u, 12u, 2u, 2u, "..");
    seed_dirent(directory + 24u, 12u, BLOCK_SIZE - 24u, 9u, 1u, "hello.txt");
    memcpy(image + HELLO_DATA_BLOCK * BLOCK_SIZE, hello, sizeof(hello) - 1u);

    output = fopen(argv[1], "wb");
    if (!output || fwrite(image, 1u, sizeof(image), output) != sizeof(image) ||
        fclose(output) != 0) {
        fprintf(stderr, "failed to write %s\n", argv[1]);
        return 1;
    }
    return 0;
}
