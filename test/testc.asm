section .data
	msg: db "123321test", 0
	f: db "%s %x %ld", 10, 0
	two: dq 2
section .text
global _start
extern printf
_start:
	finit
	fldpi
	fild qword [rel two]
	fdivp
	fsin
	fistp qword [rel two]
	lea rdi, [rel f]
	lea rsi, [rel msg]
	mov edx, 0xdeadbeef
	mov rcx, [rel two]
	call printf wrt ..plt
	mov eax, 60
	xor edi, edi
	syscall
