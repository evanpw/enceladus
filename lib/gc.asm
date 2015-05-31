bits 64
section .text
global main, _main, stackBottom, gcAllocate, gcAllocateFromC, ccall
global splcall0, splcall1, splcall2, splcall3, splcall4, splcall5
global addRoot, removeRoots
extern gcCollect, __globalVarTable, try_mymalloc, mymalloc, _die, die, _Z4main
extern _malloc, _free

main:
_main:
    push rbp
    mov rbp, rsp

    ; Allocate a stack for C code
    mov rdi, 0x400000
    call _malloc
    mov qword [rel cstack], rax
    mov qword [rel cstackbase], rax

    ; Initialize the GC
    mov qword [rel stackBottom], rbp

    ; The actual program
    call _Z4main

    mov rdi, qword [rel cstackbase]
    call _free

    ; If we reach here without calling die, then exit code = 0
    xor rax, rax
    leave
    ret


addRoot:
    push rbp
    mov rbp, rsp

    ; rdi = array
    ; rsi = root
    mov qword [rdi + 0], 1
    mov qword [rdi + 8], rsi
    mov rax, qword [rel additionalRoots]
    mov qword [rdi + 16], rax
    mov qword [rel additionalRoots], rdi

    leave
    ret


removeRoots:
    push rbp
    mov rbp, rsp

    ; rdi = array
    mov rcx, qword [rdi + 0]    ; Array length
    inc rcx                     ; Add one to skip the length word
    lea rdi, [rdi + 8 * rcx]    ; Jump to the last entry
    mov rdi, qword [rdi]        ; Get the previous additionalRoots values
    mov qword [rel additionalRoots], rdi

    leave
    ret


ccall:
    push rbp
    mov rbp, rsp

    mov qword [rel splstack], rsp
    mov rsp, qword [rel cstack]

    call rax

    mov rsp, qword [rel splstack]

    leave
    ret


splcall0:
    push rbp
    mov rbp, rsp

    ; Callee-save registers
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rdi

    mov qword [rel cstack], rsp
    mov rsp, qword [rel splstack]

    call rdi

    mov qword [rel splstack], rsp
    mov rsp, qword [rel cstack]

    ; Callee-save registers
    pop rdi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx

    leave
    ret


splcall1:
    push rbp
    mov rbp, rsp

    ; Callee-save registers
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rdi ; For alignment

    mov qword [rel cstack], rsp
    mov rsp, qword [rel splstack]

    ; Shift all parameters over by one and call
    mov rax, rdi
    mov rdi, rsi
    call rax

    mov qword [rel splstack], rsp
    mov rsp, qword [rel cstack]

    ; Callee-save registers
    pop rdi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx

    leave
    ret


splcall2:
    push rbp
    mov rbp, rsp

    ; Callee-save registers
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rdi ; For alignment

    mov qword [rel cstack], rsp
    mov rsp, qword [rel splstack]

    ; Shift all parameters over by one and call
    mov rax, rdi
    mov rdi, rsi
    mov rsi, rdx
    call rax

    mov qword [rel splstack], rsp
    mov rsp, qword [rel cstack]

    ; Callee-save registers
    pop rdi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx

    leave
    ret


splcall3:
    push rbp
    mov rbp, rsp

    ; Callee-save registers
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rdi ; For alignment

    mov qword [rel cstack], rsp
    mov rsp, qword [rel splstack]

    ; Shift all parameters over by one and call
    mov rax, rdi
    mov rdi, rsi
    mov rsi, rdx
    mov rdx, rcx
    call rax

    mov qword [rel splstack], rsp
    mov rsp, qword [rel cstack]

    ; Callee-save registers
    pop rdi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx

    leave
    ret


splcall4:
    push rbp
    mov rbp, rsp

    ; Callee-save registers
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rdi ; For alignment

    mov qword [rel cstack], rsp
    mov rsp, qword [rel splstack]

    ; Shift all parameters over by one and call
    mov rax, rdi
    mov rdi, rsi
    mov rsi, rdx
    mov rdx, rcx
    mov rcx, r8
    call rax

    mov qword [rel splstack], rsp
    mov rsp, qword [rel cstack]

    ; Callee-save registers
    pop rdi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx

    leave
    ret


splcall5:
    push rbp
    mov rbp, rsp

    ; Callee-save registers
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rdi ; For alignment

    mov qword [rel cstack], rsp
    mov rsp, qword [rel splstack]

    ; Shift all parameters over by one and call
    mov rax, rdi
    mov rdi, rsi
    mov rsi, rdx
    mov rdx, rcx
    mov rcx, r8
    mov r8, r9
    call rax

    mov qword [rel splstack], rsp
    mov rsp, qword [rel cstack]

    ; Callee-save registers
    pop rdi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx

    leave
    ret


gcAllocate:
    push rbp
    mov rbp, rsp

    ; Save size argument and callee-save register rbx
    push rdi
    push rbx

    ; Try to allocate from the free list
    call try_mymalloc
    test rax, rax
    jnz .finish

    ; If not successful, collect garbage
    mov rdi, rbp                            ; Frame pointer of gcAllocate
    mov rsi, qword [rel stackBottom]        ; Frame pointer at beginning of execution
    mov rdx, qword [rel additionalRoots]    ; List of additional roots: globals and C-allocated heap vars
    call gcCollect

    ; Try again, allowing the allocator to request more memory from the OS
    mov rdi, qword [rbp - 8]
    call mymalloc
    test rax, rax
    jnz .finish

    ; If all of this fails, the we're out of memory
    mov rdi, outOfMemoryMessage
%ifdef __APPLE__
    call _die
%else
    call die
%endif

.finish:
    leave
    ret


gcAllocateFromC:
    push rbp
    mov rbp, rsp

    ; Save size argument and callee-save register rbx
    push rdi
    push rbx

    ; Try to allocate from the free list
    call try_mymalloc
    test rax, rax
    jnz .finish

    ; If not successful, collect garbage
    mov rdi, qword [rel splstack]           ; Frame pointer at last Simple->C transition
    mov rsi, qword [rel stackBottom]        ; Frame pointer at beginning of execution
    mov rdx, qword [rel additionalRoots]    ; List of additional roots: globals and C-allocated heap vars
    call gcCollect

    ; Try again, allowing the allocator to request more memory from the OS
    mov rdi, qword [rbp - 8]
    call mymalloc
    test rax, rax
    jnz .finish

    ; If all of this fails, the we're out of memory
    mov rdi, outOfMemoryMessage
%ifdef __APPLE__
    call _die
%else
    call die
%endif

.finish:
    leave
    ret


section .data
stackBottom:        dq 0
additionalRoots:    dq __globalVarTable
cstack              dq 0
cstackbase          dq 0
splstack            dq 0

; Type String
outOfMemoryMessage:
    dq 4294967296, 1, 0, 0
    db "*** Exception: Out of Memory", 0
