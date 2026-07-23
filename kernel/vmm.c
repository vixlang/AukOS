#include "include/aukos/vmm.h"

#include "include/aukos/log.h"
#include "include/aukos/memory.h"
#include "include/aukos/serial.h"

#include <stddef.h>
#include <stdint.h>

#define PAGE_PRESENT 0x001ull
#define PAGE_WRITABLE 0x002ull
#define PAGE_USER 0x004ull
#define PAGE_HUGE 0x080ull
#define PAGE_NO_EXECUTE (1ull << 63)
#define PAGE_ADDR_MASK 0x000ffffffffff000ull
#define LOW_MEMORY_MAP_SIZE 0x40000000ull
#define USER_SELFTEST_VA 0x0000004000000000ull
#define MSR_EFER 0xc0000080u
#define EFER_NXE (1ull << 11)

static struct address_space kernel_address_space;

static uintptr_t align_up(uintptr_t value)
{
    return (value + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);
}

static uint64_t read_msr(uint32_t msr)
{
    uint32_t low;
    uint32_t high;

    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static void write_msr(uint32_t msr, uint64_t value)
{
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"((uint32_t)value),
                      "d"((uint32_t)(value >> 32)));
}

static void write_hex64(uint64_t value)
{
    static const char digits[] = "0123456789abcdef";

    serial_write("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        serial_write((char[]){ digits[(value >> shift) & 0xf], '\0' });
    }
}

static void zero_page(uint64_t *page)
{
    for (size_t index = 0; index < PAGE_SIZE / sizeof(uint64_t); index++) {
        page[index] = 0;
    }
}

static uint64_t *alloc_page_table(void)
{
    uint64_t *table = pmm_alloc_page();

    if (table == 0) {
        return 0;
    }

    zero_page(table);
    return table;
}

static uint16_t pml4_index(uintptr_t virtual_address)
{
    return (virtual_address >> 39) & 0x1ff;
}

static uint16_t pdpt_index(uintptr_t virtual_address)
{
    return (virtual_address >> 30) & 0x1ff;
}

static uint16_t pd_index(uintptr_t virtual_address)
{
    return (virtual_address >> 21) & 0x1ff;
}

static uint16_t pt_index(uintptr_t virtual_address)
{
    return (virtual_address >> 12) & 0x1ff;
}

static uint64_t page_flags_from_vmm_flags(uint32_t flags)
{
    uint64_t page_flags = PAGE_NO_EXECUTE;

    if ((flags & VMM_USER_FLAG_WRITABLE) != 0) {
        page_flags |= PAGE_WRITABLE;
    }

    if ((flags & VMM_USER_FLAG_USER) != 0) {
        page_flags |= PAGE_USER;
    }

    if ((flags & VMM_USER_FLAG_EXECUTABLE) != 0) {
        page_flags &= ~PAGE_NO_EXECUTE;
    }

    return page_flags;
}

static uint32_t vmm_flags_from_page_flags(uint64_t flags)
{
    uint32_t result = VMM_USER_FLAG_READABLE;

    if ((flags & PAGE_WRITABLE) != 0) {
        result |= VMM_USER_FLAG_WRITABLE;
    }
    if ((flags & PAGE_USER) != 0) {
        result |= VMM_USER_FLAG_USER;
    }
    if ((flags & PAGE_NO_EXECUTE) == 0) {
        result |= VMM_USER_FLAG_EXECUTABLE;
    }
    return result;
}

static int valid_page_range(uintptr_t start, uintptr_t size,
                            uintptr_t minimum, uintptr_t maximum)
{
    if (size == 0 || (start & (PAGE_SIZE - 1u)) != 0 ||
        (size & (PAGE_SIZE - 1u)) != 0 || start < minimum || start >= maximum ||
        size > maximum - start) {
        return 0;
    }
    return 1;
}

static uint64_t *find_page_entry(const struct address_space *space,
                                 uintptr_t virtual_address)
{
    uint64_t *pml4;
    uint64_t *pdpt;
    uint64_t *pd;
    uint64_t *pt;

    if (!space || !space->pml4_physical) {
        return 0;
    }
    pml4 = (uint64_t *)space->pml4_physical;
    if ((pml4[pml4_index(virtual_address)] & PAGE_PRESENT) == 0) {
        return 0;
    }
    pdpt = (uint64_t *)(uintptr_t)(pml4[pml4_index(virtual_address)] & PAGE_ADDR_MASK);
    if ((pdpt[pdpt_index(virtual_address)] & PAGE_PRESENT) == 0) {
        return 0;
    }
    pd = (uint64_t *)(uintptr_t)(pdpt[pdpt_index(virtual_address)] & PAGE_ADDR_MASK);
    if ((pd[pd_index(virtual_address)] & (PAGE_PRESENT | PAGE_HUGE)) != PAGE_PRESENT) {
        return 0;
    }
    pt = (uint64_t *)(uintptr_t)(pd[pd_index(virtual_address)] & PAGE_ADDR_MASK);
    return &pt[pt_index(virtual_address)];
}

