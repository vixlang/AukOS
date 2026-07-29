#ifndef AUKOS_MULTIBOOT2_H
#define AUKOS_MULTIBOOT2_H

#include "memory_map.h"
#include "boot_framebuffer.h"

#include <stdint.h>

#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36d76289u

int multiboot2_read_memory_map(uint32_t magic, uintptr_t info_address, struct memory_map *out_map);
int multiboot2_read_framebuffer(uint32_t magic, uintptr_t info_address,
                                struct boot_framebuffer *out_framebuffer);
void multiboot2_log_memory_map(const struct memory_map *map);

#endif
