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

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OP_AND	0
#define OP_OR	1
#define OP_NOT	2
#define OP_XOR	3
#define OP_LSH	4
#define OP_RSH	5

#define OP_ADD	0
#define OP_SUB	1
#define OP_MUL	2
#define OP_DIV	3
#define OP_MOD	4

#define OP_LDI	0
#define OP_LDR	1
#define OP_LDU	2
#define OP_LDL	3
#define OP_LDM	4
#define OP_LDB	5
#define OP_STM	6
#define OP_STB	7

#define OP_J	0
#define OP_JC	1

static FILE *in, *out;
static char *lab, *op, *arg;
static char *buf, *p;
static int pa;
static uint32_t pc;
static unsigned long line;

typedef struct labels {
	char *label;
	uint32_t addr;
	struct labels *next;
} label_t;
static label_t *head;

/*
 * Format:
 * [.label:]	[op]	[arg]	[; comment]
 */

static void __dead
usage(void)
{

	fprintf(stderr, "usage: %s infile outfile\n", getprogname());

	exit(1);
}

/*
 * Error unknown character
 */
static void __dead
unknown(void)
{

	errx(1, "%lu: Unknown character '%c'", line, *buf);
}

/*
 * Write bytes to output file
 */
static void
aout(uint8_t b)
{

	fputc(b, out);
}

/*
 * Convert string to number
 */
static uint32_t
readnum(const char *a, int base)
{
	char *ep;
	uint32_t ret;
	uint64_t ulval;

	errno = 0;
	ulval = strtoul(a, &ep, base);
	if (a[0] == '\0' || (*ep != '\0' && *ep != ' ' && *ep != '\t'))
		errx(1, "%lu: not a number '%s'", line, a);
	if ((errno == ERANGE && ulval == ULONG_MAX) || ulval > UINT_MAX)
		errx(1, "%lu: out of range '%s'", line, a);

	ret = (uint32_t) ulval;

	return ret;
}

/*
 * Sanity check single register (e.g., j)
 */
static void
onereg(void)
{
	char *a = arg;
	uint8_t b;

	if (*a++ != 'r')
		errx(1, "%lu: Need a register, found '%c'", line, *a);

	b = *a++;
	if (b >= '0' && b <= '9')
		b = ((b - '0') << 4) | (b - '0');
	else
		b = ((b - 'a' + 10) << 4) | (b - 'a' + 10);

	while (*a == ' ' || *a == '\t')
		++a;

	if (*a != '\0')
		errx(1, "e4");

	aout(b);
}

/*
 * Sanity check register, register in arg
 */
static void
tworegs(void)
{
	char *a = arg;
	uint8_t b, c;

	if (*a++ != 'r')
		errx(1, "%lu: Need a register, found '%c'", line, *a);

	b = *a++;
	if (b >= '0' && b <= '9')
		b = (b - '0') << 4;
	else
		b = (b - 'a' + 10) << 4;

	while (*a == ' ' || *a == '\t')
		++a;

	if (*a++ != ',')
		errx(1, "e2");

	while (*a == ' ' || *a == '\t')
		++a;

	if (*a++ != 'r')
		errx(1, "%lu: Need a register, found '%c'", line, *a);

	c = *a++;
	if (c >= '0' && c <= '9')
		b |= c - '0';
	else
		b |= c - 'a' + 10;

	while (*a == ' ' || *a == '\t')
		++a;

	if (*a != '\0')
		errx(1, "e4");

	aout(b);
}

/*
 * Sanity check register, immediate in arg
 */
static void
regimm(void)
{
	char *a = arg;
	int base = 10;
	uint8_t b;
	uint32_t c;

	if (*a++ != 'r')
		errx(1, "%lu: Need a register, found '%c'", line, *a);

	b = *a++;
	if (b >= '0' && b <= '9')
		b = ((b - '0') << 4) | (b - '0');
	else
		b = ((b - 'a' + 10) << 4) | (b - 'a' + 10);

	while (*a == ' ' || *a == '\t')
		++a;

	if (*a++ != ',')
		errx(1, "e6");

	while (*a == ' ' || *a == '\t')
		++a;

	if (*a == '$') {
		++a;
		base = 16;
	}

	if ((c = readnum(a, base)) > USHRT_MAX)
		errx(1, "%lu: e7", line);

	while (isalnum(*a++))
		;

	while (*a == ' ' || *a == '\t')
		++a;

	if (*a != '\0')
		errx(1, "e8");

	aout(c & 0xff);
	aout(c >> 8);
	aout(b);
}

