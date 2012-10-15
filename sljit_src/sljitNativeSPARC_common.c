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

SLJIT_API_FUNC_ATTRIBUTE SLJIT_CONST char* sljit_get_platform_name(void)
{
	return "SPARC" SLJIT_CPUINFO;
}

/* Length of an instruction word
   Both for sparc-32 and sparc-64 */
typedef sljit_ui sljit_ins;

#define TMP_REG1	(SLJIT_NO_REGISTERS + 1)
#define TMP_REG2	(SLJIT_NO_REGISTERS + 2)
#define TMP_REG3	(SLJIT_NO_REGISTERS + 3)

static SLJIT_CONST sljit_ub reg_map[SLJIT_NO_REGISTERS + 7] = {
	0, 8, 9, 10, 11, 12, 16, 17, 18, 19, 20, 14, 1, 13, 14
};

/* --------------------------------------------------------------------- */
/*  Instrucion forms                                                     */
/* --------------------------------------------------------------------- */

#define D(d)		(reg_map[d] << 25)
#define S1(s1)		(reg_map[s1] << 14)
#define S2(s2)		(reg_map[s2])
#define S1A(s1)		((s1) << 14)
#define S2A(s1)		(s1)
#define IMM(imm)	(((imm) & 0x1fff) | 0x2000)

#define DR(dr)		(reg_map[dr])
#define OP1(opcode)	((opcode) << 30)
#define OP2(opcode)	((opcode) << 22)
#define OP3(opcode)	((opcode) << 19)

#define JMPL		(OP1(0x2) | OP3(0x38))
#define NOP		(OP1(0x0) | OP2(0x4))
#define OR		(OP1(0x2) | OP3(0x2))
#define RESTORE		(OP1(0x2) | OP3(0x3d))
#define SAVE		(OP1(0x2) | OP3(0x3c))
#define SETHI		(OP1(0x0) | OP2(0x4))

#define SIMM_MAX	(0x0fff)
#define SIMM_MIN	(-0x1000)

/* dest_reg is the absolute name of the register
   Useful for reordering instructions in the delay slot. */
