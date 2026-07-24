#ifndef AUKOS_LIBC_ALLOCATOR_H
#define AUKOS_LIBC_ALLOCATOR_H

#include <stddef.h>

typedef void *(*aukos_allocator_grow_fn)(void *context, size_t size);

struct aukos_allocator_block;

struct aukos_allocator {
    struct aukos_allocator_block *head;
    aukos_allocator_grow_fn grow;
    void *context;
};

void aukos_allocator_init(struct aukos_allocator *allocator,
                          aukos_allocator_grow_fn grow, void *context);
void *aukos_allocator_malloc(struct aukos_allocator *allocator, size_t size);
void *aukos_allocator_calloc(struct aukos_allocator *allocator, size_t count,
                             size_t size);
void *aukos_allocator_realloc(struct aukos_allocator *allocator, void *pointer,
                              size_t size);
void aukos_allocator_free(struct aukos_allocator *allocator, void *pointer);

#endif
