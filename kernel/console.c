#include "include/aukos/console.h"
#include "include/aukos/io.h"

#include <stddef.h>
#include <stdint.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_BUFFER ((volatile uint16_t *)0xb8000)
#define VGA_FONT_BUFFER ((volatile uint8_t *)0xa0000)
#define VGA_COLOR 0x0f
#define VGA_COLOR_GREEN 0x0a
#define VGA_CRTC_COLOR_COMMAND_PORT 0x3d4u
#define VGA_CRTC_COLOR_DATA_PORT 0x3d5u
#define VGA_CRTC_MONO_COMMAND_PORT 0x3b4u
#define VGA_CRTC_MONO_DATA_PORT 0x3b5u
#define VGA_MISC_OUTPUT_READ_PORT 0x3ccu
#define VGA_SEQUENCER_COMMAND_PORT 0x3c4u
#define VGA_SEQUENCER_DATA_PORT 0x3c5u
#define VGA_SEQUENCER_RESET 0x00u
#define VGA_SEQUENCER_CLOCKING_MODE 0x01u
#define VGA_GRAPHICS_COMMAND_PORT 0x3ceu
#define VGA_GRAPHICS_DATA_PORT 0x3cfu
#define VGA_ATTRIBUTE_PORT 0x3c0u
#define VGA_INPUT_STATUS_1_PORT 0x3dau
#define VGA_MISC_OUTPUT_WRITE_PORT 0x3c2u
#define VGA_DAC_WRITE_INDEX_PORT 0x3c8u
#define VGA_DAC_DATA_PORT 0x3c9u
#define VGA_VBE_DISPI_INDEX_PORT 0x01ceu
#define VGA_VBE_DISPI_DATA_PORT 0x01cfu
#define VGA_VBE_DISPI_ENABLE_INDEX 0x04u
#define VGA_CRTC_MAXIMUM_SCAN_LINE 0x09u
#define VGA_FONT_HEIGHT 16u
#define VGA_FONT_GLYPHS 256u
#define VGA_FONT_SLOT_BYTES 32u

extern const uint8_t _binary_build_vga8x16_bin_start[];
extern const uint8_t _binary_build_vga8x16_bin_end[];

static size_t row;
static size_t column;
static int font_ready;
static uint8_t current_color = VGA_COLOR;
static uint8_t ansi_state;
static uint8_t ansi_private;
static size_t ansi_parameters[3];
static size_t ansi_parameter_index;

static const uint8_t vga_80x25_crtc[] = {
    0x5fu, 0x4fu, 0x50u, 0x82u, 0x55u, 0x81u, 0xbfu, 0x1fu,
    0x00u, 0x4fu, 0x0du, 0x0fu, 0x00u, 0x00u, 0x00u, 0x50u,
    0x9cu, 0x0eu, 0x8fu, 0x28u, 0x1fu, 0x96u, 0xb9u, 0xa3u,
    0xffu
};

static const uint8_t vga_text_graphics[] = {
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x10u, 0x0eu, 0x00u, 0xffu
};

static const uint8_t vga_text_attribute[] = {
    0x00u, 0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u,
    0x08u, 0x09u, 0x0au, 0x0bu, 0x0cu, 0x0du, 0x0eu, 0x0fu,
    0x0cu, 0x00u, 0x0fu, 0x08u, 0x00u
};

static const uint8_t vga_text_palette[] = {
    0u, 0u, 0u,       0u, 0u, 42u,      0u, 42u, 0u,      0u, 42u, 42u,
    42u, 0u, 0u,      42u, 0u, 42u,     42u, 21u, 0u,     42u, 42u, 42u,
    21u, 21u, 21u,    21u, 21u, 63u,    21u, 63u, 21u,    21u, 63u, 63u,
    63u, 21u, 21u,    63u, 21u, 63u,    63u, 63u, 21u,    63u, 63u, 63u
};

static uint16_t crtc_command_port(void)
{
    return (inb(VGA_MISC_OUTPUT_READ_PORT) & 0x01u) != 0
               ? VGA_CRTC_COLOR_COMMAND_PORT
               : VGA_CRTC_MONO_COMMAND_PORT;
}

static uint8_t read_indexed(uint16_t command_port, uint16_t data_port,
                            uint8_t index)
{
    outb(command_port, index);
    return inb(data_port);
}

static void write_indexed(uint16_t command_port, uint16_t data_port,
                          uint8_t index, uint8_t value)
{
    outb(command_port, index);
    outb(data_port, value);
}

