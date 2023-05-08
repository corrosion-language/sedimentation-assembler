section .data
a: dd 0x428ad70a, 0xc2c6d446, 0xc0490fdb, 0x358637bd, 0x4356bf95, 0xc72d9c00, 0x4031eb85, 0xb58637bd
eight: dd 0x41000000
msg: db "%f", 10, 0
section .text
global _start
extern printf
extern exit
_start:
	; Calculate mean of the array
	vmovups ymm0, [rel a]
	vmovaps ymm1, ymm0
	; Sum of the array
	vhaddps ymm1, ymm1, ymm1
	vhaddps ymm1, ymm1, ymm1
	vperm2f128 ymm2, ymm1, ymm1, 1
	vaddps ymm1, ymm1, ymm2
	vdivss xmm1, xmm1, [rel eight]
	vbroadcastss ymm1, xmm1
	vsubps ymm0, ymm0, ymm1
	vmulps ymm0, ymm0, ymm0
	vhaddps ymm0, ymm0, ymm0
	vhaddps ymm0, ymm0, ymm0
	vperm2f128 ymm1, ymm0, ymm0, 1
	vaddps ymm0, ymm0, ymm1
	vdivss xmm0, xmm0, [rel eight]
	vsqrtss xmm0, xmm0, xmm0
	vcvtss2sd xmm0, xmm0, xmm0
	lea rdi, [rel msg]
	mov al, 1
	call printf wrt ..plt
	xor edi, edi
	call exit wrt ..plt
