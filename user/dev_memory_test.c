#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define PAGE_SIZE 4096u
#define LARGE_ALLOCATION_COUNT 20u
#define LARGE_ALLOCATION_SIZE (64u * 1024u)
#define STRESS_ITERATIONS 16u
#define AVAILABLE_MMAP_SLOTS 28u

static void write_message(const char *message)
{
    write(1, message, strlen(message));
}

static int fail(const char *message)
{
    write_message("[dev_memory_test] FAIL: ");
    write_message(message);
    write_message("\n");
    return 1;
}

static int test_allocator(void)
{
    unsigned char *allocations[LARGE_ALLOCATION_COUNT];
    unsigned char *pointer;
    unsigned char *resized;

    for (size_t index = 0; index < LARGE_ALLOCATION_COUNT; index++) {
        allocations[index] = malloc(LARGE_ALLOCATION_SIZE);
        if (!allocations[index] || ((uintptr_t)allocations[index] & 15u) != 0) {
            return fail("large allocation/alignment");
        }
        for (size_t offset = 0; offset < LARGE_ALLOCATION_SIZE; offset += PAGE_SIZE) {
            allocations[index][offset] = (unsigned char)(index + offset / PAGE_SIZE);
        }
        allocations[index][LARGE_ALLOCATION_SIZE - 1u] = (unsigned char)index;
    }
    for (size_t index = 0; index < LARGE_ALLOCATION_COUNT; index++) {
        for (size_t offset = 0; offset < LARGE_ALLOCATION_SIZE; offset += PAGE_SIZE) {
            if (allocations[index][offset] !=
                (unsigned char)(index + offset / PAGE_SIZE)) {
                return fail("large allocation contents");
            }
        }
        if (allocations[index][LARGE_ALLOCATION_SIZE - 1u] != (unsigned char)index) {
            return fail("large allocation tail");
        }
        free(allocations[index]);
    }

    pointer = calloc(1024, 8);
    if (!pointer) return fail("calloc allocation");
    for (size_t index = 0; index < 8192; index++) {
        if (pointer[index] != 0) return fail("calloc zero fill");
    }
    free(pointer);
    if (calloc((size_t)-1, 2) != 0) return fail("calloc overflow");

    pointer = malloc(4096);
    if (!pointer) return fail("reuse setup");
    free(pointer);
    if (malloc(4096) != pointer) return fail("free reuse");
    free(pointer);

    pointer = malloc(256);
    if (!pointer) return fail("realloc setup");
    for (size_t index = 0; index < 256; index++) pointer[index] = (unsigned char)index;
    resized = realloc(pointer, 8192);
    if (!resized) return fail("realloc grow");
    for (size_t index = 0; index < 256; index++) {
        if (resized[index] != (unsigned char)index) return fail("realloc grow prefix");
    }
    resized = realloc(resized, 128);
    if (!resized) return fail("realloc shrink");
    for (size_t index = 0; index < 128; index++) {
        if (resized[index] != (unsigned char)index) return fail("realloc shrink prefix");
    }
    free(resized);
    return 0;
}

