section .text
global __main
extern __read, __print, __exit
__main:
mov eax, 20000
mov dword [_N], eax
mov eax, 1
mov dword [_x], eax
mov eax, 0
mov dword [_i], eax
_loop:
mov eax, dword [_x]
push eax
mov eax, dword [_x]
push eax
mov eax, dword [_x]
imul eax, dword [esp]
pop ebx
push eax
mov eax, dword [_N]
xchg eax, dword [esp]
sub eax, dword [esp]
pop ebx
push eax
mov eax, 2
push eax
mov eax, dword [_x]
imul eax, dword [esp]
pop ebx
xchg eax, dword [esp]
cdq
idiv dword [esp]
pop ebx
xchg eax, dword [esp]
sub eax, dword [esp]
pop ebx
mov dword [_x], eax
mov eax, dword [_x]
call __print
mov eax, dword [_i]
push eax
mov eax, 1
add eax, dword [esp]
pop ebx
mov dword [_i], eax
mov eax, dword [_i]
push eax
mov eax, 100
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
mov eax, 0
jmp __3
__2:
mov eax, 1
__3:
cmp eax, 0
je __4
jmp _loop
__4:
mov eax, dword [_x]
call __print
call __exit
section .data
_N: dd 0
_x: dd 0
_i: dd 0
