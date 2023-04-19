section .bss
out: resb 10
section .text
global _start
_start:
	adc rdi, 0
	xor rdi, [out]
	mov cl, '0'
	lea rdi, [out]
	.L1:
		mov byte [rdi], cl
		inc rdi
		inc cl
		cmp cl, '9'
		jle .L1
	mov eax, 1
	mov edi, 1
	mov rsi, out
	mov edx, 10
	syscall

	xor ecx, ecx
	mov rdi, out
	.L2:
		add byte [rdi+rcx], 1
		inc rcx
		cmp rcx, 10
		jl .L2
	mov eax, 1
	mov edi, 1
	mov rsi, out
	mov edx, 10
	syscall

	xor ecx, ecx
	.L3:
		sub byte [ecx*2 + out + 1], 1
		inc ecx
		cmp ecx, 5
		jl .L3
	mov eax, 1
	mov edi, 1
	mov rsi, out
	mov edx, 10
	syscall

	mov eax, 60
	xor edi, edi
	syscall
