bits 64
section .text
extern _scanf
global __read

__read:
    ; Realign stack to 16 bytes
    mov rbx, rsp
    and rsp, -16
    add rsp, -8
    push rbx

    lea rsi, [rel __read_result]
    lea rdi, [rel __read_format]
    xor rax, rax
    call _scanf

    pop rbx
    mov rsp, rbx

    mov rax, [rel __read_result]
    ret

section .data
__read_format: db "%ld", 0
__read_result: dq 0
