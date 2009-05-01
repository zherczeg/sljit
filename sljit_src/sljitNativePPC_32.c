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

// ppc 32-bit arch dependent functions

static int load_immediate(struct sljit_compiler *compiler, int reg, sljit_w imm)
{
	if (imm <= SIMM_MAX && imm >= SIMM_MIN)
		return push_inst(compiler, INS_FORM_IMM(14, reg, 0, (imm & 0xffff)));

	TEST_FAIL(push_inst(compiler, INS_FORM_IMM(15, reg, 0, ((imm >> 16) & 0xffff))));
	return (imm & 0xffff) ? push_inst(compiler, INS_FORM_IMM(24, reg, reg, (imm & 0xffff))) : SLJIT_NO_ERROR;
}

#define INS_CLEAR_LEFT(dst, src, from) \
	INS_FORM_OP1(21, src, dst, ((from) << 6) | (31 << 1))

static int emit_single_op(struct sljit_compiler *compiler, int op, int flags,
	int dst, int src1, int src2)
{
	switch (op) {
	case SLJIT_ADD:
		if (flags & ALT_FORM1) {
			SLJIT_ASSERT(src2 == TMP_REG2);
			return push_inst(compiler, INS_FORM_IMM(14, dst, src1, compiler->imm));
		}
		if (flags & ALT_FORM2) {
			SLJIT_ASSERT(src2 == TMP_REG2);
			return push_inst(compiler, INS_FORM_IMM(15, dst, src1, compiler->imm));
		}
		return push_inst(compiler, INS_FORM_OP2(31, dst, src1, src2, (10 << 1) | 1 | (1 << 10)));

	case SLJIT_ADDC:
		return push_inst(compiler, INS_FORM_OP2(31, dst, src1, src2, (138 << 1) | 1 | (1 << 10)));

	case SLJIT_SUB:
		if (flags & ALT_FORM1) {
			SLJIT_ASSERT(src2 == TMP_REG2);
			return push_inst(compiler, INS_FORM_IMM(8, dst, src1, compiler->imm));
		}
		return push_inst(compiler, INS_FORM_OP2(31, dst, src2, src1, (8 << 1) | 1 | (1 << 10)));

	case SLJIT_SUBC:
		return push_inst(compiler, INS_FORM_OP2(31, dst, src2, src1, (136 << 1) | 1 | (1 << 10)));

	case SLJIT_MUL:
		if (flags & ALT_FORM1) {
			SLJIT_ASSERT(src2 == TMP_REG2);
			return push_inst(compiler, INS_FORM_IMM(7, dst, src1, compiler->imm));
		}
		return push_inst(compiler, INS_FORM_OP2(31, dst, src2, src1, (235 << 1) | 1 | (1 << 10)));

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
			compiler->imm &= 0x1f;
			return push_inst(compiler, INS_FORM_OP1(21, src1, dst, (compiler->imm << 11) | ((31 - compiler->imm) << 1) | 1));
		}
		return push_inst(compiler, INS_FORM_OP2(31, src1, dst, src2, (24 << 1) | 1));

	case SLJIT_LSHR:
		if (flags & ALT_FORM1) {
			SLJIT_ASSERT(src2 == TMP_REG2);
			compiler->imm &= 0x1f;
			return push_inst(compiler, INS_FORM_OP1(21, src1, dst, (((32 - compiler->imm) & 0x1f) << 11) | (compiler->imm << 6) | (31 << 1) | 1));
		}
		return push_inst(compiler, INS_FORM_OP2(31, src1, dst, src2, (536 << 1) | 1));

	case SLJIT_ASHR:
		if (flags & ALT_FORM1) {
			SLJIT_ASSERT(src2 == TMP_REG2);
			compiler->imm &= 0x1f;
			return push_inst(compiler, INS_FORM_OP1(31, src1, dst, (compiler->imm << 11) | (824 << 1) | 1));
		}
		return push_inst(compiler, INS_FORM_OP2(31, src1, dst, src2, (792 << 1) | 1));

	case (OP1_OFFSET + SLJIT_MOV):
	case (OP1_OFFSET + SLJIT_MOV_UI):
	case (OP1_OFFSET + SLJIT_MOV_SI):
		SLJIT_ASSERT(src1 == TMP_REG1);
		if (dst != src2)
			return push_inst(compiler, INS_FORM_OP2(31, src2, dst, src2, 444 << 1));
		return SLJIT_NO_ERROR;

	case (OP1_OFFSET + SLJIT_MOV_UB):
	case (OP1_OFFSET + SLJIT_MOV_SB):
		SLJIT_ASSERT(src1 == TMP_REG1);
		if ((flags & (REG_DEST | REG2_SOURCE)) == (REG_DEST | REG2_SOURCE)) {
			if (op == (OP1_OFFSET + SLJIT_MOV_SB))
				return push_inst(compiler, INS_FORM_OP1(31, src2, dst, 954 << 1));
			else
				return push_inst(compiler, INS_CLEAR_LEFT(dst, src2, 24));
		}
		else if ((flags & REG_DEST) && op == (OP1_OFFSET + SLJIT_MOV_SB))
			return push_inst(compiler, INS_FORM_OP1(31, src2, dst, 954 << 1));
		else if (dst != src2)
			SLJIT_ASSERT_IMPOSSIBLE();
		return SLJIT_NO_ERROR;

	case (OP1_OFFSET + SLJIT_MOV_UH):
	case (OP1_OFFSET + SLJIT_MOV_SH):
		SLJIT_ASSERT(src1 == TMP_REG1);
		if ((flags & (REG_DEST | REG2_SOURCE)) == (REG_DEST | REG2_SOURCE)) {
			if (op == (OP1_OFFSET + SLJIT_MOV_SH))
				return push_inst(compiler, INS_FORM_OP1(31, src2, dst, 922 << 1));
			else
				return push_inst(compiler, INS_CLEAR_LEFT(dst, src2, 16));
		}
		else if (dst != src2)
			SLJIT_ASSERT_IMPOSSIBLE();
		return SLJIT_NO_ERROR;

	case (OP1_OFFSET + SLJIT_NOT):
		return push_inst(compiler, INS_FORM_OP2(31, src2, dst, src2, (124 << 1) | 1));

	case (OP1_OFFSET + SLJIT_NEG):
		return push_inst(compiler, INS_FORM_OP1(31, dst, src2, (104 << 1) | 1 | (1 << 10)));
	}

	SLJIT_ASSERT_IMPOSSIBLE();
	return SLJIT_NO_ERROR;
}

static int emit_const(struct sljit_compiler *compiler, int reg, sljit_w initval)
{
	TEST_FAIL(push_inst(compiler, INS_FORM_IMM(15, reg, 0, ((initval >> 16) & 0xffff))));
	return push_inst(compiler, INS_FORM_IMM(24, reg, reg, (initval & 0xffff)));
}

void sljit_set_jump_addr(sljit_uw addr, sljit_uw new_addr)
{
	sljit_i *inst = (sljit_i*)addr;

	inst[0] = (inst[0] & 0xffff0000) | ((new_addr >> 16) & 0xffff);
	inst[1] = (inst[1] & 0xffff0000) | (new_addr & 0xffff);
	SLJIT_CACHE_FLUSH(inst, inst + 2);
}

void sljit_set_const(sljit_uw addr, sljit_w new_constant)
{
	sljit_i *inst = (sljit_i*)addr;

	inst[0] = (inst[0] & 0xffff0000) | ((new_constant >> 16) & 0xffff);
	inst[1] = (inst[1] & 0xffff0000) | (new_constant & 0xffff);
	SLJIT_CACHE_FLUSH(inst, inst + 2);
}
