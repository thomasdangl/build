
section .data

hello: db "Hello World!"

section .text

extern puts

main:

lea rdi, [hello]
call puts

xor rax, rax
ret

