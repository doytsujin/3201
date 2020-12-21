/*
 * Copyright (c) 2020 Brian Callahan <bcallah@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef uint32_t	reg;

static reg	       *to, *from;
static uint8_t		ram[1048577];

struct cpu {
	reg r0;
	reg r1;
	reg r2;
	reg r3;
	reg r4;
	reg r5;
	reg r6;
	reg r7;
	reg r8;
	reg r9;
	reg ra;
	reg rb;
	reg rc;
	reg rd;
	reg re;
	reg rf;

	reg fs;
	reg fz;
	reg fp;
	reg fc;

	reg pc;
};

static void
monitor(struct cpu *s3201)
{

	printf("\033[2J");

	printf("r0: %08x\n", s3201->r0);
	printf("r1: %08x\n", s3201->r1);
	printf("r2: %08x\n", s3201->r2);
	printf("r3: %08x\n", s3201->r3);
	printf("r4: %08x\n", s3201->r4);
	printf("r5: %08x\n", s3201->r5);
	printf("r6: %08x\n", s3201->r6);
	printf("r7: %08x\n", s3201->r7);
	printf("r8: %08x\n", s3201->r8);
	printf("r9: %08x\n", s3201->r9);
	printf("ra: %08x\n", s3201->ra);
	printf("rb: %08x\n", s3201->rb);
	printf("rc: %08x\n", s3201->rc);
	printf("rd: %08x\n", s3201->rd);
	printf("re: %08x\n", s3201->re);
	printf("rf: %08x\n", s3201->rf);
	printf("fl: 0000%x%x%x%x\n", s3201->fs, s3201->fz, s3201->fp, s3201->fc);
	printf("pc: %08x\n", s3201->pc);
	printf("op: %02x %02x %02x %02x\n\n", ram[s3201->pc], ram[s3201->pc + 1], ram[s3201->pc + 2], ram[s3201->pc + 3]);

	usleep(100000);
}

static void
reset(struct cpu *s3201)
{

	s3201->r0 = 0;
	s3201->r1 = 0;
	s3201->r2 = 0;
	s3201->r3 = 0;
	s3201->r4 = 0;
	s3201->r5 = 0;
	s3201->r6 = 0;
	s3201->r7 = 0;
	s3201->r8 = 0;
	s3201->r9 = 0;
	s3201->ra = 0;
	s3201->rb = 0;
	s3201->rc = 0;
	s3201->rd = 0;
	s3201->re = 0;
	s3201->rf = 0;

	s3201->fs = 0;
	s3201->fz = 0;
	s3201->fp = 0;
	s3201->fc = 0;

	s3201->pc = 0;
}

static void
decode_reg(struct cpu *s3201, uint8_t regcode)
{
	uint8_t to_reg, from_reg;

	to_reg = regcode >> 4;
	from_reg = regcode & 0xf;

	switch (to_reg) {
	case 0:
		to = &s3201->r0;
		break;
	case 1:
		to = &s3201->r1;
		break;
	case 2:
		to = &s3201->r2;
		break;
	case 3:
		to = &s3201->r3;
		break;
	case 4:
		to = &s3201->r4;
		break;
	case 5:
		to = &s3201->r5;
		break;
	case 6:
		to = &s3201->r6;
		break;
	case 7:
		to = &s3201->r7;
		break;
	case 8:
		to = &s3201->r8;
		break;
	case 9:
		to = &s3201->r9;
		break;
	case 10:
		to = &s3201->ra;
		break;
	case 11:
		to = &s3201->rb;
		break;
	case 12:
		to = &s3201->rc;
		break;
	case 13:
		to = &s3201->rd;
		break;
	case 14:
		to = &s3201->re;
		break;
	case 15:
		to = &s3201->rf;
	}

	switch (from_reg) {
	case 0:
		from = &s3201->r0;
		break;
	case 1:
		from = &s3201->r1;
		break;
	case 2:
		from = &s3201->r2;
		break;
	case 3:
		from = &s3201->r3;
		break;
	case 4:
		from = &s3201->r4;
		break;
	case 5:
		from = &s3201->r5;
		break;
	case 6:
		from = &s3201->r6;
		break;
	case 7:
		from = &s3201->r7;
		break;
	case 8:
		from = &s3201->r8;
		break;
	case 9:
		from = &s3201->r9;
		break;
	case 10:
		from = &s3201->ra;
		break;
	case 11:
		from = &s3201->rb;
		break;
	case 12:
		from = &s3201->rc;
		break;
	case 13:
		from = &s3201->rd;
		break;
	case 14:
		from = &s3201->re;
		break;
	case 15:
		from = &s3201->rf;
	}
}

/*
 * Identical to how parity bit works on 8080
 * Returns 0 if parity is odd; 1 if parity is even
 */
static int
parity(uint32_t a)
{
	uint8_t p = 0;

	while (a) {
		p = !p;
		a = a & (a - 1);
	}

	return !p;
}

static void
set_flags(struct cpu *s3201, uint64_t carry)
{

	if (*to > 0x7fffffff)
		s3201->fs = 1;
	else
		s3201->fs = 0;

	if (*to == 0)
		s3201->fz = 1;
	else
		s3201->fz = 0;

	s3201->fp = parity(*to);

	if (carry > 0xffffffff)
		s3201->fc = 1;
	else
		s3201->fc = 0;
}

/*
 * Logical operations (and, or, not, shifts, etc.)
 * Register to register
 */
