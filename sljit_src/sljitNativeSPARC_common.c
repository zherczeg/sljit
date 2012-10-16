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
	0, 8, 9, 10, 11, 12, 16, 17, 18, 19, 20, 14, 1, 13, 15
};

/* --------------------------------------------------------------------- */
/*  Instrucion forms                                                     */
/* --------------------------------------------------------------------- */

#define D(d)		(reg_map[d] << 25)
#define S1(s1)		(reg_map[s1] << 14)
#define S2(s2)		(reg_map[s2])
#define S1A(s1)		((s1) << 14)
#define S2A(s1)		(s1)
#define IMM_ARG		0x2000
#define IMM(imm)	(((imm) & 0x1fff) | IMM_ARG)

#define DR(dr)		(reg_map[dr])
#define OP1(opcode)	((opcode) << 30)
#define OP2(opcode)	((opcode) << 22)
#define OP3(opcode)	((opcode) << 19)
#define SET_FLAGS	OP3(0x10)

#define ADD		(OP1(0x2) | OP3(0x00))
#define ADDC		(OP1(0x2) | OP3(0x08))
#define JMPL		(OP1(0x2) | OP3(0x38))
#define NOP		(OP1(0x0) | OP2(0x04))
#define OR		(OP1(0x2) | OP3(0x02))
#define RESTORE		(OP1(0x2) | OP3(0x3d))
#define SAVE		(OP1(0x2) | OP3(0x3c))
#define SETHI		(OP1(0x0) | OP2(0x04))
#define SLL		(OP1(0x2) | OP3(0x25))
#define SLLX		(OP1(0x2) | OP3(0x25) | (1 << 12))
#define SRA		(OP1(0x2) | OP3(0x27))
#define SRAX		(OP1(0x2) | OP3(0x27) | (1 << 12))
#define SRL		(OP1(0x2) | OP3(0x26))
#define SRLX		(OP1(0x2) | OP3(0x26) | (1 << 12))
#define SUB		(OP1(0x2) | OP3(0x04))
#define SUBC		(OP1(0x2) | OP3(0x0c))
#define TA		(OP1(0x2) | OP3(0x3a) | (8 << 25))

#if (defined SLJIT_CONFIG_SPARC_32 && SLJIT_CONFIG_SPARC_32)
#define SLL_W		SLL
#else
#define SLL_W		SLLX
#endif

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

/* Creates an index in data_transfer_insts array. */
#define WORD_DATA	0x00
#define BYTE_DATA	0x01
#define HALF_DATA	0x02
#define INT_DATA	0x03
#define SIGNED_DATA	0x04
#define LOAD_DATA	0x08

#define MEM_MASK	0x0f

#define WRITE_BACK	0x00010
#define ARG_TEST	0x00020
#define CUMULATIVE_OP	0x00040
#define IMM_OP		0x00080
#define SRC2_IMM	0x00100

#define UNUSED_DEST	0x00200
#define REG_DEST	0x00400
#define REG1_SOURCE	0x00800
#define REG2_SOURCE	0x01000
#define SLOW_SRC1	0x02000
#define SLOW_SRC2	0x04000
#define SLOW_DEST	0x08000

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

	FAIL_IF(emit_mov_before_return(compiler, op, src, srcw));

	FAIL_IF(push_inst(compiler, JMPL | D(0) | S1A(31) | IMM(8), UNMOVABLE_INS));
	return push_inst(compiler, RESTORE | D(SLJIT_TEMPORARY_REG1) | S1(SLJIT_TEMPORARY_REG1) | S2(0), UNMOVABLE_INS);
}

/* --------------------------------------------------------------------- */
/*  Operators                                                            */
/* --------------------------------------------------------------------- */

#if (defined SLJIT_CONFIG_SPARC_32 && SLJIT_CONFIG_SPARC_32)
#define ARCH_32_64(a, b)	a
#else
#define ARCH_32_64(a, b)	b
#endif

