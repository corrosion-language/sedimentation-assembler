section .data
a: dd 0x428ad70a, 0xc2c6d446, 0xc0490fdb, 0x358637bd
b: dd 0x4356bf95, 0xc72d9c00, 0x4031eb85, 0xb58637bd
format: db "%f", 10, 0
section .text
global _start
extern printf
_start:
	; use SSE to calculate the SSE of 2 arrays
	movups xmm0, [rel a]
	movups xmm1, [rel b]
	subps xmm0, xmm1
	mulps xmm0, xmm0
	haddps xmm0, xmm0
	haddps xmm0, xmm0
	cvtss2sd xmm0, xmm0
	lea rdi, [rel format]
	mov al, 1
	call printf wrt ..plt
	mov eax, 60
	xor edi, edi
	syscall
