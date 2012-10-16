/*
 *    Stack-less Just-In-Time compiler
 *
 *    Copyright 2009-2012 Zoltan Herczeg (hzmester@freemail.hu). All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *      conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright notice, this list
 *      of conditions and the following disclaimer in the documentation and/or other materials
 *      provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDER(S) OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

static int load_immediate(struct sljit_compiler *compiler, int dst, sljit_w imm)
{
	if (imm <= SIMM_MAX && imm >= SIMM_MIN)
		return push_inst(compiler, OR | D(dst) | S1(0) | IMM(imm), DR(dst));

	FAIL_IF(push_inst(compiler, SETHI | D(dst) | ((imm >> 10) & 0x3fffff), DR(dst)));
	return (imm & 0x3ff) ? push_inst(compiler, OR | D(dst) | S1(dst) | IMM_ARG | (imm & 0x3ff), DR(dst)) : SLJIT_SUCCESS;
}

static SLJIT_INLINE int emit_single_op(struct sljit_compiler *compiler, int op, int flags,
	int dst, int src1, sljit_w src2)
{
	switch (GET_OPCODE(op)) {
	case SLJIT_MOV:
	case SLJIT_MOV_UI:
	case SLJIT_MOV_SI:
		SLJIT_ASSERT(src1 == TMP_REG1);
		if (dst != src2)
			return push_inst(compiler, OR | D(dst) | S1(0) | S2(src2), DR(dst));
		return SLJIT_SUCCESS;

	case SLJIT_MOV_UB:
	case SLJIT_MOV_SB:
		SLJIT_ASSERT(src1 == TMP_REG1);
/*		if ((flags & (REG_DEST | REG2_SOURCE)) == (REG_DEST | REG2_SOURCE)) {
			if (op == SLJIT_MOV_SB)
				return push_inst(compiler, SEB | T(src2) | D(dst), DR(dst));
			return push_inst(compiler, ANDI | S(src2) | T(dst) | IMM(0xff), DR(dst));
		}
		else if (dst != src2)
			SLJIT_ASSERT_STOP();*/
		return SLJIT_SUCCESS;

	case SLJIT_MOV_UH:
	case SLJIT_MOV_SH:
		SLJIT_ASSERT(src1 == TMP_REG1);
/*		if ((flags & (REG_DEST | REG2_SOURCE)) == (REG_DEST | REG2_SOURCE)) {
			if (op == SLJIT_MOV_SH)
				return push_inst(compiler, SEH | T(src2) | D(dst), DR(dst));
			return push_inst(compiler, ANDI | S(src2) | T(dst) | IMM(0xffff), DR(dst));
		}
		else if (dst != src2)
			SLJIT_ASSERT_STOP();*/
		return SLJIT_SUCCESS;
	}

	SLJIT_ASSERT_STOP();
	return SLJIT_SUCCESS;
}

static SLJIT_INLINE int emit_const(struct sljit_compiler *compiler, int dst, sljit_w init_value)
{
	FAIL_IF(push_inst(compiler, SETHI | D(dst) | ((init_value >> 10) & 0x3fffff), DR(dst)));
	return push_inst(compiler, OR | D(dst) | S1(dst) | IMM_ARG | (init_value & 0x3ff), DR(dst));
}

SLJIT_API_FUNC_ATTRIBUTE void sljit_set_jump_addr(sljit_uw addr, sljit_uw new_addr)
{
	sljit_ins *inst = (sljit_ins*)addr;

	inst[0] = (inst[0] & 0xffff0000) | ((new_addr >> 16) & 0xffff);
	inst[1] = (inst[1] & 0xffff0000) | (new_addr & 0xffff);
	SLJIT_CACHE_FLUSH(inst, inst + 2);
}

SLJIT_API_FUNC_ATTRIBUTE void sljit_set_const(sljit_uw addr, sljit_w new_constant)
{
	sljit_ins *inst = (sljit_ins*)addr;

	inst[0] = (inst[0] & 0xffff0000) | ((new_constant >> 16) & 0xffff);
	inst[1] = (inst[1] & 0xffff0000) | (new_constant & 0xffff);
	SLJIT_CACHE_FLUSH(inst, inst + 2);
}