static SLJIT_CONST sljit_ins data_transfer_insts[16] = {
/* s u w */ ARCH_32_64(OP1(3) | OP3(0x04) /* stw */, OP1(3) | OP3(0x0e) /* stx */),
/* s u b */ OP1(3) | OP3(0x05) /* stb */,
/* s u h */ OP1(3) | OP3(0x06) /* sth */,
/* s u i */ OP1(3) | OP3(0x04) /* stw */,

/* s s w */ ARCH_32_64(OP1(3) | OP3(0x04) /* stw */, OP1(3) | OP3(0x0e) /* stx */),
/* s s b */ OP1(3) | OP3(0x05) /* stb */,
/* s s h */ OP1(3) | OP3(0x06) /* sth */,
/* s s i */ OP1(3) | OP3(0x04) /* stw */,

/* l u w */ ARCH_32_64(OP1(3) | OP3(0x00) /* lduw */, OP1(3) | OP3(0x0b) /* ldx */),
/* l u b */ OP1(3) | OP3(0x01) /* ldub */,
/* l u h */ OP1(3) | OP3(0x02) /* lduh */,
/* l u i */ OP1(3) | OP3(0x00) /* lduw */,

/* l s w */ ARCH_32_64(OP1(3) | OP3(0x00) /* lduw */, OP1(3) | OP3(0x0b) /* ldx */),
/* l s b */ OP1(3) | OP3(0x09) /* ldsb */,
/* l s h */ OP1(3) | OP3(0x0a) /* ldsh */,
/* l s i */ ARCH_32_64(OP1(3) | OP3(0x00) /* lduw */, OP1(3) | OP3(0x08) /* ldsw */),
};

#undef ARCH_32_64

/* Can perform an operation using at most 1 instruction. */
static int getput_arg_fast(struct sljit_compiler *compiler, int flags, int reg, int arg, sljit_w argw)
{
	SLJIT_ASSERT(arg & SLJIT_MEM);

	if (!(flags & WRITE_BACK)) {
		if ((!(arg & 0xf0) && argw <= SIMM_MAX && argw >= SIMM_MIN)
				|| ((arg & 0xf0) && (argw & 0x3) == 0)) {
			/* Works for both absoulte and relative addresses (immediate case). */
			if (SLJIT_UNLIKELY(flags & ARG_TEST))
				return 1;
			FAIL_IF(push_inst(compiler, data_transfer_insts[flags & MEM_MASK] | D(reg)
				| S1(arg & 0xf) | ((arg & 0xf0) ? S2((arg >> 4) & 0xf) : IMM(argw)),
				(flags & LOAD_DATA) ? DR(reg) : MOVABLE_INS));
			return -1;
		}
	}
	return (flags & ARG_TEST) ? SLJIT_SUCCESS : 0;
}

/* See getput_arg below.
   Note: can_cache is called only for binary operators. Those
   operators always uses word arguments without write back. */
static int can_cache(int arg, sljit_w argw, int next_arg, sljit_w next_argw)
{
	SLJIT_ASSERT((arg & SLJIT_MEM) && (next_arg & SLJIT_MEM));

	/* Simple operation except for updates. */
	if (arg & 0xf0) {
		argw &= 0x3;
		SLJIT_ASSERT(argw);
		next_argw &= 0x3;
		if ((arg & 0xf0) == (next_arg & 0xf0) && argw == next_argw)
			return 1;
		return 0;
	}

	if (((next_argw - argw) <= SIMM_MAX && (next_argw - argw) >= SIMM_MIN))
		return 1;
	return 0;
}

