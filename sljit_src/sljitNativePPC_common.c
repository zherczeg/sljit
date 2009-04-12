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

// Length of an instruction word
// Both for ppc-32 and ppc-64
typedef unsigned int sljit_i;

#define TMP_REG1	(SLJIT_LOCALS_REG + 1)
#define TMP_REG2	(SLJIT_LOCALS_REG + 2)
#define TMP_REG3	(SLJIT_LOCALS_REG + 3)
#define REAL_STACK_PTR	(SLJIT_LOCALS_REG + 4)

#define INS_FORM_IMM(opcode, D, A, IMM) \
		(((opcode) << 26) | (reg_map[D] << 21) | (reg_map[A] << 16) | (IMM))
#define INS_FORM_OP0(opcode, D, opcode2) \
		(((opcode) << 26) | (reg_map[D] << 21) | (opcode2))
#define INS_FORM_OP1(opcode, D, A, opcode2) \
		(((opcode) << 26) | (reg_map[D] << 21) | (reg_map[A] << 16) | (opcode2))
#define INS_FORM_OP2(opcode, D, A, B, opcode2) \
		(((opcode) << 26) | (reg_map[D] << 21) | (reg_map[A] << 16) | (reg_map[B] << 11) | (opcode2))

// SLJIT_LOCALS_REG is not the real stack register, since it must
// point to the head of the stack chain
static sljit_ub reg_map[SLJIT_NO_REGISTERS + 5] = {
  0, 3, 4, 5, 30, 29, 28, 31, 6, 7, 8, 1
};

static int push_inst(struct sljit_compiler *compiler, sljit_i ins)
{
	sljit_i *ptr;

	ptr = (sljit_i*)ensure_buf(compiler, sizeof(sljit_i));
	if (!ptr) {
		compiler->error = SLJIT_MEMORY_ERROR;
		return SLJIT_MEMORY_ERROR;
	}
	*ptr = ins;
	compiler->size++;
	return SLJIT_NO_ERROR;
}

void* sljit_generate_code(struct sljit_compiler *compiler)
{
	struct sljit_memory_fragment *buf;
	sljit_i *code;
	sljit_i *code_ptr;
	sljit_i *buf_ptr;
	sljit_i *buf_end;

	FUNCTION_ENTRY();

	SLJIT_ASSERT(compiler->size > 0);
	reverse_buf(compiler);

	code = SLJIT_MALLOC_EXEC(compiler->size * sizeof(sljit_uw));

	buf = compiler->buf;

	code_ptr = code;
	do {
		buf_ptr = (sljit_i*)buf->memory;
		buf_end = buf_ptr + (buf->used_size >> 2);
		do {
			*code_ptr++ = *buf_ptr++;
		} while (buf_ptr < buf_end);

		buf = buf->next;
	} while (buf);

	SLJIT_ASSERT(code_ptr - code <= compiler->size);

	SLJIT_CACHE_FLUSH((char *)code, (char *)code_ptr);

	compiler->error = SLJIT_CODE_GENERATED;
	return code;
}

void sljit_free_code(void* code)
{
	SLJIT_FREE_EXEC(code);
}

#ifdef SLJIT_CONFIG_PPC_32
#include "sljitNativePPC_32.c"
#else
#include "sljitNativePPC_64.c"
#endif

int sljit_emit_enter(struct sljit_compiler *compiler, int args, int general, int local_size)
{
	FUNCTION_ENTRY();
	SLJIT_ASSERT(args >= 0 && args <= SLJIT_NO_GEN_REGISTERS);
	SLJIT_ASSERT(general >= 0 && general <= SLJIT_NO_GEN_REGISTERS);
	SLJIT_ASSERT(args <= general);
	SLJIT_ASSERT(local_size >= 0 && local_size <= SLJIT_MAX_LOCAL_SIZE);
	SLJIT_ASSERT(compiler->general == -1);

	sljit_emit_enter_verbose();

	compiler->general = general;

	TEST_FAIL(push_inst(compiler, INS_FORM_OP0(31, 0, 0x802a6)));
#ifdef SLJIT_CONFIG_PPC_32
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(36, SLJIT_LOCALS_REG, REAL_STACK_PTR, (-(int)(sizeof(sljit_w))) & 0xffff )));
	if (general >= 1)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(36, SLJIT_GENERAL_REG1, REAL_STACK_PTR, (-2 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (general >= 2)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(36, SLJIT_GENERAL_REG2, REAL_STACK_PTR, (-3 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (general >= 3)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(36, SLJIT_GENERAL_REG3, REAL_STACK_PTR, (-4 * (int)(sizeof(sljit_w))) & 0xffff )));
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(36, 0, REAL_STACK_PTR, sizeof(sljit_w))));
#else
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(62, SLJIT_LOCALS_REG, REAL_STACK_PTR, (-(int)(sizeof(sljit_w))) & 0xffff )));
	if (general >= 1)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(62, SLJIT_GENERAL_REG1, REAL_STACK_PTR, (-2 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (general >= 2)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(62, SLJIT_GENERAL_REG2, REAL_STACK_PTR, (-3 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (general >= 3)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(62, SLJIT_GENERAL_REG3, REAL_STACK_PTR, (-4 * (int)(sizeof(sljit_w))) & 0xffff )));
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(62, 0, REAL_STACK_PTR, 2 * sizeof(sljit_w))));
#endif

	if (args >= 1)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, SLJIT_TEMPORARY_REG1, SLJIT_GENERAL_REG1, SLJIT_TEMPORARY_REG1, 444 << 1)));
	if (args >= 2)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, SLJIT_TEMPORARY_REG2, SLJIT_GENERAL_REG2, SLJIT_TEMPORARY_REG2, 444 << 1)));
	if (args >= 3)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, SLJIT_TEMPORARY_REG3, SLJIT_GENERAL_REG3, SLJIT_TEMPORARY_REG3, 444 << 1)));