static int map_2m_page(uint64_t *pml4, uintptr_t virtual_address, uintptr_t physical_address, uint64_t flags)
{
    uint64_t *pdpt;
    uint64_t *pd;
    uint16_t pml4_slot = pml4_index(virtual_address);
    uint16_t pdpt_slot = pdpt_index(virtual_address);
    uint16_t pd_slot = pd_index(virtual_address);

    if ((pml4[pml4_slot] & PAGE_PRESENT) == 0) {
        pdpt = alloc_page_table();
        if (pdpt == 0) {
            return -1;
        }
        pml4[pml4_slot] = (uintptr_t)pdpt | PAGE_PRESENT | PAGE_WRITABLE;
    }

    pdpt = (uint64_t *)(uintptr_t)(pml4[pml4_slot] & PAGE_ADDR_MASK);
    if ((pdpt[pdpt_slot] & PAGE_PRESENT) == 0) {
        pd = alloc_page_table();
        if (pd == 0) {
            return -1;
        }
        pdpt[pdpt_slot] = (uintptr_t)pd | PAGE_PRESENT | PAGE_WRITABLE;
    }

    pd = (uint64_t *)(uintptr_t)(pdpt[pdpt_slot] & PAGE_ADDR_MASK);
    pd[pd_slot] = (physical_address & PAGE_ADDR_MASK) | flags | PAGE_PRESENT | PAGE_HUGE;
    return 0;
}

static int map_range_2m(uint64_t *pml4, uintptr_t virtual_base, uintptr_t physical_base, uintptr_t size, uint64_t flags)
{
    for (uintptr_t offset = 0; offset < size; offset += 0x200000ull) {
        if (map_2m_page(pml4, virtual_base + offset, physical_base + offset, flags) != 0) {
            return -1;
        }
    }

    return 0;
}

int vmm_map_page(struct address_space *space, uintptr_t virtual_address, uintptr_t physical_address, uint32_t flags)
{
    uint64_t *pml4;
    uint64_t *pdpt;
    uint64_t *pd;
    uint64_t *pt;
    uint64_t page_flags = page_flags_from_vmm_flags(flags);
    uint16_t pml4_slot = pml4_index(virtual_address);
    uint16_t pdpt_slot = pdpt_index(virtual_address);
    uint16_t pd_slot = pd_index(virtual_address);
    uint16_t pt_slot = pt_index(virtual_address);
    int allocated_pdpt = 0;
    int allocated_pd = 0;

    if (!space || !space->pml4_physical ||
        (virtual_address & (PAGE_SIZE - 1)) != 0 ||
        (physical_address & (PAGE_SIZE - 1)) != 0) {
        return -1;
    }
    pml4 = (uint64_t *)space->pml4_physical;

    if ((pml4[pml4_slot] & PAGE_PRESENT) == 0) {
        pdpt = alloc_page_table();
        if (pdpt == 0) {
            return -1;
        }
        pml4[pml4_slot] = (uintptr_t)pdpt | PAGE_PRESENT | PAGE_WRITABLE | (page_flags & PAGE_USER);
        allocated_pdpt = 1;
    } else {
        pml4[pml4_slot] |= page_flags & PAGE_USER;
    }

    pdpt = (uint64_t *)(uintptr_t)(pml4[pml4_slot] & PAGE_ADDR_MASK);
    if ((pdpt[pdpt_slot] & PAGE_PRESENT) == 0) {
        pd = alloc_page_table();
        if (pd == 0) {
            if (allocated_pdpt) {
                pml4[pml4_slot] = 0;
                pmm_free_page(pdpt);
            }
            return -1;
        }
        pdpt[pdpt_slot] = (uintptr_t)pd | PAGE_PRESENT | PAGE_WRITABLE | (page_flags & PAGE_USER);
        allocated_pd = 1;
    } else {
        pdpt[pdpt_slot] |= page_flags & PAGE_USER;
    }

    pd = (uint64_t *)(uintptr_t)(pdpt[pdpt_slot] & PAGE_ADDR_MASK);
    if ((pd[pd_slot] & PAGE_PRESENT) == 0) {
        pt = alloc_page_table();
        if (pt == 0) {
            if (allocated_pd) {
                pdpt[pdpt_slot] = 0;
                pmm_free_page(pd);
            }
            if (allocated_pdpt) {
                pml4[pml4_slot] = 0;
                pmm_free_page(pdpt);
            }
            return -1;
        }
        pd[pd_slot] = (uintptr_t)pt | PAGE_PRESENT | PAGE_WRITABLE | (page_flags & PAGE_USER);
    } else {
        pd[pd_slot] |= page_flags & PAGE_USER;
    }

    if ((pd[pd_slot] & PAGE_HUGE) != 0) {
        if (allocated_pd) {
            pdpt[pdpt_slot] = 0;
            pmm_free_page(pd);
        }
        if (allocated_pdpt) {
            pml4[pml4_slot] = 0;
            pmm_free_page(pdpt);
        }
        return -1;
    }

    pt = (uint64_t *)(uintptr_t)(pd[pd_slot] & PAGE_ADDR_MASK);
    pt[pt_slot] = (physical_address & PAGE_ADDR_MASK) | page_flags | PAGE_PRESENT;
    return 0;
}