/* Emit the necessary instructions. See can_cache above. */
static int getput_arg(struct sljit_compiler *compiler, int flags, int reg, int arg, sljit_w argw, int next_arg, sljit_w next_argw)
{
	int base, arg2;

	SLJIT_ASSERT(arg & SLJIT_MEM);
	if (!(next_arg & SLJIT_MEM)) {
		next_arg = 0;
		next_argw = 0;
	}

	base = arg & 0xf;
	if (SLJIT_UNLIKELY(arg & 0xf0)) {
		argw &= 0x3;
		SLJIT_ASSERT(argw != 0);

		/* Using the cache. */
		if (((SLJIT_MEM | (arg & 0xf0)) == compiler->cache_arg) && (argw == compiler->cache_argw))
			arg2 = TMP_REG3;
		else {
			if ((arg & 0xf0) == (next_arg & 0xf0) && argw == (next_argw & 0x3)) {
				compiler->cache_arg = SLJIT_MEM | (arg & 0xf0);
				compiler->cache_argw = argw;
				arg2 = TMP_REG3;
			}
			else if ((flags & LOAD_DATA) && reg != base && (reg << 4) != (arg & 0xf0))
				arg2 = reg;
			else /* It must be a mov operation, so tmp1 must be free to use. */
				arg2 = TMP_REG1;
			FAIL_IF(push_inst(compiler, SLL_W | D(arg2) | S1((arg >> 4) & 0xf) | IMM_ARG | argw, DR(arg2)));
		}
	}
	else {
		/* Using the cache. */
		if ((compiler->cache_arg == SLJIT_MEM) && (argw - compiler->cache_argw) <= SIMM_MAX && (argw - compiler->cache_argw) >= SIMM_MIN) {
			if (argw != compiler->cache_argw) {
				FAIL_IF(push_inst(compiler, ADD | D(TMP_REG3) | S1(TMP_REG3) | IMM(argw - compiler->cache_argw), DR(TMP_REG3)));
				compiler->cache_argw = argw;
			}
			arg2 = TMP_REG3;
		} else {
			if ((next_argw - argw) <= SIMM_MAX && (next_argw - argw) >= SIMM_MIN) {
				compiler->cache_arg = SLJIT_MEM;
				compiler->cache_argw = argw;
				arg2 = TMP_REG3;
			}
			else if ((flags & LOAD_DATA) && reg != base)
				arg2 = reg;
			else /* It must be a mov operation, so tmp1 must be free to use. */
				arg2 = TMP_REG1;
			FAIL_IF(load_immediate(compiler, arg2, argw));
		}
	}

	if (!base)
		return push_inst(compiler, data_transfer_insts[flags & MEM_MASK] | D(reg) | S1(arg2) | IMM(0), (flags & LOAD_DATA) ? DR(reg) : MOVABLE_INS);
	if (!(flags & WRITE_BACK))
		return push_inst(compiler, data_transfer_insts[flags & MEM_MASK] | D(reg) | S1(base) | S2(arg2), (flags & LOAD_DATA) ? DR(reg) : MOVABLE_INS);
	FAIL_IF(push_inst(compiler, data_transfer_insts[flags & MEM_MASK] | D(reg) | S1(base) | S2(arg2), (flags & LOAD_DATA) ? DR(reg) : MOVABLE_INS));
	return push_inst(compiler, ADD | D(base) | S1(base) | S2(arg2), DR(base));
}

static SLJIT_INLINE int emit_op_mem(struct sljit_compiler *compiler, int flags, int reg, int arg, sljit_w argw)
{
	if (getput_arg_fast(compiler, flags, reg, arg, argw))
		return compiler->error;
	compiler->cache_arg = 0;
	compiler->cache_argw = 0;
	return getput_arg(compiler, flags, reg, arg, argw, 0, 0);
}

