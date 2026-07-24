BITS 64

USER_DATA_SELECTOR equ 0x18 | 3
USER_CODE_SELECTOR equ 0x20 | 3

section .text
global x86_64_enter_user

; void x86_64_enter_user(uintptr_t entry, uintptr_t stack_top, uintptr_t pml4,
;                        uintptr_t argc, uintptr_t argv, uintptr_t envp)
x86_64_enter_user:
    cli
    mov cr3, rdx

    mov r10, rdi
    mov r11, rsi
    mov rdi, rcx
    mov rsi, r8
    mov rdx, r9

    mov ax, USER_DATA_SELECTOR
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push qword USER_DATA_SELECTOR
    push r11
    push qword 0x202
    push qword USER_CODE_SELECTOR
    push r10
    iretq

.halt:
    hlt
    jmp .halt
