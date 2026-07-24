#include "include/aukos/elf.h"

#include "include/aukos/log.h"
#include "include/aukos/memory.h"
#include "include/aukos/serial.h"

#include <stddef.h>
#include <stdint.h>

#define EI_NIDENT 16
#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'
#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define ET_EXEC 2
#define EM_X86_64 62
#define PT_LOAD 1
#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

struct elf64_header {
    unsigned char ident[EI_NIDENT];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} __attribute__((packed));

struct elf64_program_header {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
} __attribute__((packed));

extern const unsigned char _binary_build_user_hello_elf_start[];
extern const unsigned char _binary_build_user_hello_elf_end[];

static uintptr_t align_down(uintptr_t value, uintptr_t alignment)
{
    return value & ~(alignment - 1);
}

static uintptr_t align_up(uintptr_t value, uintptr_t alignment)
{
    if (value > UINTPTR_MAX - (alignment - 1)) {
        return 0;
    }
    return (value + alignment - 1) & ~(alignment - 1);
}

static void memory_copy(uint8_t *destination, const uint8_t *source, size_t size)
{
    for (size_t index = 0; index < size; index++) {
        destination[index] = source[index];
    }
}

static void memory_zero(uint8_t *destination, size_t size)
{
    for (size_t index = 0; index < size; index++) {
        destination[index] = 0;
    }
}

static int reader_read_exact(const struct elf_reader *reader, uint64_t offset,
                             void *buffer, size_t size)
{
    if (!reader || !reader->read_at || (!buffer && size != 0u) ||
        offset > reader->size || size > reader->size - offset) {
        return -1;
    }
    return reader->read_at(reader->context, offset, buffer, size);
}

static int validate_header(const struct elf64_header *header,
                           const struct elf_reader *reader)
{
    if (reader->size < sizeof(*header)) {
        return -1;
    }

    if (header->ident[0] != ELFMAG0 || header->ident[1] != ELFMAG1 || header->ident[2] != ELFMAG2 || header->ident[3] != ELFMAG3) {
        return -1;
    }

    if (header->ident[4] != ELFCLASS64 || header->ident[5] != ELFDATA2LSB) {
        return -1;
    }

    if (header->type != ET_EXEC || header->machine != EM_X86_64) {
        return -1;
    }

    if (header->phentsize != sizeof(struct elf64_program_header) ||
        header->phnum == 0 ||
        header->phoff > reader->size ||
        (uint64_t)header->phnum >
            (reader->size - header->phoff) / header->phentsize) {
        return -1;
    }

    return 0;
}

static uint32_t vmm_flags_from_elf(uint32_t flags)
{
    uint32_t vmm_flags = VMM_USER_FLAG_USER | VMM_USER_FLAG_READABLE;

    if ((flags & PF_W) != 0) {
        vmm_flags |= VMM_USER_FLAG_WRITABLE;
    }

    if ((flags & PF_X) != 0) {
        vmm_flags |= VMM_USER_FLAG_EXECUTABLE;
    }

    (void)PF_R;
    return vmm_flags;
}

static int load_segment(const struct elf_reader *reader,
                        struct address_space *space,
                        const struct elf64_program_header *segment)
{
    uintptr_t start = align_down((uintptr_t)segment->vaddr, PAGE_SIZE);
    uintptr_t end = align_up((uintptr_t)(segment->vaddr + segment->memsz), PAGE_SIZE);
    uintptr_t file_start = (uintptr_t)segment->vaddr;
    uintptr_t file_end = file_start + segment->filesz;

    if (segment->memsz == 0 || segment->vaddr > UINTPTR_MAX - segment->memsz ||
        segment->filesz > segment->memsz || segment->offset > reader->size ||
        segment->filesz > reader->size - segment->offset || end == 0 ||
        start < VMM_USER_MIN || end > VMM_ELF_END) {
        return -1;
    }

    for (uintptr_t virtual_page = start; virtual_page < end; virtual_page += PAGE_SIZE) {
        uintptr_t existing_physical = 0;
        uint8_t *physical_page;
        uintptr_t page_end = virtual_page + PAGE_SIZE;
        uintptr_t copy_start = virtual_page > file_start ? virtual_page : file_start;
        uintptr_t copy_end = page_end < file_end ? page_end : file_end;
        int already_mapped = vmm_query_page(space, virtual_page, &existing_physical) == 0;

        if (already_mapped) {
            uint32_t existing_flags;

            physical_page = (uint8_t *)(existing_physical & ~(PAGE_SIZE - 1));
            if (vmm_query_page_flags(space, virtual_page, &existing_flags) != 0 ||
                vmm_map_page(space, virtual_page, (uintptr_t)physical_page,
                             existing_flags | vmm_flags_from_elf(segment->flags)) != 0) {
                return -1;
            }
        } else {
            physical_page = pmm_alloc_page();
            if (physical_page == 0) {
                return -1;
            }

            memory_zero(physical_page, PAGE_SIZE);
            if (vmm_map_page(space, virtual_page, (uintptr_t)physical_page, vmm_flags_from_elf(segment->flags)) != 0) {
                pmm_free_page(physical_page);
                return -1;
            }
        }

        if (copy_start < copy_end) {
            uint64_t source_offset = segment->offset + (copy_start - file_start);
            size_t destination_offset = (size_t)(copy_start - virtual_page);
            if (reader_read_exact(reader, source_offset,
                                  physical_page + destination_offset,
                                  (size_t)(copy_end - copy_start)) != 0) {
                return -1;
            }
        }
    }

    return 0;
}