static int emit_op(struct sljit_compiler *compiler, int op, int flags,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	/* arg1 goes to TMP_REG1 or src reg
	   arg2 goes to TMP_REG2, imm or src reg
	   TMP_REG3 can be used for caching
	   result goes to TMP_REG2, so put result can use TMP_REG1 and TMP_REG3. */
	int dst_r = TMP_REG2;
	int src1_r;
	sljit_w src2_r = 0;
	int sugg_src2_r = TMP_REG2;

	compiler->cache_arg = 0;
	compiler->cache_argw = 0;

	if (dst >= SLJIT_TEMPORARY_REG1 && dst <= TMP_REG3) {
		dst_r = dst;
		flags |= REG_DEST;
		if (GET_OPCODE(op) >= SLJIT_MOV && GET_OPCODE(op) <= SLJIT_MOVU_SI)
			sugg_src2_r = dst_r;
	}
	else if (dst == SLJIT_UNUSED) {
		if (op >= SLJIT_MOV && op <= SLJIT_MOVU_SI && !(src2 & SLJIT_MEM))
			return SLJIT_SUCCESS;
		if (GET_FLAGS(op))
			flags |= UNUSED_DEST;
	}
	else if ((dst & SLJIT_MEM) && !getput_arg_fast(compiler, flags | ARG_TEST, TMP_REG1, dst, dstw))
		flags |= SLOW_DEST;

	if (flags & IMM_OP) {
		if ((src2 & SLJIT_IMM) && src2w) {
			if (src2w <= SIMM_MAX && src2w >= SIMM_MIN) {
				flags |= SRC2_IMM;
				src2_r = src2w;
			}
		}
		if ((src1 & SLJIT_IMM) && src1w && (flags & CUMULATIVE_OP) && !(flags & SRC2_IMM)) {
			if (src1w <= SIMM_MAX && src1w >= SIMM_MIN) {
				flags |= SRC2_IMM;
				src2_r = src1w;

				/* And swap arguments. */
				src1 = src2;
				src1w = src2w;
				src2 = SLJIT_IMM;
				/* src2w = src2_r unneeded. */
			}
		}
	}

	/* Source 1. */
	if (src1 >= SLJIT_TEMPORARY_REG1 && src1 <= TMP_REG3) {
		src1_r = src1;
		flags |= REG1_SOURCE;
	}
	else if (src1 & SLJIT_IMM) {
		if (src1w) {
			FAIL_IF(load_immediate(compiler, TMP_REG1, src1w));
			src1_r = TMP_REG1;
		}
		else
			src1_r = 0;
	}
	else {
		if (getput_arg_fast(compiler, flags | LOAD_DATA, TMP_REG1, src1, src1w))
			FAIL_IF(compiler->error);
		else
			flags |= SLOW_SRC1;
		src1_r = TMP_REG1;
	}

	/* Source 2. */
	if (src2 >= SLJIT_TEMPORARY_REG1 && src2 <= TMP_REG3) {
		src2_r = src2;
		flags |= REG2_SOURCE;
		if (!(flags & REG_DEST) && GET_OPCODE(op) >= SLJIT_MOV && GET_OPCODE(op) <= SLJIT_MOVU_SI)
			dst_r = src2_r;
	}
	else if (src2 & SLJIT_IMM) {
		if (!(flags & SRC2_IMM)) {
			if (src2w || (GET_OPCODE(op) >= SLJIT_MOV && GET_OPCODE(op) <= SLJIT_MOVU_SI)) {
				FAIL_IF(load_immediate(compiler, sugg_src2_r, src2w));
				src2_r = sugg_src2_r;
			}
			else
				src2_r = 0;
		}
	}
	else {
		if (getput_arg_fast(compiler, flags | LOAD_DATA, sugg_src2_r, src2, src2w))
			FAIL_IF(compiler->error);
		else
			flags |= SLOW_SRC2;
		src2_r = sugg_src2_r;
	}

	if ((flags & (SLOW_SRC1 | SLOW_SRC2)) == (SLOW_SRC1 | SLOW_SRC2)) {
		SLJIT_ASSERT(src2_r == TMP_REG2);
		if (!can_cache(src1, src1w, src2, src2w) && can_cache(src1, src1w, dst, dstw)) {
			FAIL_IF(getput_arg(compiler, flags | LOAD_DATA, TMP_REG2, src2, src2w, src1, src1w));
			FAIL_IF(getput_arg(compiler, flags | LOAD_DATA, TMP_REG1, src1, src1w, dst, dstw));
		}
		else {
			FAIL_IF(getput_arg(compiler, flags | LOAD_DATA, TMP_REG1, src1, src1w, src2, src2w));
			FAIL_IF(getput_arg(compiler, flags | LOAD_DATA, TMP_REG2, src2, src2w, dst, dstw));
		}
	}
	else if (flags & SLOW_SRC1)
		FAIL_IF(getput_arg(compiler, flags | LOAD_DATA, TMP_REG1, src1, src1w, dst, dstw));
	else if (flags & SLOW_SRC2)
		FAIL_IF(getput_arg(compiler, flags | LOAD_DATA, sugg_src2_r, src2, src2w, dst, dstw));

	FAIL_IF(emit_single_op(compiler, op, flags, dst_r, src1_r, src2_r));

	if (dst & SLJIT_MEM) {
		if (!(flags & SLOW_DEST)) {
			getput_arg_fast(compiler, flags, dst_r, dst, dstw);
			return compiler->error;
		}
		return getput_arg(compiler, flags, dst_r, dst, dstw, 0, 0);
	}

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE int sljit_emit_op0(struct sljit_compiler *compiler, int op)
{
	CHECK_ERROR();
	check_sljit_emit_op0(compiler, op);

	op = GET_OPCODE(op);
	switch (op) {
	case SLJIT_BREAKPOINT:
		return push_inst(compiler, TA, UNMOVABLE_INS);
	case SLJIT_NOP:
		return push_inst(compiler, NOP, UNMOVABLE_INS);
	}

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE int sljit_emit_op1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
#if (defined SLJIT_CONFIG_SPARC_32 && SLJIT_CONFIG_SPARC_32)
	#define inp_flags 0
#endif

	CHECK_ERROR();
	check_sljit_emit_op1(compiler, op, dst, dstw, src, srcw);
	ADJUST_LOCAL_OFFSET(dst, dstw);
	ADJUST_LOCAL_OFFSET(src, srcw);

	SLJIT_COMPILE_ASSERT(SLJIT_MOV + 7 == SLJIT_MOVU, movu_offset);

	switch (GET_OPCODE(op)) {
	case SLJIT_MOV:
		return emit_op(compiler, SLJIT_MOV, inp_flags | WORD_DATA, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOV_UI:
		return emit_op(compiler, SLJIT_MOV_UI, inp_flags | INT_DATA, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOV_SI:
		return emit_op(compiler, SLJIT_MOV_SI, inp_flags | INT_DATA | SIGNED_DATA, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOV_UB:
		return emit_op(compiler, SLJIT_MOV_UB, inp_flags | BYTE_DATA, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (unsigned char)srcw : srcw);

	case SLJIT_MOV_SB:
		return emit_op(compiler, SLJIT_MOV_SB, inp_flags | BYTE_DATA | SIGNED_DATA, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (signed char)srcw : srcw);

	case SLJIT_MOV_UH:
		return emit_op(compiler, SLJIT_MOV_UH, inp_flags | HALF_DATA, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (unsigned short)srcw : srcw);

	case SLJIT_MOV_SH:
		return emit_op(compiler, SLJIT_MOV_SH, inp_flags | HALF_DATA | SIGNED_DATA, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (signed short)srcw : srcw);

	case SLJIT_MOVU:
		return emit_op(compiler, SLJIT_MOV, inp_flags | WORD_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOVU_UI:
		return emit_op(compiler, SLJIT_MOV_UI, inp_flags | INT_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOVU_SI:
		return emit_op(compiler, SLJIT_MOV_SI, inp_flags | INT_DATA | SIGNED_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOVU_UB:
		return emit_op(compiler, SLJIT_MOV_UB, inp_flags | BYTE_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (unsigned char)srcw : srcw);

	case SLJIT_MOVU_SB:
		return emit_op(compiler, SLJIT_MOV_SB, inp_flags | BYTE_DATA | SIGNED_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (signed char)srcw : srcw);

	case SLJIT_MOVU_UH:
		return emit_op(compiler, SLJIT_MOV_UH, inp_flags | HALF_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (unsigned short)srcw : srcw);

	case SLJIT_MOVU_SH:
		return emit_op(compiler, SLJIT_MOV_SH, inp_flags | HALF_DATA | SIGNED_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (signed short)srcw : srcw);

	case SLJIT_NOT:
		return emit_op(compiler, op, inp_flags, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_NEG:
		return emit_op(compiler, SLJIT_SUB | GET_ALL_FLAGS(op), inp_flags | IMM_OP, dst, dstw, SLJIT_IMM, 0, src, srcw);

	case SLJIT_CLZ:
		return emit_op(compiler, op, inp_flags, dst, dstw, TMP_REG1, 0, src, srcw);
	}

#if (defined SLJIT_CONFIG_MIPS_32 && SLJIT_CONFIG_MIPS_32)
	#undef inp_flags
#endif
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
