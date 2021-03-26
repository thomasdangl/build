
section .data

hello: db "Hello World!\n"

section .text

_start:

mov rax, 1
mov rdi, 1
lea rsi, [hello]
mov rdx, 13
syscall

mov rax, 60
mov rdi, 0
syscall

