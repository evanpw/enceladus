bits 64
section .text
extern printf, scanf, malloc, puts, exit
extern __main
global main, __read, __print, __cons, __die

main:
    ; The main function from the compiled program
    call __main

    xor rax, rax
    ret

__die:
    ;; Print error message based on value of rax

    mov rdi, [__error_messages + 8 * rax]
    call puts

    ; Kill the process
    mov rdi, 1
    call exit

    ; Useless
    ret

__read:
    lea rsi, [rel __read_result]
    lea rdi, [rel __read_format]
    xor rax, rax
    call scanf

    mov rax, [rel __read_result]
    ret

; Print the number stored in rax, followed by a newline
__print:
    mov rsi, rax
    lea rdi, [rel __format]
    xor rax, rax
    call printf

    ret

; Create a list with head = rdi (an Int), tail = rsi (a pointer).
; returns the pointer to the new cons cell in rax
__cons:
    push rbp
    mov rbp, rsp

    push rdi
    push rsi

    mov rdi, 16
    call malloc

    pop rsi
    pop rdi
    mov qword [rax], rdi
    mov qword [rax + 8], rsi

    mov rsp, rbp
    pop rbp
    ret

section .data
__format: db "%ld", 0xA, 0
__read_format: db "%ld", 0
__read_result: dq 0

; Array of error messages for __die
__error_messages: dq __error_head_empty
__error_head_empty: db "*** Exception: Called head on empty list", 0
