
section .text

main:

push rbp
mov rsp, rbp

mov edx, 2
mov dx, 2
add r8d, 8
sub r8w, 2
mov ax, 3
xor r9d, r9d
xor r9b, r9b
xor r8d, 1000

mov spl, 6
mov ah, 4
mov dh, 7
add ah, dh

pop rbp
ret

