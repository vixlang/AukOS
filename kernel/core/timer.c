#include "include/aukos/timer.h"

#include "include/aukos/log.h"
#include "include/aukos/io.h"
#include "include/aukos/panic.h"
#include "include/aukos/scheduler.h"

#include <stdint.h>

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xa0
#define PIC2_DATA 0xa1
#define PIC_EOI 0x20
#define PIT_CHANNEL0 0x40
#define PIT_COMMAND 0x43
#define PIT_BASE_FREQUENCY 1193182u

static volatile uint64_t ticks;

static void pic_remap_and_unmask_timer(void)
{
    outb(PIC1_COMMAND, 0x11);
    outb(PIC2_COMMAND, 0x11);
    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);
    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);
    outb(PIC1_DATA, 0xfe);
    outb(PIC2_DATA, 0xff);
}

void timer_init(uint32_t frequency_hz)
{
    uint32_t divisor = PIT_BASE_FREQUENCY / frequency_hz;

    if (divisor == 0) {
        divisor = 1;
    }

    ticks = 0;
    pic_remap_and_unmask_timer();
    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xff));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xff));
    log_info("timer: PIT IRQ0 initialized");
}

uint64_t timer_ticks(void)
{
    return ticks;
}

int timer_wait_for_ticks(uint64_t target_ticks, uint64_t spin_limit)
{
    while (ticks < target_ticks && spin_limit > 0) {
        __asm__ volatile ("hlt");
        spin_limit--;
    }

    return ticks >= target_ticks ? 0 : -1;
}

void x86_64_irq_dispatch(const struct interrupt_frame *frame)
{
    (void)frame;

    ticks++;
    scheduler_on_tick();
    outb(PIC1_COMMAND, PIC_EOI);
}
