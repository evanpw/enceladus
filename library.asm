section .text
	extern _printf, _scanf
	extern __main
	global _main, __read, __print
	
_main:
	; The main function from the compiled program
	call __main
	
	; Exit
	mov eax, 1
	call __kernel
	
__read:
	; Realign stack to 16 bytes
	mov ebx, esp
	and esp, 0xFFFFFFF0
	add esp, -12
	push ebx

	add esp, -8
	push dword __read_result
	push dword __read_format
	call _scanf
	add esp, 16
	
	pop ebx
	mov esp, ebx
	mov eax, dword [__read_result]
	
	ret
	
; Print the number stored in eax, followed by a newline
__print:
	; Realign stack to 16 bytes
	mov ebx, esp
	and esp, 0xFFFFFFF0
	add esp, -12
	push ebx

	add esp, -8
	push eax
	push dword __format
	call _printf
	add esp, 16
	
	pop ebx
	mov esp, ebx
	
	ret
	
__kernel:
	int 0x80
	ret
	
section .data
__format: db "%d", 0xA, 0
__read_format: db "%d", 0
__read_result: dd 0