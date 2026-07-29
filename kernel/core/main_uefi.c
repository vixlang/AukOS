#include "include/aukos/console.h"
#include "include/aukos/elf.h"
#include "include/aukos/ethernet.h"
#include "include/aukos/ext4.h"
#include "include/aukos/fat32.h"
#include "include/aukos/gdt.h"
#include "include/aukos/idt.h"
#include "include/aukos/keyboard.h"
#include "include/aukos/log.h"
#include "include/aukos/block.h"
#include "include/aukos/memory.h"
#include "include/aukos/net.h"
#include "include/aukos/pci.h"
#include "include/aukos/ramdisk.h"
#include "include/aukos/uefi_memory.h"
#include "include/aukos/scheduler.h"
#include "include/aukos/serial.h"
#include "include/aukos/syscall.h"
#include "include/aukos/task.h"
#include "include/aukos/timer.h"
#include "include/aukos/user.h"
#include "include/aukos/vfs.h"
#include "include/aukos/tmpfs.h"
#include "include/aukos/vmm.h"
#include "include/aukos/virtio_blk.h"

#include <stdint.h>

void kernel_main(uint64_t memory_map_addr, uint64_t map_size, uint64_t desc_size,
                 const struct boot_framebuffer *framebuffer) {
    struct memory_map memory_map;
    void *page;
    void *heap_object;
    void *tss_stack;

    serial_init();
    console_init(framebuffer);
    if (framebuffer != 0 && framebuffer->address != 0) {
        vmm_set_boot_framebuffer((uintptr_t)framebuffer->address,
                                 (uintptr_t)framebuffer->pitch *
                                     framebuffer->height);
    }
    log_set_level(LOG_DEBUG);
    log_info("AukOS kernel entered x86_64 long mode (UEFI)");
    log_info("serial: COM1 initialized");
    if (console_framebuffer_ready()) {
        log_info("console: high-resolution framebuffer using Terminus 8x20 font");
    } else if (console_vga_8x20_ready()) {
        log_info("console: VGA 80x20 text mode using Terminus 8x20 font");
    } else {
        log_error("console: failed to configure console framebuffer/font");
    }
    idt_init();
    log_info("idt: loaded CPU exception table");

    if (uefi_read_memory_map((const struct uefi_memory_descriptor *)memory_map_addr, map_size, desc_size, &memory_map) == 0) {
        uefi_log_memory_map(&memory_map);
        memory_init(&memory_map);
        tss_stack = pmm_alloc_page();
        if (tss_stack != 0) {
            gdt_init_tss((uintptr_t)tss_stack + PAGE_SIZE);
        } else {
            log_error("gdt: failed to allocate TSS stack");
        }
        vmm_init();
        tasking_init();
        scheduler_init();
        scheduler_run_selftest();
        timer_init(100);
        interrupts_enable();
        if (timer_wait_for_ticks(2, 1000000) == 0 && scheduler_ticks() >= 2) {
            log_info("sched: timer tick self-test passed");
        } else {
            log_error("sched: timer tick self-test failed");
        }
        interrupts_disable();
        syscall_init();
        syscall_run_selftest();
        elf_run_selftest();
        vfs_init();
        tmpfs_init();
        syscall_run_vfs_selftest();
        block_init();
        ramdisk_init();
        block_run_selftest();
        pci_init();
        virtio_blk_init();
        fat32_run_selftest();
        ext4_run_selftest();
        net_init();
        ethernet_init();
        net_run_selftest();
        keyboard_init();
    } else {
        log_error("memory: initialization skipped");
    }

    page = pmm_alloc_page();
    heap_object = kmalloc(64);
    if (page != 0 && heap_object != 0) {
        log_info("memory: allocation self-test passed");
        pmm_free_page(page);
    } else {
        log_error("memory: allocation self-test failed");
    }

    console_clear();
    user_run_init();

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