int vmm_query_page(const struct address_space *space, uintptr_t virtual_address, uintptr_t *out_physical_address)
{
    uint64_t *pml4;
    uint64_t *pdpt;
    uint64_t *pd;
    uint64_t *pt;
    uint64_t entry;
    uint16_t pml4_slot = pml4_index(virtual_address);
    uint16_t pdpt_slot = pdpt_index(virtual_address);
    uint16_t pd_slot = pd_index(virtual_address);
    uint16_t pt_slot = pt_index(virtual_address);

    if (!space || !space->pml4_physical || !out_physical_address) {
        return -1;
    }
    pml4 = (uint64_t *)space->pml4_physical;
    if ((pml4[pml4_slot] & PAGE_PRESENT) == 0) {
        return -1;
    }

    pdpt = (uint64_t *)(uintptr_t)(pml4[pml4_slot] & PAGE_ADDR_MASK);
    if ((pdpt[pdpt_slot] & PAGE_PRESENT) == 0) {
        return -1;
    }

    pd = (uint64_t *)(uintptr_t)(pdpt[pdpt_slot] & PAGE_ADDR_MASK);
    if ((pd[pd_slot] & PAGE_PRESENT) == 0) {
        return -1;
    }

    if ((pd[pd_slot] & PAGE_HUGE) != 0) {
        *out_physical_address = (pd[pd_slot] & PAGE_ADDR_MASK) + (virtual_address & 0x1fffffull);
        return 0;
    }

    pt = (uint64_t *)(uintptr_t)(pd[pd_slot] & PAGE_ADDR_MASK);
    entry = pt[pt_slot];
    if ((entry & PAGE_PRESENT) == 0) {
        return -1;
    }

    *out_physical_address = (entry & PAGE_ADDR_MASK) + (virtual_address & (PAGE_SIZE - 1));
    return 0;
}

int vmm_query_page_flags(const struct address_space *space, uintptr_t virtual_address,
                         uint32_t *out_flags)
{
    uint64_t *entry;

    if (!out_flags) {
        return -1;
    }
    entry = find_page_entry(space, virtual_address);
    if (!entry || (*entry & PAGE_PRESENT) == 0) {
        return -1;
    }
    *out_flags = vmm_flags_from_page_flags(*entry);
    return 0;
}

int vmm_unmap_page(struct address_space *space, uintptr_t virtual_address,
                   uintptr_t *out_physical_address)
{
    uint64_t *entry;

    if ((virtual_address & (PAGE_SIZE - 1u)) != 0) {
        return -1;
    }
    entry = find_page_entry(space, virtual_address);
    if (!entry || (*entry & PAGE_PRESENT) == 0) {
        return -1;
    }
    if (out_physical_address) {
        *out_physical_address = *entry & PAGE_ADDR_MASK;
    }
    *entry = 0;
    __asm__ volatile ("invlpg (%0)" : : "r"(virtual_address) : "memory");
    return 0;
}

int vmm_protect_page(struct address_space *space, uintptr_t virtual_address,
                     uint32_t flags)
{
    uint64_t *entry;
    uintptr_t physical;

    if ((virtual_address & (PAGE_SIZE - 1u)) != 0) {
        return -1;
    }
    entry = find_page_entry(space, virtual_address);
    if (!entry || (*entry & PAGE_PRESENT) == 0) {
        return -1;
    }
    physical = *entry & PAGE_ADDR_MASK;
    *entry = physical | page_flags_from_vmm_flags(flags) | PAGE_PRESENT;
    __asm__ volatile ("invlpg (%0)" : : "r"(virtual_address) : "memory");
    return 0;
}

