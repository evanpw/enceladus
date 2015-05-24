bits 64
section .text
global initGC, walkStack, stackBottom
extern _walkStackC

initGC:
    mov rax, rsp
    add rax, 8
    mov qword [rel stackBottom], rax
    ret

walkStack:
    push rbp
    mov rbp, rsp

    ; Get top of stack before the call to this function
    mov rdi, rbp
    add rdi, 16

    ; Get bottom of stack at the beginning of the program
    mov rsi, qword [rel stackBottom]

    mov r15, rsp
    and rsp, -16
    add rsp, -8
    push r15
    call _walkStackC
    pop rsp

    pop rbp
    ret

section .data
stackBottom:
    dq 0