static int test_mmap(void)
{
    size_t length = 3u * PAGE_SIZE;
    void *slot_mappings[AVAILABLE_MMAP_SLOTS];
    unsigned char *mapping = mmap(0, length, PROT_READ | PROT_WRITE,
                                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    unsigned char *reused;

    if (mapping == MAP_FAILED || ((uintptr_t)mapping & (PAGE_SIZE - 1u)) != 0) {
        return fail("anonymous mmap");
    }
    for (size_t index = 0; index < length; index++) {
        if (mapping[index] != 0) return fail("anonymous mmap zero fill");
    }
    for (size_t index = 0; index < length; index += PAGE_SIZE) {
        mapping[index] = (unsigned char)(0x40u + index / PAGE_SIZE);
        mapping[index + PAGE_SIZE - 1u] = (unsigned char)(0x70u + index / PAGE_SIZE);
    }
    if (mprotect(mapping, length, PROT_READ) != 0 ||
        mprotect(mapping, length, PROT_READ | PROT_WRITE) != 0) {
        return fail("mprotect full region");
    }
    if (mprotect(mapping, length, PROT_WRITE | PROT_EXEC) == 0 ||
        mprotect(mapping, PAGE_SIZE, PROT_READ) == 0) {
        return fail("invalid mprotect accepted");
    }
    if (mmap(mapping, length, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) != MAP_FAILED) {
        return fail("overlapping mmap");
    }
    if (mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS, -1, 0) !=
            MAP_FAILED ||
        mmap(0, PAGE_SIZE, PROT_WRITE | PROT_EXEC,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) != MAP_FAILED ||
        mmap((void *)0x50000000u, PAGE_SIZE, PROT_READ,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) != MAP_FAILED ||
        mmap(mapping + 1, PAGE_SIZE, PROT_READ,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) != MAP_FAILED ||
        mmap(0, (size_t)-1, PROT_READ,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) != MAP_FAILED) {
        return fail("invalid mmap accepted");
    }
    if (munmap(mapping, PAGE_SIZE) == 0) return fail("partial munmap accepted");
    if (munmap(mapping, length) != 0) return fail("munmap full region");
    if (munmap(mapping, length) == 0) return fail("double munmap accepted");
    reused = mmap(0, length, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (reused != mapping) return fail("mmap region slot/address reuse");
    if (munmap(reused, length) != 0) return fail("mmap reuse cleanup");

    for (size_t index = 0; index < AVAILABLE_MMAP_SLOTS; index++) {
        slot_mappings[index] = mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
                                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (slot_mappings[index] == MAP_FAILED) return fail("mmap slot capacity");
    }
    if (mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) != MAP_FAILED) {
        return fail("mmap slot exhaustion");
    }
    for (size_t index = 0; index < AVAILABLE_MMAP_SLOTS; index++) {
        if (munmap(slot_mappings[index], PAGE_SIZE) != 0) {
            return fail("mmap slot cleanup");
        }
    }
    return 0;
}

static int fork_isolation_iteration(size_t iteration)
{
    unsigned char *heap = malloc(PAGE_SIZE);
    unsigned char *mapping = mmap(0, 2u * PAGE_SIZE, PROT_READ | PROT_WRITE,
                                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    pid_t child;
    int status = 1;

    if (!heap || mapping == MAP_FAILED) return fail("stress allocation");
    heap[0] = (unsigned char)(0x20u + iteration);
    heap[PAGE_SIZE - 1u] = (unsigned char)(0x30u + iteration);
    mapping[0] = (unsigned char)(0x40u + iteration);
    mapping[2u * PAGE_SIZE - 1u] = (unsigned char)(0x50u + iteration);

    child = fork();
    if (child < 0) return fail("stress fork");
    if (child == 0) {
        heap[0] ^= 0xffu;
        heap[PAGE_SIZE - 1u] ^= 0xffu;
        mapping[0] ^= 0xffu;
        mapping[2u * PAGE_SIZE - 1u] ^= 0xffu;
        _exit(0);
    }
    if (waitpid(child, &status, 0) != child || status != 0) {
        return fail("stress wait/reap");
    }
    if (heap[0] != (unsigned char)(0x20u + iteration) ||
        heap[PAGE_SIZE - 1u] != (unsigned char)(0x30u + iteration) ||
        mapping[0] != (unsigned char)(0x40u + iteration) ||
        mapping[2u * PAGE_SIZE - 1u] != (unsigned char)(0x50u + iteration)) {
        return fail("fork address-space isolation");
    }
    free(heap);
    if (munmap(mapping, 2u * PAGE_SIZE) != 0) return fail("stress munmap");
    return 0;
}

int main(int argc, char **argv, char **envp)
{
    (void)argc;
    (void)argv;
    (void)envp;

    if (test_allocator() != 0 || test_mmap() != 0) return 1;
    for (size_t iteration = 0; iteration < STRESS_ITERATIONS; iteration++) {
        if (fork_isolation_iteration(iteration) != 0) return 1;
    }
    write_message("[dev_memory_test] PASS\n");
    return 0;
}
