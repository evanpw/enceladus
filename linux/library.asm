bits 64
section .text
extern printf, scanf, malloc, puts, exit
extern __main
global main, __read, __print, __cons, __die, __incref, __decref

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

; Increment the reference count of the cons cell at rdi
__incref:
    cmp rdi, 0
    je __end_incref

    add qword [rdi - 8], 1

__end_incref:
    ret

; Decrement the reference count of the cons cell at rdi, and if it is zero,
; then deallocate it (TODO)
__decref:
    cmp rdi, 0
    je __end_decref

    add qword [rdi - 8], -1

    cmp qword [rdi - 8], 0
    jge __end_decref

    ; Reference count should never be negative. We've screwed something up
    mov rax, 2
    call __die

__end_decref:
    ret

; Create a list with head = rdi (an Int), tail = rsi (a pointer).
; returns the pointer to the new cons cell in rax
__cons:
    push rbp
    mov rbp, rsp

    push rdi
    push rsi

    mov rdi, 24
    call malloc

    pop rsi
    pop rdi

    mov qword [rax], 0
    mov qword [rax + 8], rdi
    mov qword [rax + 16], rsi
    add rax, 8 ; Return a pointer to the actual cell, not the ref count

    ; This cons cell has a reference to the next one
    mov rdi, rsi
    call __incref

    mov rsp, rbp
    pop rbp
    ret

section .data
__format: db "%ld", 0xA, 0
__read_format: db "%ld", 0
__read_result: dq 0

; Array of error messages for __die
__error_messages:
    dq __error_head_empty
    dq __error_tail_empty
    dq __error_negative_refs

__error_head_empty: db "*** Exception: Called head on empty list", 0
__error_tail_empty: db "*** Exception: Called tail on empty list", 0
__error_negative_refs: db "*** Exception: Reference count is negative", 0
