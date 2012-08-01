; Translation of factorial.spl to bad assembly language

section .text
	global __main
	
__main:
	; read n
	; call __read
	mov eax, 8
	mov dword [_n], eax

	; i <- 1
	mov eax, 1
	mov dword [_i], eax

_loop:
	;; if not (n > 1) then goto exit_loop
	
	; n > 1
	mov eax, dword [_n]
	push eax
	mov eax, 1
	cmp dword [esp], eax
	jg __label1
	mov eax, 0
	jmp __label2
  __label1:
	mov eax, 1
  __label2:
  	pop ebx
	
	; not
	cmp eax, 0
	je __label3
	mov eax, 0
	jmp __label4
  __label3:
	mov eax, 1
  __label4:

	; if
	cmp eax, 1
	je __label5
	jmp __label6
	
  __label5:
	; goto exit_loop
	jmp _exit_loop
  __label6:

	; i <- i * n
	mov eax, dword [_i]
	push eax
	mov eax, dword [_n]
	imul dword [esp]
	mov dword [_i], eax
	pop ebx
	
	; n <- n - 1
	mov eax, dword [_n]
	push eax
	mov eax, 1
	sub dword [esp], eax
	mov eax, dword [esp]
	mov dword [_n], eax
	pop ebx
	
	; goto loop
	jmp _loop
	
  _exit_loop:

	; print i
	mov eax, dword [_i]
	call __print

	; exit
	mov eax, 1
	call __kernel
	
; Print the number stored in eax, followed by a newline
__print:
	push eax
	push ebx
	mov ebx, 10
	mov edi, __end_buffer

  __convert_loop:
	xor edx, edx
	idiv ebx
	
	dec edi
	add dl, '0'
	mov byte [edi], dl
	
	cmp eax, 0
	jne __convert_loop
	
	mov ebx, __end_buffer
	sub ebx, edi
	inc ebx
	
	mov eax, 4
	push dword ebx
	push dword edi
	push dword 1
	call __kernel
	add esp, 12
	
	pop ebx
	pop eax
	ret
	
__kernel:
	int 0x80
	ret
	
section .data
__buffer: times 32 db 0
__end_buffer: db 0xA

_n:	dd 0
_i:	dd 0