int vmm_create_user_address_space(struct address_space *space)
{
    uint64_t *pml4 = alloc_page_table();
    uint64_t *kernel_pml4 = (uint64_t *)kernel_address_space.pml4_physical;
    uint64_t *kernel_low_pdpt;
    uint64_t *user_low_pdpt;

    if (!space || pml4 == 0 || kernel_pml4 == 0) {
        if (pml4) {
            pmm_free_page(pml4);
        }
        return -1;
    }

    for (uint16_t index = 0; index < 512; index++) {
        pml4[index] = kernel_pml4[index];
    }

    if ((kernel_pml4[0] & PAGE_PRESENT) != 0) {
        kernel_low_pdpt = (uint64_t *)(uintptr_t)(kernel_pml4[0] & PAGE_ADDR_MASK);
        user_low_pdpt = alloc_page_table();
        if (user_low_pdpt == 0) {
            pmm_free_page(pml4);
            return -1;
        }

        for (uint16_t index = 0; index < 512; index++) {
            user_low_pdpt[index] = kernel_low_pdpt[index];
        }

        pml4[0] = (kernel_pml4[0] & ~PAGE_ADDR_MASK) | (uintptr_t)user_low_pdpt | PAGE_USER;
    }

    space->pml4_physical = (uintptr_t)pml4;
    for (size_t index = 0; index < VMM_MAX_REGIONS; index++) {
        space->regions[index].type = VM_REGION_NONE;
        space->regions[index].start = 0;
        space->regions[index].size = 0;
        space->regions[index].flags = 0;
    }
    space->heap_break = VMM_HEAP_BASE;
    return 0;
}

static int ranges_overlap(uintptr_t first_start, uintptr_t first_size,
                          uintptr_t second_start, uintptr_t second_size)
{
    return first_start < second_start + second_size &&
           second_start < first_start + first_size;
}

const struct vm_region *vmm_find_region(const struct address_space *space,
                                        uintptr_t start, uintptr_t size,
                                        enum vm_region_type required_type)
{
    if (!space || size == 0 || start > UINTPTR_MAX - size) {
        return 0;
    }
    for (size_t index = 0; index < VMM_MAX_REGIONS; index++) {
        const struct vm_region *region = &space->regions[index];

        if (region->type != VM_REGION_NONE &&
            (required_type == VM_REGION_NONE || region->type == (uint32_t)required_type) &&
            region->start == start && region->size == size) {
            return region;
        }
    }
    return 0;
}

static struct vm_region *find_mutable_region(struct address_space *space,
                                              uintptr_t start, uintptr_t size,
                                              enum vm_region_type required_type)
{
    return (struct vm_region *)(uintptr_t)vmm_find_region(space, start, size,
                                                           required_type);
}

int vmm_add_region(struct address_space *space, uintptr_t start, uintptr_t size,
                   uint32_t flags, enum vm_region_type type)
{
    struct vm_region *free_region = 0;

    if (!space || type == VM_REGION_NONE ||
        !valid_page_range(start, size, VMM_USER_MIN, VMM_USER_STACK_TOP)) {
        return -1;
    }
    for (size_t index = 0; index < VMM_MAX_REGIONS; index++) {
        struct vm_region *region = &space->regions[index];

        if (region->type == VM_REGION_NONE) {
            if (!free_region) {
                free_region = region;
            }
            continue;
        }
        if (ranges_overlap(start, size, region->start, region->size)) {
            return -1;
        }
    }
    if (!free_region) {
        return -1;
    }
    free_region->start = start;
    free_region->size = size;
    free_region->flags = flags;
    free_region->type = (uint32_t)type;
    return 0;
}

static void clear_region(struct vm_region *region)
{
    region->start = 0;
    region->size = 0;
    region->flags = 0;
    region->type = VM_REGION_NONE;
}

int vmm_map_anonymous_region(struct address_space *space, uintptr_t start,
                             uintptr_t size, uint32_t flags,
                             enum vm_region_type type)
{
    uintptr_t mapped = 0;

    if (vmm_add_region(space, start, size, flags, type) != 0) {
        return -1;
    }
    while (mapped < size) {
        void *page = pmm_alloc_page();

        if (!page) {
            break;
        }
        zero_page((uint64_t *)page);
        if (vmm_map_page(space, start + mapped, (uintptr_t)page,
                         flags | VMM_USER_FLAG_USER) != 0) {
            pmm_free_page(page);
            break;
        }
        mapped += PAGE_SIZE;
    }
    if (mapped == size) {
        return 0;
    }
    while (mapped != 0) {
        uintptr_t physical;

        mapped -= PAGE_SIZE;
        if (vmm_unmap_page(space, start + mapped, &physical) == 0) {
            pmm_free_page((void *)physical);
        }
    }
    clear_region(find_mutable_region(space, start, size, type));
    return -1;
}

