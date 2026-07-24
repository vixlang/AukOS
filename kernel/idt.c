#include "include/aukos/idt.h"

#include "include/aukos/panic.h"
#include "include/aukos/scheduler.h"
#include "include/aukos/task.h"
#include "include/aukos/vmm.h"

#include <stdint.h>

#define IDT_GATE_INTERRUPT 0x8e
#define KERNEL_CODE_SELECTOR 0x08

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t attributes;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

#define DECLARE_EXCEPTION_STUB(vector) extern void x86_64_exception_stub_##vector(void)

DECLARE_EXCEPTION_STUB(0);
DECLARE_EXCEPTION_STUB(1);
DECLARE_EXCEPTION_STUB(2);
DECLARE_EXCEPTION_STUB(3);
DECLARE_EXCEPTION_STUB(4);
DECLARE_EXCEPTION_STUB(5);
DECLARE_EXCEPTION_STUB(6);
DECLARE_EXCEPTION_STUB(7);
DECLARE_EXCEPTION_STUB(8);
DECLARE_EXCEPTION_STUB(9);
DECLARE_EXCEPTION_STUB(10);
DECLARE_EXCEPTION_STUB(11);
DECLARE_EXCEPTION_STUB(12);
DECLARE_EXCEPTION_STUB(13);
DECLARE_EXCEPTION_STUB(14);
DECLARE_EXCEPTION_STUB(15);
DECLARE_EXCEPTION_STUB(16);
DECLARE_EXCEPTION_STUB(17);
DECLARE_EXCEPTION_STUB(18);
DECLARE_EXCEPTION_STUB(19);
DECLARE_EXCEPTION_STUB(20);
DECLARE_EXCEPTION_STUB(21);
DECLARE_EXCEPTION_STUB(22);
DECLARE_EXCEPTION_STUB(23);
DECLARE_EXCEPTION_STUB(24);
DECLARE_EXCEPTION_STUB(25);
DECLARE_EXCEPTION_STUB(26);
DECLARE_EXCEPTION_STUB(27);
DECLARE_EXCEPTION_STUB(28);
DECLARE_EXCEPTION_STUB(29);
DECLARE_EXCEPTION_STUB(30);
DECLARE_EXCEPTION_STUB(31);
extern void x86_64_irq0_stub(void);

#define EXCEPTION_STUB(vector) x86_64_exception_stub_##vector

static struct idt_entry idt[256];

static void idt_set_gate(uint8_t vector, void (*handler)(void))
{
    uint64_t address = (uint64_t)handler;

    idt[vector].offset_low = (uint16_t)address;
    idt[vector].selector = KERNEL_CODE_SELECTOR;
    idt[vector].ist = 0;
    idt[vector].attributes = IDT_GATE_INTERRUPT;
    idt[vector].offset_mid = (uint16_t)(address >> 16);
    idt[vector].offset_high = (uint32_t)(address >> 32);
    idt[vector].reserved = 0;
}

void idt_init(void)
{
    struct idt_pointer pointer;

    idt_set_gate(0, EXCEPTION_STUB(0));
    idt_set_gate(1, EXCEPTION_STUB(1));
    idt_set_gate(2, EXCEPTION_STUB(2));
    idt_set_gate(3, EXCEPTION_STUB(3));
    idt_set_gate(4, EXCEPTION_STUB(4));
    idt_set_gate(5, EXCEPTION_STUB(5));
    idt_set_gate(6, EXCEPTION_STUB(6));
    idt_set_gate(7, EXCEPTION_STUB(7));
    idt_set_gate(8, EXCEPTION_STUB(8));
    idt_set_gate(9, EXCEPTION_STUB(9));
    idt_set_gate(10, EXCEPTION_STUB(10));
    idt_set_gate(11, EXCEPTION_STUB(11));
    idt_set_gate(12, EXCEPTION_STUB(12));
    idt_set_gate(13, EXCEPTION_STUB(13));
    idt_set_gate(14, EXCEPTION_STUB(14));
    idt_set_gate(15, EXCEPTION_STUB(15));
    idt_set_gate(16, EXCEPTION_STUB(16));
    idt_set_gate(17, EXCEPTION_STUB(17));
    idt_set_gate(18, EXCEPTION_STUB(18));
    idt_set_gate(19, EXCEPTION_STUB(19));
    idt_set_gate(20, EXCEPTION_STUB(20));
    idt_set_gate(21, EXCEPTION_STUB(21));
    idt_set_gate(22, EXCEPTION_STUB(22));
    idt_set_gate(23, EXCEPTION_STUB(23));
    idt_set_gate(24, EXCEPTION_STUB(24));
    idt_set_gate(25, EXCEPTION_STUB(25));
    idt_set_gate(26, EXCEPTION_STUB(26));
    idt_set_gate(27, EXCEPTION_STUB(27));
    idt_set_gate(28, EXCEPTION_STUB(28));
    idt_set_gate(29, EXCEPTION_STUB(29));
    idt_set_gate(30, EXCEPTION_STUB(30));
    idt_set_gate(31, EXCEPTION_STUB(31));
    idt_set_gate(32, x86_64_irq0_stub);

    pointer.limit = sizeof(idt) - 1;
    pointer.base = (uint64_t)&idt;

    __asm__ volatile ("lidt %0" : : "m"(pointer) : "memory");
}

void x86_64_exception_dispatch(const struct interrupt_frame *frame)
{
    if (frame->vector == 14u && (frame->cs & 3u) == 3u) {
        uintptr_t fault_address;

        __asm__ volatile ("mov %%cr2, %0" : "=r"(fault_address));
        if (current_process &&
            vmm_grow_user_stack(&current_process->address_space, fault_address,
                                frame->rsp, frame->error_code) == 0) {
            return;
        }
        if (current_process) {
            uint64_t pid = current_process->pid;

            process_exit(current_process, 11);
            scheduler_wake_child_waiters(pid);
            scheduler_exit_current();
        }
    }
    panic_exception(frame);
}

void interrupts_enable(void)
{
    __asm__ volatile ("sti");
}

void interrupts_disable(void)
{
    __asm__ volatile ("cli");
}
