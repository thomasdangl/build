
section .text

str: db "%d\n"

extern printf

main:

push rbp
mov rbp, rsp
sub rsp, 16

mov rbx, 10

xor rax, rax
mov [rbp-8], rax
inc rax
mov [rbp], rax

loop:
lea rdi, [str]
mov rsi, [rbp]
mov rax, 1
call printf

mov rax, [rbp]
mov rdx, [rbp-8]
mov [rbp-8], rax
add rax, rdx
mov [rbp], rax

dec rbx
cmp rbx, 0
jne loop

xor rax, rax
mov rsp, rbp
pop rbp
ret