static void
logic_ops(struct cpu *s3201, uint16_t op)
{
	uint64_t carry = 0;

	switch (op) {
	case 0:		/* and */
		*to = *to & *from;
		break;
	case 1:		/* or */
		*to = *to | *from;
		break;
	case 2:		/* not */
		*to = ~(*to);
		break;
	case 3:		/* xor */
		*to = *to ^ *from;
		break;
	case 4:		/* lshift */
		if (*to >> 31)
			carry = 1;

		*to <<= 1;
		break;
	case 5:		/* rshift */
		if (*to & 0x1)
			carry = 1;

		*to >>= 1;
	}

	set_flags(s3201, carry);
}

/*
 * Arithmetic operations (+, -, *, /, etc.)
 * Register to register
 */
static void
arith_ops(struct cpu *s3201, uint16_t op)
{
	uint64_t carry, f, t;
	uint8_t cmp = 0;

	t = *to;
	f = *from;

	switch (op) {
	case 0:		/* add */
		carry = t + f;
		break;
	case 1:		/* sub */
		carry = t - f;
		break;
	case 2:		/* mul */
		carry = t * f;
		break;
	case 3:		/* div */
		if (f == 0)
			carry = 0;
		else
			carry = t / f;
		break;
	case 4:		/* mod */
		if (f == 0)
			carry = 0;
		else
			carry = t % f;
		break;
	case 5:		/* cmp */
		cmp = 1;
		carry = t - f;
	}

	if (cmp == 0)
		*to = carry & 0xffffffff;
	set_flags(s3201, carry);
}

/*
 * Load register/memory to register/memory
 */
static void
ldrm(struct cpu *s3201, uint16_t op)
{

	switch (op) {
	case 0:		/* ldr */
		*to = *from;
		break;
	case 1:		/* ldm */
		*to = ram[*from];
		*to |= ram[*from + 1] << 4;
		*to |= ram[*from + 2] << 8;
		*to |= ram[*from + 3] << 12;
		break;
	case 2:		/* ldb */
		*to = ram[*from];
		break;
	case 3:		/* stm */
		ram[*to] = *from & 0xff;
		ram[*to + 1] = (*from >> 4) & 0xff;
		ram[*to + 2] = (*from >> 8) & 0xff;
		ram[*to + 3] = (*from >> 12) & 0xff;
		break;
	case 4:		/* stb */
		ram[*to] = *from & 0xff;
	}
}

/*
 * Load immediate into upper half of register
 */
static void
ldu(struct cpu *s3201, uint16_t op)
{

	*to = op << 16;
}

/*
 * Load immediate into upper half of register
 */
static void
ldl(struct cpu *s3201, uint16_t op)
{

	*to = op;
}

/*
 * or immediate, for memory address loading ala MIPS
 */
static void
ori(struct cpu *s3201, uint16_t op)
{

	*to |= op;
}

/*
 * Jumps
 * Address to jump to in to register
 */
static void
j(struct cpu *s3201, uint16_t op)
{

	switch (op) {
	case 0:		/* Unconditional j */
		s3201->pc = *to;
		break;
	case 1:		/* jc */
		if (s3201->fc == 1)
			s3201->pc = *to;
		break;
	case 2:		/* jpo */
		if (s3201->fp == 1)
			s3201->pc = *to;
		break;
	case 3:		/* jz */
		if (s3201->fz == 1)
			s3201->pc = *to;
		break;
	case 4:		/* js */
		if (s3201->fs == 1)
			s3201->pc = *to;
		break;
	case 5:		/* jnc */
		if (s3201->fc == 0)
			s3201->pc = *to;
		break;
	case 6:		/* jpe */
		if (s3201->fp == 0)
			s3201->pc = *to;
		break;
	case 7:		/* jnz */
		if (s3201->fz == 0)
			s3201->pc = *to;
		break;
	case 8:		/* jns */
		if (s3201->fs == 0)
			s3201->pc = *to;
		break;
	}
}

static int
execute(struct cpu *s3201, uint8_t opcode)
{
	uint64_t carry;
	uint16_t op;

	if ((s3201->pc - 1) % 4 != 0)
		return 0;

	/* Second and third bytes are op or immediate (little endian) */
	op = ram[s3201->pc++];
	op |= ram[s3201->pc++] << 8;

	/* Fourth byte is the register byte */
	decode_reg(s3201, ram[s3201->pc++]);

	switch (opcode) {
	case 0x00:	/* nop */
		break;
	case 0x01:	/* logical ops */
		logic_ops(s3201, op);
		break;
	case 0x02:	/* arithmetic ops */
		arith_ops(s3201, op);
		break;
	case 0x03:	/* ld register or memory to register */
		ldrm(s3201, op);
		break;
	case 0x04:	/* ld immediate to register upper */
		ldu(s3201, op);
		break;
	case 0x05:	/* ld immediate to register lower */
		ldl(s3201, op);
		break;
	case 0x06:	/* store register to memory */
		ldrm(s3201, op);
		break;
	case 0x07:	/* or immediate */
		ori(s3201, op);
		break;
	case 0x08:	/* jump */
		j(s3201, op);
		break;
	case 0x09:	/* call */
		break;
	case 0x0a:	/* ret */
		break;
	case 0xff:	/* halt */
		return 0;
	}

	return 1;
}

int
main(int argc, char *argv[])
{
	struct cpu s3201;
	int fd, i = 0;

	memset(ram, 0, sizeof(ram));
	reset(&s3201);

	if ((fd = open(argv[1], O_RDONLY)) != -1) {
		while (i < sizeof(ram) - 1) {
			if (read(fd, &ram[i++], 1) < 1)
				break;
		}
		close(fd);
	}

	monitor(&s3201);
	while (execute(&s3201, ram[s3201.pc++]))
		monitor(&s3201);

	printf("\033[2J");

	return 0;
}
