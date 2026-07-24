#ifndef AUKOS_GDT_H
#define AUKOS_GDT_H

#include <stdint.h>

void gdt_init_tss(uintptr_t kernel_stack_top);

#endif
