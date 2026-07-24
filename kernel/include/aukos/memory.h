#ifndef AUKOS_MEMORY_H
#define AUKOS_MEMORY_H

#include "memory_map.h"

#include <stddef.h>
#include <stdint.h>

#define PAGE_SIZE 4096ull

void memory_init(const struct memory_map *map);
void *pmm_alloc_page(void);
void pmm_free_page(void *page);
uint64_t pmm_available_page_count(void);
void pmm_test_fail_after(uint64_t successful_allocations);
void *kmalloc(size_t size);

#endif