/*
 * Logic operations
 */
static void
logic(int inst)
{

	aout(0x01);

	switch(inst) {
	case OP_XOR:
		aout(OP_XOR);
		aout(0x00);

		tworegs();
	}
}

/*
 * Arithmetic operations
 */
static void
arith(int inst)
{

	aout(0x02);

	switch (inst) {
	case OP_ADD:
		aout(OP_ADD);
		aout(0x00);

		tworegs();

		break;
	case OP_SUB:
		aout(OP_SUB);
		aout(0x00);

		tworegs();
	}
}

/*
 * Jump instructions
 */
static void
j(int inst)
{

	aout(0x08);

	switch (inst) {
	case OP_J:
		aout(OP_J);
		aout(0x00);

		onereg();

		break;
	case OP_JC:
		aout(OP_JC);
		aout(0x00);

		onereg();
	}
}

/*
 * Load instructions
 */
static void
ld(int inst)
{

	switch (inst) {
	case OP_LDR:
		aout(0x03);
		aout(0x00);
		aout(0x00);

		tworegs();

		break;
	case OP_LDU:
		aout(0x04);

		regimm();

		break;
	case OP_LDL:
		aout(0x05);

		regimm();

		break;
	case OP_LDM:
		aout(0x03);
		aout(0x01);
		aout(0x00);

		tworegs();

		break;
	case OP_LDB:
		aout(0x03);
		aout(0x02);
		aout(0x00);

		tworegs();

		break;
	case OP_STM:
		aout(0x03);
		aout(0x03);
		aout(0x00);

		tworegs();

		break;
	case OP_STB:
		aout(0x03);
		aout(0x04);
		aout(0x00);

		tworegs();
	}
}

/*
 * org
 */
static void
org(void)
{
	char *a = arg;
	int base = 10;

	if (*a == '$') {
		++a;
		base = 16;
	}

	if ((pc = readnum(a, base)) % 4 != 0)
		errx(1, "%lu: Invalid code location '%u'", line, pc);

	pc -= 4;

	while (isalnum(*a++))
		;

	while (*a == ' ' || *a == '\t')
		++a;

	if (*a != '\0')
		errx(1, "e9");
}

/*
 * Unrecognized op
 */
static void
badop(void)
{

	errx(1, "%lu: Unknown op '%s'", line, op);
}

/*
 * Figure out what we have and write out machine code
 */
static void
writeout(void)
{

	switch (op[0]) {
	case 'a':
		if (!strcmp(op, "add"))
			arith(OP_ADD);
		else
			badop();
		break;
	case 'j':
		if (!strcmp(op, "j"))
			j(OP_J);
		else if (!strcmp(op, "jc"))
			j(OP_JC);
		else
			badop();
		break;
	case 'l':
		if (!strcmp(op, "ldi"))
			ld(OP_LDI);
		else if (!strcmp(op, "ldr"))
			ld(OP_LDR);
		else if (!strcmp(op, "ldu"))
			ld(OP_LDU);
		else if (!strcmp(op, "ldl"))
			ld(OP_LDL);
		else if (!strcmp(op, "ldm"))
			ld(OP_LDM);
		else
			badop();
		break;
	case 'o':
		if (!strcmp(op, "org"))
			org();
		else
			badop();
		break;
	case 's':
		if (!strcmp(op, "stb"))
			ld(OP_STB);
		else if (!strcmp(op, "stm"))
			ld(OP_STM);
		else if (!strcmp(op, "sub"))
			arith(OP_SUB);
		else
			badop();
		break;
	case 'x':
		if (!strcmp(op, "xor"))
			logic(OP_XOR);
		else
			badop();
		break;
	default:
		badop();
	}
}

/*
 * Legal label characters
 */
static int
islegallab(void)
{

	if (isalnum(*buf) || *buf == '_' || *buf == '.')
		return 1;

	return 0;
}

/*
 * Legal op characters
 */
static int
islegalop(void)
{

	if (isalpha(*buf))
		return 1;

	return 0;
}

/*
 * Legal arg characters
 */
static int
islegalarg(void)
{

	if (*buf != '\n' && *buf != ';' && *buf != '\0')
		return 1;

	return 0;
}

/*
 * Save label address to the linked list
 */
