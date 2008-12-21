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

// Last register + 1
#define TMP_REG1	(SLJIT_GENERAL_REG3 + 1)
#define TMP_REG2	(SLJIT_GENERAL_REG3 + 2)
#define TMP_REG3	(SLJIT_GENERAL_REG3 + 3)
#define TMP_PC		(SLJIT_GENERAL_REG3 + 4)

static sljit_ub reg_map[SLJIT_NO_REGISTERS + 5] = {
   0, 0, 1, 2, 4, 5, 6, 3, 7, 8, 15
};

static int push_cpool(struct sljit_compiler *compiler)
{
	sljit_uw* inst;
	sljit_uw* cpool_ptr;
	sljit_uw* cpool_end;

	SLJIT_ASSERT(compiler->cpool_fill > 0);
	inst = (sljit_uw*)ensure_buf(compiler, sizeof(sljit_uw));
	if (!inst) {
		compiler->error = SLJIT_MEMORY_ERROR;
		return 1;
	}
	compiler->size++;
	*inst = 0xff000000 | compiler->cpool_fill;

	cpool_ptr = compiler->cpool;
	cpool_end = cpool_ptr + compiler->cpool_fill;
	while (cpool_ptr < cpool_end) {
		inst = (sljit_uw*)ensure_buf(compiler, sizeof(sljit_uw));
		if (!inst) {
			compiler->error = SLJIT_MEMORY_ERROR;
			return 1;
		}
		compiler->size++;
		*inst = *cpool_ptr++;
	}
	compiler->cpool_diff = 0xffffffff;
	compiler->cpool_fill = 0;
	return 0;
}

static sljit_uw* alloc_inst(struct sljit_compiler *compiler)
{
	sljit_uw* inst;

	// Test wheter the constant pool must be copied
	if (compiler->cpool_diff != 0xffffffff && compiler->size - compiler->cpool_diff >= (2040 / sizeof(sljit_w)))
		if (push_cpool(compiler))
			return NULL;

	inst = (sljit_uw*)ensure_buf(compiler, sizeof(sljit_uw));
	if (!inst) {
		compiler->error = SLJIT_MEMORY_ERROR;
		return NULL;
	}
	compiler->size++;
	return inst;
}

// if const_ == 0, that means variable instruction
static sljit_uw* alloc_const_inst(struct sljit_compiler *compiler, sljit_w const_)
{
	sljit_uw* inst;
	sljit_uw* cpool_ptr;
	sljit_uw* cpool_end;

	// Test wheter the constant pool must be copied
	compiler->cpool_index = CPOOL_SIZE;
	if (compiler->cpool_diff != 0xffffffff && compiler->size - compiler->cpool_diff >= (2040 / sizeof(sljit_w))) {
		if (push_cpool(compiler))
			return NULL;
	}
	else if (const_ != 0 && compiler->cpool_fill > 0) {
		cpool_ptr = compiler->cpool;
		cpool_end = cpool_ptr + compiler->cpool_fill;
		while (cpool_ptr < cpool_end) {
			if (*cpool_ptr == const_) {
				compiler->cpool_index = cpool_ptr - compiler->cpool;
				break;
			}
			cpool_ptr++;
		}
	}

	if (compiler->cpool_index == CPOOL_SIZE) {
		// Must allocate a new place
		if (compiler->cpool_fill < CPOOL_SIZE) {
			compiler->cpool_index = compiler->cpool_fill;
			compiler->cpool_fill++;
		}
		else {
			if (push_cpool(compiler))
				return NULL;
			compiler->cpool_index = 0;
			compiler->cpool_fill = 1;
		}
	}

	inst = (sljit_uw*)ensure_buf(compiler, sizeof(sljit_uw));
	if (!inst) {
		compiler->error = SLJIT_MEMORY_ERROR;
		return NULL;
	}
	compiler->size++;
	return inst;
}

static INLINE void patch_pc_rel(sljit_uw *last_pc_patch, sljit_uw *code_ptr)
{
	sljit_uw diff;

	while (last_pc_patch < code_ptr) {
		if ((*last_pc_patch & 0x0c0f0000) == 0x040f0000) {
			diff = code_ptr - last_pc_patch;
			SLJIT_ASSERT((*last_pc_patch & 0x3) == 0 && (*last_pc_patch & (1 << 25)) == 0);
			if (diff >= 2 || (*last_pc_patch & 0xfff) > 0) {
				diff = (diff - 2) << 2;
				SLJIT_ASSERT((*last_pc_patch & 0xfff) + diff <= 0xfff);
				*last_pc_patch += diff;
			}
			else {
				// In practice, this should never happen
				SLJIT_ASSERT(diff == 1);
				*last_pc_patch |= 0x004;
				*last_pc_patch &= ~(1 << 23);
			}
		}
		last_pc_patch++;
	}
}

