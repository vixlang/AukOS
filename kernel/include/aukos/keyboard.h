#ifndef AUKOS_KEYBOARD_H
#define AUKOS_KEYBOARD_H

#include <stdint.h>

#define KEYBOARD_KEY_UP 0x80u
#define KEYBOARD_KEY_DOWN 0x81u
#define KEYBOARD_KEY_LEFT 0x82u
#define KEYBOARD_KEY_RIGHT 0x83u

void keyboard_init(void);
uint8_t keyboard_read_char(void);

#endif
