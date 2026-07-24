#include <aukos/vmm.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    const uintptr_t first_growth = VMM_EXEC_STACK_INITIAL_BASE - 4096u;

    assert(VMM_EXEC_STACK_INITIAL_BASE > VMM_EXEC_STACK_RESERVE_BASE);
    assert(VMM_EXEC_STACK_GUARD_BASE + 4096u ==
           VMM_EXEC_STACK_RESERVE_BASE);
    assert(VMM_SIGNAL_STACK_TOP == VMM_EXEC_STACK_GUARD_BASE);
    assert(VMM_SIGNAL_STACK_GUARD_BASE + 4096u == VMM_SIGNAL_STACK_BASE);
    assert(VMM_MMAP_END == VMM_SIGNAL_STACK_GUARD_BASE);

    assert(vmm_user_stack_fault_valid(first_growth + 8u,
                                      first_growth + 16u, 0x6u));
    assert(vmm_user_stack_fault_valid(first_growth + 8u,
                                      first_growth + 4096u, 0x4u));
    assert(!vmm_user_stack_fault_valid(VMM_EXEC_STACK_GUARD_BASE,
                                       VMM_EXEC_STACK_GUARD_BASE, 0x6u));
    assert(!vmm_user_stack_fault_valid(VMM_USER_STACK_TOP,
                                       VMM_USER_STACK_TOP, 0x6u));
    assert(!vmm_user_stack_fault_valid(first_growth, first_growth, 0x7u));
    assert(!vmm_user_stack_fault_valid(first_growth, first_growth, 0x2u));
    assert(!vmm_user_stack_fault_valid(first_growth, first_growth,
                                       0x4u | 0x10u));
    assert(!vmm_user_stack_fault_valid(first_growth,
                                       first_growth - VMM_STACK_FAULT_WINDOW - 1u,
                                       0x6u));

    puts("vmm_stack_test: PASS");
    return 0;
}
