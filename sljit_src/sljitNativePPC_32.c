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