int vmm_map_user_stack(struct address_space *space)
{
    const uint32_t flags = VMM_USER_FLAG_READABLE | VMM_USER_FLAG_WRITABLE |
                           VMM_USER_FLAG_USER;
    uintptr_t mapped = 0;
    const uintptr_t initial_size = VMM_EXEC_STACK_INITIAL_PAGES * PAGE_SIZE;
    const uintptr_t reserve_size = VMM_EXEC_STACK_MAX_PAGES * PAGE_SIZE;

    if (vmm_add_region(space, VMM_EXEC_STACK_RESERVE_BASE, reserve_size,
                       flags, VM_REGION_EXEC_STACK) != 0) {
        return -1;
    }
    while (mapped < initial_size) {
        void *page = pmm_alloc_page();

        if (!page) {
            break;
        }
        zero_page((uint64_t *)page);
        if (vmm_map_page(space, VMM_EXEC_STACK_INITIAL_BASE + mapped,
                         (uintptr_t)page, flags) != 0) {
            pmm_free_page(page);
            break;
        }
        mapped += PAGE_SIZE;
    }
    if (mapped == initial_size) {
        return 0;
    }
    while (mapped != 0u) {
        uintptr_t physical;

        mapped -= PAGE_SIZE;
        if (vmm_unmap_page(space, VMM_EXEC_STACK_INITIAL_BASE + mapped,
                           &physical) == 0) {
            pmm_free_page((void *)physical);
        }
    }
    clear_region(find_mutable_region(space, VMM_EXEC_STACK_RESERVE_BASE,
                                     reserve_size, VM_REGION_EXEC_STACK));
    return -1;
}

int vmm_grow_user_stack(struct address_space *space, uintptr_t fault_address,
                        uintptr_t stack_pointer, uint64_t error_code)
{
    const uint32_t flags = VMM_USER_FLAG_READABLE | VMM_USER_FLAG_WRITABLE |
                           VMM_USER_FLAG_USER;
    const uintptr_t reserve_size = VMM_EXEC_STACK_MAX_PAGES * PAGE_SIZE;
    uintptr_t page_address = fault_address & ~(PAGE_SIZE - 1u);
    uintptr_t existing;
    void *page;

    if (!vmm_user_stack_fault_valid(fault_address, stack_pointer, error_code) ||
        !vmm_find_region(space, VMM_EXEC_STACK_RESERVE_BASE, reserve_size,
                         VM_REGION_EXEC_STACK) ||
        vmm_query_page(space, page_address, &existing) == 0) {
        return -1;
    }
    page = pmm_alloc_page();
    if (!page) {
        return -1;
    }
    zero_page((uint64_t *)page);
    if (vmm_map_page(space, page_address, (uintptr_t)page, flags) != 0) {
        pmm_free_page(page);
        return -1;
    }
    return 0;
}

int vmm_unmap_region(struct address_space *space, uintptr_t start, uintptr_t size,
                     enum vm_region_type required_type)
{
    struct vm_region *region = find_mutable_region(space, start, size,
                                                    required_type);

    if (!region) {
        return -1;
    }
    for (uintptr_t offset = 0; offset < size; offset += PAGE_SIZE) {
        uintptr_t physical;

        if (vmm_query_page(space, start + offset, &physical) != 0) {
            return -1;
        }
    }
    for (uintptr_t offset = 0; offset < size; offset += PAGE_SIZE) {
        uintptr_t physical;

        if (vmm_unmap_page(space, start + offset, &physical) != 0) {
            return -1;
        }
        pmm_free_page((void *)physical);
    }
    clear_region(region);
    return 0;
}

int vmm_protect_region(struct address_space *space, uintptr_t start, uintptr_t size,
                       uint32_t flags, enum vm_region_type required_type)
{
    struct vm_region *region = find_mutable_region(space, start, size,
                                                    required_type);

    if (!region || ((flags & VMM_USER_FLAG_WRITABLE) != 0 &&
                    (flags & VMM_USER_FLAG_EXECUTABLE) != 0)) {
        return -1;
    }
    for (uintptr_t offset = 0; offset < size; offset += PAGE_SIZE) {
        uintptr_t physical;

        if (vmm_query_page(space, start + offset, &physical) != 0) {
            return -1;
        }
    }
    for (uintptr_t offset = 0; offset < size; offset += PAGE_SIZE) {
        if (vmm_protect_page(space, start + offset,
                             flags | VMM_USER_FLAG_USER) != 0) {
            return -1;
        }
    }
    region->flags = flags | VMM_USER_FLAG_USER;
    return 0;
}

uintptr_t vmm_find_free_range(const struct address_space *space, uintptr_t range_start,
                              uintptr_t range_end, uintptr_t size)
{
    uintptr_t candidate;

    if (!space || range_start >= range_end ||
        !valid_page_range(range_start, range_end - range_start,
                                     VMM_USER_MIN, VMM_USER_STACK_TOP) ||
        size == 0 || (size & (PAGE_SIZE - 1u)) != 0) {
        return 0;
    }
    for (candidate = range_start; candidate <= range_end - size;
         candidate += PAGE_SIZE) {
        int available = 1;

        for (size_t index = 0; index < VMM_MAX_REGIONS; index++) {
            const struct vm_region *region = &space->regions[index];

            if (region->type != VM_REGION_NONE &&
                ranges_overlap(candidate, size, region->start, region->size)) {
                available = 0;
                candidate = align_up(region->start + region->size) - PAGE_SIZE;
                break;
            }
        }
        if (available) {
            return candidate;
        }
    }
    return 0;
}