void* sljit_generate_code(struct sljit_compiler *compiler)
{
	struct sljit_memory_fragment *buf;
	sljit_uw *code;
	sljit_uw *code_ptr;
	sljit_uw *last_pc_patch;
	sljit_uw *buf_ptr;
	sljit_uw *buf_end;
	sljit_uw size;
	sljit_uw copy;

	struct sljit_label *label;
	struct sljit_jump *jump;
	struct sljit_const *const_;

	FUNCTION_ENTRY();
	SLJIT_ASSERT(compiler->size > 0);
	reverse_buf(compiler);

	// Second code generation pass
	size = compiler->size + compiler->cpool_fill;
	code = SLJIT_MALLOC(size * sizeof(sljit_uw));
	if (!code) {
		compiler->error = SLJIT_MEMORY_ERROR;
		return NULL;
	}
	buf = compiler->buf;

	code_ptr = code;
	last_pc_patch = code;
	copy = 0;
	label = compiler->labels;
	jump = compiler->jumps;
	const_ = compiler->consts;
	do {
		buf_ptr = (sljit_uw*)buf->memory;
		buf_end = buf_ptr + (buf->used_size >> 2);
		while (buf_ptr < buf_end) {
			if (copy > 0) {
				*code_ptr++ = *buf_ptr++;
				if (--copy == 0)
					last_pc_patch = code_ptr;
			}
			else if ((*buf_ptr & 0xf0000000) != 0xf0000000) {
				*code_ptr++ = *buf_ptr++;
			}
			else if ((*buf_ptr & 0x0f000000) == 0x0f000000) {
				// Fortunately, no need to shift
				copy = *buf_ptr++ & 0x00ffffff;
				SLJIT_ASSERT(copy > 0);
				// unconditional branch
				*code_ptr++ = 0xea000000 | ((copy - 1) & 0x00ffffff);
				patch_pc_rel(last_pc_patch, code_ptr);
			}
			else {
				SLJIT_ASSERT_IMPOSSIBLE();
			}
		}
		buf = buf->next;
	} while (buf != NULL);

	if (compiler->cpool_fill > 0) {
		patch_pc_rel(last_pc_patch, code_ptr);

		buf_ptr = compiler->cpool;
		buf_end = buf_ptr + compiler->cpool_fill;
		while (buf_ptr < buf_end)
			*code_ptr++ = *buf_ptr++;
	}

	SLJIT_ASSERT(copy == 0);
	SLJIT_ASSERT(code_ptr - code == size);
	compiler->error = SLJIT_CODE_GENERATED;
	return code;
}

void sljit_free_code(void* code)
{
	SLJIT_FREE(code);
}

#define MOV_REG(dst, src)	0xe1a00000 | reg_map[src] | (reg_map[dst] << 12)

int sljit_emit_enter(struct sljit_compiler *compiler, int type, int args, int general)
{
	sljit_uw *inst;

	FUNCTION_ENTRY();
	// TODO: support the others
	SLJIT_ASSERT(type == CALL_TYPE_CDECL);
	SLJIT_ASSERT(args >= 0 && args <= SLJIT_NO_GEN_REGISTERS);
	SLJIT_ASSERT(general >= 0 && general <= SLJIT_NO_GEN_REGISTERS);
	SLJIT_ASSERT(args <= general);
	SLJIT_ASSERT(compiler->general == -1);

	sljit_emit_enter_verbose();

	compiler->general = general;

	inst = alloc_inst(compiler);
	TEST_MEM_ERROR(inst);
	// Push general registers, temporary registers
        // stmdb sp!, {..., lr}
	*inst = 0xe92d0000 | 0x4000 | 0x0180;
	if (general >= 3)
		*inst |= 0x0070;
	else if (general >= 2)
		*inst |= 0x0030;
	else if (general >= 1)
		*inst |= 0x0010;

	if (args >= 1) {
		inst = alloc_inst(compiler);
		TEST_MEM_ERROR(inst);
		*inst = MOV_REG(SLJIT_GENERAL_REG1, SLJIT_TEMPORARY_REG1);
	}
	if (args >= 2) {
		inst = alloc_inst(compiler);
		TEST_MEM_ERROR(inst);
		*inst = MOV_REG(SLJIT_GENERAL_REG2, SLJIT_TEMPORARY_REG2);
	}
	if (args >= 3) {
		inst = alloc_inst(compiler);
		TEST_MEM_ERROR(inst);
		*inst = MOV_REG(SLJIT_GENERAL_REG3, SLJIT_TEMPORARY_REG3);
	}

	return SLJIT_NO_ERROR;
}

