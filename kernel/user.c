#include "include/aukos/user.h"

#include "include/aukos/elf.h"
#include "include/aukos/log.h"
#include "include/aukos/memory.h"
#include "include/aukos/task.h"
#include "include/aukos/vfs.h"
#include "include/aukos/vmm.h"

#include <stdint.h>

#define USER_STACK_TOP VMM_USER_STACK_TOP

extern void x86_64_enter_user(uintptr_t entry, uintptr_t stack_top, uintptr_t pml4, uintptr_t argc, uintptr_t argv, uintptr_t envp) __attribute__((noreturn));

struct user_stack {
    uintptr_t stack_top;
    uintptr_t argv;
    uintptr_t envp;
};

static void memory_copy(uint8_t *destination, const uint8_t *source, size_t size)
{
    for (size_t index = 0; index < size; index++) {
        destination[index] = source[index];
    }
}

static size_t string_length(const char *value)
{
    size_t length = 0;
    while (value[length] != '\0') {
        length++;
    }
    return length;
}

static uintptr_t align_down(uintptr_t value, uintptr_t alignment)
{
    return value & ~(alignment - 1);
}

static void map_user_stack(struct address_space *space, struct user_stack *stack)
{
    uintptr_t stack_physical;
    uint8_t *stack_bytes;
    const char *argv0 = "/bin/aush";
    const char *environment[] = {"PATH=/bin", "HOME=/", "TMPDIR=/tmp"};
    uintptr_t environment_virtual[3];
    size_t argv0_size = string_length(argv0) + 1;
    uintptr_t stack_base = USER_STACK_TOP - PAGE_SIZE;
    uintptr_t cursor = USER_STACK_TOP;
    uintptr_t argv0_virtual;
    uint64_t *table;

    if (vmm_map_user_stack(space) != 0 ||
        vmm_query_page(space, USER_STACK_TOP - PAGE_SIZE, &stack_physical) != 0) {
        log_error("user: failed to map stack");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }
    stack_bytes = (uint8_t *)stack_physical;

    if (vmm_map_anonymous_region(
            space, VMM_SIGNAL_STACK_BASE,
            VMM_SIGNAL_STACK_PAGES * PAGE_SIZE,
            VMM_USER_FLAG_READABLE | VMM_USER_FLAG_WRITABLE | VMM_USER_FLAG_USER,
            VM_REGION_SIGNAL_STACK) != 0) {
        log_error("user: failed to map signal stack");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }

    cursor -= argv0_size;
    argv0_virtual = cursor;
    memory_copy(stack_bytes + (cursor - stack_base),
                (const uint8_t *)argv0, argv0_size);
    for (size_t index = 3u; index > 0u; index--) {
        size_t item = index - 1u;
        size_t length = string_length(environment[item]) + 1u;

        cursor -= length;
        environment_virtual[item] = cursor;
        memory_copy(stack_bytes + (cursor - stack_base),
                    (const uint8_t *)environment[item], length);
    }
    cursor = align_down(cursor, 16);
    cursor -= 4u * sizeof(uint64_t);
    stack->envp = cursor;
    table = (uint64_t *)(stack_bytes + (cursor - stack_base));
    table[0] = environment_virtual[0];
    table[1] = environment_virtual[1];
    table[2] = environment_virtual[2];
    table[3] = 0;

    cursor = align_down(cursor - 2u * sizeof(uint64_t), 16);
    stack->argv = cursor;
    table = (uint64_t *)(stack_bytes + (cursor - stack_base));
    table[0] = argv0_virtual;
    table[1] = 0;
    cursor -= sizeof(uint64_t);
    *(uint64_t *)(stack_bytes + (cursor - stack_base)) = 0;
    stack->stack_top = cursor;
}

__attribute__((noreturn)) void user_run_init(void)
{
    struct vfs_file *file;
    struct vfs_inode *inode;
    struct address_space space;
    struct loaded_program program;
    struct process *init;
    struct user_stack user_stack;
    void *elf_data;
    size_t elf_size;

    if (vfs_lookup("/bin/aush", &inode) != 0) {
        log_error("user: /bin/aush not found");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }

    if (vfs_open("/bin/aush", 0, 0, &file) != 0) {
        log_error("user: failed to open /bin/aush");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }

    elf_size = inode->size;
    elf_data = kmalloc(elf_size);
    if (!elf_data) {
        log_error("user: failed to allocate memory for ELF");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }

    if (vfs_read(file, elf_data, elf_size) != elf_size) {
        log_error("user: failed to read ELF file");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }

    vfs_close(file);

    if (vmm_create_user_address_space(&space) != 0 || elf_load(elf_data, elf_size, &space, &program) != 0) {
        log_error("user: failed to load /bin/aush");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }

    map_user_stack(&space, &user_stack);
    init = process_create_user(&space, 0);
    if (init == 0) {
        log_error("user: failed to create init process");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }

    current_process = init;
    init->state = PROCESS_RUNNING;
    log_info("user: entering /bin/aush");
    x86_64_enter_user(program.entry, user_stack.stack_top, space.pml4_physical, 1, user_stack.argv, user_stack.envp);
}