static int load_vga_8x16_font(void)
{
    const uint8_t *font = _binary_build_vga8x16_bin_start;
    size_t font_size = (size_t)(_binary_build_vga8x16_bin_end - font);

    if (font_size != VGA_FONT_GLYPHS * VGA_FONT_HEIGHT) {
        return 0;
    }

    write_indexed(VGA_SEQUENCER_COMMAND_PORT, VGA_SEQUENCER_DATA_PORT,
                  0x02u, 0x04u);
    write_indexed(VGA_SEQUENCER_COMMAND_PORT, VGA_SEQUENCER_DATA_PORT,
                  0x04u, 0x06u);
    write_indexed(VGA_GRAPHICS_COMMAND_PORT, VGA_GRAPHICS_DATA_PORT,
                  0x04u, 0x02u);
    write_indexed(VGA_GRAPHICS_COMMAND_PORT, VGA_GRAPHICS_DATA_PORT,
                  0x05u, 0x00u);
    write_indexed(VGA_GRAPHICS_COMMAND_PORT, VGA_GRAPHICS_DATA_PORT,
                  0x06u, 0x04u);

    for (size_t glyph = 0; glyph < VGA_FONT_GLYPHS; glyph++) {
        for (size_t row_index = 0; row_index < VGA_FONT_SLOT_BYTES;
             row_index++) {
            uint8_t value = row_index < VGA_FONT_HEIGHT
                                ? font[glyph * VGA_FONT_HEIGHT + row_index]
                                : 0;
            VGA_FONT_BUFFER[glyph * VGA_FONT_SLOT_BYTES + row_index] = value;
        }
    }

    write_indexed(VGA_SEQUENCER_COMMAND_PORT, VGA_SEQUENCER_DATA_PORT,
                  0x02u, 0x03u);
    write_indexed(VGA_SEQUENCER_COMMAND_PORT, VGA_SEQUENCER_DATA_PORT,
                  0x04u, 0x02u);
    write_indexed(VGA_GRAPHICS_COMMAND_PORT, VGA_GRAPHICS_DATA_PORT,
                  0x04u, 0x00u);
    write_indexed(VGA_GRAPHICS_COMMAND_PORT, VGA_GRAPHICS_DATA_PORT,
                  0x05u, 0x10u);
    write_indexed(VGA_GRAPHICS_COMMAND_PORT, VGA_GRAPHICS_DATA_PORT,
                  0x06u, 0x0eu);
    return 1;
}

static void configure_vga_8x16_font(void)
{
    uint8_t clocking_mode;
    uint8_t maximum_scan_line;
    int font_loaded;
    uint16_t crtc_command;
    uint16_t crtc_data;

    outw(VGA_VBE_DISPI_INDEX_PORT, VGA_VBE_DISPI_ENABLE_INDEX);
    outw(VGA_VBE_DISPI_DATA_PORT, 0x00u);

    write_indexed(VGA_SEQUENCER_COMMAND_PORT, VGA_SEQUENCER_DATA_PORT,
                  VGA_SEQUENCER_RESET, 0x01u);
    outb(VGA_MISC_OUTPUT_WRITE_PORT, 0x67u);
    write_indexed(VGA_SEQUENCER_COMMAND_PORT, VGA_SEQUENCER_DATA_PORT,
                  VGA_SEQUENCER_CLOCKING_MODE, 0x01u);
    write_indexed(VGA_SEQUENCER_COMMAND_PORT, VGA_SEQUENCER_DATA_PORT,
                  0x02u, 0x03u);
    write_indexed(VGA_SEQUENCER_COMMAND_PORT, VGA_SEQUENCER_DATA_PORT,
                  0x03u, 0x00u);
    write_indexed(VGA_SEQUENCER_COMMAND_PORT, VGA_SEQUENCER_DATA_PORT,
                  0x04u, 0x02u);
    write_indexed(VGA_SEQUENCER_COMMAND_PORT, VGA_SEQUENCER_DATA_PORT,
                  VGA_SEQUENCER_RESET, 0x03u);

    crtc_command = crtc_command_port();
    crtc_data = (uint16_t)(crtc_command + 1u);
    write_indexed(crtc_command, crtc_data, 0x11u, 0x0eu);
    for (uint8_t index = 0; index < sizeof(vga_80x25_crtc); index++) {
        write_indexed(crtc_command, crtc_data, index,
                      vga_80x25_crtc[index]);
    }

    for (uint8_t index = 0; index < sizeof(vga_text_graphics); index++) {
        write_indexed(VGA_GRAPHICS_COMMAND_PORT, VGA_GRAPHICS_DATA_PORT,
                      index, vga_text_graphics[index]);
    }

    for (uint8_t index = 0; index < sizeof(vga_text_attribute); index++) {
        (void)inb(VGA_INPUT_STATUS_1_PORT);
        outb(VGA_ATTRIBUTE_PORT, index);
        outb(VGA_ATTRIBUTE_PORT, vga_text_attribute[index]);
    }
    (void)inb(VGA_INPUT_STATUS_1_PORT);
    outb(VGA_ATTRIBUTE_PORT, 0x20u);

    outb(VGA_DAC_WRITE_INDEX_PORT, 0u);
    for (size_t index = 0; index < sizeof(vga_text_palette); index++) {
        outb(VGA_DAC_DATA_PORT, vga_text_palette[index]);
    }

    font_loaded = load_vga_8x16_font();

    clocking_mode = read_indexed(VGA_SEQUENCER_COMMAND_PORT,
                                 VGA_SEQUENCER_DATA_PORT,
                                 VGA_SEQUENCER_CLOCKING_MODE);
    maximum_scan_line = read_indexed(crtc_command,
                                     crtc_data,
                                     VGA_CRTC_MAXIMUM_SCAN_LINE);
    font_ready = font_loaded && (clocking_mode & 0x01u) != 0 &&
                 (maximum_scan_line & 0x1fu) == VGA_FONT_HEIGHT - 1u;
}

