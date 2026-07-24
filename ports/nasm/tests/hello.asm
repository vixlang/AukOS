default rel
global main
extern aukos_vix_write_str

section .rodata
message: db "[nasm_generated_exec_test] PASS", 10, 0

section .text
main:
    push rbp
    mov rbp, rsp
    lea rdi, [rel message]
    call aukos_vix_write_str wrt ..plt
    xor eax, eax
    pop rbp
    ret
