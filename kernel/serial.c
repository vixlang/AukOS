#include "include/aukos/serial.h"
#include "include/aukos/io.h"

#include <stdint.h>

#define COM1 0x3f8

void serial_init(void)
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xc7);
    outb(COM1 + 4, 0x0b);
}

void serial_write(const char *message)
{
    while (*message != '\0') {
        while ((inb(COM1 + 5) & 0x20) == 0) {
        }

        outb(COM1, (uint8_t)*message);
        message++;
    }
}

void serial_write_line(const char *message)
{
    serial_write(message);
    serial_write("\r\n");
}
void serial_write_hex(uint64_t val)
{
    char buffer[17];
    for(int i = 0; i < 16; i++) {
        uint8_t byte = (val >> ((15 - i) * 4)) & 0x0F;
        buffer[i] = (byte < 10) ? ('0' + byte) : ('A' + byte - 10);
    }
    buffer[16] = '\0';
    serial_write(buffer);
}
void serial_write_dec(uint64_t val)
{
    char buffer[21];
    int i = 0;
    if (val == 0) {
        buffer[i++] = '0';
    } else {
        while (val > 0) {
            buffer[i++] = '0' + (val % 10);
            val /= 10;
        }
    }
    for (int j = 0; j < i / 2; j++) {
        char temp = buffer[j];
        buffer[j] = buffer[i - j - 1];
        buffer[i - j - 1] = temp;
    }
    buffer[i] = '\0';
    serial_write(buffer);
}
