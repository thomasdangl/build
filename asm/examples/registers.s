
section .text

r: db "R%d: %d\n"

extern printf

print_reg:
push rbp
mov rbp, rsp

mov rax, 2
call printf

pop rbp
ret

main::

push rbp
mov rbp, rsp

mov r8, 23
mov rdi, r
mov rsi, 8
mov rdx, r8
call print_reg

mov r9, 42
mov rdi, r
mov rsi, 9
mov rdx, r9
call print_reg

mov r10, 123
mov rdi, r
mov rsi, 10
mov rdx, r10
call print_reg

mov r11, 321
mov rdi, r
mov rsi, 11
mov rdx, r11
call print_reg

mov r12, 4
mov rdi, r
mov rsi, 12
mov rdx, r12
call print_reg

mov r13, 7
mov rdi, r
mov rsi, 13
mov rdx, r13
call print_reg

mov r14, 0
mov rdi, r
mov rsi, 14
mov rdx, r14
call print_reg

mov r15, 105
mov rdi, r
mov rsi, 15
mov rdx, r15
call print_reg

mov rsp, rbp
pop rbp
ret

