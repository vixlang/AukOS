#ifndef AUKOS_BOOT_FRAMEBUFFER_H
#define AUKOS_BOOT_FRAMEBUFFER_H

#include <stdint.h>

#define BOOT_FRAMEBUFFER_FORMAT_RGB 1u
#define BOOT_FRAMEBUFFER_FORMAT_BGR 2u

struct boot_framebuffer {
    uint64_t address;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bits_per_pixel;
    uint8_t format;
    uint8_t red_position;
    uint8_t green_position;
    uint8_t blue_position;
};

#endif
