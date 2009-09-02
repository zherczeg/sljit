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

#define RLD(dst, src, sh, mb, type) \
	INS_FORM_OP1(30, (src), (dst), (type) | (((sh) & 0x1f) << 11) | (((sh) & 0x20) >> 4) | (((mb) & 0x1f) << 6) | ((mb) & 0x20))

#define SLJIT_PUSH_RLDICR(reg, shift) \
	push_inst(compiler, RLD(reg, reg, 63 - shift, shift, 1 << 2))

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
	return push_inst(compiler, INS_FORM_IMM(24, reg, reg, (imm & 0xffff)));
}

#define INS_CLEAR_LEFT(dst, src, from) \
	INS_FORM_OP1(30, src, dst, ((from) << 6) | 0x20)

// Sign extension for integer operations
#define UN_EXTS() \
	if ((flags & (ALT_SIGN_EXT | REG2_SOURCE)) == (ALT_SIGN_EXT | REG2_SOURCE)) { \
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(31, src2, TMP_REG2, 986 << 1))); \
		src2 = TMP_REG2; \
	}

#define BIN_EXTS() \
	if (flags & ALT_SIGN_EXT) { \
		if (flags & REG1_SOURCE) { \
			TEST_FAIL(push_inst(compiler, INS_FORM_OP1(31, src1, TMP_REG1, 986 << 1))); \
			src1 = TMP_REG1; \
		} \
		if (flags & REG2_SOURCE) { \
			TEST_FAIL(push_inst(compiler, INS_FORM_OP1(31, src2, TMP_REG2, 986 << 1))); \
			src2 = TMP_REG2; \
		} \
	}

#define BIN_IMM_EXTS() \
	if ((flags & (ALT_SIGN_EXT | REG1_SOURCE)) == (ALT_SIGN_EXT | REG1_SOURCE)) { \
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(31, src1, TMP_REG1, 986 << 1))); \
		src1 = TMP_REG1; \
	}

