
section .data

hello: db "Hello World!\n"

section .text

extern printf

main:

push rbp
mov rbp, rsp

mov rdi, hello
mov rsi, 1
xor rax, rax
call printf

xor rax, rax
mov rsp, rbp
pop rbp
ret

