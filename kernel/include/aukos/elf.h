#ifndef AUKOS_ELF_H
#define AUKOS_ELF_H

#include "vmm.h"

#include <stddef.h>
#include <stdint.h>

struct loaded_program {
    uintptr_t entry;
    uintptr_t low_address;
    uintptr_t high_address;
};

struct elf_reader {
    void *context;
    uint64_t size;
    int (*read_at)(void *context, uint64_t offset, void *buffer, size_t size);
};

int elf_load(const void *image, size_t image_size, struct address_space *space, struct loaded_program *program);
int elf_load_reader(const struct elf_reader *reader, struct address_space *space,
                    struct loaded_program *program);
void elf_run_selftest(void);

#endif
