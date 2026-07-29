#ifndef AUKOS_CONSOLE_H
#define AUKOS_CONSOLE_H

#include "boot_framebuffer.h"

void console_init(const struct boot_framebuffer *framebuffer);
int console_framebuffer_ready(void);
int console_vga_8x20_ready(void);
void console_remap_framebuffer(uint64_t address);
uint16_t console_terminal_rows(void);
uint16_t console_terminal_columns(void);
uint16_t console_width_pixels(void);
uint16_t console_height_pixels(void);
void console_clear(void);
void console_put_char(char character);
void console_write_color(const char *message, unsigned char color);
void console_write(const char *message);
void console_write_line(const char *message);

#endif