static int emit_single_op(struct sljit_compiler *compiler, int op, int flags,
	int dst, int src1, int src2)
{
	switch (op) {
	case SLJIT_ADD:
		if (flags & ALT_FORM1) {
			// Flags not set: BIN_IMM_EXTS unnecessary
			SLJIT_ASSERT(src2 == TMP_REG2);
			return push_inst(compiler, INS_FORM_IMM(14, dst, src1, compiler->imm));
		}
		if (flags & ALT_FORM2) {
			// Flags not set: BIN_IMM_EXTS unnecessary
			SLJIT_ASSERT(src2 == TMP_REG2);
			return push_inst(compiler, INS_FORM_IMM(15, dst, src1, compiler->imm));
		}
		if (flags & ALT_FORM3) {
			SLJIT_ASSERT(src2 == TMP_REG2);
			BIN_IMM_EXTS();
			return push_inst(compiler, INS_FORM_IMM(13, dst, src1, compiler->imm));
		}
		BIN_EXTS();
		return push_inst(compiler, INS_FORM_OP2(31, dst, src1, src2, (10 << 1) | 1 | (1 << 10)));

	case SLJIT_ADDC:
		BIN_EXTS();
		return push_inst(compiler, INS_FORM_OP2(31, dst, src1, src2, (138 << 1) | 1 | (1 << 10)));

	case SLJIT_SUB:
		if (flags & ALT_FORM1) {
			// Flags not set: BIN_IMM_EXTS unnecessary
			SLJIT_ASSERT(src2 == TMP_REG2);
			return push_inst(compiler, INS_FORM_IMM(8, dst, src1, compiler->imm));
		}
		if (flags & ALT_FORM2) {
			SLJIT_ASSERT(src2 == TMP_REG2);
			return push_inst(compiler, INS_FORM_CMP1(10, 4 | ((flags & ALT_SIGN_EXT) ? 0 : 1), src1, compiler->imm));
		}
		if (flags & ALT_FORM3) {
			return push_inst(compiler, INS_FORM_CMP2(31, 4 | ((flags & ALT_SIGN_EXT) ? 0 : 1), src1, src2, (32 << 1)));
		}
		BIN_EXTS();
		if (flags & ALT_FORM4)
			TEST_FAIL(push_inst(compiler, INS_FORM_CMP2(31, 4 | ((flags & ALT_SIGN_EXT) ? 0 : 1), src1, src2, (32 << 1))));
		return push_inst(compiler, INS_FORM_OP2(31, dst, src2, src1, (8 << 1) | 1 | (1 << 10)));

	case SLJIT_SUBC:
		BIN_EXTS();
		if (flags & ALT_FORM4) {
			// Unfortunately this is really complicated case
			TEST_FAIL(push_inst(compiler, INS_FORM_OP1(31, ZERO_REG, src1, 234 << 1)));
			TEST_FAIL(push_inst(compiler, INS_FORM_CMP2(31, 4 | ((flags & ALT_SIGN_EXT) ? 0 : 1), ZERO_REG, src2, (32 << 1))));
			TEST_FAIL(push_inst(compiler, INS_FORM_IMM(14, ZERO_REG, 0, 0)));
		}
		return push_inst(compiler, INS_FORM_OP2(31, dst, src2, src1, (136 << 1) | 1 | (1 << 10)));

	case SLJIT_MUL:
		if (flags & ALT_FORM1) {
			SLJIT_ASSERT(src2 == TMP_REG2);
			return push_inst(compiler, INS_FORM_IMM(7, dst, src1, compiler->imm));
		}
		BIN_EXTS();
		if (flags & ALT_FORM2)
			return push_inst(compiler, INS_FORM_OP2(31, dst, src2, src1, (235 << 1) | 1 | (1 << 10)));
		else
			return push_inst(compiler, INS_FORM_OP2(31, dst, src2, src1, (233 << 1) | 1 | (1 << 10)));

	case SLJIT_AND:
		if (flags & ALT_FORM1) {
			SLJIT_ASSERT(src2 == TMP_REG2);
			return push_inst(compiler, INS_FORM_IMM(28, src1, dst, compiler->imm));
		}
		if (flags & ALT_FORM2) {
			SLJIT_ASSERT(src2 == TMP_REG2);
			return push_inst(compiler, INS_FORM_IMM(29, src1, dst, compiler->imm));
		}
		return push_inst(compiler, INS_FORM_OP2(31, src1, dst, src2, (28 << 1) | 1));

	case SLJIT_OR:
		if (flags & ALT_FORM1) {
			SLJIT_ASSERT(src2 == TMP_REG2);
			return push_inst(compiler, INS_FORM_IMM(24, src1, dst, compiler->imm));
		}
		if (flags & ALT_FORM2) {
			SLJIT_ASSERT(src2 == TMP_REG2);
			return push_inst(compiler, INS_FORM_IMM(25, src1, dst, compiler->imm));
		}
		return push_inst(compiler, INS_FORM_OP2(31, src1, dst, src2, (444 << 1) | 1));

	case SLJIT_XOR:
		if (flags & ALT_FORM1) {
			SLJIT_ASSERT(src2 == TMP_REG2);
			return push_inst(compiler, INS_FORM_IMM(26, src1, dst, compiler->imm));
		}
		if (flags & ALT_FORM2) {
			SLJIT_ASSERT(src2 == TMP_REG2);
			return push_inst(compiler, INS_FORM_IMM(27, src1, dst, compiler->imm));
		}
		return push_inst(compiler, INS_FORM_OP2(31, src1, dst, src2, (316 << 1) | 1));

	case SLJIT_SHL:
		if (flags & ALT_FORM1) {
			SLJIT_ASSERT(src2 == TMP_REG2);
			if (flags & ALT_FORM2) {
				compiler->imm &= 0x1f;
				return push_inst(compiler, INS_FORM_OP1(21, src1, dst, (compiler->imm << 11) | ((31 - compiler->imm) << 1) | 1));
			}
			else {
				compiler->imm &= 0x3f;
				return push_inst(compiler, RLD(dst, src1, compiler->imm, 63 - compiler->imm, (1 << 2) | 1));
			}
		}
		if (flags & ALT_FORM2)
			return push_inst(compiler, INS_FORM_OP2(31, src1, dst, src2, (24 << 1) | 1));
		else
			return push_inst(compiler, INS_FORM_OP2(31, src1, dst, src2, (27 << 1) | 1));

	case SLJIT_LSHR:
		if (flags & ALT_FORM1) {
			SLJIT_ASSERT(src2 == TMP_REG2);
			if (flags & ALT_FORM2) {
				compiler->imm &= 0x1f;
				return push_inst(compiler, INS_FORM_OP1(21, src1, dst, (((32 - compiler->imm) & 0x1f) << 11) | (compiler->imm << 6) | (31 << 1) | 1));
			}
			else {
				compiler->imm &= 0x3f;
				return push_inst(compiler, RLD(dst, src1, 64 - compiler->imm, compiler->imm, 1));
			}
		}
		if (flags & ALT_FORM2)
			return push_inst(compiler, INS_FORM_OP2(31, src1, dst, src2, (536 << 1) | 1));
		else
			return push_inst(compiler, INS_FORM_OP2(31, src1, dst, src2, (539 << 1) | 1));

	case SLJIT_ASHR:
		if (flags & ALT_FORM1) {
			SLJIT_ASSERT(src2 == TMP_REG2);
			if (flags & ALT_FORM2) {
				compiler->imm &= 0x1f;
				return push_inst(compiler, INS_FORM_OP1(31, src1, dst, (compiler->imm << 11) | (824 << 1) | 1));
			}
			else {
				compiler->imm &= 0x3f;
				return push_inst(compiler, INS_FORM_OP1(31, src1, dst, ((compiler->imm & 0x1f) << 11) | (413 << 2) | ((compiler->imm & 0x20) >> 4) | 1));
			}
		}
		if (flags & ALT_FORM2)
			return push_inst(compiler, INS_FORM_OP2(31, src1, dst, src2, (792 << 1) | 1));
		else
			return push_inst(compiler, INS_FORM_OP2(31, src1, dst, src2, (794 << 1) | 1));

	case SLJIT_MOV:
		SLJIT_ASSERT(src1 == TMP_REG1);
		if (dst != src2)
			return push_inst(compiler, INS_FORM_OP2(31, src2, dst, src2, 444 << 1));
		return SLJIT_NO_ERROR;

	case SLJIT_MOV_UI:
	case SLJIT_MOV_SI:
		SLJIT_ASSERT(src1 == TMP_REG1);
		if ((flags & (REG_DEST | REG2_SOURCE)) == (REG_DEST | REG2_SOURCE)) {
			if (op == SLJIT_MOV_SI)
				return push_inst(compiler, INS_FORM_OP1(31, src2, dst, 986 << 1));
			else
				return push_inst(compiler, INS_CLEAR_LEFT(dst, src2, 0));
		}
		else if (dst != src2)
			SLJIT_ASSERT_IMPOSSIBLE();
		return SLJIT_NO_ERROR;

	case SLJIT_MOV_UB:
	case SLJIT_MOV_SB:
		SLJIT_ASSERT(src1 == TMP_REG1);
		if ((flags & (REG_DEST | REG2_SOURCE)) == (REG_DEST | REG2_SOURCE)) {
			if (op == SLJIT_MOV_SB)
				return push_inst(compiler, INS_FORM_OP1(31, src2, dst, 954 << 1));
			else
				return push_inst(compiler, INS_CLEAR_LEFT(dst, src2, 24));
		}
		else if ((flags & REG_DEST) && op == SLJIT_MOV_SB)
			return push_inst(compiler, INS_FORM_OP1(31, src2, dst, 954 << 1));
		else if (dst != src2)
			SLJIT_ASSERT_IMPOSSIBLE();
		return SLJIT_NO_ERROR;

	case SLJIT_MOV_UH:
	case SLJIT_MOV_SH:
		SLJIT_ASSERT(src1 == TMP_REG1);
		if ((flags & (REG_DEST | REG2_SOURCE)) == (REG_DEST | REG2_SOURCE)) {
			if (op == SLJIT_MOV_SH)
				return push_inst(compiler, INS_FORM_OP1(31, src2, dst, 922 << 1));
			else
				return push_inst(compiler, INS_CLEAR_LEFT(dst, src2, 16));
		}
		else if (dst != src2)
			SLJIT_ASSERT_IMPOSSIBLE();
		return SLJIT_NO_ERROR;

	case SLJIT_NOT:
		UN_EXTS();
		return push_inst(compiler, INS_FORM_OP2(31, src2, dst, src2, (124 << 1) | 1));

	case SLJIT_NEG:
		UN_EXTS();
		return push_inst(compiler, INS_FORM_OP1(31, dst, src2, (104 << 1) | 1 | (1 << 10)));
	}

	SLJIT_ASSERT_IMPOSSIBLE();
	return SLJIT_NO_ERROR;
}