int sljit_emit_return(struct sljit_compiler *compiler, int reg)
{
	sljit_uw *inst;

	FUNCTION_ENTRY();
	SLJIT_ASSERT(reg >= 0 && reg <= SLJIT_NO_REGISTERS);
	SLJIT_ASSERT(compiler->general >= 0);

	sljit_emit_return_verbose();

	if (reg != SLJIT_PREF_RET_REG && reg != SLJIT_NO_REG) {
		inst = alloc_inst(compiler);
		TEST_MEM_ERROR(inst);
		*inst = MOV_REG(SLJIT_PREF_RET_REG, reg);
	}

	inst = alloc_inst(compiler);
	TEST_MEM_ERROR(inst);
	// Push general registers, temporary registers
        // ldmia sp!, {..., lr}
	*inst = 0xe8bd0000 | 0x4000 | 0x0180;
	if (compiler->general >= 3)
		*inst |= 0x0070;
	else if (compiler->general >= 2)
		*inst |= 0x0030;
	else if (compiler->general >= 1)
		*inst |= 0x0010;

	inst = alloc_inst(compiler);
	TEST_MEM_ERROR(inst);
	// mov pc, lr
	*inst = 0xe1a0f00e;

	return SLJIT_NO_ERROR;
}

// ---------------------------------------------------------------------
//  Operators
// ---------------------------------------------------------------------

#define OP1_OFFSET	(SLJIT_ASHR + 1)

#define TEST_ERROR(ret) \
	if (ret) { \
		return SLJIT_MEMORY_ERROR; \
	}

#define EMIT_DATA_PROCESS_INS(opcode, dst, src1, src2) \
	(0xe0000000 | ((opcode) << 20) | (reg_map[dst] << 12) | (reg_map[src1] << 16) | (src2))
#define EMIT_DATA_TRANSFER(add, load, target, base1, base2) \
	(0xe5000000 | ((add) << 23) | ((load) << 20) | (reg_map[target] << 12) | (reg_map[base1] << 16) | (base2))

// flags:
// Arguments are swapped
#define ARGS_SWAPPED	0x1
// Inverted immediate
#define INV_IMM		0x2
// dst: reg
// src1: reg
// src2: reg or imm (if allowed)
// This flag fits for data processing instructions
#define SRC2_IMM	(1 << 25)

static int emit_single_op(struct sljit_compiler *compiler, int op, int flags,
	int dst, int src1, int src2)
{
	sljit_uw *inst;

	switch (op) {
	case (OP1_OFFSET + SLJIT_MOV):
		SLJIT_ASSERT(src1 == TMP_REG1);
		SLJIT_ASSERT((flags & ARGS_SWAPPED) == 0);
		if (dst != src2) {
			inst = alloc_inst(compiler);
			TEST_MEM_ERROR(inst);
			if (src2 & SRC2_IMM) {
				if (flags & INV_IMM)
					*inst = EMIT_DATA_PROCESS_INS(0x1e, dst, SLJIT_NO_REG, src2);
				else
					*inst = EMIT_DATA_PROCESS_INS(0x1a, dst, SLJIT_NO_REG, src2);
			}
			else
				*inst = EMIT_DATA_PROCESS_INS(0x1a, dst, SLJIT_NO_REG, reg_map[src2]);
		}
		return SLJIT_NO_ERROR;

	case (OP1_OFFSET + SLJIT_NOT):
		return SLJIT_NO_ERROR;
	}
	SLJIT_ASSERT_IMPOSSIBLE();
}

// Tests whether the immediate can be stored in the 12 bit imm field
// returns 0 if not possible
static sljit_uw get_immediate(sljit_uw imm)
{
	int rol = 0;

	if (imm == 0)
		return SRC2_IMM | 0;

	imm = (imm << 24) | (imm >> 8);
	while ((imm & 0xff000000) == 0) {
		imm <<= 8;
		rol += 4;
	}

	if ((imm & 0xf0000000) == 0) {
		imm <<= 4;
		rol += 2;
	}

	if ((imm & 0xc0000000) == 0) {
		imm <<= 2;
		rol += 1;
	}

	if ((imm & 0x00ffffff) == 0)
		return SRC2_IMM | (imm >> 24) | (rol << 8);
	else
		return 0;
}

