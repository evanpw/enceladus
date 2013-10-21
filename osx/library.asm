bits 64
section .text
extern _printf, _scanf, _malloc
extern __main
global _main, __read, __print

_main:
    ; The main function from the compiled program
    call __main

    xor eax, eax
    ret

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

; Print the number stored in rax, followed by a newline
__print:
    ; Realign stack to 16 bytes
    mov rbx, rsp
    and rsp, -16
    add rsp, -8
    push rbx

    mov rsi, rax
    lea rdi, [rel __format]
    xor rax, rax
    call _printf

    pop rbx
    mov rsp, rbx

    ret

; Create a list with head = rdi (an Int), tail = rsi (a pointer).
; returns the pointer to the new cons cell in rax
__cons:
    push rbp
    mov rbp, rsp

    push rdi
    push rsi

    ; Realign stack to 16 bytes
    mov rbx, rsp
    and rsp, -16
    add rsp, -8
    push rbx

    mov rdi, 16
    call _malloc

    ; Unalign
    pop rbx
    mov rsp, rbx

    pop rsi
    pop rdi
    mov qword [rax], rdi
    mov qword [rax + 8], rsi

    mov rsp, rbp
    ret


section .data
__format: db "%ld", 0xA, 0
__read_format: db "%ld", 0
__read_result: dq 0
