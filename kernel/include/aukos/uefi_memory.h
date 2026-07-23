#ifndef AUKOS_UEFI_MEMORY_H
#define AUKOS_UEFI_MEMORY_H

#include "memory_map.h"

#include <stdint.h>

struct uefi_memory_descriptor {
    uint32_t type;
    uint32_t pad;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
};

int uefi_read_memory_map(const struct uefi_memory_descriptor *uefi_map, uint64_t map_size, uint64_t desc_size, struct memory_map *out_map);
void uefi_log_memory_map(const struct memory_map *map);

#endif