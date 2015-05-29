bits 64
section .text
global initGC, _runGC, stackBottom
extern walkStackC, __globalVarTable, walkHeap

initGC:
    mov rax, rsp
    mov qword [rel stackBottom], rax
    ret

_runGC:
    push rbp

    lea rdi, [rsp + 8]                  ; Top of stack before call to walkStack
    mov rsi, qword [rel stackBottom]    ; Bottom of stack at start of main
    mov rdx, rbp                        ; Frame pointer of caller
    mov rcx, __globalVarTable           ; Table of globals generated by compiler
    call walkStackC

    call walkHeap

    pop rbp
    ret

section .data
stackBottom:
    dq 0
