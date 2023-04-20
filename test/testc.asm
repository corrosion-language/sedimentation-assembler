section .data
	msg: db "123321test text this will work", 10, 0
	f: db "%s %x %f\n", 0
	six: dq 6
section .text
global _start
extern printf
_start:
	finit
	fldpi
	fild qword [rel six]
	fdivp
	fsin
	fstp qword [rel six]
	mov rdi, msg
	mov rax, [rel six]
	push rax
	mov rax, 0xdeadbeef
	push rax
	mov rax, msg
	push rax
	call printf wrt ..plt
	mov eax, 60
	xor edi, edi
	syscall
