#include "include/aukos/vmm.h"

#define PAGE_FAULT_PRESENT 0x1u
#define PAGE_FAULT_USER 0x4u
#define PAGE_FAULT_INSTRUCTION 0x10u

int vmm_user_stack_fault_valid(uintptr_t fault_address,
                               uintptr_t stack_pointer, uint64_t error_code)
{
    uintptr_t distance;

    if ((error_code & (PAGE_FAULT_PRESENT | PAGE_FAULT_USER |
                       PAGE_FAULT_INSTRUCTION)) != PAGE_FAULT_USER ||
        fault_address < VMM_EXEC_STACK_RESERVE_BASE ||
        fault_address >= VMM_USER_STACK_TOP ||
        stack_pointer < VMM_EXEC_STACK_RESERVE_BASE ||
        stack_pointer > VMM_USER_STACK_TOP) {
        return 0;
    }

    distance = fault_address < stack_pointer
                   ? stack_pointer - fault_address
                   : fault_address - stack_pointer;
    return distance <= VMM_STACK_FAULT_WINDOW;
}
