#include "include/aukos/gdt.h"

#include "include/aukos/log.h"

#include <stdint.h>

#define TSS_SELECTOR 0x28

struct tss64 {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

extern uint64_t gdt64_tss[];

static struct tss64 kernel_tss;

void gdt_init_tss(uintptr_t kernel_stack_top)
{
    uintptr_t base = (uintptr_t)&kernel_tss;
    uint32_t limit = sizeof(kernel_tss) - 1;

    kernel_tss.rsp0 = kernel_stack_top;
    kernel_tss.iomap_base = sizeof(kernel_tss);

    gdt64_tss[0] = ((uint64_t)(limit & 0xffff)) |
        (((uint64_t)base & 0xffffff) << 16) |
        ((uint64_t)0x89 << 40) |
        (((uint64_t)(limit >> 16) & 0xf) << 48) |
        (((uint64_t)(base >> 24) & 0xff) << 56);
    gdt64_tss[1] = base >> 32;

    __asm__ volatile ("ltr %0" : : "r"((uint16_t)TSS_SELECTOR));
    log_info("gdt: TSS loaded");
}
