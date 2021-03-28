
section .text

str: db "%d\n"

extern printf

main:

push rbp
mov rbp, rsp
sub rsp, 16

mov rbx, 10

loop:
lea rdi, [str]
mov rsi, rbx
mov rax, 1
call printf
dec rbx
cmp rbx, 0
jne loop

xor rax, rax
mov rsp, rbp
pop rbp
ret