static int load_immediate(struct sljit_compiler *compiler, int reg, sljit_uw imm)
{
	// Get the immediate at most from two instructions
	sljit_uw *inst;
	sljit_uw rimm, tmp;
	int round = 2;
	int rol1;
	int rol2;
	int byte1;

	rimm = (imm << 24) | (imm >> 8);
	tmp = rimm;
	do {
		rol1 = 0;

		while ((tmp & 0xff000000) == 0) {
			tmp <<= 8;
			rol1 += 4;
		}

		if ((tmp & 0xf0000000) == 0) {
			tmp <<= 4;
			rol1 += 2;
		}

		if ((tmp & 0xc0000000) == 0) {
			tmp <<= 2;
			rol1 += 1;
		}

		if ((tmp & 0x00ffffff) == 0) {
			inst = alloc_inst(compiler);
			TEST_MEM_ERROR(inst);
			*inst = EMIT_DATA_PROCESS_INS((round == 2) ? 0x1a : 0x1e, reg, SLJIT_NO_REG, SRC2_IMM | (tmp >> 24) | (rol1 << 8));
			return SLJIT_NO_ERROR;
		}

		rol2 = rol1 + 4;
		byte1 = tmp >> 24;
		tmp <<= 8;

		while ((tmp & 0xff000000) == 0) {
			tmp <<= 8;
			rol2 += 4;
		}

		if ((tmp & 0xf0000000) == 0) {
			tmp <<= 4;
			rol2 += 2;
		}

		if ((tmp & 0xc0000000) == 0) {
			tmp <<= 2;
			rol2 += 1;
		}

		if ((tmp & 0x00ffffff) == 0) {
			inst = alloc_inst(compiler);
			TEST_MEM_ERROR(inst);
			*inst = EMIT_DATA_PROCESS_INS((round == 2) ? 0x1a : 0x1e, reg, SLJIT_NO_REG, SRC2_IMM | (byte1) | (rol1 << 8));

			inst = alloc_inst(compiler);
			TEST_MEM_ERROR(inst);
			*inst = EMIT_DATA_PROCESS_INS((round == 2) ? 0x18 : 0x1c, reg, reg, SRC2_IMM | (tmp >> 24) | (rol2 << 8));
			return SLJIT_NO_ERROR;
		}

		tmp = ~rimm;
	} while (--round > 0);

	inst = alloc_const_inst(compiler, imm);
	TEST_MEM_ERROR(inst);

	compiler->cpool[compiler->cpool_index] = imm;
	*inst = EMIT_DATA_TRANSFER(1, 1, reg, TMP_PC, compiler->cpool_index << 2);

	return SLJIT_NO_ERROR;
}

#define ARG_LOAD	0x1
static int getput_arg(struct sljit_compiler *compiler, int flags, int reg, int arg, sljit_w argw)
{
	sljit_uw *inst;

	if (arg & SLJIT_IMM) {
		SLJIT_ASSERT(flags & ARG_LOAD);
		return load_immediate(compiler, reg, argw);
	}

	SLJIT_ASSERT(arg & SLJIT_MEM_FLAG);

	// Fast case
	if ((arg & 0xf) != 0) {
		if (((arg >> 4) & 0xf) == 0) {
			if (argw >= 0 && argw <= 0xfff) {
				inst = alloc_inst(compiler);
				TEST_MEM_ERROR(inst);
				*inst = EMIT_DATA_TRANSFER(1, flags & ARG_LOAD, reg, arg & 0xf, argw);
				return SLJIT_NO_ERROR;
			}
			if (argw < 0 && argw >= -0xfff) {
				inst = alloc_inst(compiler);
				TEST_MEM_ERROR(inst);
				*inst = EMIT_DATA_TRANSFER(0, flags & ARG_LOAD, reg, arg & 0xf, -argw);
				return SLJIT_NO_ERROR;
			}
		}
		else if (argw == 0) {
			inst = alloc_inst(compiler);
			TEST_MEM_ERROR(inst);
			*inst = EMIT_DATA_TRANSFER(1, flags & ARG_LOAD, reg, arg & 0xf, reg_map[(arg >> 4) & 0xf] | SRC2_IMM);
			return SLJIT_NO_ERROR;
		}
	}

	return SLJIT_NO_ERROR;
}

// allow_imm
//  0 - immediate is not allowed, src2 must be a register
//  1 - immediate allowed
//  2 - both immediate and inverted immediate are allowed

