#ifndef AUKOS_CONSOLE_H
#define AUKOS_CONSOLE_H

void console_init(void);
int console_vga_8x16_ready(void);
void console_clear(void);
void console_put_char(char character);
void console_write_color(const char *message, unsigned char color);
void console_write(const char *message);
void console_write_line(const char *message);

#endif
