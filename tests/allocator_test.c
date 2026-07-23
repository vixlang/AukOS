#include "allocator.h"

#include <assert.h>
#include <stdint.h>

#define ARENA_SIZE (256u * 1024u)

struct test_arena {
    unsigned char bytes[ARENA_SIZE] __attribute__((aligned(16)));
    size_t used;
};

static void *grow_arena(void *context, size_t size)
{
    struct test_arena *arena = context;
    void *result;

    if (size > ARENA_SIZE - arena->used) {
        return (void *)-1;
    }
    result = &arena->bytes[arena->used];
    arena->used += size;
    return result;
}

static void initialize(struct aukos_allocator *allocator, struct test_arena *arena)
{
    arena->used = 0;
    aukos_allocator_init(allocator, grow_arena, arena);
}

int main(void)
{
    static struct test_arena arena;
    struct aukos_allocator allocator;
    unsigned char *first;
    unsigned char *second;
    unsigned char *third;
    unsigned char *resized;

    initialize(&allocator, &arena);
    assert(aukos_allocator_malloc(&allocator, 0) == 0);
    first = aukos_allocator_malloc(&allocator, 1);
    second = aukos_allocator_malloc(&allocator, 17);
    assert(first && second);
    assert(((uintptr_t)first & 15u) == 0 && ((uintptr_t)second & 15u) == 0);
    aukos_allocator_free(&allocator, first);
    assert(aukos_allocator_malloc(&allocator, 1) == first);

    initialize(&allocator, &arena);
    first = aukos_allocator_malloc(&allocator, 256);
    second = aukos_allocator_malloc(&allocator, 256);
    third = aukos_allocator_malloc(&allocator, 256);
    assert(first && second && third);
    aukos_allocator_free(&allocator, second);
    aukos_allocator_free(&allocator, first);
    assert(aukos_allocator_malloc(&allocator, 500) == first);

    initialize(&allocator, &arena);
    first = aukos_allocator_calloc(&allocator, 128, 4);
    assert(first);
    for (size_t index = 0; index < 512; index++) assert(first[index] == 0);
    assert(aukos_allocator_calloc(&allocator, SIZE_MAX, 2) == 0);

    for (size_t index = 0; index < 512; index++) first[index] = (unsigned char)index;
    resized = aukos_allocator_realloc(&allocator, first, 4096);
    assert(resized);
    for (size_t index = 0; index < 512; index++) assert(resized[index] == (unsigned char)index);
    resized = aukos_allocator_realloc(&allocator, resized, 128);
    assert(resized);
    for (size_t index = 0; index < 128; index++) assert(resized[index] == (unsigned char)index);

    first = aukos_allocator_malloc(&allocator, 32);
    assert(first);
    first[0] = 0x5a;
    assert(aukos_allocator_realloc(&allocator, first, ARENA_SIZE) == 0);
    assert(first[0] == 0x5a);
    aukos_allocator_free(&allocator, first);
    aukos_allocator_free(&allocator, 0);
    return 0;
}