static int emit_const(struct sljit_compiler *compiler, int reg, sljit_w initval)
{
	TEST_FAIL(push_inst(compiler, INS_FORM_IMM(15, reg, 0, ((initval >> 48) & 0xffff))));
	TEST_FAIL(push_inst(compiler, INS_FORM_IMM(24, reg, reg, ((initval >> 32) & 0xffff))));
	TEST_FAIL(SLJIT_PUSH_RLDICR(reg, 31));
	TEST_FAIL(push_inst(compiler, INS_FORM_IMM(25, reg, reg, ((initval >> 16) & 0xffff))));
	return push_inst(compiler, INS_FORM_IMM(24, reg, reg, (initval & 0xffff)));
}

void sljit_set_jump_addr(sljit_uw addr, sljit_uw new_addr)
{
	sljit_i *inst = (sljit_i*)addr;

	inst[0] = (inst[0] & 0xffff0000) | ((new_addr >> 48) & 0xffff);
	inst[1] = (inst[1] & 0xffff0000) | ((new_addr >> 32) & 0xffff);
	inst[3] = (inst[3] & 0xffff0000) | ((new_addr >> 16) & 0xffff);
	inst[4] = (inst[4] & 0xffff0000) | (new_addr & 0xffff);
	SLJIT_CACHE_FLUSH(inst, inst + 5);
}

void sljit_set_const(sljit_uw addr, sljit_w new_constant)
{
	sljit_i *inst = (sljit_i*)addr;

	inst[0] = (inst[0] & 0xffff0000) | ((new_constant >> 48) & 0xffff);
	inst[1] = (inst[1] & 0xffff0000) | ((new_constant >> 32) & 0xffff);
	inst[3] = (inst[3] & 0xffff0000) | ((new_constant >> 16) & 0xffff);
	inst[4] = (inst[4] & 0xffff0000) | (new_constant & 0xffff);
	SLJIT_CACHE_FLUSH(inst, inst + 5);
}
