#include <aukos/elf.h>
#include <aukos/log.h>
#include <aukos/memory.h>
#include <aukos/tmpfs.h>
#include <aukos/vfs.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_TEST_MAPPINGS 16u

struct test_mapping {
    uintptr_t virtual_address;
    void *physical;
    uint32_t flags;
};

static struct test_mapping mappings[MAX_TEST_MAPPINGS];
static size_t mapping_count;

void log_write(enum log_level level, const char *message)
{
    (void)level;
    (void)message;
}

void *kmalloc(size_t size)
{
    return calloc(1u, size);
}

void *pmm_alloc_page(void)
{
    return aligned_alloc(PAGE_SIZE, PAGE_SIZE);
}

void pmm_free_page(void *page)
{
    free(page);
}

uint64_t pmm_available_page_count(void)
{
    return UINT64_MAX;
}

int vmm_query_page(const struct address_space *space, uintptr_t virtual_address,
                   uintptr_t *physical)
{
    (void)space;
    for (size_t index = 0; index < mapping_count; index++) {
        if (mappings[index].virtual_address == virtual_address) {
            *physical = (uintptr_t)mappings[index].physical;
            return 0;
        }
    }
    return -1;
}

int vmm_query_page_flags(const struct address_space *space,
                         uintptr_t virtual_address, uint32_t *flags)
{
    (void)space;
    for (size_t index = 0; index < mapping_count; index++) {
        if (mappings[index].virtual_address == virtual_address) {
            *flags = mappings[index].flags;
            return 0;
        }
    }
    return -1;
}

int vmm_map_page(struct address_space *space, uintptr_t virtual_address,
                 uintptr_t physical, uint32_t flags)
{
    (void)space;
    for (size_t index = 0; index < mapping_count; index++) {
        if (mappings[index].virtual_address == virtual_address) {
            mappings[index].physical = (void *)physical;
            mappings[index].flags = flags;
            return 0;
        }
    }
    if (mapping_count == MAX_TEST_MAPPINGS) {
        return -1;
    }
    mappings[mapping_count].virtual_address = virtual_address;
    mappings[mapping_count].physical = (void *)physical;
    mappings[mapping_count].flags = flags;
    mapping_count++;
    return 0;
}

int vmm_add_region(struct address_space *space, uintptr_t start, uintptr_t size,
                   uint32_t flags, enum vm_region_type type)
{
    (void)start;
    (void)size;
    (void)flags;
    (void)type;
    space->regions[0].type = VM_REGION_ELF;
    return 0;
}

static void reset_mappings(void)
{
    for (size_t index = 0; index < mapping_count; index++) {
        free(mappings[index].physical);
    }
    mapping_count = 0u;
}

static int vfs_reader(void *context, uint64_t offset, void *buffer, size_t size)
{
    return vfs_read_at(context, buffer, size, offset) == size ? 0 : -1;
}

struct failing_context {
    const uint8_t *data;
    size_t size;
    uint64_t fail_at;
};

static int failing_reader(void *opaque, uint64_t offset, void *buffer,
                          size_t size)
{
    struct failing_context *context = opaque;
    uint8_t *output = buffer;

    if (offset >= context->fail_at || size > context->fail_at - offset ||
        offset > context->size || size > context->size - offset) {
        return -1;
    }
    for (size_t index = 0; index < size; index++) {
        output[index] = context->data[offset + index];
    }
    return 0;
}

int main(void)
{
    FILE *fixture = fopen("build/user/hello.elf", "rb");
    struct address_space space = {0};
    struct loaded_program program;
    struct vfs_file *file;
    struct elf_reader reader;
    struct failing_context failing;
    uint8_t *image;
    long image_size;

    assert(fixture);
    assert(fseek(fixture, 0, SEEK_END) == 0);
    image_size = ftell(fixture);
    assert(image_size > (long)PAGE_SIZE);
    assert(fseek(fixture, 0, SEEK_SET) == 0);
    image = malloc((size_t)image_size);
    assert(image);
    assert(fread(image, 1u, (size_t)image_size, fixture) == (size_t)image_size);
    fclose(fixture);

    assert(elf_load(image, (size_t)image_size, &space, &program) == 0);
    assert(program.entry == 0x40000000u && mapping_count != 0u);
    reset_mappings();

    vfs_init();
    assert(vfs_register_filesystem("tmpfs", tmpfs_mount_empty) == 0);
    assert(vfs_mount("/", "tmpfs") == 0);
    assert(vfs_mkdir("/work", 0755u) == 0);
    assert(vfs_open("/work/generated.elf", VFS_O_CREAT | VFS_O_RDWR,
                    0755u, &file) == 0);
    assert(vfs_write(file, image, (size_t)image_size) == (size_t)image_size);
    reader.context = file;
    reader.size = (uint64_t)image_size;
    reader.read_at = vfs_reader;
    assert(elf_load_reader(&reader, &space, &program) == 0);
    assert(program.entry == 0x40000000u);
    reset_mappings();

    failing.data = image;
    failing.size = (size_t)image_size;
    failing.fail_at = PAGE_SIZE;
    reader.context = &failing;
    reader.read_at = failing_reader;
    assert(elf_load_reader(&reader, &space, &program) == -1);
    reset_mappings();

    assert(vfs_unlink("/work/generated.elf") == 0);
    assert(vfs_close(file) == 0);
    free(image);
    puts("elf_reader_test: PASS");
    return 0;
}
