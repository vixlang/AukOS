BITS 32

MB2_MAGIC equ 0xe85250d6 ;;magic num
MB2_ARCH_I386 equ 0
MB2_HEADER_LENGTH equ multiboot_header_end - multiboot_header
MB2_CHECKSUM equ -(MB2_MAGIC + MB2_ARCH_I386 + MB2_HEADER_LENGTH)

PRESENT equ 1 << 0
WRITABLE equ 1 << 1
HUGE_PAGE equ 1 << 7

section .multiboot
align 8
multiboot_header:
    dd MB2_MAGIC
    dd MB2_ARCH_I386 ;; start from 32位保护
    dd MB2_HEADER_LENGTH
    dd MB2_CHECKSUM
    dw 5
    dw 0
    dd 20
    dd 1024
    dd 768
    dd 32
    align 8
    dw 0
    dw 0
    dd 8
multiboot_header_end:

section .text
global _start
extern kernel_main

_start:
    cli
    mov [multiboot_magic], eax
    mov [multiboot_info], ebx
    mov esp, stack_top

    lgdt [gdt64_pointer]

    call setup_page_tables

    mov eax, pml4_table
    mov cr3, eax

    mov eax, cr4
    or eax, (1 << 5) | (1 << 9) | (1 << 10)
    mov cr4, eax

    mov ecx, 0xc0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, cr0
    and eax, ~(1 << 2)
    or eax, 1 << 1
    or eax, 1 << 31
    mov cr0, eax

    jmp gdt64.code:long_mode_start

BITS 64
long_mode_start:
    mov ax, gdt64.data
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rsp, stack_top
    mov rbp, 0
    mov edi, [rel multiboot_magic]
    mov esi, [rel multiboot_info]
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
global gdt64_tss
gdt64_tss:
    dq 0
    dq 0
gdt64_pointer:
    dw $ - gdt64 - 1
    dq gdt64

section .bss
align 4096
pml4_table:
    resq 512
pdpt_table:
    resq 512
pd_tables:
    resq 512 * 4
stack_bottom:
    resb 16384
stack_top:
multiboot_magic:
    resd 1
multiboot_info:
    resd 1

section .text
BITS 32
setup_page_tables:
    mov eax, pdpt_table
    or eax, PRESENT | WRITABLE
    mov [pml4_table], eax

    mov ecx, 0
.map_pdpt:
    mov eax, ecx
    shl eax, 12
    add eax, pd_tables
    or eax, PRESENT | WRITABLE
    mov [pdpt_table + ecx * 8], eax
    mov dword [pdpt_table + ecx * 8 + 4], 0
    inc ecx
    cmp ecx, 4
    jne .map_pdpt

    mov ecx, 0
.map_pd:
    mov eax, ecx
    shl eax, 21
    or eax, PRESENT | WRITABLE | HUGE_PAGE
    mov [pd_tables + ecx * 8], eax
    mov dword [pd_tables + ecx * 8 + 4], 0
    inc ecx
    cmp ecx, 2048
    jne .map_pd
    ret
