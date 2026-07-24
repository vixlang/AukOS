BITS 64

section .text
global _start
global gdt64_tss
extern kernel_main

_start:
    cli
    lgdt [rel gdt64_pointer]

    push qword 0x08
    lea rax, [rel .reload_cs]
    push rax
    retfq

.reload_cs:

    mov rax, cr0
    and rax, ~(1 << 2)
    or rax, 1 << 1
    mov cr0, rax

    mov rax, cr4
    or rax, (1 << 9) | (1 << 10)
    mov cr4, rax

    mov ax, gdt64.data
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rsp, stack_top
    mov rbp, 0
    call kernel_main

.halt:
    hlt
    jmp .halt

section .rodata
align 8
gdt64:
    dq 0
.code: equ $ - gdt64
    dq 0x00209a0000000000
.data: equ $ - gdt64
    dq 0x0000920000000000
.user_data: equ $ - gdt64
    dq 0x0000f20000000000
.user_code: equ $ - gdt64
    dq 0x0020fa0000000000
.tss: equ $ - gdt64
gdt64_tss:
    dq 0
    dq 0
gdt64_pointer:
    dw $ - gdt64 - 1
    dq gdt64

section .bss
align 4096
stack_bottom:
    resb 16384
stack_top:
