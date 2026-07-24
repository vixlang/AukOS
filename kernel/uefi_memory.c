#include "include/aukos/uefi_memory.h"

#include "include/aukos/log.h"
#include "include/aukos/serial.h"

#include <stdint.h>

#define UEFI_MEMORY_AVAILABLE 7
#define MAX_FREE_RANGES 32

static void write_hex64(uint64_t value) {
    static const char digits[] = "0123456789abcdef";

    serial_write("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        serial_write((char[]){ digits[(value >> shift) & 0xf], '\0' });
    }
}

static void write_u64_dec(uint64_t value) {
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

void uefi_log_memory_map(const struct memory_map *map) {
    uint64_t available_bytes = 0;
    uint64_t available_regions = 0;

    if (map->count == 0) {
        log_error("uefi: memory map is empty");
        return;
    }

    log_info("uefi: memory map found");

    for (uint32_t index = 0; index < map->count; index++) {
        const struct memory_map_entry *entry = &map->entries[index];

        serial_write("[DEBUG] mmap: base=");
        write_hex64(entry->base);
        serial_write(" length=");
        write_hex64(entry->length);
        serial_write(" type=");
        write_u64_dec(entry->type);
        serial_write("\r\n");

        if (entry->type == UEFI_MEMORY_AVAILABLE) {
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

int uefi_read_memory_map(const struct uefi_memory_descriptor *uefi_map, uint64_t map_size, uint64_t desc_size, struct memory_map *out_map) {
    uint64_t offset = 0;
    uint32_t count = 0;

    out_map->count = 0;

    while (offset < map_size && count < 32) {
        const struct uefi_memory_descriptor *desc = (const struct uefi_memory_descriptor *)((uint8_t *)uefi_map + offset);

        if (desc->type == UEFI_MEMORY_AVAILABLE) {
            struct memory_map_entry *entry = &out_map->entries[count];
            entry->base = desc->physical_start;
            entry->length = desc->number_of_pages * 4096;
            entry->type = 1;
            count++;
        }

        offset += desc_size;
    }

    out_map->count = count;
    return count > 0 ? 0 : -1;
}