section .text
global __main
extern __read, __print, __exit
__main:
mov eax, 1
mov dword [_n], eax
mov eax, 0
mov dword [_sum], eax
_main_loop:
mov eax, dword [_n]
push eax
mov eax, 3
xchg eax, dword [esp]
cdq
idiv dword [esp]
pop ebx
push eax
mov eax, 3
imul eax, dword [esp]
pop ebx
push eax
mov eax, dword [_n]
cmp dword [esp], eax
je __0
mov eax, 0
jmp __1
__0:
mov eax, 1
__1:
pop ebx
cmp eax, 0
je __2
jmp _is_multiple
__2:
mov eax, dword [_n]
push eax
mov eax, 5
xchg eax, dword [esp]
cdq
idiv dword [esp]
pop ebx
push eax
mov eax, 5
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
jmp _is_multiple
__5:
jmp _not_multiple
_is_multiple:
mov eax, dword [_sum]
push eax
mov eax, dword [_n]
add eax, dword [esp]
pop ebx
mov dword [_sum], eax
_not_multiple:
mov eax, dword [_n]
push eax
mov eax, 1
add eax, dword [esp]
pop ebx
mov dword [_n], eax
mov eax, dword [_n]
push eax
mov eax, 999
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
mov eax, dword [_sum]
call __print
call __exit
section .data
_n: dd 0
_sum: dd 0
