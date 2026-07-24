BITS 64

section .bss
align 16
x86_64_resume_stack:
    resb 4096
x86_64_resume_stack_top:
align 16
x86_64_syscall_stack:
    resb 16384
x86_64_syscall_stack_top:
syscall_saved_user_rsp:
    resq 1
syscall_saved_r12:
    resq 1

section .text

USER_DATA_SELECTOR equ 0x18 | 3
USER_CODE_SELECTOR equ 0x20 | 3

global x86_64_resume_user
global x86_64_switch_and_resume
global x86_64_switch_exec
global x86_64_switch_exit
global x86_64_syscall_entry

extern syscall_dispatch
extern syscall_exec_after_switch
extern scheduler_exit_after_switch
extern syscall_next_cr3

; void x86_64_switch_exec(uintptr_t cr3)
x86_64_switch_exec:
    cli
    mov cr3, rdi
    lea rsp, [rel x86_64_resume_stack_top]
    call syscall_exec_after_switch
    ud2

; void x86_64_switch_exit(uintptr_t cr3, struct process *exited)
x86_64_switch_exit:
    cli
    mov cr3, rdi
    mov rdi, rsi
    lea rsp, [rel x86_64_resume_stack_top]
    call scheduler_exit_after_switch
    ud2

; void x86_64_switch_and_resume(uintptr_t cr3, struct user_context *ctx)
x86_64_switch_and_resume:
    cli
    mov cr3, rdi
    mov rdi, rsi
    lea rsp, [rel x86_64_resume_stack_top]
    jmp x86_64_resume_user

; void x86_64_resume_user(struct user_context *ctx)
x86_64_resume_user:
    cli

    ; Build iretq frame: SS, RSP, RFLAGS, CS, RIP
    push qword [rdi + 19*8]         ; SS
    push qword [rdi + 18*8]         ; RSP
    push qword [rdi + 17*8]         ; RFLAGS
    push qword [rdi + 16*8]         ; CS
    push qword [rdi + 15*8]         ; RIP

    ; Load user data segments
    mov ax, USER_DATA_SELECTOR
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Restore GPRs (rdi last)
    mov rax, [rdi + 14*8]
    mov rbx, [rdi + 13*8]
    mov rcx, [rdi + 12*8]
    mov rdx, [rdi + 11*8]
    mov rbp, [rdi + 10*8]
    mov rsi, [rdi + 9*8]
    mov r8,  [rdi + 7*8]
    mov r9,  [rdi + 6*8]
    mov r10, [rdi + 5*8]
    mov r11, [rdi + 4*8]
    mov r12, [rdi + 3*8]
    mov r13, [rdi + 2*8]
    mov r14, [rdi + 1*8]
    mov r15, [rdi + 0*8]
    mov rdi, [rdi + 8*8]

    iretq

; x86_64_syscall_entry
x86_64_syscall_entry:
    swapgs

    ; SYSCALL does not switch RSP. Preserve the user values in single-core
    ; scratch storage, then leave the sparsely mapped user stack immediately.
    mov [rel syscall_saved_user_rsp], rsp
    mov [rel syscall_saved_r12], r12
    lea rsp, [rel x86_64_syscall_stack_top]

    ; Build iretq frame
    push qword USER_DATA_SELECTOR   ; SS
    push qword [rel syscall_saved_user_rsp]
    push r11                        ; user RFLAGS
    push qword USER_CODE_SELECTOR   ; CS
    push rcx                        ; user RIP

    ; Save all GPRs
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push qword [rel syscall_saved_r12]
    push r13
    push r14
    push r15

    ; Pass rsp as argument to syscall_dispatch
    mov rdi, rsp
    call syscall_dispatch

    ; Restore all GPRs
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Check if we need to switch CR3
    ; Use RIP-relative addressing to access syscall_next_cr3
    push rax
    lea rax, [rel syscall_next_cr3]
    mov rax, [rax]
    test rax, rax
    jz .no_cr3_switch
    mov cr3, rax
    lea rax, [rel syscall_next_cr3]
    mov qword [rax], 0
.no_cr3_switch:
    pop rax

    ; iretq frame on stack: RIP, CS, RFLAGS, RSP, SS
    iretq
