bits 64
section .text
extern _printf, _scanf, _malloc, _puts, _exit
extern __main
global _main, __read, __print, __cons, __die, __incref, __decref

_main:
    ; The main function from the compiled program
    call __main

    xor rax, rax
    ret

__die:
    ;; Print error message based on value of rax

    ; Realign stack to 16 bytes
    mov rbx, rsp
    and rsp, -16
    add rsp, -8
    push rbx

    lea rdi, [rel __error_messages]
    mov rdi, [rdi + 8 * rax]
    call _puts

    ; Fix stack
    pop rbx
    mov rsp, rbx

    ; Kill the process
    mov rdi, 1
    call _exit

    ; Useless
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

__end_decref:
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

    mov rdi, 24
    call _malloc

    ; Unalign
    pop rbx
    mov rsp, rbx

    pop rsi
    pop rdi

    mov qword [rax], 0
    mov qword [rax + 8], rdi
    mov qword [rax + 16], rsi

    add rax, 8 ; Skip the reference count

    mov rsp, rbp
    pop rbp
    ret

section .data
__format: db "%ld", 0xA, 0
__read_format: db "%ld", 0
__read_result: dq 0

; Array of error messages for __die
__error_messages: dq __error_head_empty, __error_tail_empty
__error_head_empty: db "*** Exception: Called head on empty list", 0
__error_tail_empty: db "*** Exception: Called tail on empty list", 0
