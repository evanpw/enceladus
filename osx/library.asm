bits 64
section .text
extern _printf, _scanf, _malloc, _puts, _exit, _free
extern __main
global _main, __read, _print, __cons, __die, __incref, __decref, __decref_no_free

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

    ; Kill the process
    mov rdi, 1
    call _exit

    ; Fix stack
    pop rbx
    mov rsp, rbx

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

; Increment the reference count of the cons cell at rdi
__incref:
    cmp rdi, 0
    je __end_incref

    add qword [rdi - 8], 1

__end_incref:
    ret

; Decrement the reference count of the cons cell at rdi, and if it is zero,
; then deallocate it
__decref:
    cmp rdi, 0
    je __end_decref

    ; Get to the reference count, and the actuall address returned by malloc
    add rdi, -8

    add qword [rdi], -1

    cmp qword [rdi], 0
    jl __negref
    jne __end_decref

    ; If we reach here, the ref count is zero. Deallocate this cons cell, and
    ; then decrement the reference of the next (tail-recursively)
    mov rsi, qword [rdi + 16]
    push rsi

    ; Realign stack to 16 bytes
    mov rbx, rsp
    and rsp, -16
    add rsp, -8
    push rbx

    call _free

    pop rbx
    mov rsp, rbx

    pop rdi
    jmp __decref

__negref:
    ; Reference count should never be negative. We've screwed something up
    mov rax, 2
    call __die

__end_decref:
    ret

; Decrement the reference count of the cons cell at rdi, but do not free it if
; it reaches zero. This is used for temporary values (like return values of
; functions, which will be assigned a reference later.)
__decref_no_free:
    cmp rdi, 0
    je __end_decref_no_free

    ; Get to the reference count, and the actuall address returned by malloc
    add rdi, -8

    add qword [rdi], -1

    cmp qword [rdi], 0
    jge __end_decref_no_free

    ; Reference count should never be negative. We've screwed something up
    mov rax, 2
    call __die

__end_decref_no_free:
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
    add rax, 8 ; Return a pointer to the actual cell, not the ref count

    ; This cons cell has a reference to the next one
    push rax
    mov rdi, rsi
    call __incref
    pop rax

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