#ifdef SLJIT_CONFIG_PPC_32
	compiler->local_size = (general + 1 + 1) * sizeof(sljit_w) + local_size;
#else
	compiler->local_size = (general + 1 + 7) * sizeof(sljit_w) + local_size;
#endif
	compiler->local_size = (compiler->local_size + 15) & ~0xf;

#ifdef SLJIT_CONFIG_PPC_32
	if (compiler->local_size <= 0x8000) {
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(37, REAL_STACK_PTR, REAL_STACK_PTR, ((-compiler->local_size) & 0xffff))));
	}
	else {
		TEST_FAIL(load_immediate(compiler, 0, -compiler->local_size));
		TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, REAL_STACK_PTR, REAL_STACK_PTR, 0, 183 << 1)));
	}
	TEST_FAIL(push_inst(compiler, INS_FORM_IMM(14, SLJIT_LOCALS_REG, REAL_STACK_PTR, 2 * sizeof(sljit_w))));
#else
	if (compiler->local_size <= 0x8000) {
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(62, REAL_STACK_PTR, REAL_STACK_PTR, ((-compiler->local_size) & 0xffff) | 1 )));
	}
	else {
		TEST_FAIL(load_immediate(compiler, 0, -compiler->local_size));
		TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, REAL_STACK_PTR, REAL_STACK_PTR, 0, 181 << 1)));
	}
	TEST_FAIL(push_inst(compiler, INS_FORM_IMM(14, SLJIT_LOCALS_REG, REAL_STACK_PTR, 7 * sizeof(sljit_w))));
#endif

	return SLJIT_NO_ERROR;
}

void sljit_fake_enter(struct sljit_compiler *compiler, int args, int general, int local_size)
{
	SLJIT_ASSERT(args >= 0 && args <= SLJIT_NO_GEN_REGISTERS);
	SLJIT_ASSERT(general >= 0 && general <= SLJIT_NO_GEN_REGISTERS);
	SLJIT_ASSERT(args <= general);
	SLJIT_ASSERT(local_size >= 0 && local_size <= SLJIT_MAX_LOCAL_SIZE);
	SLJIT_ASSERT(compiler->general == -1);

	sljit_fake_enter_verbose();

	compiler->general = general;
}

int sljit_emit_return(struct sljit_compiler *compiler, int reg)
{
	FUNCTION_ENTRY();
	SLJIT_ASSERT(reg >= 0 && reg <= SLJIT_NO_REGISTERS);
	SLJIT_ASSERT(compiler->general >= 0);

	sljit_emit_return_verbose();

	if (reg != SLJIT_PREF_RET_REG && reg != SLJIT_NO_REG)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, reg, SLJIT_PREF_RET_REG, reg, 444 << 1)));

	if (compiler->local_size <= 0x7fff) {
		TEST_FAIL(push_inst(compiler, INS_FORM_IMM(14, REAL_STACK_PTR, REAL_STACK_PTR, compiler->local_size & 0xffff)));
	}
	else {
		TEST_FAIL(load_immediate(compiler, 0, compiler->local_size));
		TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, REAL_STACK_PTR, REAL_STACK_PTR, 0, 266 << 1)));
	}

#ifdef SLJIT_CONFIG_PPC_32
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(32, 0, REAL_STACK_PTR, sizeof(sljit_w))));
	if (compiler->general >= 3)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(32, SLJIT_GENERAL_REG3, REAL_STACK_PTR, (-4 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (compiler->general >= 2)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(32, SLJIT_GENERAL_REG2, REAL_STACK_PTR, (-3 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (compiler->general >= 1)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(32, SLJIT_GENERAL_REG1, REAL_STACK_PTR, (-2 * (int)(sizeof(sljit_w))) & 0xffff )));
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(32, SLJIT_LOCALS_REG, REAL_STACK_PTR, (-(int)(sizeof(sljit_w))) & 0xffff )));
#else
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(58, 0, REAL_STACK_PTR, 2 * sizeof(sljit_w))));
	if (compiler->general >= 3)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(58, SLJIT_GENERAL_REG3, REAL_STACK_PTR, (-4 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (compiler->general >= 2)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(58, SLJIT_GENERAL_REG2, REAL_STACK_PTR, (-3 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (compiler->general >= 1)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(58, SLJIT_GENERAL_REG1, REAL_STACK_PTR, (-2 * (int)(sizeof(sljit_w))) & 0xffff )));
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(58, SLJIT_LOCALS_REG, REAL_STACK_PTR, (-(int)(sizeof(sljit_w))) & 0xffff )));
#endif

	TEST_FAIL(push_inst(compiler, INS_FORM_OP0(31, 0, 0x803a6)));
	TEST_FAIL(push_inst(compiler, 0x4e800020));

	return SLJIT_NO_ERROR;
}

