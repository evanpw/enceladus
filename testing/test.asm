section .text
global __main
extern __read, __print, __exit
__main:
call __read
mov dword [_n], eax
mov eax, 1
mov dword [_i], eax
_loop:
mov eax, dword [_n]
push eax
mov eax, 1
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
jmp _exit_loop
__4:
mov eax, dword [_i]
push eax
mov eax, dword [_n]
imul eax, dword [esp]
pop ebx
mov dword [_i], eax
mov eax, dword [_n]
push eax
mov eax, 1
xchg eax, dword [esp]
sub eax, dword [esp]
pop ebx
mov dword [_n], eax
jmp _loop
_exit_loop:
mov eax, dword [_i]
call __print
call __exit
section .data
_n: dd 0
_i: dd 0