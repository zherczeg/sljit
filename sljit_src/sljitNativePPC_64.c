/*
 *    Stack-less Just-In-Time compiler
 *    Copyright (c) Zoltan Herczeg
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 */

// ppc 64-bit arch dependent functions

#ifdef __GNUC__
#define SLJIT_CLZ(src, dst) \
	asm volatile ( "cntlzd %0, %1" : "=r"(dst) : "r"(src) )
#else
#error "Must implement count leading zeroes"
#endif

#define SLJIT_PUSH_RLDICR(reg, shift) \
	push_inst(compiler, INS_FORM_OP1(30, (reg), (reg), (1 << 2) | (((63 - shift) & 0x1f) << 11) | (((63 - shift) & 0x20) >> 4) | ((shift & 0x1f) << 6) | (shift & 0x20)))

static int load_immediate(struct sljit_compiler *compiler, int reg, sljit_w imm)
{
	sljit_uw tmp;
	sljit_uw shift;
	sljit_uw tmp2;
	sljit_uw shift2;

	if (imm <= SIMM_MAX && imm >= SIMM_MIN)
		return push_inst(compiler, INS_FORM_IMM(14, reg, 0, (imm & 0xffff)));

	if (imm <= 0x7fffffff && imm >= -0x80000000l) {
		TEST_FAIL(push_inst(compiler, INS_FORM_IMM(15, reg, 0, ((imm >> 16) & 0xffff))));
		return (imm & 0xffff) ? push_inst(compiler, INS_FORM_IMM(24, reg, reg, (imm & 0xffff))) : SLJIT_NO_ERROR;
	}

	// Count leading zeroes
	tmp = (imm >= 0) ? imm : ~imm;
	SLJIT_CLZ(tmp, shift);
	SLJIT_ASSERT(shift > 0);
	shift--;
	tmp = (imm << shift);

	if ((tmp & ~0xffff000000000000ul) == 0) {
		TEST_FAIL(push_inst(compiler, INS_FORM_IMM(14, reg, 0, ((tmp >> 48) & 0xffff))));
		shift += 15;
		return SLJIT_PUSH_RLDICR(reg, shift);
	}

	if ((tmp & ~0xffffffff00000000ul) == 0) {
		TEST_FAIL(push_inst(compiler, INS_FORM_IMM(15, reg, 0, ((tmp >> 48) & 0xffff))));
		TEST_FAIL(push_inst(compiler, INS_FORM_IMM(24, reg, reg, ((tmp >> 32) & 0xffff))));
		shift += 31;
		return SLJIT_PUSH_RLDICR(reg, shift);
	}

	// cut out the 16 bit from immediate
	shift += 15;
	tmp2 = imm & ((1ul << (63 - shift)) - 1);


	if (tmp2 <= 0xffff) {
		TEST_FAIL(push_inst(compiler, INS_FORM_IMM(14, reg, 0, ((tmp >> 48) & 0xffff))));
		TEST_FAIL(SLJIT_PUSH_RLDICR(reg, shift));
		return push_inst(compiler, INS_FORM_IMM(24, reg, reg, tmp2));
	}

	if (tmp2 <= 0xffffffff) {
		TEST_FAIL(push_inst(compiler, INS_FORM_IMM(14, reg, 0, ((tmp >> 48) & 0xffff))));
		TEST_FAIL(SLJIT_PUSH_RLDICR(reg, shift));
		TEST_FAIL(push_inst(compiler, INS_FORM_IMM(25, reg, reg, (tmp2 >> 16))));
		return (imm & 0xffff) ? push_inst(compiler, INS_FORM_IMM(24, reg, reg, (tmp2 & 0xffff))) : SLJIT_NO_ERROR;
	}

	SLJIT_CLZ(tmp2, shift2);
	tmp2 <<= shift2;

	if ((tmp2 & ~0xffff000000000000ul) == 0) {
		TEST_FAIL(push_inst(compiler, INS_FORM_IMM(14, reg, 0, ((tmp >> 48) & 0xffff))));
		shift2 += 15;
		shift += (63 - shift2);
		TEST_FAIL(SLJIT_PUSH_RLDICR(reg, shift));
		TEST_FAIL(push_inst(compiler, INS_FORM_IMM(24, reg, reg, ((tmp2 >> 48) & 0xffff))));
		return SLJIT_PUSH_RLDICR(reg, shift2);
	}

	// The general version
	TEST_FAIL(push_inst(compiler, INS_FORM_IMM(15, reg, 0, ((imm >> 48) & 0xffff))));
	TEST_FAIL(push_inst(compiler, INS_FORM_IMM(24, reg, reg, ((imm >> 32) & 0xffff))));
	TEST_FAIL(SLJIT_PUSH_RLDICR(reg, 31));
	TEST_FAIL(push_inst(compiler, INS_FORM_IMM(25, reg, reg, ((imm >> 16) & 0xffff))));
	TEST_FAIL(push_inst(compiler, INS_FORM_IMM(24, reg, reg, (imm & 0xffff))));
	return SLJIT_NO_ERROR;
}