static int emit_op(struct sljit_compiler *compiler, int op, int allow_imm,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	// arg1 goes to TMP_REG1 or src reg
	// arg2 goes to TMP_REG2, imm or src reg

	// We prefers register and simple consts
	int dst_r = (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_GENERAL_REG3) ? dst : TMP_REG2;
	int src1_r;
	int src2_r = 0;
	int flags = 0;

	if (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_GENERAL_REG3)
		dst_r = dst;
	else
		dst_r = TMP_REG2;

	// Source 1
	if (src1 >= SLJIT_TEMPORARY_REG1 && src1 <= TMP_REG3)
		src1_r = src1;
	else if (src2 >= SLJIT_TEMPORARY_REG1 && src2 <= TMP_REG3) {
		flags |= ARGS_SWAPPED;
		src1_r = src2;
		src2 = src1;
		src2w = src1w;
	}
	else {
		if (allow_imm && (src1 & SLJIT_IMM)) {
			src2_r = get_immediate(src1w);
			if (src2_r != 0) {
				flags |= ARGS_SWAPPED;
				src1 = src2;
				src1w = src2w;
			}
			if (allow_imm == 2) {
				src2_r = get_immediate(~src1w);
				if (src2_r != 0) {
					flags |= ARGS_SWAPPED | INV_IMM;
					src1 = src2;
					src1w = src2w;
				}
			}
		}

		src1_r = TMP_REG1;
		getput_arg(compiler, ARG_LOAD, TMP_REG1, src1, src1w);
	}

	// Source 2
	if (src2_r == 0) {
		if (src2 >= SLJIT_TEMPORARY_REG1 && src2 <= TMP_REG3)
			src2_r = src2;
		else {
			do {
				if (allow_imm && (src2 & SLJIT_IMM)) {
					src2_r = get_immediate(src2w);
					if (src2_r != 0)
						break;
					if (allow_imm == 2) {
						src2_r = get_immediate(~src2w);
						if (src2_r != 0) {
							flags |= INV_IMM;
							break;
						}
					}
				}

				src2_r = dst_r;
				getput_arg(compiler, ARG_LOAD, dst_r, src2, src2w);
			} while (0);
		}
	}


	emit_single_op(compiler, op, flags, dst_r, src1_r, src2_r);

	if (dst_r == TMP_REG2 && dst != SLJIT_NO_REG)
		getput_arg(compiler, 0, dst_r, dst, dstw);
	return SLJIT_NO_ERROR;
}

int sljit_emit_op1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
	FUNCTION_ENTRY();

	SLJIT_ASSERT(op >= SLJIT_MOV && op <= SLJIT_NEG);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_SRC(src, srcw);
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_op1_verbose();

	switch (op) {
	case SLJIT_MOV:
		return emit_op(compiler, OP1_OFFSET + op, 2, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_NOT:
		return emit_op(compiler, OP1_OFFSET + op, 2, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_NEG:
		return sljit_emit_op2(compiler, SLJIT_SUB, dst, dstw, SLJIT_IMM, 0, src, srcw);
	}

	return SLJIT_NO_ERROR;
}

int sljit_emit_op2(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	FUNCTION_ENTRY();

	SLJIT_ASSERT(op >= SLJIT_ADD && op <= SLJIT_ASHR);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_SRC(src1, src1w);
	FUNCTION_CHECK_SRC(src2, src2w);
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_op2_verbose();
	
	return SLJIT_NO_ERROR;
}

// ---------------------------------------------------------------------
//  Conditional instructions
// ---------------------------------------------------------------------

struct sljit_label* sljit_emit_label(struct sljit_compiler *compiler)
{
	FUNCTION_ENTRY();

	sljit_emit_label_verbose();

	return NULL;
}

struct sljit_jump* sljit_emit_jump(struct sljit_compiler *compiler, int type)
{
	FUNCTION_ENTRY();
	SLJIT_ASSERT((type & ~0x1ff) == 0);
	SLJIT_ASSERT((type & 0xff) >= SLJIT_C_EQUAL && (type & 0xff) <= SLJIT_JUMP);

	sljit_emit_jump_verbose();

	return NULL;
}

int sljit_emit_cond_set(struct sljit_compiler *compiler, int dst, sljit_w dstw, int type)
{
	FUNCTION_ENTRY();
	SLJIT_ASSERT(type >= SLJIT_C_EQUAL && type <= SLJIT_C_NOT_OVERFLOW);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_set_cond_verbose();

	return SLJIT_NO_ERROR;
}

struct sljit_const* sljit_emit_const(struct sljit_compiler *compiler, int dst, sljit_w dstw, sljit_w constant)
{
	FUNCTION_ENTRY();
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_const_verbose();

	return NULL;
}

void sljit_set_jump_addr(sljit_uw addr, sljit_uw new_addr)
{
}

void sljit_set_const(sljit_uw addr, sljit_w constant)
{
}
