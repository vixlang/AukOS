#include "include/aukos/keyboard.h"

#include "include/aukos/io.h"
#include "include/aukos/log.h"

#include <stdint.h>

#define PS2_STATUS_PORT 0x64u
#define PS2_DATA_PORT 0x60u
#define PS2_STATUS_OUTPUT_FULL 0x01u

static int shift_pressed;

static char translate_scancode(uint8_t scancode)
{
    static const char normal[128] = {
        [0x01] = 27,
        [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',
        [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0a] = '9', [0x0b] = '0',
        [0x0c] = '-', [0x0d] = '=', [0x0e] = '\b', [0x0f] = '\t',
        [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',
        [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',
        [0x1a] = '[', [0x1b] = ']', [0x1c] = '\n',
        [0x1e] = 'a', [0x1f] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g',
        [0x23] = 'h', [0x24] = 'j', [0x25] = 'k', [0x26] = 'l', [0x27] = ';',
        [0x28] = '\'', [0x29] = '`', [0x2b] = '\\',
        [0x2c] = 'z', [0x2d] = 'x', [0x2e] = 'c', [0x2f] = 'v', [0x30] = 'b',
        [0x31] = 'n', [0x32] = 'm', [0x33] = ',', [0x34] = '.', [0x35] = '/',
        [0x39] = ' ',
    };
    static const char shifted[128] = {
        [0x01] = 27,
        [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$', [0x06] = '%',
        [0x07] = '^', [0x08] = '&', [0x09] = '*', [0x0a] = '(', [0x0b] = ')',
        [0x0c] = '_', [0x0d] = '+', [0x0e] = '\b', [0x0f] = '\t',
        [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R', [0x14] = 'T',
        [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I', [0x18] = 'O', [0x19] = 'P',
        [0x1a] = '{', [0x1b] = '}', [0x1c] = '\n',
        [0x1e] = 'A', [0x1f] = 'S', [0x20] = 'D', [0x21] = 'F', [0x22] = 'G',
        [0x23] = 'H', [0x24] = 'J', [0x25] = 'K', [0x26] = 'L', [0x27] = ':',
        [0x28] = '"', [0x29] = '~', [0x2b] = '|',
        [0x2c] = 'Z', [0x2d] = 'X', [0x2e] = 'C', [0x2f] = 'V', [0x30] = 'B',
        [0x31] = 'N', [0x32] = 'M', [0x33] = '<', [0x34] = '>', [0x35] = '?',
        [0x39] = ' ',
    };

    if (scancode == 0x2a || scancode == 0x36) {
        shift_pressed = 1;
        return 0;
    }
    if (scancode == 0xaa || scancode == 0xb6) {
        shift_pressed = 0;
        return 0;
    }
    if ((scancode & 0x80u) != 0) {
        return 0;
    }

    return shift_pressed ? shifted[scancode] : normal[scancode];
}

void keyboard_init(void)
{
    shift_pressed = 0;
    log_info("keyboard: ps/2 polling input initialized");
}

char keyboard_read_char(void)
{
    char character;

    for (;;) {
        while ((inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) == 0) {
            __asm__ volatile ("pause");
        }

        character = translate_scancode(inb(PS2_DATA_PORT));
        if (character != 0) {
            return character;
        }
    }
}