uintptr_t vmm_resize_heap(struct address_space *space, uintptr_t requested_break)
{
    uintptr_t old_break;
    uintptr_t old_end;
    uintptr_t new_end;
    struct vm_region *region = 0;

    if (!space) {
        return 0;
    }
    old_break = space->heap_break;
    if (requested_break == 0) {
        return old_break;
    }
    if (requested_break < VMM_HEAP_BASE || requested_break > VMM_HEAP_END) {
        return old_break;
    }
    old_end = align_up(old_break);
    new_end = align_up(requested_break);
    if (old_end != VMM_HEAP_BASE) {
        region = find_mutable_region(space, VMM_HEAP_BASE,
                                     old_end - VMM_HEAP_BASE, VM_REGION_HEAP);
        if (!region) {
            return old_break;
        }
    }
    if (new_end > old_end) {
        uintptr_t mapped = old_end;

        if (new_end > VMM_HEAP_END ||
            (region == 0 && vmm_add_region(space, VMM_HEAP_BASE,
                                           new_end - VMM_HEAP_BASE,
                                           VMM_USER_FLAG_READABLE |
                                           VMM_USER_FLAG_WRITABLE |
                                           VMM_USER_FLAG_USER,
                                           VM_REGION_HEAP) != 0)) {
            return old_break;
        }
        if (region) {
            region->size = new_end - VMM_HEAP_BASE;
        } else {
            region = find_mutable_region(space, VMM_HEAP_BASE,
                                         new_end - VMM_HEAP_BASE, VM_REGION_HEAP);
        }
        while (mapped < new_end) {
            void *page = pmm_alloc_page();

            if (!page) {
                break;
            }
            zero_page((uint64_t *)page);
            if (vmm_map_page(space, mapped, (uintptr_t)page,
                             VMM_USER_FLAG_READABLE | VMM_USER_FLAG_WRITABLE |
                             VMM_USER_FLAG_USER) != 0) {
                pmm_free_page(page);
                break;
            }
            mapped += PAGE_SIZE;
        }
        if (mapped != new_end) {
            while (mapped > old_end) {
                uintptr_t physical;

                mapped -= PAGE_SIZE;
                if (vmm_unmap_page(space, mapped, &physical) == 0) {
                    pmm_free_page((void *)physical);
                }
            }
            if (old_end == VMM_HEAP_BASE) {
                clear_region(region);
            } else {
                region->size = old_end - VMM_HEAP_BASE;
            }
            return old_break;
        }
    } else if (new_end < old_end) {
        for (uintptr_t address = new_end; address < old_end; address += PAGE_SIZE) {
            uintptr_t physical;

            if (vmm_unmap_page(space, address, &physical) == 0) {
                pmm_free_page((void *)physical);
            }
        }
        if (new_end == VMM_HEAP_BASE) {
            clear_region(region);
        } else {
            region->size = new_end - VMM_HEAP_BASE;
        }
    }
    space->heap_break = requested_break;
    return requested_break;
}

static void destroy_user_page_tables(struct address_space *space)
{
    uint64_t *pml4;
    uint64_t *pdpt;

    if (!space || !space->pml4_physical) {
        return;
    }
    pml4 = (uint64_t *)space->pml4_physical;
    if ((pml4[0] & PAGE_PRESENT) != 0) {
        pdpt = (uint64_t *)(uintptr_t)(pml4[0] & PAGE_ADDR_MASK);
        for (size_t pdpt_slot = 1; pdpt_slot < 512; pdpt_slot++) {
            uint64_t *pd;

            if ((pdpt[pdpt_slot] & PAGE_PRESENT) == 0) {
                continue;
            }
            pd = (uint64_t *)(uintptr_t)(pdpt[pdpt_slot] & PAGE_ADDR_MASK);
            for (size_t pd_slot = 0; pd_slot < 512; pd_slot++) {
                uint64_t *pt;

                if ((pd[pd_slot] & PAGE_PRESENT) == 0) {
                    continue;
                }
                if ((pd[pd_slot] & PAGE_HUGE) != 0) {
                    continue;
                }
                pt = (uint64_t *)(uintptr_t)(pd[pd_slot] & PAGE_ADDR_MASK);
                for (size_t pt_slot = 0; pt_slot < 512; pt_slot++) {
                    if ((pt[pt_slot] & PAGE_PRESENT) != 0) {
                        pmm_free_page((void *)(uintptr_t)(pt[pt_slot] & PAGE_ADDR_MASK));
                    }
                }
                pmm_free_page(pt);
            }
            pmm_free_page(pd);
        }
        pmm_free_page(pdpt);
    }
    pmm_free_page(pml4);
}

