#ifndef AUKOS_VMM_H
#define AUKOS_VMM_H

#include <stdint.h>

#define VMM_MAX_REGIONS 32u
#define VMM_USER_MIN 0x0000000040000000ull
#define VMM_ELF_END 0x0000000050000000ull
#define VMM_HEAP_BASE 0x0000000050000000ull
#define VMM_HEAP_END 0x0000000060000000ull
#define VMM_MMAP_BASE 0x0000000060000000ull
#define VMM_USER_STACK_TOP 0x0000000080000000ull
#define VMM_EXEC_STACK_INITIAL_PAGES 8u
#define VMM_EXEC_STACK_MAX_PAGES 256u
#define VMM_SIGNAL_STACK_PAGES 8u
#define VMM_STACK_GUARD_PAGES 1u
#define VMM_EXEC_STACK_INITIAL_BASE \
    (VMM_USER_STACK_TOP - (VMM_EXEC_STACK_INITIAL_PAGES * 4096ull))
#define VMM_EXEC_STACK_RESERVE_BASE \
    (VMM_USER_STACK_TOP - (VMM_EXEC_STACK_MAX_PAGES * 4096ull))
#define VMM_EXEC_STACK_GUARD_BASE \
    (VMM_EXEC_STACK_RESERVE_BASE - (VMM_STACK_GUARD_PAGES * 4096ull))
#define VMM_SIGNAL_STACK_TOP VMM_EXEC_STACK_GUARD_BASE
#define VMM_SIGNAL_STACK_BASE \
    (VMM_SIGNAL_STACK_TOP - (VMM_SIGNAL_STACK_PAGES * 4096ull))
#define VMM_SIGNAL_STACK_GUARD_BASE \
    (VMM_SIGNAL_STACK_BASE - (VMM_STACK_GUARD_PAGES * 4096ull))
#define VMM_MMAP_END VMM_SIGNAL_STACK_GUARD_BASE
#define VMM_STACK_FAULT_WINDOW (16u * 4096ull)

#define VMM_USER_FLAG_WRITABLE 0x01u
#define VMM_USER_FLAG_USER 0x02u
#define VMM_USER_FLAG_EXECUTABLE 0x04u
#define VMM_USER_FLAG_READABLE 0x08u

enum vm_region_type {
    VM_REGION_NONE = 0,
    VM_REGION_ELF,
    VM_REGION_HEAP,
    VM_REGION_MMAP,
    VM_REGION_EXEC_STACK,
    VM_REGION_SIGNAL_STACK,
};

struct vm_region {
    uintptr_t start;
    uintptr_t size;
    uint32_t flags;
    uint32_t type;
};

struct address_space {
    uintptr_t pml4_physical;
    struct vm_region regions[VMM_MAX_REGIONS];
    uintptr_t heap_break;
};

#define VMM_KERNEL_DIRECT_BASE 0xffff800000000000ull

void vmm_init(void);
const struct address_space *vmm_kernel_address_space(void);
uintptr_t vmm_kernel_direct_map(uintptr_t physical_address);
int vmm_create_user_address_space(struct address_space *space);
void vmm_destroy_user_address_space(struct address_space *space);
int vmm_clone_user_address_space(const struct address_space *source,
                                 struct address_space *destination);
int vmm_map_page(struct address_space *space, uintptr_t virtual_address, uintptr_t physical_address, uint32_t flags);
int vmm_query_page(const struct address_space *space, uintptr_t virtual_address, uintptr_t *out_physical_address);
int vmm_query_page_flags(const struct address_space *space, uintptr_t virtual_address,
                         uint32_t *out_flags);
int vmm_unmap_page(struct address_space *space, uintptr_t virtual_address,
                   uintptr_t *out_physical_address);
int vmm_protect_page(struct address_space *space, uintptr_t virtual_address,
                     uint32_t flags);
int vmm_add_region(struct address_space *space, uintptr_t start, uintptr_t size,
                   uint32_t flags, enum vm_region_type type);
int vmm_map_anonymous_region(struct address_space *space, uintptr_t start,
                             uintptr_t size, uint32_t flags,
                             enum vm_region_type type);
int vmm_map_user_stack(struct address_space *space);
int vmm_grow_user_stack(struct address_space *space, uintptr_t fault_address,
                        uintptr_t stack_pointer, uint64_t error_code);
int vmm_user_stack_fault_valid(uintptr_t fault_address,
                               uintptr_t stack_pointer, uint64_t error_code);
int vmm_unmap_region(struct address_space *space, uintptr_t start, uintptr_t size,
                     enum vm_region_type required_type);
int vmm_protect_region(struct address_space *space, uintptr_t start, uintptr_t size,
                       uint32_t flags, enum vm_region_type required_type);
uintptr_t vmm_find_free_range(const struct address_space *space, uintptr_t range_start,
                              uintptr_t range_end, uintptr_t size);
uintptr_t vmm_resize_heap(struct address_space *space, uintptr_t requested_break);
const struct vm_region *vmm_find_region(const struct address_space *space,
                                        uintptr_t start, uintptr_t size,
                                        enum vm_region_type required_type);

#endif
