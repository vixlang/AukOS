#include "include/aukos/multiboot2.h"

#include "include/aukos/log.h"
#include "include/aukos/serial.h"

#include <stdint.h>

#define MULTIBOOT_TAG_TYPE_END 0
#define MULTIBOOT_TAG_TYPE_MMAP 6
#define MULTIBOOT_MEMORY_AVAILABLE 1

struct multiboot_tag {
    uint32_t type;
    uint32_t size;
};

struct multiboot_mmap_entry {
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;
    uint32_t reserved;
} __attribute__((packed));

struct multiboot_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    struct multiboot_mmap_entry entries[];
};

static uintptr_t align_up(uintptr_t value, uintptr_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

static void write_hex64(uint64_t value)
{
    static const char digits[] = "0123456789abcdef";

    serial_write("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        serial_write((char[]){ digits[(value >> shift) & 0xf], '\0' });
    }
}

static void write_u64_dec(uint64_t value)
{
    char buffer[21];
    int index = sizeof(buffer) - 1;

    buffer[index] = '\0';
    if (value == 0) {
        serial_write("0");
        return;
    }

    while (value > 0 && index > 0) {
        index--;
        buffer[index] = (char)('0' + (value % 10));
        value /= 10;
    }

    serial_write(&buffer[index]);
}

void multiboot2_log_memory_map(const struct memory_map *map)
{
    uint64_t available_bytes = 0;
    uint64_t available_regions = 0;

    if (map->count == 0) {
        log_error("multiboot2: memory map is empty");
        return;
    }

    log_info("multiboot2: memory map found");

    for (uint32_t index = 0; index < map->count; index++) {
        const struct memory_map_entry *entry = &map->entries[index];

        serial_write("[DEBUG] mmap: base=");
        write_hex64(entry->base);
        serial_write(" length=");
        write_hex64(entry->length);
        serial_write(" type=");
        write_u64_dec(entry->type);
        serial_write("\r\n");

        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
            available_bytes += entry->length;
            available_regions++;
        }
    }

    serial_write("[INFO] memory: available_regions=");
    write_u64_dec(available_regions);
    serial_write(" available_kib=");
    write_u64_dec(available_bytes / 1024);
    serial_write("\r\n");
}

static int copy_memory_map(const struct multiboot_tag_mmap *mmap, struct memory_map *out_map)
{
    uintptr_t entry_address = (uintptr_t)mmap->entries;
    uintptr_t entries_end = (uintptr_t)mmap + mmap->size;

    out_map->count = 0;
    while (entry_address < entries_end && out_map->count < 32) {
        const struct multiboot_mmap_entry *entry = (const struct multiboot_mmap_entry *)entry_address;
        struct memory_map_entry *out_entry = &out_map->entries[out_map->count];

        out_entry->base = entry->base_addr;
        out_entry->length = entry->length;
        out_entry->type = entry->type;
        out_map->count++;

        entry_address += mmap->entry_size;
    }

    return out_map->count > 0 ? 0 : -1;
}

int multiboot2_read_memory_map(uint32_t magic, uintptr_t info_address, struct memory_map *out_map)
{
    out_map->count = 0;

    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        log_error("multiboot2: invalid bootloader magic");
        return -1;
    }

    if (info_address == 0) {
        log_error("multiboot2: missing boot information");
        return -1;
    }

    uint32_t total_size = *(const uint32_t *)info_address;
    uintptr_t tag_address = info_address + 8;
    uintptr_t tags_end = info_address + total_size;

    while (tag_address < tags_end) {
        const struct multiboot_tag *tag = (const struct multiboot_tag *)tag_address;

        if (tag->type == MULTIBOOT_TAG_TYPE_END) {
            break;
        }

        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
            return copy_memory_map((const struct multiboot_tag_mmap *)tag, out_map);
        }

        tag_address = align_up(tag_address + tag->size, 8);
    }

    log_error("multiboot2: memory map tag not found");
    return -1;
}
