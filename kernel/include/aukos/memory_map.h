#ifndef AUKOS_MEMORY_MAP_H
#define AUKOS_MEMORY_MAP_H

#include <stdint.h>

#define MEMORY_MAP_MAX_ENTRIES 32

struct memory_map_entry {
    uint64_t base;
    uint64_t length;
    uint32_t type;
};

struct memory_map {
    struct memory_map_entry entries[MEMORY_MAP_MAX_ENTRIES];
    uint32_t count;
};

#endif