; Fibonacci sequence (results in r0) -- resets when result > 0xffffffff
; Load and store to RAM test in r9 -- never resets
	org	$100	; Sets pc to 0x100 for labels (not using any)
	ldu	r7, $1	; r7 = 0x10000
	ldl	r8, 0	; r8 = 0
	ldl	rc, 1	; rc = 1
	ldl	r0, $0	; r0 = 0
	ldl	r4, $0	; r4 = 0
	ldl	r5, $1	; r5 = 1
	ldl	ra, 16	; ra = 16
	ldl	rb, $20	; rb = 32
	add	r4, r5	; r4 = r4 + r5
	jc	ra	; if carry then jump to address[ra] (16 in this case)
	ldr	r0, r4	; r0 = r4
	ldr	r4, r5	; r4 = r5
	ldr	r5, r0	; r5 = r0
	ldm	r8, r7	; r8 = address[r7]
	add	r8, rc	; r8 = r8 + rc
	ldr	r9, r8	; r9 = r8
	stm	r7, r8	; address[r7] = r8
	xor	r8, r8	; r8 ^ r8
	j	rb	; jump to address[rb] (32 in this case)
