#ifndef AUKOS_SERIAL_H
#define AUKOS_SERIAL_H

#include <stdint.h>

void serial_init(void);
void serial_write(const char *message);
void serial_write_line(const char *message);
void serial_write_hex(uint64_t val);
void serial_write_dec(uint64_t val);

#endif