void vmm_destroy_user_address_space(struct address_space *space)
{
    if (!space || !space->pml4_physical ||
        space->pml4_physical == kernel_address_space.pml4_physical) {
        return;
    }
    destroy_user_page_tables(space);
    space->pml4_physical = 0;
    space->heap_break = VMM_HEAP_BASE;
    for (size_t index = 0; index < VMM_MAX_REGIONS; index++) {
        clear_region(&space->regions[index]);
    }
}

int vmm_clone_user_address_space(const struct address_space *source,
                                 struct address_space *destination)
{
    if (!source || !destination ||
        vmm_create_user_address_space(destination) != 0) {
        return -1;
    }
    for (size_t index = 0; index < VMM_MAX_REGIONS; index++) {
        const struct vm_region *region = &source->regions[index];

        if (region->type == VM_REGION_NONE) {
            continue;
        }
        if (vmm_add_region(destination, region->start, region->size,
                           region->flags, (enum vm_region_type)region->type) != 0) {
            vmm_destroy_user_address_space(destination);
            return -1;
        }
        for (uintptr_t offset = 0; offset < region->size; offset += PAGE_SIZE) {
            uintptr_t source_physical;
            uint32_t page_flags;
            void *page;

            if (vmm_query_page(source, region->start + offset,
                               &source_physical) != 0) {
                continue;
            }
            if (vmm_query_page_flags(source, region->start + offset,
                                     &page_flags) != 0) {
                vmm_destroy_user_address_space(destination);
                return -1;
            }
            page = pmm_alloc_page();
            if (!page) {
                vmm_destroy_user_address_space(destination);
                return -1;
            }
            for (size_t byte = 0; byte < PAGE_SIZE; byte++) {
                ((uint8_t *)page)[byte] = ((const uint8_t *)source_physical)[byte];
            }
            if (vmm_map_page(destination, region->start + offset,
                             (uintptr_t)page, page_flags) != 0) {
                pmm_free_page(page);
                vmm_destroy_user_address_space(destination);
                return -1;
            }
        }
    }
    destination->heap_break = source->heap_break;
    return 0;
}

static void write_cr3(uintptr_t value)
{
    __asm__ volatile ("mov %0, %%cr3" : : "r"(value) : "memory");
}

static void test_direct_map(void)
{
    uint64_t *identity_page = pmm_alloc_page();
    uint64_t *direct_page;

    if (identity_page == 0) {
        log_error("vmm: direct-map self-test skipped");
        return;
    }

    direct_page = (uint64_t *)vmm_kernel_direct_map((uintptr_t)identity_page);
    identity_page[0] = 0x41756b4f53564d4dull;

    if (direct_page[0] == identity_page[0]) {
        log_info("vmm: higher-half direct-map self-test passed");
    } else {
        log_error("vmm: higher-half direct-map self-test failed");
    }

    pmm_free_page(identity_page);
}

static void test_user_address_space(void)
{
    struct address_space user_space;
    void *physical_page = pmm_alloc_page();
    uintptr_t queried_physical;
    uint32_t queried_flags;
    uint64_t expected_pages = pmm_available_page_count() + (physical_page ? 1u : 0u);

    if (physical_page == 0) {
        log_error("vmm: user address-space self-test skipped");
        return;
    }

    if (vmm_create_user_address_space(&user_space) != 0) {
        log_error("vmm: failed to create user address space");
        pmm_free_page(physical_page);
        return;
    }

    if (vmm_map_page(&user_space, USER_SELFTEST_VA, (uintptr_t)physical_page, VMM_USER_FLAG_WRITABLE | VMM_USER_FLAG_USER) != 0) {
        log_error("vmm: failed to map user page");
        pmm_free_page(physical_page);
        vmm_destroy_user_address_space(&user_space);
        return;
    }

    if (vmm_query_page(&user_space, USER_SELFTEST_VA, &queried_physical) == 0 &&
        queried_physical == (uintptr_t)physical_page &&
        vmm_protect_page(&user_space, USER_SELFTEST_VA,
                         VMM_USER_FLAG_READABLE | VMM_USER_FLAG_USER) == 0 &&
        vmm_query_page_flags(&user_space, USER_SELFTEST_VA, &queried_flags) == 0 &&
        (queried_flags & (VMM_USER_FLAG_WRITABLE | VMM_USER_FLAG_EXECUTABLE)) == 0) {
        vmm_destroy_user_address_space(&user_space);
        if (pmm_available_page_count() == expected_pages) {
            log_info("vmm: user map/protect/destroy lifecycle self-test passed");
        } else {
            log_error("vmm: user address-space lifecycle leaked pages");
        }
    } else {
        log_error("vmm: user address-space self-test failed");
        vmm_destroy_user_address_space(&user_space);
    }
}

