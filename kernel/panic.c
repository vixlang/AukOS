#include "include/aukos/panic.h"

#include "include/aukos/log.h"
#include "include/aukos/serial.h"

static void write_hex64(uint64_t value)
{
    static const char digits[] = "0123456789abcdef";

    serial_write("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        serial_write((char[]){ digits[(value >> shift) & 0xf], '\0' });
    }
}

static void write_field(const char *name, uint64_t value)
{
    serial_write(name);
    serial_write("=");
    write_hex64(value);
    serial_write("\r\n");
}

static uint64_t interrupted_rsp(const struct interrupt_frame *frame)
{
    return (uint64_t)&frame->rflags + sizeof(frame->rflags);
}

__attribute__((noreturn)) void panic_exception(const struct interrupt_frame *frame)
{
    uint64_t cr2 = 0;

    if (frame->vector == 14) {
        __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
    }

    log_error("panic: unhandled CPU exception");
    write_field("vector", frame->vector);
    write_field("error", frame->error_code);
    if (frame->vector == 14) {
        write_field("cr2", cr2);
    }
    write_field("rip", frame->rip);
    write_field("rsp", interrupted_rsp(frame));

    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}