int elf_load_reader(const struct elf_reader *reader, struct address_space *space,
                    struct loaded_program *program)
{
    struct elf64_header header;
    uintptr_t low = UINTPTR_MAX;
    uintptr_t high = 0;
    uint32_t region_flags = VMM_USER_FLAG_USER | VMM_USER_FLAG_READABLE;

    if (!space || !program || reader_read_exact(reader, 0u, &header,
                                                sizeof(header)) != 0 ||
        validate_header(&header, reader) != 0) {
        return -1;
    }

    for (uint16_t index = 0; index < header.phnum; index++) {
        struct elf64_program_header segment;
        uint64_t offset = header.phoff + (uint64_t)index * header.phentsize;

        if (reader_read_exact(reader, offset, &segment, sizeof(segment)) != 0) {
            return -1;
        }

        if (segment.type != PT_LOAD) {
            continue;
        }

        if (load_segment(reader, space, &segment) != 0) {
            return -1;
        }
        region_flags |= vmm_flags_from_elf(segment.flags);

        if (segment.vaddr < low) {
            low = (uintptr_t)segment.vaddr;
        }

        if (segment.vaddr + segment.memsz > high) {
            high = (uintptr_t)(segment.vaddr + segment.memsz);
        }
    }

    if (low == UINTPTR_MAX || high <= low || header.entry < low || header.entry >= high ||
        vmm_add_region(space, align_down(low, PAGE_SIZE),
                       align_up(high, PAGE_SIZE) - align_down(low, PAGE_SIZE),
                       region_flags, VM_REGION_ELF) != 0) {
        return -1;
    }

    program->entry = (uintptr_t)header.entry;
    program->low_address = low;
    program->high_address = high;
    return 0;
}

struct memory_reader_context {
    const uint8_t *data;
    size_t size;
};

static int memory_read_at(void *context, uint64_t offset, void *buffer,
                          size_t size)
{
    struct memory_reader_context *memory = context;

    if (!memory || offset > memory->size || size > memory->size - offset) {
        return -1;
    }
    memory_copy(buffer, memory->data + (size_t)offset, size);
    return 0;
}

int elf_load(const void *image_data, size_t image_size,
             struct address_space *space, struct loaded_program *program)
{
    struct memory_reader_context context = {image_data, image_size};
    struct elf_reader reader = {&context, image_size, memory_read_at};

    if (!image_data) {
        return -1;
    }
    return elf_load_reader(&reader, space, program);
}

#ifndef AUKOS_HOST_TEST
void elf_run_selftest(void)
{
    struct address_space user_space;
    struct loaded_program program;
    uintptr_t entry_physical = 0;
    size_t image_size = (size_t)(_binary_build_user_hello_elf_end - _binary_build_user_hello_elf_start);

    if (vmm_create_user_address_space(&user_space) != 0) {
        log_error("elf: failed to create user address space");
        return;
    }

    if (elf_load(_binary_build_user_hello_elf_start, image_size, &user_space, &program) != 0) {
        log_error("elf: failed to load embedded /bin/hello");
        vmm_destroy_user_address_space(&user_space);
        return;
    }

    if (vmm_query_page(&user_space, program.entry, &entry_physical) != 0 || entry_physical == 0) {
        log_error("elf: entry page is not mapped");
        vmm_destroy_user_address_space(&user_space);
        return;
    }

    serial_write("[INFO] elf: embedded /bin/hello loaded entry=");
    static const char digits[] = "0123456789abcdef";
    serial_write("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        serial_write((char[]){ digits[(program.entry >> shift) & 0xf], '\0' });
    }
    serial_write("\r\n");
    log_info("elf: loader self-test passed");
    vmm_destroy_user_address_space(&user_space);
}
#endif
