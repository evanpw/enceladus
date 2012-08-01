section .text
	global __exit, __read, __print

__exit:
	mov eax, 1
	call __kernel
	
__read:
	mov eax, 8
	ret
	
; Print the number stored in eax, followed by a newline
__print:
	push eax
	push ebx
	mov ebx, 10
	mov edi, __end_buffer
	
	mov eax, dword [esp + 4]
	cmp eax, 0
	jnl __positive1
	
	neg eax
	
  __positive1:

  __convert_loop:
	xor edx, edx
	idiv ebx
	
	dec edi
	add dl, '0'
	mov byte [edi], dl
	
	cmp eax, 0
	jne __convert_loop
	
	mov eax, dword [esp + 4]
	cmp eax, 0
	jnl __positive2
	
	dec edi
	mov byte [edi], '-'	
	
  __positive2:	
	
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