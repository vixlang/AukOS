#include "include/aukos/memory.h"

#include "include/aukos/log.h"
#include "include/aukos/serial.h"

#define MULTIBOOT_MEMORY_AVAILABLE 1
#define MAX_FREE_RANGES 32
#define HEAP_SIZE (64ull * 1024ull)

struct free_range {
    uintptr_t current;
    uintptr_t end;
};

extern char __kernel_end[];

static struct free_range free_ranges[MAX_FREE_RANGES];
static uint32_t free_range_count;
static uint64_t free_page_count;
static void *recycled_pages;
static uint64_t allocation_failure_countdown = UINT64_MAX;
static uint8_t kernel_heap[HEAP_SIZE] __attribute__((aligned(16)));
static uintptr_t heap_current;
static uintptr_t heap_end;

static uintptr_t align_up(uintptr_t value, uintptr_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

static uintptr_t align_down(uintptr_t value, uintptr_t alignment)
{
    return value & ~(alignment - 1);
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

static void add_free_range(uintptr_t start, uintptr_t end)
{
    if (start >= end || free_range_count == MAX_FREE_RANGES) {
        return;
    }

    free_ranges[free_range_count].current = start;
    free_ranges[free_range_count].end = end;
    free_range_count++;
    free_page_count += (end - start) / PAGE_SIZE;
}

void memory_init(const struct memory_map *map)
{
    uintptr_t kernel_end = align_up((uintptr_t)__kernel_end, PAGE_SIZE);

    free_range_count = 0;
    free_page_count = 0;
    recycled_pages = 0;
    heap_current = (uintptr_t)kernel_heap;
    heap_end = heap_current + HEAP_SIZE;

    for (uint32_t index = 0; index < map->count; index++) {
        const struct memory_map_entry *entry = &map->entries[index];

        if (entry->type != MULTIBOOT_MEMORY_AVAILABLE) {
            continue;
        }

        uintptr_t start = align_up((uintptr_t)entry->base, PAGE_SIZE);
        uintptr_t end = align_down((uintptr_t)(entry->base + entry->length), PAGE_SIZE);

        if (end <= kernel_end) {
            continue;
        }

        if (start < kernel_end) {
            start = kernel_end;
        }

        add_free_range(start, end);
    }

    serial_write("[INFO] pmm: free_pages=");
    write_u64_dec(free_page_count);
    serial_write(" free_ranges=");
    write_u64_dec(free_range_count);
    serial_write("\r\n");

    log_info("heap: bump allocator initialized");
}

void *pmm_alloc_page(void)
{
    if (allocation_failure_countdown == 0u) {
        return 0;
    }
    if (allocation_failure_countdown != UINT64_MAX) {
        allocation_failure_countdown--;
    }
    if (recycled_pages != 0) {
        void *page = recycled_pages;
        recycled_pages = *(void **)page;
        free_page_count--;
        return page;
    }

    for (uint32_t index = 0; index < free_range_count; index++) {
        struct free_range *range = &free_ranges[index];

        if (range->current < range->end) {
            void *page = (void *)range->current;
            range->current += PAGE_SIZE;
            free_page_count--;
            return page;
        }
    }

    return 0;
}

void pmm_free_page(void *page)
{
    uintptr_t address = (uintptr_t)page;

    if ((address & (PAGE_SIZE - 1)) != 0) {
        return;
    }

    *(void **)page = recycled_pages;
    recycled_pages = page;
    free_page_count++;
}

uint64_t pmm_available_page_count(void)
{
    return free_page_count;
}

void pmm_test_fail_after(uint64_t successful_allocations)
{
    allocation_failure_countdown = successful_allocations;
}

void *kmalloc(size_t size)
{
    uintptr_t allocation;

    if (size == 0) {
        return 0;
    }

    allocation = align_up(heap_current, 16);
    if (allocation + size > heap_end) {
        return 0;
    }

    heap_current = allocation + size;
    return (void *)allocation;
}