static void
savelabaddr(void)
{
	label_t *curr, *new;

	if ((new = malloc(sizeof(label_t))) == NULL)
		err(1, "malloc");
	new->label = strdup(lab);
	new->addr = pc;
	new->next = NULL;

	if (head == NULL) {
		head = new;
	} else {
		curr = head;

		while (curr->next != NULL) {
			if (!strcmp(lab, curr->label))
				errx(1, "%lu: Duplicate label '%s'", line, lab);
			curr = curr->next;
		}

		if (!strcmp(lab, curr->label))
			errx(1, "%lu: Duplicate label '%s'", line, lab);

		curr->next = new;
	}
}

/*
 * Get a label token
 */
static void
toklab(void)
{
	char *s, *t;
	size_t len;

	for (s = buf; islegallab(); buf++)
		;
	len = buf - s;

	if (*buf++ != ':')
		errx(1, "%lu: Missing ':' at end of label '%s'\n", line, lab);

	if ((lab = malloc(len + 2)) == NULL)
		err(1, "malloc");
	t = lab;

	while (len--)
		*t++ = *s++;
	*t++ = '\0';
	*t = '\0';

	if (pa == 1)
		savelabaddr();
}

/*
 * Get an operator token
 */
static void
tokop(void)
{
	char *s, *t;
	size_t len;

	for (s = buf; islegalop(); buf++)
		;
	len = buf - s;

	if (*buf != ' ' && *buf != '\t')
		errx(1, "%lu: Invalid op '%s'\n", line, s);

	if ((op = malloc(len + 2)) == NULL)
		err(1, "malloc");
	t = op;

	while (len--)
		*t++ = *s++;
	*t++ = '\0';
	*t = '\0';
}

/*
 * Get an argument token
 */
static void
tokarg(void)
{
	char *s, *t;
	size_t len;

	for (s = buf; islegalarg(); buf++)
		;
	len = buf - s;

	if (*buf != ' ' && *buf != '\t' && *buf != ';' && *buf != '\n' &&
	    *buf != '\0') {
		errx(1, "%lu: Invalid arg '%s'\n", line, s);
	}

	if ((arg = malloc(len + 2)) == NULL)
		err(1, "malloc");
	t = arg;

	while (len--)
		*t++ = *s++;
	*t++ = '\0';
	*t = '\0';
}

/*
 * Process a comment
 */
static int
comment(void)
{
	int c;

	c = *buf++;
	while (c != '\n' && c != '\0')
		c = *buf++;

	if (c == '\0')
		return 0;

	return 1;
}

/*
 * Get one line of tokens
 */
static int
tokline(void)
{

	free(lab);
	lab = NULL;
	free(op);
	op = NULL;
	free(arg);
	arg = NULL;

	if (*buf == '\0')
		return 0;

	if (*buf == '\n') {
		++line;
		++buf;
		return 1;
	}

	if (*buf == ';')
		return comment();

	if (*buf == '.')
		toklab();

	while (*buf == ' ' || *buf == '\t')
		++buf;

	if (isalpha(*buf)) {
		tokop();

		while (*buf == ' ' || *buf == '\t')
			++buf;

		tokarg();
	}

	while (*buf == ' ' || *buf == '\t')
		++buf;

	if (*buf == ';') {
		if (comment() == 0)
			return 0;
	}

	while (*buf == ' ' || *buf == '\t')
		++buf;

	if (pa == 1 && op != NULL)
		pc += 4;

	if (pa == 2 && op != NULL)
		writeout();

	return 1;
}

/*
 * Get label addresses
 */
static void
pass1(void)
{

	fputs("Pass 1\n", stdout);
	while (tokline() == 1)
		;

	buf = p;
}

/*
 * Write out instructions
 */
static void
pass2(void)
{

	line = 0;

	fputs("Pass 2\n", stdout);
	while (tokline() == 1)
		;
}

int
main(int argc, char *argv[])
{
	off_t size;

	if (argc != 3)
		usage();

	line = 1;

	if ((in = fopen(argv[1], "r")) == NULL)
		err(1, "fopen");

	fseek(in, 0L, SEEK_END);
	size = ftello(in);
	(void) fseek(in, 0L, SEEK_SET);

	if ((buf = malloc(size + 1)) == NULL)
		err(1, "malloc");
	p = buf;

	fread(buf, 1, size, in);
	buf[size] = '\0';

	fclose(in);

	if ((out = fopen(argv[2], "w+")) == NULL)
		err(1, "fopen");

	fputs("System 3201 assembler\n", stdout);
	pa = 1;
	pass1();
	pa = 2;
	pass2();

	return 0;
}
