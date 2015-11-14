bits 64
section .text
global main, _main, stackBottom, gcAllocate, _gcAllocate, gcAllocateFromC, ccall, _ccall
global splcall0, splcall1, splcall2, splcall3, splcall4, splcall5
global addRoot, removeRoots
extern gcCollectAndAllocate, __globalVarTable, try_mymalloc, _panic, panic, splmain
extern initializeHeap
extern _malloc, _free, malloc, free, saveCommandLine

main:
_main:
    push rbp
    mov rbp, rsp

    call saveCommandLine

    ; Allocate a stack for C code
    mov rdi, 0x400000
%ifdef __APPLE__
    call _malloc
%else
    call malloc
%endif
    add rax, 0x400000
    mov qword [rel cstack], rax

    ; Allocate an initial heap
    call initializeHeap

    ; Initialize the GC
    lea rax, [rbp - 16]
    mov qword [rel stackBottom], rax

    ; The actual program
    call splmain

    ; free the c stack (to make valgrind happy)
    mov rdi, qword [rel cstack]
    add rdi, -0x400000
%ifdef __APPLE__
    call _free
%else
    call free
%endif

    ; If we reach here without calling panic, then exit code = 0
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


_ccall:
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

    push qword 0
    push rsi
    call rax
    add rsp, 16

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

    push rdx
    push rsi
    call rax
    add rsp, 16

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

    push qword 0
    push rcx
    push rdx
    push rsi
    call rax
    add rsp, 32

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

    push r8
    push rcx
    push rdx
    push rsi
    call rax
    add rsp, 32

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

    push qword 0
    push r9
    push r8
    push rcx
    push rdx
    push rsi
    call rax
    add rsp, 48

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


_gcAllocate:
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

    ; If not successful, collect garbage first, then allocate
    mov rdi, qword [rbp - 8]                ; Size to allocate
    mov rsi, rbp                            ; Frame pointer of gcAllocate
    mov rdx, qword [rel stackBottom]        ; Frame pointer at beginning of execution
    mov rcx, qword [rel additionalRoots]    ; List of additional roots: globals and C-allocated heap vars
    call gcCollectAndAllocate

    test rax, rax
    jnz .finish

    ; If all of this fails, the we're out of memory
    mov rdi, outOfMemoryMessage
%ifdef __APPLE__
    call _panic
%else
    call panic
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
    mov rdi, qword [rbp - 8]                ; Size to allocate
    mov rsi, qword [rel splstack]           ; Frame pointer at last Simple->C transition
    mov rdx, qword [rel stackBottom]        ; Frame pointer at beginning of execution
    mov rcx, qword [rel additionalRoots]    ; List of additional roots: globals and C-allocated heap vars
    call gcCollectAndAllocate

    test rax, rax
    jnz .finish

    ; If all of this fails, the we're out of memory
    mov rdi, outOfMemoryMessage
%ifdef __APPLE__
    call _panic
%else
    call panic
%endif

.finish:
    leave
    ret


section .data
stackBottom:        dq 0
additionalRoots:    dq __globalVarTable
cstack              dq 0
splstack            dq 0

; Type String
outOfMemoryMessage:
    dq 4294967296, 1, 0, 0
    db "*** Exception: Out of Memory", 0