static uint16_t vga_entry(char character)
{
    return (uint16_t)character | ((uint16_t)VGA_COLOR << 8);
}

static uint16_t vga_entry_color(char character, uint8_t color)
{
    return (uint16_t)character | ((uint16_t)color << 8);
}

static void update_cursor(void)
{
    uint16_t position = (uint16_t)(row * VGA_WIDTH + column);
    uint16_t crtc_command = crtc_command_port();
    uint16_t crtc_data = (uint16_t)(crtc_command + 1u);

    outb(crtc_command, 0x0fu);
    outb(crtc_data, (uint8_t)(position & 0xffu));
    outb(crtc_command, 0x0eu);
    outb(crtc_data, (uint8_t)(position >> 8));
}

static void clear_row(size_t target_row)
{
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        VGA_BUFFER[target_row * VGA_WIDTH + x] = vga_entry(' ');
    }
}

static void clear_to_end_of_row(void)
{
    for (size_t x = column; x < VGA_WIDTH; x++) {
        VGA_BUFFER[row * VGA_WIDTH + x] = vga_entry(' ');
    }
}

static size_t ansi_parameter(size_t index, size_t fallback)
{
    if (index > ansi_parameter_index || ansi_parameters[index] == 0u) {
        return fallback;
    }
    return ansi_parameters[index];
}

static void ansi_reset(void)
{
    ansi_state = 0u;
    ansi_private = 0u;
    ansi_parameter_index = 0u;
    for (size_t index = 0; index < 3u; index++) {
        ansi_parameters[index] = 0u;
    }
}

static uint8_t ansi_vga_color(size_t color)
{
    static const uint8_t ansi_to_vga[8] = {
        0u, 4u, 2u, 6u, 1u, 5u, 3u, 7u
    };

    return ansi_to_vga[color & 7u];
}