static void test_user_stack_growth(void)
{
    struct address_space source;
    struct address_space clone;
    const uintptr_t first_growth = VMM_EXEC_STACK_INITIAL_BASE - PAGE_SIZE;
    const uintptr_t failed_growth = first_growth - PAGE_SIZE;
    uintptr_t physical;
    uint64_t baseline = pmm_available_page_count();
    uint64_t failure_baseline;
    int clone_created = 0;
    int passed = 1;

    if (vmm_create_user_address_space(&source) != 0) {
        log_error("vmm: growable stack self-test could not create address space");
        return;
    }

    failure_baseline = pmm_available_page_count();
    pmm_test_fail_after(0u);
    if (vmm_map_user_stack(&source) == 0 ||
        vmm_find_region(&source, VMM_EXEC_STACK_RESERVE_BASE,
                        VMM_EXEC_STACK_MAX_PAGES * PAGE_SIZE,
                        VM_REGION_EXEC_STACK) != 0 ||
        pmm_available_page_count() != failure_baseline) {
        passed = 0;
    }
    pmm_test_fail_after(UINT64_MAX);

    if (!passed || vmm_map_user_stack(&source) != 0 ||
        vmm_query_page(&source, VMM_EXEC_STACK_INITIAL_BASE, &physical) != 0 ||
        vmm_query_page(&source, first_growth, &physical) == 0 ||
        vmm_grow_user_stack(&source, first_growth + 32u,
                            first_growth + 64u, 0x6u) != 0 ||
        vmm_query_page(&source, first_growth, &physical) != 0) {
        passed = 0;
    }

    failure_baseline = pmm_available_page_count();
    pmm_test_fail_after(0u);
    if (vmm_grow_user_stack(&source, failed_growth + 32u,
                            failed_growth + 64u, 0x6u) == 0 ||
        vmm_query_page(&source, failed_growth, &physical) == 0 ||
        pmm_available_page_count() != failure_baseline) {
        passed = 0;
    }
    pmm_test_fail_after(UINT64_MAX);

    if (vmm_clone_user_address_space(&source, &clone) != 0) {
        passed = 0;
    } else {
        clone_created = 1;
        if (vmm_query_page(&clone, first_growth, &physical) != 0 ||
            vmm_query_page(&clone, failed_growth, &physical) == 0) {
            passed = 0;
        }
    }

    if (vmm_user_stack_fault_valid(VMM_EXEC_STACK_GUARD_BASE,
                                   VMM_EXEC_STACK_GUARD_BASE, 0x6u) ||
        vmm_user_stack_fault_valid(first_growth, first_growth, 0x7u) ||
        vmm_user_stack_fault_valid(first_growth, first_growth, 0x4u | 0x10u)) {
        passed = 0;
    }

    if (clone_created) {
        vmm_destroy_user_address_space(&clone);
    }
    vmm_destroy_user_address_space(&source);
    if (pmm_available_page_count() != baseline) {
        passed = 0;
    }

    if (passed) {
        log_info("vmm: growable stack/clone/failure rollback self-test passed");
    } else {
        log_error("vmm: growable stack/clone/failure rollback self-test failed");
    }
}

uintptr_t vmm_kernel_direct_map(uintptr_t physical_address)
{
    return VMM_KERNEL_DIRECT_BASE + physical_address;
}

void vmm_init(void)
{
    uint64_t *pml4 = alloc_page_table();

    write_msr(MSR_EFER, read_msr(MSR_EFER) | EFER_NXE);

    if (pml4 == 0) {
        log_error("vmm: failed to allocate kernel pml4");
        return;
    }

    if (map_range_2m(pml4, 0, 0, LOW_MEMORY_MAP_SIZE, PAGE_WRITABLE) != 0) {
        log_error("vmm: failed to map identity low memory");
        return;
    }

    if (map_range_2m(pml4, VMM_KERNEL_DIRECT_BASE, 0, LOW_MEMORY_MAP_SIZE, PAGE_WRITABLE) != 0) {
        log_error("vmm: failed to map higher-half direct window");
        return;
    }

    kernel_address_space.pml4_physical = (uintptr_t)pml4;
    write_cr3(kernel_address_space.pml4_physical);

    serial_write("[INFO] vmm: kernel pml4=");
    write_hex64(kernel_address_space.pml4_physical);
    serial_write("\r\n");
    log_info("vmm: identity and higher-half direct map initialized");
    test_direct_map();
    test_user_address_space();
    test_user_stack_growth();
}

const struct address_space *vmm_kernel_address_space(void)
{
    return &kernel_address_space;
}