static int push_inst(struct sljit_compiler *compiler, sljit_ins ins, int delay_slot)
{
	sljit_ins *ptr = (sljit_ins*)ensure_buf(compiler, sizeof(sljit_ins));
	FAIL_IF(!ptr);
	*ptr = ins;
	compiler->size++;
	compiler->delay_slot = delay_slot;
	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE void* sljit_generate_code(struct sljit_compiler *compiler)
{
	struct sljit_memory_fragment *buf;
	sljit_ins *code;
	sljit_ins *code_ptr;
	sljit_ins *buf_ptr;
	sljit_ins *buf_end;
	sljit_uw word_count;
	sljit_uw addr;

	struct sljit_label *label;
	struct sljit_jump *jump;
	struct sljit_const *const_;

	CHECK_ERROR_PTR();
	check_sljit_generate_code(compiler);
	reverse_buf(compiler);

	code = (sljit_ins*)SLJIT_MALLOC_EXEC(compiler->size * sizeof(sljit_ins));
	PTR_FAIL_WITH_EXEC_IF(code);
	buf = compiler->buf;

	code_ptr = code;
	word_count = 0;
	label = compiler->labels;
	jump = compiler->jumps;
	const_ = compiler->consts;
	do {
		buf_ptr = (sljit_ins*)buf->memory;
		buf_end = buf_ptr + (buf->used_size >> 2);
		do {
			*code_ptr = *buf_ptr++;
			SLJIT_ASSERT(!label || label->size >= word_count);
			SLJIT_ASSERT(!jump || jump->addr >= word_count);
			SLJIT_ASSERT(!const_ || const_->addr >= word_count);
			/* These structures are ordered by their address. */
			if (label && label->size == word_count) {
				/* Just recording the address. */
				label->addr = (sljit_uw)code_ptr;
				label->size = code_ptr - code;
				label = label->next;
			}
			if (jump && jump->addr == word_count) {
#if (defined SLJIT_CONFIG_SPARC_32 && SLJIT_CONFIG_SPARC_32)
				jump->addr = (sljit_uw)(code_ptr - 3);
#else
				jump->addr = (sljit_uw)(code_ptr - 6);
#endif
//				code_ptr = optimize_jump(jump, code_ptr, code);
				jump = jump->next;
			}
			if (const_ && const_->addr == word_count) {
				/* Just recording the address. */
				const_->addr = (sljit_uw)code_ptr;
				const_ = const_->next;
			}
			code_ptr ++;
			word_count ++;
		} while (buf_ptr < buf_end);

		buf = buf->next;
	} while (buf);

	if (label && label->size == word_count) {
		label->addr = (sljit_uw)code_ptr;
		label->size = code_ptr - code;
		label = label->next;
	}

	SLJIT_ASSERT(!label);
	SLJIT_ASSERT(!jump);
	SLJIT_ASSERT(!const_);
	SLJIT_ASSERT(code_ptr - code <= (int)compiler->size);

	compiler->error = SLJIT_ERR_COMPILED;
	compiler->executable_size = compiler->size * sizeof(sljit_ins);
	SLJIT_CACHE_FLUSH(code, code_ptr);
	return code;
}

/* --------------------------------------------------------------------- */
/*  Entry, exit                                                          */
/* --------------------------------------------------------------------- */

#if (defined SLJIT_CONFIG_SPARC_32 && SLJIT_CONFIG_SPARC_32)
#include "sljitNativeSPARC_32.c"
#else
#include "sljitNativeSPARC_64.c"
#endif

SLJIT_API_FUNC_ATTRIBUTE int sljit_emit_enter(struct sljit_compiler *compiler, int args, int temporaries, int saveds, int local_size)
{
	CHECK_ERROR();
	check_sljit_emit_enter(compiler, args, temporaries, saveds, local_size);

	compiler->temporaries = temporaries;
	compiler->saveds = saveds;
#if (defined SLJIT_DEBUG && SLJIT_DEBUG)
	compiler->logical_local_size = local_size;
#endif

	local_size += 23 * sizeof(sljit_w);
	local_size = (local_size + 7) & ~0x7;
	compiler->local_size = local_size;

	if (local_size <= SIMM_MAX) {
		FAIL_IF(push_inst(compiler, SAVE | D(SLJIT_LOCALS_REG) | S1(SLJIT_LOCALS_REG) | IMM(-local_size), UNMOVABLE_INS));
	}
	else {
		FAIL_IF(load_immediate(compiler, TMP_REG1, -local_size));
		FAIL_IF(push_inst(compiler, SAVE | D(SLJIT_LOCALS_REG) | S1(SLJIT_LOCALS_REG) | S2(TMP_REG1), UNMOVABLE_INS));
	}

	if (args >= 1)
		FAIL_IF(push_inst(compiler, OR | D(SLJIT_SAVED_REG1) | S1(0) | S2A(24), DR(SLJIT_SAVED_REG1)));
	if (args >= 2)
		FAIL_IF(push_inst(compiler, OR | D(SLJIT_SAVED_REG2) | S1(0) | S2A(25), DR(SLJIT_SAVED_REG2)));
	if (args >= 3)
		FAIL_IF(push_inst(compiler, OR | D(SLJIT_SAVED_REG3) | S1(0) | S2A(26), DR(SLJIT_SAVED_REG3)));

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE void sljit_set_context(struct sljit_compiler *compiler, int args, int temporaries, int saveds, int local_size)
{
	CHECK_ERROR_VOID();
	check_sljit_set_context(compiler, args, temporaries, saveds, local_size);

	compiler->temporaries = temporaries;
	compiler->saveds = saveds;
#if (defined SLJIT_DEBUG && SLJIT_DEBUG)
	compiler->logical_local_size = local_size;
#endif

	local_size += 23 * sizeof(sljit_w);
	compiler->local_size = (local_size + 7) & ~0x7;
}

SLJIT_API_FUNC_ATTRIBUTE int sljit_emit_return(struct sljit_compiler *compiler, int op, int src, sljit_w srcw)
{
	CHECK_ERROR();
	check_sljit_emit_return(compiler, op, src, srcw);
	ADJUST_LOCAL_OFFSET(src, srcw);

	FAIL_IF(push_inst(compiler, JMPL | D(0) | S1A(31) | IMM(8), UNMOVABLE_INS));
	return push_inst(compiler, RESTORE | D(SLJIT_TEMPORARY_REG1) | S1(SLJIT_TEMPORARY_REG1) | S2(0), UNMOVABLE_INS);
}

/* --------------------------------------------------------------------- */
/*  Operators                                                            */
/* --------------------------------------------------------------------- */

SLJIT_API_FUNC_ATTRIBUTE int sljit_emit_op0(struct sljit_compiler *compiler, int op)
{
	CHECK_ERROR();
	check_sljit_emit_op0(compiler, op);

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE int sljit_emit_op1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
	CHECK_ERROR();
	check_sljit_emit_op1(compiler, op, dst, dstw, src, srcw);
	ADJUST_LOCAL_OFFSET(dst, dstw);
	ADJUST_LOCAL_OFFSET(src, srcw);

	SLJIT_COMPILE_ASSERT(SLJIT_MOV + 7 == SLJIT_MOVU, movu_offset);

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE int sljit_emit_op2(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	CHECK_ERROR();
	check_sljit_emit_op2(compiler, op, dst, dstw, src1, src1w, src2, src2w);
	ADJUST_LOCAL_OFFSET(dst, dstw);
	ADJUST_LOCAL_OFFSET(src1, src1w);
	ADJUST_LOCAL_OFFSET(src2, src2w);

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE int sljit_get_register_index(int reg)
{
	check_sljit_get_register_index(reg);
	return reg_map[reg];
}

SLJIT_API_FUNC_ATTRIBUTE int sljit_emit_op_custom(struct sljit_compiler *compiler,
	void *instruction, int size)
{
	CHECK_ERROR();
	check_sljit_emit_op_custom(compiler, instruction, size);
	SLJIT_ASSERT(size == 4);

	return push_inst(compiler, *(sljit_ins*)instruction, UNMOVABLE_INS);
}

/* --------------------------------------------------------------------- */
/*  Floating point operators                                             */
/* --------------------------------------------------------------------- */

SLJIT_API_FUNC_ATTRIBUTE int sljit_is_fpu_available(void)
{
	return 1;
}

SLJIT_API_FUNC_ATTRIBUTE int sljit_emit_fop1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
	CHECK_ERROR();
	check_sljit_emit_fop1(compiler, op, dst, dstw, src, srcw);

	compiler->cache_arg = 0;
	compiler->cache_argw = 0;

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE int sljit_emit_fop2(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	CHECK_ERROR();
	check_sljit_emit_fop2(compiler, op, dst, dstw, src1, src1w, src2, src2w);

	compiler->cache_arg = 0;
	compiler->cache_argw = 0;

	return SLJIT_SUCCESS;
}

/* --------------------------------------------------------------------- */
/*  Other instructions                                                   */
/* --------------------------------------------------------------------- */

SLJIT_API_FUNC_ATTRIBUTE int sljit_emit_fast_enter(struct sljit_compiler *compiler, int dst, sljit_w dstw)
{
	CHECK_ERROR();
	check_sljit_emit_fast_enter(compiler, dst, dstw);
	ADJUST_LOCAL_OFFSET(dst, dstw);

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE int sljit_emit_fast_return(struct sljit_compiler *compiler, int src, sljit_w srcw)
{
	CHECK_ERROR();
	check_sljit_emit_fast_return(compiler, src, srcw);
	ADJUST_LOCAL_OFFSET(src, srcw);

	return SLJIT_SUCCESS;
}

/* --------------------------------------------------------------------- */
/*  Conditional instructions                                             */
/* --------------------------------------------------------------------- */

SLJIT_API_FUNC_ATTRIBUTE struct sljit_label* sljit_emit_label(struct sljit_compiler *compiler)
{
	struct sljit_label *label;

	CHECK_ERROR_PTR();
	check_sljit_emit_label(compiler);

	if (compiler->last_label && compiler->last_label->size == compiler->size)
		return compiler->last_label;

	label = (struct sljit_label*)ensure_abuf(compiler, sizeof(struct sljit_label));
	PTR_FAIL_IF(!label);
	set_label(label, compiler);
	compiler->delay_slot = UNMOVABLE_INS;
	return label;
}

SLJIT_API_FUNC_ATTRIBUTE struct sljit_jump* sljit_emit_jump(struct sljit_compiler *compiler, int type)
{
	struct sljit_jump *jump;

	CHECK_ERROR_PTR();
	check_sljit_emit_jump(compiler, type);

	jump = (struct sljit_jump*)ensure_abuf(compiler, sizeof(struct sljit_jump));
	PTR_FAIL_IF(!jump);
	set_jump(jump, compiler, type & SLJIT_REWRITABLE_JUMP);
	type &= 0xff;

	return jump;
}

SLJIT_API_FUNC_ATTRIBUTE int sljit_emit_ijump(struct sljit_compiler *compiler, int type, int src, sljit_w srcw)
{
	CHECK_ERROR();
	check_sljit_emit_ijump(compiler, type, src, srcw);
	ADJUST_LOCAL_OFFSET(src, srcw);

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE int sljit_emit_cond_value(struct sljit_compiler *compiler, int op, int dst, sljit_w dstw, int type)
{
	CHECK_ERROR();
	check_sljit_emit_cond_value(compiler, op, dst, dstw, type);
	ADJUST_LOCAL_OFFSET(dst, dstw);

	if (dst == SLJIT_UNUSED)
		return SLJIT_SUCCESS;

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE struct sljit_const* sljit_emit_const(struct sljit_compiler *compiler, int dst, sljit_w dstw, sljit_w init_value)
{
	struct sljit_const *const_;

	CHECK_ERROR_PTR();
	check_sljit_emit_const(compiler, dst, dstw, init_value);
	ADJUST_LOCAL_OFFSET(dst, dstw);

	const_ = (struct sljit_const*)ensure_abuf(compiler, sizeof(struct sljit_const));
	PTR_FAIL_IF(!const_);
	set_const(const_, compiler);

	return const_;
}