static void ansi_finish(char command)
{
    if (command == 'H' || command == 'f') {
        size_t target_row = ansi_parameter(0u, 1u);
        size_t target_column = ansi_parameter(1u, 1u);

        row = target_row > VGA_HEIGHT ? VGA_HEIGHT - 1u : target_row - 1u;
        column = target_column > VGA_WIDTH ? VGA_WIDTH - 1u : target_column - 1u;
    } else if (command == 'J' && ansi_parameters[0] == 2u) {
        for (size_t target_row = 0; target_row < VGA_HEIGHT; target_row++) {
            clear_row(target_row);
        }
        row = 0u;
        column = 0u;
    } else if (command == 'K') {
        clear_to_end_of_row();
    } else if (command == 'A') {
        size_t amount = ansi_parameter(0u, 1u);
        row = amount > row ? 0u : row - amount;
    } else if (command == 'B') {
        size_t amount = ansi_parameter(0u, 1u);
        row = amount >= VGA_HEIGHT - row ? VGA_HEIGHT - 1u : row + amount;
    } else if (command == 'C') {
        size_t amount = ansi_parameter(0u, 1u);
        column = amount >= VGA_WIDTH - column ? VGA_WIDTH - 1u : column + amount;
    } else if (command == 'D') {
        size_t amount = ansi_parameter(0u, 1u);
        column = amount > column ? 0u : column - amount;
    } else if (command == 'm') {
        for (size_t index = 0; index <= ansi_parameter_index; index++) {
            size_t parameter = ansi_parameters[index];

            if (parameter == 0u) {
                current_color = VGA_COLOR;
            } else if (parameter == 1u) {
                current_color |= 0x08u;
            } else if (parameter == 22u) {
                current_color &= 0xf7u;
            } else if (parameter == 7u) {
                current_color = (uint8_t)((current_color << 4u) |
                                          (current_color >> 4u));
            } else if (parameter >= 30u && parameter <= 37u) {
                current_color = (uint8_t)((current_color & 0xf8u) |
                                          ansi_vga_color(parameter - 30u));
            } else if (parameter == 39u) {
                current_color = (uint8_t)((current_color & 0xf0u) |
                                          (VGA_COLOR & 0x0fu));
            } else if (parameter >= 40u && parameter <= 47u) {
                current_color = (uint8_t)((current_color & 0x0fu) |
                    (ansi_vga_color(parameter - 40u) << 4u));
            } else if (parameter >= 90u && parameter <= 97u) {
                current_color = (uint8_t)((current_color & 0xf0u) | 0x08u |
                                          ansi_vga_color(parameter - 90u));
            } else if (parameter >= 100u && parameter <= 107u) {
                current_color = (uint8_t)((current_color & 0x0fu) |
                    ((ansi_vga_color(parameter - 100u) | 0x08u) << 4u));
            }
        }
    }
    (void)ansi_private;
    ansi_reset();
    update_cursor();
}

static int ansi_consume(char character)
{
    if (ansi_state == 0u) {
        if ((unsigned char)character == 27u) {
            ansi_state = 1u;
            return 1;
        }
        return 0;
    }
    if (ansi_state == 1u) {
        if (character == '[') {
            ansi_state = 2u;
            return 1;
        }
        ansi_reset();
        return 0;
    }
    if (character == '?') {
        ansi_private = 1u;
        return 1;
    }
    if (character >= '0' && character <= '9') {
        ansi_parameters[ansi_parameter_index] =
            ansi_parameters[ansi_parameter_index] * 10u +
            (size_t)(character - '0');
        return 1;
    }
    if (character == ';') {
        if (ansi_parameter_index + 1u < 3u) {
            ansi_parameter_index++;
        }
        return 1;
    }
    ansi_finish(character);
    return 1;
}

static void scroll(void)
{
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA_BUFFER[(y - 1) * VGA_WIDTH + x] = VGA_BUFFER[y * VGA_WIDTH + x];
        }
    }

    clear_row(VGA_HEIGHT - 1);
    row = VGA_HEIGHT - 1;
    column = 0;
}

static void newline(void)
{
    column = 0;
    row++;
    if (row >= VGA_HEIGHT) {
        scroll();
    }
}

static void put_char_color(char character, uint8_t color)
{
    if (character == '\b') {
        if (column > 0) {
            column--;
            VGA_BUFFER[row * VGA_WIDTH + column] = vga_entry(' ');
        }
        return;
    }

    if (character == '\r') {
        column = 0;
        return;
    }

    if (character == '\n') {
        newline();
        return;
    }

    VGA_BUFFER[row * VGA_WIDTH + column] = vga_entry_color(character, color);
    column++;
    if (column >= VGA_WIDTH) {
        newline();
    }
}

static void put_char(char character)
{
    put_char_color(character, current_color);
}

void console_put_char(char character)
{
    if (ansi_consume(character)) {
        return;
    }
    put_char(character);
    update_cursor();
}

void console_init(void)
{
    configure_vga_8x16_font();
    console_clear();
}

int console_vga_8x16_ready(void)
{
    return font_ready;
}

void console_clear(void)
{
    row = 0;
    column = 0;
    current_color = VGA_COLOR;
    ansi_reset();

    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        clear_row(y);
    }

    update_cursor();
}

void console_write(const char *message)
{
    while (*message != '\0') {
        console_put_char(*message);
        message++;
    }
}

void console_write_color(const char *message, unsigned char color)
{
    while (*message != '\0') {
        put_char_color(*message, color);
        update_cursor();
        message++;
    }
}

void console_write_line(const char *message)
{
    console_write(message);
    console_write("\r\n");
}
