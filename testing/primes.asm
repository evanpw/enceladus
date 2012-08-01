section .text
global __main
extern __read, __print, __exit
__main:
mov eax, 2
mov dword [_n], eax
_main_loop:
mov eax, 2
mov dword [_i], eax
_inner_loop:
mov eax, dword [_i]
push eax
mov eax, dword [_n]
push eax
mov eax, 1
xchg eax, dword [esp]
sub eax, dword [esp]
pop ebx
cmp dword [esp], eax
jg __0
mov eax, 0
jmp __1
__0:
mov eax, 1
__1:
pop ebx
cmp eax, 0
je __2
jmp _is_prime
__2:
mov eax, dword [_n]
push eax
mov eax, dword [_i]
xchg eax, dword [esp]
cdq
idiv dword [esp]
pop ebx
mov dword [_d], eax
mov eax, dword [_d]
push eax
mov eax, dword [_i]
imul eax, dword [esp]
pop ebx
push eax
mov eax, dword [_n]
cmp dword [esp], eax
je __3
mov eax, 0
jmp __4
__3:
mov eax, 1
__4:
pop ebx
cmp eax, 0
je __5
jmp _end_inner_loop
__5:
mov eax, dword [_i]
push eax
mov eax, 1
add eax, dword [esp]
pop ebx
mov dword [_i], eax
jmp _inner_loop
_is_prime:
mov eax, dword [_n]
call __print
_end_inner_loop:
mov eax, dword [_n]
push eax
mov eax, 1
add eax, dword [esp]
pop ebx
mov dword [_n], eax
mov eax, dword [_n]
push eax
mov eax, 100
cmp dword [esp], eax
jg __6
mov eax, 0
jmp __7
__6:
mov eax, 1
__7:
pop ebx
cmp eax, 0
je __8
mov eax, 0
jmp __9
__8:
mov eax, 1
__9:
cmp eax, 0
je __10
jmp _main_loop
__10:
call __exit
section .data
_n: dd 0
_i: dd 0
_d: dd 0
