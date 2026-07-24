#include "allocator.h"

#include <stdint.h>

#define ALLOCATOR_ALIGNMENT 16u
#define ALLOCATOR_GROW_CHUNK (64u * 1024u)

struct aukos_allocator_block {
    size_t size;
    struct aukos_allocator_block *previous;
    struct aukos_allocator_block *next;
    size_t free;
};

_Static_assert(sizeof(struct aukos_allocator_block) % ALLOCATOR_ALIGNMENT == 0,
               "allocator header must preserve payload alignment");

static int align_size(size_t size, size_t *result)
{
    if (!result || size == 0 || size > (size_t)-1 - (ALLOCATOR_ALIGNMENT - 1u)) {
        return -1;
    }
    *result = (size + ALLOCATOR_ALIGNMENT - 1u) & ~(ALLOCATOR_ALIGNMENT - 1u);
    return 0;
}

static int blocks_are_adjacent(const struct aukos_allocator_block *left,
                               const struct aukos_allocator_block *right)
{
    return (const uint8_t *)(left + 1) + left->size == (const uint8_t *)right;
}

static void merge_with_next(struct aukos_allocator_block *block)
{
    struct aukos_allocator_block *next = block->next;

    if (!next || !next->free || !blocks_are_adjacent(block, next)) {
        return;
    }
    block->size += sizeof(*block) + next->size;
    block->next = next->next;
    if (block->next) {
        block->next->previous = block;
    }
}

static void split_block(struct aukos_allocator_block *block, size_t size)
{
    struct aukos_allocator_block *remainder;

    if (block->size < size + sizeof(*block) + ALLOCATOR_ALIGNMENT) {
        return;
    }
    remainder = (struct aukos_allocator_block *)((uint8_t *)(block + 1) + size);
    remainder->size = block->size - size - sizeof(*block);
    remainder->previous = block;
    remainder->next = block->next;
    remainder->free = 1;
    if (remainder->next) {
        remainder->next->previous = remainder;
    }
    block->next = remainder;
    block->size = size;
    merge_with_next(remainder);
}

static struct aukos_allocator_block *insert_grown_block(
    struct aukos_allocator *allocator, void *memory, size_t size)
{
    struct aukos_allocator_block *block = memory;
    struct aukos_allocator_block *current = allocator->head;
    struct aukos_allocator_block *previous = 0;

    if (!memory || memory == (void *)-1 ||
        ((uintptr_t)memory & (ALLOCATOR_ALIGNMENT - 1u)) != 0 ||
        size <= sizeof(*block)) {
        return 0;
    }
    while (current && (uintptr_t)current < (uintptr_t)block) {
        previous = current;
        current = current->next;
    }
    block->size = size - sizeof(*block);
    block->previous = previous;
    block->next = current;
    block->free = 1;
    if (previous) {
        previous->next = block;
    } else {
        allocator->head = block;
    }
    if (current) {
        current->previous = block;
    }
    if (previous && previous->free && blocks_are_adjacent(previous, block)) {
        merge_with_next(previous);
        block = previous;
    }
    merge_with_next(block);
    return block;
}

void aukos_allocator_init(struct aukos_allocator *allocator,
                          aukos_allocator_grow_fn grow, void *context)
{
    allocator->head = 0;
    allocator->grow = grow;
    allocator->context = context;
}

void *aukos_allocator_malloc(struct aukos_allocator *allocator, size_t size)
{
    struct aukos_allocator_block *block;
    size_t aligned_size;

    if (!allocator || !allocator->grow || align_size(size, &aligned_size) != 0) {
        return 0;
    }
    for (block = allocator->head; block; block = block->next) {
        if (block->free && block->size >= aligned_size) {
            split_block(block, aligned_size);
            block->free = 0;
            return block + 1;
        }
    }
    if (aligned_size > (size_t)-1 - sizeof(*block)) {
        return 0;
    }
    size_t growth = aligned_size + sizeof(*block);
    if (growth < ALLOCATOR_GROW_CHUNK) {
        growth = ALLOCATOR_GROW_CHUNK;
    }
    block = insert_grown_block(allocator,
                               allocator->grow(allocator->context, growth),
                               growth);
    if (!block || block->size < aligned_size) {
        return 0;
    }
    split_block(block, aligned_size);
    block->free = 0;
    return block + 1;
}

void aukos_allocator_free(struct aukos_allocator *allocator, void *pointer)
{
    struct aukos_allocator_block *block;

    if (!allocator || !pointer) {
        return;
    }
    block = (struct aukos_allocator_block *)pointer - 1;
    block->free = 1;
    merge_with_next(block);
    if (block->previous && block->previous->free &&
        blocks_are_adjacent(block->previous, block)) {
        block = block->previous;
        merge_with_next(block);
    }
}

void *aukos_allocator_calloc(struct aukos_allocator *allocator, size_t count,
                             size_t size)
{
    size_t total;
    uint8_t *pointer;

    if (size != 0 && count > (size_t)-1 / size) {
        return 0;
    }
    total = count * size;
    pointer = aukos_allocator_malloc(allocator, total);
    if (!pointer) {
        return 0;
    }
    for (size_t index = 0; index < total; index++) {
        pointer[index] = 0;
    }
    return pointer;
}

void *aukos_allocator_realloc(struct aukos_allocator *allocator, void *pointer,
                              size_t size)
{
    struct aukos_allocator_block *block;
    size_t aligned_size;
    void *replacement;
    size_t copy_size;

    if (!pointer) {
        return aukos_allocator_malloc(allocator, size);
    }
    if (size == 0) {
        aukos_allocator_free(allocator, pointer);
        return 0;
    }
    if (align_size(size, &aligned_size) != 0) {
        return 0;
    }
    block = (struct aukos_allocator_block *)pointer - 1;
    if (block->size >= aligned_size) {
        split_block(block, aligned_size);
        return pointer;
    }
    if (block->next && block->next->free && blocks_are_adjacent(block, block->next) &&
        block->size + sizeof(*block) + block->next->size >= aligned_size) {
        merge_with_next(block);
        split_block(block, aligned_size);
        block->free = 0;
        return pointer;
    }
    replacement = aukos_allocator_malloc(allocator, size);
    if (!replacement) {
        return 0;
    }
    copy_size = block->size < size ? block->size : size;
    for (size_t index = 0; index < copy_size; index++) {
        ((uint8_t *)replacement)[index] = ((const uint8_t *)pointer)[index];
    }
    aukos_allocator_free(allocator, pointer);
    return replacement;
}
