bits 64
section .text

;; 64-bit destination
; mov rax, byte 1           ILLEGAL
; mov rax, word 1           ILLEGAL
mov rax, dword 1
mov rax, qword 1

; mov rax, bl               ILLEGAL
; mov rax, bx               ILLEGAL
; mov rax, ebx              ILLEGAL
mov rax, rbx

; mov rax, byte [rbx]       ILLEGAL
; mov rax, word [rbx]       ILLEGAL
; mov rax, dword [rbx]      ILLEGAL
mov rax, qword [rbx]

movzx rax, bl
movzx rax, bx
; movzx rax, ebx            ILLEGAL
; movzx rax, rbx            ILLEGAL

movzx rax, byte [rbx]
movzx rax, word [rbx]
; movzx rax, dword [rbx]    ILLEGAL
; movzx rax, qword [rbx]    ILLEGAL

; movzx rax, byte 1         ILLEGAL
; movzx rax, word 1         ILLEGAL
; movzx rax, dword 1        ILLEGAL
; movzx rax, qword 1        ILLEGAL

movsx rax, bl
movsx rax, bx
movsx rax, ebx
; movsx rax, rbx            ILLEGAL

movsx rax, byte [rbx]
movsx rax, word [rbx]
movsx rax, dword [rbx]
; movsx rax, qword [rbx]

; movsx rax, byte 1         ILLEGAL
; movsx rax, word 1         ILLEGAL
; movsx rax, dword 1        ILLEGAL
; movsx rax, qword 1        ILLEGAL




;; 32-bit destination
; mov eax, byte 1           ILLEGAL
; mov eax, word 1           ILLEGAL
mov eax, dword 1

; mov eax, bl               ILLEGAL
; mov eax, bx               ILLEGAL
mov eax, ebx

; mov eax, byte [rbx]       ILLEGAL
; mov eax, word [rbx]       ILLEGAL
mov eax, dword [rbx]

movzx eax, bl
movzx eax, bx
; movzx eax, ebx            ILLEGAL

movzx eax, byte [rbx]
movzx eax, word [rbx]
; movzx eax, dword [rbx]    ILLEGAL

; movzx eax, byte 1         ILLEGAL
; movzx eax, word 1         ILLEGAL
; movzx eax, dword 1        ILLEGAL

movsx eax, bl
movsx eax, bx
; movsx eax, ebx            ILLEGAL

movsx eax, byte [rbx]
movsx eax, word [rbx]
; movsx eax, dword [rbx]    ILLEGAL

; movsx eax, byte 1         ILLEGAL
; movsx eax, word 1         ILLEGAL
; movsx eax, dword 1        ILLEGAL




;; 16-bit destination
; mov ax, byte 1            ILLEGAL
mov ax, word 1

; mov ax, bl                ILLEGAL
mov ax, bx

; mov ax, byte [rbx]        ILLEGAL
mov ax, word [rbx]

movzx ax, bl
; movzx ax, bx              ILLEGAL

movzx ax, byte [rbx]
; movzx ax, word [rbx]      ILLEGAL

; movzx ax, byte 1          ILLEGAL
; movzx ax, word 1          ILLEGAL

movsx ax, bl
; movsx ax, bx              ILLEGAL

movsx ax, byte [rbx]
; movsx ax, word [rbx]      ILLEGAL

; movsx ax, byte 1          ILLEGAL
; movsx ax, word 1          ILLEGAL




;; 16-bit destination
mov al, byte 1
mov al, bl
mov al, byte [rbx]
; movzx al, bl              ILLEGAL
; movzx al, byte [rbx]      ILLEGAL
; movzx al, byte 1          ILLEGAL
; movsx al, bl              ILLEGAL
; movsx al, byte [rbx]      ILLEGAL
; movsx al, byte 1          ILLEGAL