// ---------------------------------------------------------------------
//  Operators
// ---------------------------------------------------------------------

int sljit_emit_op1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
	FUNCTION_ENTRY();

	SLJIT_ASSERT(GET_OPCODE(op) >= SLJIT_MOV && GET_OPCODE(op) <= SLJIT_NEG);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_SRC(src, srcw);
	FUNCTION_CHECK_DST(dst, dstw);
	FUNCTION_CHECK_OP1();
#endif
	sljit_emit_op1_verbose();

#ifdef SLJIT_CONFIG_PPC_64
	compiler->mode32 = op & SLJIT_INT_OPERATION;
#endif

	return SLJIT_NO_ERROR;
}

int sljit_emit_op2(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	FUNCTION_ENTRY();

	SLJIT_ASSERT(GET_OPCODE(op) >= SLJIT_ADD && GET_OPCODE(op) <= SLJIT_ASHR);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_SRC(src1, src1w);
	FUNCTION_CHECK_SRC(src2, src2w);
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_op2_verbose();

#ifdef SLJIT_CONFIG_PPC_64
	compiler->mode32 = op & SLJIT_INT_OPERATION;
#endif

	return SLJIT_NO_ERROR;
}

// ---------------------------------------------------------------------
//  Floating point operators
// ---------------------------------------------------------------------

int sljit_is_fpu_available(void)
{
	// Always available
	return 1;
}

int sljit_emit_fop1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
	int dst_freg;

	FUNCTION_ENTRY();

	SLJIT_ASSERT(op >= SLJIT_FCMP && op <= SLJIT_FABS);
#ifdef SLJIT_DEBUG
	FUNCTION_FCHECK(src, srcw);
	FUNCTION_FCHECK(dst, dstw);
#endif
	sljit_emit_fop1_verbose();

	return SLJIT_NO_ERROR;
}

int sljit_emit_fop2(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	int dst_freg;

	FUNCTION_ENTRY();

	SLJIT_ASSERT(op >= SLJIT_FADD && op <= SLJIT_FDIV);
#ifdef SLJIT_DEBUG
	FUNCTION_FCHECK(src1, src1w);
	FUNCTION_FCHECK(src2, src2w);
	FUNCTION_FCHECK(dst, dstw);
#endif
	sljit_emit_fop2_verbose();

	return SLJIT_NO_ERROR;
}

// ---------------------------------------------------------------------
//  Conditional instructions
// ---------------------------------------------------------------------

struct sljit_label* sljit_emit_label(struct sljit_compiler *compiler)
{
	struct sljit_label *label;

	FUNCTION_ENTRY();

	sljit_emit_label_verbose();

	label = NULL;

	return label;
}

struct sljit_jump* sljit_emit_jump(struct sljit_compiler *compiler, int type)
{
	struct sljit_jump *jump;

	FUNCTION_ENTRY();
	SLJIT_ASSERT((type & ~0x1ff) == 0);
	SLJIT_ASSERT((type & 0xff) >= SLJIT_C_EQUAL && (type & 0xff) <= SLJIT_CALL3);

	sljit_emit_jump_verbose();

	jump = NULL;

	return jump;
}

int sljit_emit_ijump(struct sljit_compiler *compiler, int type, int src, sljit_w srcw)
{
	struct sljit_jump *jump;

	FUNCTION_ENTRY();
	SLJIT_ASSERT(type >= SLJIT_JUMP && type <= SLJIT_CALL3);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_SRC(src, srcw);
#endif
	sljit_emit_ijump_verbose();

	return SLJIT_NO_ERROR;
}

int sljit_emit_cond_set(struct sljit_compiler *compiler, int dst, sljit_w dstw, int type)
{
	int reg;

	FUNCTION_ENTRY();
	SLJIT_ASSERT(type >= SLJIT_C_EQUAL && type <= SLJIT_C_NOT_OVERFLOW);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_set_cond_verbose();

	if (dst == SLJIT_NO_REG)
		return SLJIT_NO_ERROR;

	return SLJIT_NO_ERROR;
}

struct sljit_const* sljit_emit_const(struct sljit_compiler *compiler, int dst, sljit_w dstw, sljit_w initval)
{
	struct sljit_const *const_;
	int reg;

	FUNCTION_ENTRY();
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_const_verbose();

	const_ = NULL;

	return const_;
}

void sljit_set_jump_addr(sljit_uw addr, sljit_uw new_addr)
{

}

void sljit_set_const(sljit_uw addr, sljit_w constant)
{

}
