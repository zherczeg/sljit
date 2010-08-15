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
#define TMP_REG1	(SLJIT_LOCALS_REG + 1)
#define TMP_REG2	(SLJIT_LOCALS_REG + 2)
#define TMP_REG3	(SLJIT_LOCALS_REG + 3)
#define TMP_PC		(SLJIT_LOCALS_REG + 4)

#define TMP_FREG1	(SLJIT_FLOAT_REG4 + 1)
#define TMP_FREG2	(SLJIT_FLOAT_REG4 + 2)

// In ARM instruction words
#define CONST_POOL_ALIGNMENT	8
#define CONST_POOL_EMPTY	0xffffffff

#define ALIGN_INSTRUCTION(ptr) \
	(sljit_uw*)(((sljit_uw)(ptr) + (CONST_POOL_ALIGNMENT * sizeof(sljit_uw)) - 1) & ~((CONST_POOL_ALIGNMENT * sizeof(sljit_uw)) - 1))
#define MAX_DIFFERENCE(max_diff) \
	(((max_diff) / (int)sizeof(sljit_uw)) - (CONST_POOL_ALIGNMENT - 1))

// See sljit_emit_enter if you want to change them
static sljit_ub reg_map[SLJIT_NO_REGISTERS + 5] = {
  0, 0, 1, 2, 10, 11, 4, 5, 6, 7, 8, 13, 3, 12, 14, 15
};

static int push_cpool(struct sljit_compiler *compiler)
{
	sljit_uw* inst;
	sljit_uw* cpool_ptr;
	sljit_uw* cpool_end;
	int i;

	SLJIT_ASSERT(compiler->cpool_fill > 0 && compiler->cpool_fill <= CPOOL_SIZE);
	if (compiler->last_label && compiler->last_label->size == compiler->size)
		compiler->last_label->size += compiler->cpool_fill + (CONST_POOL_ALIGNMENT - 1) + 1;
	if (compiler->last_jump && compiler->last_jump->addr == compiler->size)
		compiler->last_jump->addr += compiler->cpool_fill + (CONST_POOL_ALIGNMENT - 1) + 1;
	if (compiler->last_const && compiler->last_const->addr == compiler->size)
		compiler->last_const->addr += compiler->cpool_fill + (CONST_POOL_ALIGNMENT - 1) + 1;

	inst = (sljit_uw*)ensure_buf(compiler, sizeof(sljit_uw));
	if (!inst) {
		compiler->error = SLJIT_MEMORY_ERROR;
		return 1;
	}
	compiler->size++;
	*inst = 0xff000000 | compiler->cpool_fill;

	for (i = 0; i < CONST_POOL_ALIGNMENT - 1; i++) {
		inst = (sljit_uw*)ensure_buf(compiler, sizeof(sljit_uw));
		if (!inst) {
			compiler->error = SLJIT_MEMORY_ERROR;
			return 1;
		}
		compiler->size++;
		*inst = 0;
	}

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
	compiler->cpool_diff = CONST_POOL_EMPTY;
	compiler->cpool_fill = 0;
	return 0;
}

// if last_ins_imm == 0, that means the constant can be changed later
static int push_inst(struct sljit_compiler *compiler)
{
	sljit_uw* inst;
	sljit_uw cpool_index;
	sljit_uw* cpool_ptr;
	sljit_uw* cpool_end;
	sljit_ub* cpool_un_ptr;

	if (compiler->last_type == LIT_INS) {
		// Test wheter the constant pool must be copied
		if (compiler->cpool_diff != CONST_POOL_EMPTY && compiler->size - compiler->cpool_diff >= MAX_DIFFERENCE(4092))
			TEST_FAIL(push_cpool(compiler));

		inst = (sljit_uw*)ensure_buf(compiler, sizeof(sljit_uw));
		if (!inst) {
			compiler->error = SLJIT_MEMORY_ERROR;
			return SLJIT_MEMORY_ERROR;
		}
		*inst = compiler->last_ins;
		compiler->last_type = LIT_NONE;
		compiler->size++;
	}
	else if (compiler->last_type >= LIT_CINS) {
		// Test wheter the constant pool must be copied
		cpool_index = CPOOL_SIZE;
		if (compiler->cpool_diff != CONST_POOL_EMPTY && compiler->size - compiler->cpool_diff >= MAX_DIFFERENCE(4092)) {
			TEST_FAIL(push_cpool(compiler));
		}
		else if (compiler->last_type == LIT_CINS && compiler->cpool_fill > 0) {
			cpool_ptr = compiler->cpool;
			cpool_end = cpool_ptr + compiler->cpool_fill;
			cpool_un_ptr = compiler->cpool_unique;
			do {
				if ((*cpool_ptr == compiler->last_imm) && !(*cpool_un_ptr)) {
					cpool_index = cpool_ptr - compiler->cpool;
					break;
				}
				cpool_ptr++;
				cpool_un_ptr++;
			} while (cpool_ptr < cpool_end);
		}

		if (cpool_index == CPOOL_SIZE) {
			// Must allocate a new place
			if (compiler->cpool_fill < CPOOL_SIZE) {
				cpool_index = compiler->cpool_fill;
				compiler->cpool_fill++;
			}
			else {
				TEST_FAIL(push_cpool(compiler));
				cpool_index = 0;
				compiler->cpool_fill = 1;
			}
		}

		inst = (sljit_uw*)ensure_buf(compiler, sizeof(sljit_uw));
		if (!inst) {
			compiler->error = SLJIT_MEMORY_ERROR;
			return SLJIT_MEMORY_ERROR;
		}
		compiler->cpool[cpool_index] = compiler->last_imm;
		compiler->cpool_unique[cpool_index] = (compiler->last_type == LIT_UCINS);
		SLJIT_ASSERT((compiler->last_ins & 0xfff) == 0);
		*inst = compiler->last_ins | cpool_index;
		compiler->last_type = LIT_NONE;
		compiler->size++;
		if (compiler->cpool_diff == CONST_POOL_EMPTY)
			compiler->cpool_diff = compiler->size;
	}

	return SLJIT_NO_ERROR;
}

#define EMIT_BLX(reg)	(0xe12fff30 | reg_map[reg])

static int emit_blx(struct sljit_compiler *compiler)
{
	if (compiler->last_type == LIT_INS) {
		// Test whether the constant pool must be copied
		if (compiler->cpool_diff != CONST_POOL_EMPTY && compiler->size - compiler->cpool_diff >= MAX_DIFFERENCE(4088))
			TEST_FAIL(push_cpool(compiler));
	}
	else if (compiler->last_type >= LIT_CINS) {
		// Test whether the constant pool must be copied
		if (compiler->cpool_diff != CONST_POOL_EMPTY && compiler->size - compiler->cpool_diff >= MAX_DIFFERENCE(4088)) {
			TEST_FAIL(push_cpool(compiler));
		}
		else if (compiler->cpool_fill >= CPOOL_SIZE) {
			TEST_FAIL(push_cpool(compiler));
		}
	}

	SLJIT_ASSERT(compiler->last_type >= LIT_CINS && ((compiler->last_ins & 0x0fffffff) == 0x059ff000));
	compiler->last_ins = (compiler->last_ins & 0xffff0fff) | (reg_map[TMP_REG1] << 12);

	TEST_FAIL(push_inst(compiler));

	compiler->last_type = LIT_INS;
	compiler->last_ins = EMIT_BLX(TMP_REG1); // blx tmp1
	return SLJIT_NO_ERROR;
}

static sljit_uw patch_pc_rel(sljit_uw *last_pc_patch, sljit_uw *code_ptr, sljit_uw* const_pool, sljit_uw copy)
{
	sljit_uw diff;
	sljit_uw index;
	sljit_uw counter = 0;
	sljit_uw* clear_const_pool = const_pool;
	sljit_uw* clear_const_pool_end = const_pool + copy;

	SLJIT_ASSERT(const_pool - code_ptr <= CONST_POOL_ALIGNMENT);
	while (clear_const_pool < clear_const_pool_end)
		*clear_const_pool++ = (sljit_uw)-1;

	while (last_pc_patch < code_ptr) {
		if ((*last_pc_patch & 0x0c0f0000) == 0x040f0000) {
			diff = const_pool - last_pc_patch;
			index = *last_pc_patch & 0xfff;

			SLJIT_ASSERT(index < copy && (*last_pc_patch & (1 << 25)) == 0);
			if ((int)const_pool[index] < 0) {
				const_pool[index] = counter;
				index = counter;
				counter++;
			}
			else
				index = const_pool[index];

			SLJIT_ASSERT(diff >= 1);
			if (diff >= 2 || index > 0) {
				diff = (diff + index - 2) << 2;
				SLJIT_ASSERT(diff <= 0xfff);
				*last_pc_patch = (*last_pc_patch & ~0xfff) | diff;
			}
			else
				*last_pc_patch = (*last_pc_patch & ~(0xfff | (1 << 23))) | 0x004;
		}
		last_pc_patch++;
	}
	return counter;
}

static INLINE int optimize_jump(struct sljit_jump *jump, sljit_uw *code_ptr, sljit_uw *code)
{
	sljit_w diff;

	if (jump->flags & SLJIT_LONG_JUMP)
		return 0;

	if (jump->flags & IS_BL)
		code_ptr--;

	if (jump->flags & JUMP_ADDR)
		diff = ((sljit_w)jump->target - (sljit_w)(code_ptr + 2)) >> 2;
	else {
		SLJIT_ASSERT(jump->flags & JUMP_LABEL);
		diff = ((sljit_w)(code + jump->label->size) - (sljit_w)(code_ptr + 2)) >> 2;
	}

	if (jump->flags & IS_BL) {
		if (diff <= 0x01ffffff && diff >= -0x02000000) {
			*code_ptr = 0x0b000000 | (*(code_ptr + 1) & 0xf0000000);
			jump->flags |= PATCH_B;
			return 1;
		}
	}
	else {
		if (diff <= 0x01ffffff && diff >= -0x02000000) {
			*code_ptr = 0x0a000000 | (*code_ptr & 0xf0000000);
			jump->flags |= PATCH_B;
		}
	}
	return 0;
}

// In some rare ocasions we may need future patches. The probability is near 0 in practice
struct future_patch {
	struct future_patch* next;
	int index;
	int value;
};

#define PROCESS_CONST_POOL() \
	if (!first_patch) \
		value = (int)const_pool_ptr[index]; \
	else { \
		curr_patch = first_patch; \
		prev_patch = 0; \
		while (1) { \
			if (!curr_patch) { \
				value = (int)const_pool_ptr[index]; \
				break; \
			} \
			if ((sljit_uw)curr_patch->index == index) { \
				value = curr_patch->value; \
				if (prev_patch) \
					prev_patch->next = curr_patch->next; \
				else \
					first_patch = curr_patch->next; \
				SLJIT_FREE(curr_patch); \
				break; \
			} \
			prev_patch = curr_patch; \
			curr_patch = curr_patch->next; \
		} \
	} \
	\
	if (value >= 0) { \
		SLJIT_ASSERT(const_pool_ptr + value <= code_ptr); \
		if ((sljit_uw)value > index) { \
			curr_patch = (struct future_patch*)SLJIT_MALLOC(sizeof(struct future_patch)); \
			if (!curr_patch) { \
				while (first_patch) { \
					curr_patch = first_patch; \
					first_patch = first_patch->next; \
					SLJIT_FREE(curr_patch); \
				} \
				SLJIT_FREE_EXEC(code); \
				return NULL; \
			} \
			curr_patch->next = first_patch; \
			curr_patch->index = value; \
			curr_patch->value = const_pool_ptr[value]; \
			first_patch = curr_patch; \
		} \
		const_pool_ptr[value] = *buf_ptr++; \
	} \
	else \
		buf_ptr++;

void* sljit_generate_code(struct sljit_compiler *compiler)
{
	struct sljit_memory_fragment *buf;
	sljit_uw *code;
	sljit_uw *code_ptr;
	sljit_uw *const_pool_ptr;
	sljit_uw *last_pc_patch;
	sljit_uw *buf_ptr;
	sljit_uw *buf_end;
	sljit_uw size;
	sljit_uw word_count;
	sljit_uw copy, skip, index;
	struct future_patch *first_patch, *curr_patch, *prev_patch;
	int value;

	struct sljit_label *label;
	struct sljit_jump *jump;
	struct sljit_const *const_;

	FUNCTION_ENTRY();

	if (compiler->last_type != LIT_NONE)
		if (push_inst(compiler))
			return NULL;

	SLJIT_ASSERT(compiler->size > 0);
	reverse_buf(compiler);

	// Second code generation pass
	size = compiler->size + (compiler->patches << 1);
	if (compiler->cpool_fill > 0)
		size += compiler->cpool_fill + CONST_POOL_ALIGNMENT - 1;
	code = (sljit_uw*)SLJIT_MALLOC_EXEC(size * sizeof(sljit_uw));
	if (!code) {
		compiler->error = SLJIT_EX_MEMORY_ERROR;
		return NULL;
	}
	buf = compiler->buf;

	code_ptr = code;
	const_pool_ptr = 0;
	last_pc_patch = code;
	copy = 0;
	skip = 0;
	index = 0;
	first_patch = NULL;
	word_count = 0;
	label = compiler->labels;
	jump = compiler->jumps;
	const_ = compiler->consts;
	do {
		buf_ptr = (sljit_uw*)buf->memory;
		buf_end = buf_ptr + (buf->used_size >> 2);
		do {
			if (copy > 0) {
				if (skip > 0) {
					buf_ptr++;
					skip--;
				}
				else {
					PROCESS_CONST_POOL();
					if (++index >= copy) {
						SLJIT_ASSERT(!first_patch);
						copy = 0;
					}
				}
			}
			else if ((*buf_ptr & 0xf0000000) != 0xf0000000) {
				*code_ptr = *buf_ptr++;
				// These structures are ordered by their address
				if (label && label->size == word_count) {
					label->addr = (sljit_uw)code_ptr;
					label->size = code_ptr - code;
					label = label->next;
				}
				if (jump && jump->addr == word_count) {
					SLJIT_ASSERT(jump->flags & (JUMP_LABEL | JUMP_ADDR));
					if (optimize_jump(jump, code_ptr, code))
						code_ptr--;

					jump->addr = (sljit_uw)code_ptr;
					jump = jump->next;
				}
				if (const_ && const_->addr == word_count) {
					const_->addr = (sljit_uw)code_ptr;
					const_ = const_->next;
				}
				code_ptr++;
			}
			else if ((*buf_ptr & 0x0f000000) == 0x0f000000) {
				// Fortunately, no need to shift
				copy = *buf_ptr++ & 0x00ffffff;
				SLJIT_ASSERT(copy > 0);
				const_pool_ptr = ALIGN_INSTRUCTION(code_ptr + 1);
				index = patch_pc_rel(last_pc_patch, code_ptr, const_pool_ptr, copy);
				if (index > 0) {
					// unconditional branch
					*code_ptr = 0xea000000 | (((const_pool_ptr - code_ptr) + index - 2) & 0x00ffffff);
					code_ptr = const_pool_ptr + index;
				}
				skip = CONST_POOL_ALIGNMENT - 1;
				index = 0;
				last_pc_patch = code_ptr;
			}
			else {
				SLJIT_ASSERT_STOP();
			}
			word_count ++;
		} while (buf_ptr < buf_end);
		buf = buf->next;
	} while (buf);

	SLJIT_ASSERT(label == NULL);
	SLJIT_ASSERT(jump == NULL);
	SLJIT_ASSERT(const_ == NULL);
	SLJIT_ASSERT(copy == 0);

	if (compiler->cpool_fill > 0) {
		const_pool_ptr = ALIGN_INSTRUCTION(code_ptr);
		index = patch_pc_rel(last_pc_patch, code_ptr, const_pool_ptr, compiler->cpool_fill);
		if (index > 0)
			code_ptr = const_pool_ptr + index;

		buf_ptr = compiler->cpool;
		buf_end = buf_ptr + compiler->cpool_fill;
		index = 0;
		while (buf_ptr < buf_end) {
			PROCESS_CONST_POOL();
			++index;
		}
		SLJIT_ASSERT(!first_patch);
	}

	jump = compiler->jumps;
	while (jump) {
		buf_ptr = (sljit_uw*)jump->addr;
		jump->addr = (sljit_uw)code_ptr;

		if (!(jump->flags & SLJIT_LONG_JUMP)) {
			if (jump->flags & PATCH_B) {
				if (!(jump->flags & JUMP_ADDR)) {
					SLJIT_ASSERT(jump->flags & JUMP_LABEL);
					SLJIT_ASSERT(((sljit_w)jump->label->addr - (sljit_w)(buf_ptr + 2)) <= 0x01ffffff && ((sljit_w)jump->label->addr - (sljit_w)(buf_ptr + 2)) >= -0x02000000);
					*buf_ptr |= (((sljit_w)jump->label->addr - (sljit_w)(buf_ptr + 2)) >> 2) & 0x00ffffff;
				}
				else {
					SLJIT_ASSERT(((sljit_w)jump->target - (sljit_w)(buf_ptr + 2)) <= 0x01ffffff && ((sljit_w)jump->target - (sljit_w)(buf_ptr + 2)) >= -0x02000000);
					*buf_ptr |= (((sljit_w)jump->target - (sljit_w)(buf_ptr + 2)) >> 2) & 0x00ffffff;
				}
			}
		}
		else {
			code_ptr[0] = (sljit_uw)buf_ptr;
			code_ptr[1] = *buf_ptr;
			sljit_set_jump_addr((sljit_uw)code_ptr, (jump->flags & JUMP_LABEL) ? jump->label->addr : jump->target);
			code_ptr += 2;
		}
		jump = jump->next;
	}

	const_ = compiler->consts;
	while (const_) {
		buf_ptr = (sljit_uw*)const_->addr;
		const_->addr = (sljit_uw)code_ptr;

		code_ptr[0] = (sljit_uw)buf_ptr;
		code_ptr[1] = *buf_ptr;
		copy = *buf_ptr;
		if (copy & (1 << 23))
			buf_ptr += ((copy & 0xfff) >> 2) + 2;
		else
			buf_ptr += 1;
		sljit_set_const((sljit_uw)code_ptr, *buf_ptr);
		code_ptr += 2;

		const_ = const_->next;
	}

	SLJIT_ASSERT(code_ptr - code <= (int)size);

	SLJIT_CACHE_FLUSH(code, code_ptr);
	compiler->error = SLJIT_CODE_GENERATED;
	return code;
}

// emit_op inp_flags
// WRITE_BACK must be the first, since it is a flag
#define WRITE_BACK	0x01
#define ALLOW_IMM	0x02
#define ALLOW_INV_IMM	0x04
#define ALLOW_ANY_IMM	(ALLOW_IMM | ALLOW_INV_IMM)
#define ARG_TEST	0x08

// Creates an index in data_transfer_insts array
#define WORD_DATA	0x00
#define BYTE_DATA	0x10
#define HALF_DATA	0x20
#define SIGNED_DATA	0x40
#define LOAD_DATA	0x80

static int emit_op(struct sljit_compiler *compiler, int op, int inp_flags,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w);

#define MOV_REG(dst, src)	0xe1a00000 | reg_map[src] | (reg_map[dst] << 12)

int sljit_emit_enter(struct sljit_compiler *compiler, int args, int temporaries, int generals, int local_size)
{
	FUNCTION_ENTRY();
	SLJIT_ASSERT(args >= 0 && args <= 3);
	SLJIT_ASSERT(temporaries >= 0 && temporaries <= SLJIT_NO_TMP_REGISTERS);
	SLJIT_ASSERT(generals >= 0 && generals <= SLJIT_NO_GEN_REGISTERS);
	SLJIT_ASSERT(args <= generals);
	SLJIT_ASSERT(local_size >= 0 && local_size <= SLJIT_MAX_LOCAL_SIZE);

	sljit_emit_enter_verbose();

	compiler->temporaries = temporaries;
	compiler->generals = generals;

	TEST_FAIL(push_inst(compiler));
	// Push general registers, temporary registers
	// stmdb sp!, {..., lr}
	compiler->last_type = LIT_INS;
	compiler->last_ins = 0xe92d0000 | (1 << 14) | (1 << 11) | (1 << 10);
	if (temporaries >= 5)
		compiler->last_ins |= 1 << 11;
	if (temporaries >= 4)
		compiler->last_ins |= 1 << 10;
	if (generals >= 5)
		compiler->last_ins |= 1 << 8;
	if (generals >= 4)
		compiler->last_ins |= 1 << 7;
	if (generals >= 3)
		compiler->last_ins |= 1 << 6;
	if (generals >= 2)
		compiler->last_ins |= 1 << 5;
	if (generals >= 1)
		compiler->last_ins |= 1 << 4;

	// Stack must be aligned to 8 bytes:
	local_size += (3 + generals) * sizeof(sljit_uw);
	local_size = (local_size + 7) & ~7;
	local_size -= (3 + generals) * sizeof(sljit_uw);
	compiler->local_size = local_size;
	if (local_size > 0)
		TEST_FAIL(emit_op(compiler, SLJIT_SUB, ALLOW_IMM, SLJIT_LOCALS_REG, 0, SLJIT_LOCALS_REG, 0, SLJIT_IMM, local_size));

	if (args >= 1) {
		TEST_FAIL(push_inst(compiler));
		compiler->last_type = LIT_INS;
		compiler->last_ins = MOV_REG(SLJIT_GENERAL_REG1, SLJIT_TEMPORARY_REG1);
	}
	if (args >= 2) {
		TEST_FAIL(push_inst(compiler));
		compiler->last_type = LIT_INS;
		compiler->last_ins = MOV_REG(SLJIT_GENERAL_REG2, SLJIT_TEMPORARY_REG2);
	}
	if (args >= 3) {
		TEST_FAIL(push_inst(compiler));
		compiler->last_type = LIT_INS;
		compiler->last_ins = MOV_REG(SLJIT_GENERAL_REG3, SLJIT_TEMPORARY_REG3);
	}

	return SLJIT_NO_ERROR;
}

void sljit_fake_enter(struct sljit_compiler *compiler, int args, int temporaries, int generals, int local_size)
{
	SLJIT_ASSERT(args >= 0 && args <= 3);
	SLJIT_ASSERT(temporaries >= 0 && temporaries <= SLJIT_NO_TMP_REGISTERS);
	SLJIT_ASSERT(generals >= 0 && generals <= SLJIT_NO_GEN_REGISTERS);
	SLJIT_ASSERT(local_size >= 0 && local_size <= SLJIT_MAX_LOCAL_SIZE);

	sljit_fake_enter_verbose();

	compiler->temporaries = temporaries;
	compiler->generals = generals;

	local_size += (3 + generals) * sizeof(sljit_uw);
	local_size = (local_size + 7) & ~7;
	local_size -= (3 + generals) * sizeof(sljit_uw);
	compiler->local_size = local_size;
}

int sljit_emit_return(struct sljit_compiler *compiler, int src, sljit_w srcw)
{
	FUNCTION_ENTRY();
#ifdef SLJIT_DEBUG
	if (src != SLJIT_UNUSED) {
		FUNCTION_CHECK_SRC(src, srcw);
	}
	else
		SLJIT_ASSERT(srcw == 0);
#endif

	sljit_emit_return_verbose();

	if (src != SLJIT_PREF_RET_REG && src != SLJIT_UNUSED) {
		TEST_FAIL(emit_op(compiler, SLJIT_MOV, ALLOW_ANY_IMM, SLJIT_PREF_RET_REG, 0, TMP_REG1, 0, src, srcw));
	}

	if (compiler->local_size > 0)
		TEST_FAIL(emit_op(compiler, SLJIT_ADD, ALLOW_IMM, SLJIT_LOCALS_REG, 0, SLJIT_LOCALS_REG, 0, SLJIT_IMM, compiler->local_size));

	TEST_FAIL(push_inst(compiler));
	// Push general registers, temporary registers
	// ldmia sp!, {..., pc}
	compiler->last_type = LIT_INS;
	compiler->last_ins = 0xe8bd0000 | (1 << 15) | (1 << 11) | (1 << 10);
	if (compiler->temporaries >= 5)
		compiler->last_ins |= 1 << 11;
	if (compiler->temporaries >= 4)
		compiler->last_ins |= 1 << 10;
	if (compiler->generals >= 5)
		compiler->last_ins |= 1 << 8;
	if (compiler->generals >= 4)
		compiler->last_ins |= 1 << 7;
	if (compiler->generals >= 3)
		compiler->last_ins |= 1 << 6;
	if (compiler->generals >= 2)
		compiler->last_ins |= 1 << 5;
	if (compiler->generals >= 1)
		compiler->last_ins |= 1 << 4;

	return SLJIT_NO_ERROR;
}

// ---------------------------------------------------------------------
//  Operators
// ---------------------------------------------------------------------

#define EMIT_DATA_PROCESS_INS(opcode, dst, src1, src2) \
	(0xe0000000 | ((opcode) << 20) | (reg_map[dst] << 12) | (reg_map[src1] << 16) | (src2))

// s/l - store/load (1 bit)
// u/s - signed/unsigned (1 bit)
// w/b/h/N - word/byte/half/NOT allowed (2 bit)
// It contans 16 items, but not all are different

static sljit_w data_transfer_insts[16] = {
/* s u w */ 0xe5000000,
/* s u b */ 0xe5400000,
/* s u h */ 0xe10000b0,
/* s u N */ 0x00000000,
/* s s w */ 0xe5000000, // sign doesn't matter for stores
/* s s b */ 0xe5400000,
/* s s h */ 0xe10000b0,
/* s s N */ 0x00000000,

/* l u w */ 0xe5100000,
/* l u b */ 0xe5500000,
/* l u h */ 0xe11000b0,
/* l u N */ 0x00000000,
/* l s w */ 0xe5100000,
/* l s b */ 0xe11000d0,
/* l s h */ 0xe11000f0,
/* l s N */ 0x00000000,
};

#define EMIT_DATA_TRANSFER(type, add, wb, target, base1, base2) \
	(data_transfer_insts[(type) >> 4] | ((add) << 23) | ((wb) << 21) | (reg_map[target] << 12) | (reg_map[base1] << 16) | (base2))
#define IS_TYPE1_TRANSFER(type) \
	(data_transfer_insts[(type) >> 4] & 0x04000000)
#define TR2_IMM(imm) \
	(((imm) & 0xf) | (((imm) & 0xf0) << 4) | (1 << 22))

// flags:
  // Arguments are swapped
#define ARGS_SWAPPED	0x01
  // Inverted immediate
#define INV_IMM		0x02
  // Source and destination is register
#define REG_DEST	0x04
#define REG_SOURCE	0x08
  // One instruction is enough
#define FAST_DEST	0x10
  // Multiple instructions are required
#define SLOW_DEST	0x20
// dst: reg
// src1: reg
// src2: reg or imm (if allowed)
// This flag fits for data processing instructions
#define SRC2_IMM	(1 << 25)

#define EMIT_INSTRUCTION(inst) \
	do { \
		TEST_FAIL(push_inst(compiler)); \
		compiler->last_type = LIT_INS; \
		compiler->last_ins = (inst); \
	} while (0)

#define SET_DATA_PROCESS_INS(opcode) \
	compiler->last_type = LIT_INS; \
	if (src2 & SRC2_IMM) \
		compiler->last_ins = EMIT_DATA_PROCESS_INS(opcode, dst, src1, src2); \
	else \
		compiler->last_ins = EMIT_DATA_PROCESS_INS(opcode, dst, src1, reg_map[src2]);

#define SET_SHIFT_INS(opcode) \
	compiler->last_type = LIT_INS; \
	if (compiler->shift_imm != 0x20) { \
		SLJIT_ASSERT(src1 == TMP_REG1); \
		SLJIT_ASSERT(!(flags & ARGS_SWAPPED)); \
		compiler->last_ins = EMIT_DATA_PROCESS_INS(0x1b, dst, SLJIT_UNUSED, (compiler->shift_imm << 7) | (opcode << 5) | reg_map[src2]); \
	} \
	else { \
		if (!(flags & ARGS_SWAPPED)) \
			compiler->last_ins = EMIT_DATA_PROCESS_INS(0x1b, dst, SLJIT_UNUSED, (reg_map[src2] << 8) | (opcode << 5) | 0x10 | reg_map[src1]); \
		else \
			compiler->last_ins = EMIT_DATA_PROCESS_INS(0x1b, dst, SLJIT_UNUSED, (reg_map[src1] << 8) | (opcode << 5) | 0x10 | reg_map[src2]); \
	}

static int emit_single_op(struct sljit_compiler *compiler, int op, int flags,
	int dst, int src1, int src2)
{
	switch (op) {
	case SLJIT_ADD:
		SLJIT_ASSERT((flags & INV_IMM) == 0);
		TEST_FAIL(push_inst(compiler));
		SET_DATA_PROCESS_INS(0x09);
		return SLJIT_NO_ERROR;

	case SLJIT_ADDC:
		SLJIT_ASSERT((flags & INV_IMM) == 0);
		TEST_FAIL(push_inst(compiler));
		SET_DATA_PROCESS_INS(0x0b);
		return SLJIT_NO_ERROR;

	case SLJIT_SUB:
		SLJIT_ASSERT((flags & INV_IMM) == 0);
		TEST_FAIL(push_inst(compiler));
		if (!(flags & ARGS_SWAPPED)) {
			SET_DATA_PROCESS_INS(0x05);
		}
		else {
			SET_DATA_PROCESS_INS(0x07);
		}
		return SLJIT_NO_ERROR;

	case SLJIT_SUBC:
		SLJIT_ASSERT((flags & INV_IMM) == 0);
		TEST_FAIL(push_inst(compiler));
		if (!(flags & ARGS_SWAPPED)) {
			SET_DATA_PROCESS_INS(0x0d);
		}
		else {
			SET_DATA_PROCESS_INS(0x0f);
		}
		return SLJIT_NO_ERROR;

	case SLJIT_MUL:
		SLJIT_ASSERT((flags & INV_IMM) == 0);
		SLJIT_ASSERT((src2 & SRC2_IMM) == 0);
		TEST_FAIL(push_inst(compiler));
		compiler->last_type = LIT_INS;
		if (dst != src2)
			compiler->last_ins = 0xe0100090 | (reg_map[dst] << 16) | (reg_map[src1] << 8) | reg_map[src2];
		else if (dst != src1)
			compiler->last_ins = 0xe0100090 | (reg_map[dst] << 16) | (reg_map[src2] << 8) | reg_map[src1];
		else {
			// Rm and Rd must not be the same register
			SLJIT_ASSERT(dst != TMP_REG1);
			compiler->last_ins = EMIT_DATA_PROCESS_INS(0x1a, TMP_REG1, SLJIT_UNUSED, reg_map[src2]);
			TEST_FAIL(push_inst(compiler));
			compiler->last_type = LIT_INS;
			compiler->last_ins = 0xe0100090 | (reg_map[dst] << 16) | (reg_map[src2] << 8) | reg_map[TMP_REG1];
		}
		return SLJIT_NO_ERROR;

	case SLJIT_AND:
		TEST_FAIL(push_inst(compiler));
		if (!(flags & INV_IMM)) {
			SET_DATA_PROCESS_INS(0x01);
		}
		else {
			SET_DATA_PROCESS_INS(0x1d);
		}
		return SLJIT_NO_ERROR;

	case SLJIT_OR:
		SLJIT_ASSERT((flags & INV_IMM) == 0);
		TEST_FAIL(push_inst(compiler));
		SET_DATA_PROCESS_INS(0x19);
		return SLJIT_NO_ERROR;

	case SLJIT_XOR:
		SLJIT_ASSERT((flags & INV_IMM) == 0);
		TEST_FAIL(push_inst(compiler));
		SET_DATA_PROCESS_INS(0x03);
		return SLJIT_NO_ERROR;

	case SLJIT_SHL:
		SLJIT_ASSERT((flags & INV_IMM) == 0);
		SLJIT_ASSERT((src2 & SRC2_IMM) == 0);
		TEST_FAIL(push_inst(compiler));
		SET_SHIFT_INS(0);
		return SLJIT_NO_ERROR;

	case SLJIT_LSHR:
		SLJIT_ASSERT((flags & INV_IMM) == 0);
		SLJIT_ASSERT((src2 & SRC2_IMM) == 0);
		TEST_FAIL(push_inst(compiler));
		SET_SHIFT_INS(1);
		return SLJIT_NO_ERROR;

	case SLJIT_ASHR:
		SLJIT_ASSERT((flags & INV_IMM) == 0);
		SLJIT_ASSERT((src2 & SRC2_IMM) == 0);
		TEST_FAIL(push_inst(compiler));
		SET_SHIFT_INS(2);
		return SLJIT_NO_ERROR;

	case SLJIT_MOV:
		SLJIT_ASSERT(src1 == TMP_REG1);
		SLJIT_ASSERT((flags & ARGS_SWAPPED) == 0);
		if (dst != src2) {
			TEST_FAIL(push_inst(compiler));
			compiler->last_type = LIT_INS;
			if (src2 & SRC2_IMM) {
				if (flags & INV_IMM)
					compiler->last_ins = EMIT_DATA_PROCESS_INS(0x1e, dst, SLJIT_UNUSED, src2);
				else
					compiler->last_ins = EMIT_DATA_PROCESS_INS(0x1a, dst, SLJIT_UNUSED, src2);
			}
			else
				compiler->last_ins = EMIT_DATA_PROCESS_INS(0x1a, dst, SLJIT_UNUSED, reg_map[src2]);
		}
		return SLJIT_NO_ERROR;

	case SLJIT_MOV_UB:
	case SLJIT_MOV_SB:
		SLJIT_ASSERT(src1 == TMP_REG1);
		SLJIT_ASSERT((flags & ARGS_SWAPPED) == 0);
		if ((flags & (REG_DEST | REG_SOURCE)) == (REG_DEST | REG_SOURCE)) {
			EMIT_INSTRUCTION(EMIT_DATA_PROCESS_INS(0x1a, dst, SLJIT_UNUSED, (24 << 7) | reg_map[src2]));
			EMIT_INSTRUCTION(EMIT_DATA_PROCESS_INS(0x1a, dst, SLJIT_UNUSED, (24 << 7) | (op == SLJIT_MOV_UB ? 0x20 : 0x40) | reg_map[dst]));
		}
		else if (dst != src2) {
			SLJIT_ASSERT(src2 & SRC2_IMM);
			TEST_FAIL(push_inst(compiler));
			compiler->last_type = LIT_INS;
			if (flags & INV_IMM)
				compiler->last_ins = EMIT_DATA_PROCESS_INS(0x1e, dst, SLJIT_UNUSED, src2);
			else
				compiler->last_ins = EMIT_DATA_PROCESS_INS(0x1a, dst, SLJIT_UNUSED, src2);
		}
		return SLJIT_NO_ERROR;

	case SLJIT_MOV_UH:
	case SLJIT_MOV_SH:
		SLJIT_ASSERT(src1 == TMP_REG1);
		SLJIT_ASSERT((flags & ARGS_SWAPPED) == 0);
		if ((flags & (REG_DEST | REG_SOURCE)) == (REG_DEST | REG_SOURCE)) {
			EMIT_INSTRUCTION(EMIT_DATA_PROCESS_INS(0x1a, dst, SLJIT_UNUSED, (16 << 7) | reg_map[src2]));
			EMIT_INSTRUCTION(EMIT_DATA_PROCESS_INS(0x1a, dst, SLJIT_UNUSED, (16 << 7) | (op == SLJIT_MOV_UH ? 0x20 : 0x40) | reg_map[dst]));
		}
		else if (dst != src2) {
			SLJIT_ASSERT(src2 & SRC2_IMM);
			TEST_FAIL(push_inst(compiler));
			compiler->last_type = LIT_INS;
			if (flags & INV_IMM)
				compiler->last_ins = EMIT_DATA_PROCESS_INS(0x1e, dst, SLJIT_UNUSED, src2);
			else
				compiler->last_ins = EMIT_DATA_PROCESS_INS(0x1a, dst, SLJIT_UNUSED, src2);
		}
		return SLJIT_NO_ERROR;

	case SLJIT_NOT:
		TEST_FAIL(push_inst(compiler));
		compiler->last_type = LIT_INS;
		if (src2 & SRC2_IMM) {
			if (flags & INV_IMM)
				compiler->last_ins = EMIT_DATA_PROCESS_INS(0x1b, dst, SLJIT_UNUSED, src2);
			else
				compiler->last_ins = EMIT_DATA_PROCESS_INS(0x1f, dst, SLJIT_UNUSED, src2);
		}
		else
			compiler->last_ins = EMIT_DATA_PROCESS_INS(0x1f, dst, SLJIT_UNUSED, reg_map[src2]);

		return SLJIT_NO_ERROR;
	}
	SLJIT_ASSERT_STOP();
	return SLJIT_NO_ERROR;
}

// Tests whether the immediate can be stored in the 12 bit imm field
// returns 0 if not possible
static sljit_uw get_immediate(sljit_uw imm)
{
	int rol;

	if (imm <= 0xff)
		return SRC2_IMM | imm;

	if ((imm & 0xff000000) == 0) {
		imm <<= 8;
		rol = 8;
	}
	else {
		imm = (imm << 24) | (imm >> 8);
		rol = 0;
	}

	if ((imm & 0xff000000) == 0) {
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

static int generate_int(struct sljit_compiler *compiler, int reg, sljit_uw imm, int positive)
{
	sljit_uw mask;
	sljit_uw imm1;
	sljit_uw imm2;
	int rol;

	// Step1: Search a zero byte (8 continous zero bit)
	mask = 0xff000000;
	rol = 8;
	while(1) {
		if ((imm & mask) == 0) {
			// rol imm by rol
			imm = (imm << rol) | (imm >> (32 - rol));
			// calculate arm rol
			rol = 4 + (rol >> 1);
			break;
		}
		rol += 2;
		mask >>= 2;
		if (mask & 0x3) {
			// rol by 8
			imm = (imm << 8) | (imm >> 24);
			mask = 0xff00;
			rol = 24;
			while (1) {
				if ((imm & mask) == 0) {
					// rol imm by rol
					imm = (imm << rol) | (imm >> (32 - rol));
					// calculate arm rol
					rol = (rol >> 1) - 8;
					break;
				}
				rol += 2;
				mask >>= 2;
				if (mask & 0x3)
					return 0;
			}
			break;
		}
	}

	// The low 8 bit must be zero
	SLJIT_ASSERT((imm & 0xff) == 0);

	if ((imm & 0xff000000) == 0) {
		imm1 = SRC2_IMM | ((imm >> 16) & 0xff) | (((rol + 4) & 0xf) << 8);
		imm2 = SRC2_IMM | ((imm >> 8) & 0xff) | (((rol + 8) & 0xf) << 8);
	}
	else if (imm & 0xc0000000) {
		imm1 = SRC2_IMM | ((imm >> 24) & 0xff) | ((rol & 0xf) << 8);
		imm <<= 8;
		rol += 4;

		if ((imm & 0xff000000) == 0) {
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
			imm2 = SRC2_IMM | (imm >> 24) | ((rol & 0xf) << 8);
		else
			return 0;
	}
	else {
		if ((imm & 0xf0000000) == 0) {
			imm <<= 4;
			rol += 2;
		}

		if ((imm & 0xc0000000) == 0) {
			imm <<= 2;
			rol += 1;
		}

		imm1 = SRC2_IMM | ((imm >> 24) & 0xff) | ((rol & 0xf) << 8);
		imm <<= 8;
		rol += 4;

		if ((imm & 0xf0000000) == 0) {
			imm <<= 4;
			rol += 2;
		}

		if ((imm & 0xc0000000) == 0) {
			imm <<= 2;
			rol += 1;
		}

		if ((imm & 0x00ffffff) == 0)
			imm2 = SRC2_IMM | (imm >> 24) | ((rol & 0xf) << 8);
		else
			return 0;
	}

	EMIT_INSTRUCTION(EMIT_DATA_PROCESS_INS(positive ? 0x1a : 0x1e, reg, SLJIT_UNUSED, imm1));
	EMIT_INSTRUCTION(EMIT_DATA_PROCESS_INS(positive ? 0x18 : 0x1c, reg, reg, imm2));
	return 1;
}

static int load_immediate(struct sljit_compiler *compiler, int reg, sljit_uw imm)
{
	sljit_uw tmp;

	// Create imm by 1 inst
	tmp = get_immediate(imm);
	if (tmp) {
		EMIT_INSTRUCTION(EMIT_DATA_PROCESS_INS(0x1a, reg, SLJIT_UNUSED, tmp));
		return SLJIT_NO_ERROR;
	}

	tmp = get_immediate(~imm);
	if (tmp) {
		EMIT_INSTRUCTION(EMIT_DATA_PROCESS_INS(0x1e, reg, SLJIT_UNUSED, tmp));
		return SLJIT_NO_ERROR;
	}

	// Create imm by 2 inst
	TEST_FAIL(generate_int(compiler, reg, imm, 1)); // Maybe SLJIT_NO_ERROR, if successful
	TEST_FAIL(generate_int(compiler, reg, ~imm, 0)); // Maybe SLJIT_NO_ERROR, if successful

	// Load integer
	TEST_FAIL(push_inst(compiler));
	compiler->last_type = LIT_CINS;
	compiler->last_ins = EMIT_DATA_TRANSFER(WORD_DATA | LOAD_DATA, 1, 0, reg, TMP_PC, 0);
	compiler->last_imm = imm;

	return SLJIT_NO_ERROR;
}

// Can perform an operation using at most 1 instruction
static int getput_arg_fast(struct sljit_compiler *compiler, int inp_flags, int reg, int arg, sljit_w argw)
{
	sljit_uw imm;

	if (arg & SLJIT_IMM) {
		imm = get_immediate(argw);
		if (imm != 0) {
			if (inp_flags & ARG_TEST)
				return 1;
			EMIT_INSTRUCTION(EMIT_DATA_PROCESS_INS(0x1a, reg, SLJIT_UNUSED, imm));
			return -1;
		}
		imm = get_immediate(~argw);
		if (imm != 0) {
			if (inp_flags & ARG_TEST)
				return 1;
			EMIT_INSTRUCTION(EMIT_DATA_PROCESS_INS(0x1e, reg, SLJIT_UNUSED, imm));
			return -1;
		}
		return (inp_flags & ARG_TEST) ? SLJIT_NO_ERROR : 0;
	}

	SLJIT_ASSERT(arg & SLJIT_MEM_FLAG);

	// Fast loads/stores
	if ((arg & 0xf) != 0) {
		if ((arg & 0xf0) == 0) {
			if (IS_TYPE1_TRANSFER(inp_flags)) {
				if (argw >= 0 && argw <= 0xfff) {
					if (inp_flags & ARG_TEST)
						return 1;
					EMIT_INSTRUCTION(EMIT_DATA_TRANSFER(inp_flags, 1, inp_flags & WRITE_BACK, reg, arg & 0xf, argw));
					return -1;
				}
				if (argw < 0 && argw >= -0xfff) {
					if (inp_flags & ARG_TEST)
						return 1;
					EMIT_INSTRUCTION(EMIT_DATA_TRANSFER(inp_flags, 0, inp_flags & WRITE_BACK, reg, arg & 0xf, -argw));
					return -1;
				}
			}
			else {
				if (argw >= 0 && argw <= 0xff) {
					if (inp_flags & ARG_TEST)
						return 1;
					EMIT_INSTRUCTION(EMIT_DATA_TRANSFER(inp_flags, 1, inp_flags & WRITE_BACK, reg, arg & 0xf, TR2_IMM(argw)));
					return -1;
				}
				if (argw < 0 && argw >= -0xff) {
					if (inp_flags & ARG_TEST)
						return 1;
					argw = -argw;
					EMIT_INSTRUCTION(EMIT_DATA_TRANSFER(inp_flags, 0, inp_flags & WRITE_BACK, reg, arg & 0xf, TR2_IMM(argw)));
					return -1;
				}
			}
		}
		else if (argw == 0) {
			if (inp_flags & ARG_TEST)
				return 1;
			EMIT_INSTRUCTION(EMIT_DATA_TRANSFER(inp_flags, 1, inp_flags & WRITE_BACK, reg, arg & 0xf,
				reg_map[(arg >> 4) & 0xf] | (IS_TYPE1_TRANSFER(inp_flags) ? SRC2_IMM : 0)));
			return -1;
		}
	}

	return (inp_flags & ARG_TEST) ? SLJIT_NO_ERROR : 0;
}

// see getput_arg below
// Note: can_cache is called only for binary operators. Those operator always
// uses word arguments without write back
static int can_cache(int arg, sljit_w argw, int next_arg, sljit_w next_argw)
{
	if (arg & SLJIT_IMM)
		return 0;

	if ((arg & 0xf) == 0) {
		if ((next_arg & SLJIT_MEM_FLAG) && ((sljit_uw)argw - (sljit_uw)next_argw <= 0xfff || (sljit_uw)next_argw - (sljit_uw)argw <= 0xfff))
			return 1;
		return 0;
	}

	if (!(arg & 0xf0) && argw <= 0xfffff && argw >= -0xfffff)
		return 0;

	if (argw <= 0xfff && argw >= -0xfff) {
		if (arg == next_arg && (next_argw <= 0xfff && next_argw >= -0xfff))
			return 1;
		return 0;
	}

	if (argw == next_argw && (next_arg & SLJIT_MEM_FLAG))
		return 1;

	if (arg == next_arg && ((sljit_uw)argw - (sljit_uw)next_argw <= 0xfff || (sljit_uw)next_argw - (sljit_uw)argw <= 0xfff))
		return 1;

	return 0;
}

#define GETPUT_ARG_DATA_TRANSFER(add, wb, target, base, imm) \
	TEST_FAIL(push_inst(compiler)); \
	compiler->last_type = LIT_INS; \
	if (max_delta & 0xf00) \
		compiler->last_ins = EMIT_DATA_TRANSFER(inp_flags, add, wb, target, base, imm); \
	else \
		compiler->last_ins = EMIT_DATA_TRANSFER(inp_flags, add, wb, target, base, TR2_IMM(imm));

#define TEST_WRITE_BACK() \
	if (inp_flags & WRITE_BACK) { \
		tmp_reg = arg & 0xf; \
		if (reg == tmp_reg) { \
			/* This can only happen for stores */ \
			/* since ldr reg, [reg, ...]! has no meaning */ \
			SLJIT_ASSERT(!(inp_flags & LOAD_DATA)); \
			EMIT_INSTRUCTION(EMIT_DATA_PROCESS_INS(0x1a, TMP_REG3, SLJIT_UNUSED, reg_map[reg])); \
			reg = TMP_REG3; \
		} \
	}

// emit the necessary instructions
// see can_cache above
static int getput_arg(struct sljit_compiler *compiler, int inp_flags, int reg, int arg, sljit_w argw, int next_arg, sljit_w next_argw)
{
	int tmp_reg;
	sljit_w max_delta;
	sljit_w sign;

	if (arg & SLJIT_IMM) {
		SLJIT_ASSERT(inp_flags & LOAD_DATA);
		return load_immediate(compiler, reg, argw);
	}

	SLJIT_ASSERT(arg & SLJIT_MEM_FLAG);

	tmp_reg = (inp_flags & LOAD_DATA) ? reg : TMP_REG3;
	max_delta = IS_TYPE1_TRANSFER(inp_flags) ? 0xfff : 0xff;

	if ((arg & 0xf) == SLJIT_UNUSED) {
		// Write back is not used
		if ((compiler->cache_arg & SLJIT_IMM) && (((sljit_uw)argw - (sljit_uw)compiler->cache_argw) <= (sljit_uw)max_delta || ((sljit_uw)compiler->cache_argw - (sljit_uw)argw) <= (sljit_uw)max_delta)) {
			if (((sljit_uw)argw - (sljit_uw)compiler->cache_argw) <= (sljit_uw)max_delta) {
				sign = 1;
				argw = argw - compiler->cache_argw;
			}
			else {
				sign = 0;
				argw = compiler->cache_argw - argw;
			}

			if (max_delta & 0xf00) {
				EMIT_INSTRUCTION(EMIT_DATA_TRANSFER(inp_flags, sign, 0, reg, TMP_REG3, argw));
			}
			else {
				EMIT_INSTRUCTION(EMIT_DATA_TRANSFER(inp_flags, sign, 0, reg, TMP_REG3, TR2_IMM(argw)));
			}
			return SLJIT_NO_ERROR;
		}

		// With write back, we can create some sophisticated loads, but
		// it is hard to decide whether we should convert downward (0s) or upward (1s)
		if ((next_arg & SLJIT_MEM_FLAG) && ((sljit_uw)argw - (sljit_uw)next_argw <= (sljit_uw)max_delta || (sljit_uw)next_argw - (sljit_uw)argw <= (sljit_uw)max_delta)) {
			SLJIT_ASSERT(inp_flags & LOAD_DATA);

			compiler->cache_arg = SLJIT_IMM;
			compiler->cache_argw = argw;
			tmp_reg = TMP_REG3;
		}

		TEST_FAIL(load_immediate(compiler, tmp_reg, argw));
		GETPUT_ARG_DATA_TRANSFER(1, 0, reg, tmp_reg, 0);
		return SLJIT_NO_ERROR;
	}

	// Extended imm addressing for [reg+imm] format
	sign = (max_delta << 8) | 0xff;
	if (!(arg & 0xf0) && argw <= sign && argw >= -sign) {
		TEST_WRITE_BACK();
		if (argw >= 0) {
			sign = 1;
		}
		else {
			sign = 0;
			argw = -argw;
		}

		TEST_FAIL(push_inst(compiler));
		compiler->last_type = LIT_INS;
		// Optimization: add is 0x08, sub is 0x04. Sign is 1 for add and 0 for sub
		if (max_delta & 0xf00)
			compiler->last_ins = EMIT_DATA_PROCESS_INS(0x04 << sign, tmp_reg, arg & 0xf, SRC2_IMM | (argw >> 12) | 0xa00);
		else
			compiler->last_ins = EMIT_DATA_PROCESS_INS(0x04 << sign, tmp_reg, arg & 0xf, SRC2_IMM | (argw >> 8) | 0xc00);

		argw &= max_delta;
		GETPUT_ARG_DATA_TRANSFER(sign, inp_flags & WRITE_BACK, reg, tmp_reg, argw);
		return SLJIT_NO_ERROR;
	}

	if (compiler->cache_arg == arg && ((sljit_uw)argw - (sljit_uw)compiler->cache_argw) <= (sljit_uw)max_delta) {
		SLJIT_ASSERT(!(inp_flags & WRITE_BACK));
		argw = argw - compiler->cache_argw;
		GETPUT_ARG_DATA_TRANSFER(1, 0, reg, TMP_REG3, argw);
		return SLJIT_NO_ERROR;
	}

	if (compiler->cache_arg == arg && ((sljit_uw)compiler->cache_argw - (sljit_uw)argw) <= (sljit_uw)max_delta) {
		SLJIT_ASSERT(!(inp_flags & WRITE_BACK));
		argw = compiler->cache_argw - argw;
		GETPUT_ARG_DATA_TRANSFER(0, 0, reg, TMP_REG3, argw);
		return SLJIT_NO_ERROR;
	}

	if ((compiler->cache_arg & SLJIT_IMM) && compiler->cache_argw == argw) {
		TEST_WRITE_BACK();
		if (arg & 0xf0) {
			if (inp_flags & (WRITE_BACK | LOAD_DATA)) {
				EMIT_INSTRUCTION(EMIT_DATA_PROCESS_INS(0x08, tmp_reg, arg & 0xf, reg_map[(arg >> 4) & 0xf]));
				arg = tmp_reg | SLJIT_MEM_FLAG;
			}
			else {
				SLJIT_ASSERT(tmp_reg == TMP_REG3);
				EMIT_INSTRUCTION(EMIT_DATA_PROCESS_INS(0x08, TMP_REG3, TMP_REG3, reg_map[(arg >> 4) & 0xf]));
			}
		}

		EMIT_INSTRUCTION(EMIT_DATA_TRANSFER(inp_flags, 1, inp_flags & WRITE_BACK, reg, arg & 0xf, reg_map[TMP_REG3] | (max_delta & 0xf00 ? SRC2_IMM : 0)));
		return SLJIT_NO_ERROR;
	}

	if (argw >= -max_delta && argw <= max_delta) {
		// Since getput_arg_fast doesn't caught it, that means two arguments is given
		SLJIT_ASSERT(argw != 0);
		SLJIT_ASSERT((arg & 0xf0) != SLJIT_UNUSED);

		TEST_WRITE_BACK();
		if (argw >= 0)
			sign = 1;
		else {
			sign = 0;
			argw = -argw;
		}

		if (arg == next_arg && !(inp_flags & WRITE_BACK) && (next_argw <= max_delta && next_argw >= -max_delta)) {
			SLJIT_ASSERT(inp_flags & LOAD_DATA);

			compiler->cache_arg = arg;
			compiler->cache_argw = 0;
			tmp_reg = TMP_REG3;
		}

		EMIT_INSTRUCTION(EMIT_DATA_PROCESS_INS(0x08, tmp_reg, arg & 0xf, reg_map[(arg >> 4) & 0xf]));
		GETPUT_ARG_DATA_TRANSFER(sign, inp_flags & WRITE_BACK, reg, tmp_reg, argw);
		return SLJIT_NO_ERROR;
	}

	if (argw == next_argw && (next_arg & SLJIT_MEM_FLAG)) {
		SLJIT_ASSERT(inp_flags & LOAD_DATA);
		TEST_FAIL(load_immediate(compiler, TMP_REG3, argw));

		compiler->cache_arg = SLJIT_IMM;
		compiler->cache_argw = argw;

		TEST_WRITE_BACK();
		if (arg & 0xf0) {
			if (inp_flags & WRITE_BACK) {
				EMIT_INSTRUCTION(EMIT_DATA_PROCESS_INS(0x08, arg & 0xf, arg & 0xf, reg_map[(arg >> 4) & 0xf]));
			}
			else {
				EMIT_INSTRUCTION(EMIT_DATA_PROCESS_INS(0x08, tmp_reg, arg & 0xf, reg_map[(arg >> 4) & 0xf]));
				arg = tmp_reg | SLJIT_MEM_FLAG;
			}
		}

		EMIT_INSTRUCTION(EMIT_DATA_TRANSFER(inp_flags, 1, inp_flags & WRITE_BACK, reg, arg & 0xf, reg_map[TMP_REG3] | (max_delta & 0xf00 ? SRC2_IMM : 0)));
		return SLJIT_NO_ERROR;
	}

	if (arg == next_arg && !(inp_flags & WRITE_BACK) && ((sljit_uw)argw - (sljit_uw)next_argw <= (sljit_uw)max_delta || (sljit_uw)next_argw - (sljit_uw)argw <= (sljit_uw)max_delta)) {
		SLJIT_ASSERT(inp_flags & LOAD_DATA);
		TEST_FAIL(load_immediate(compiler, TMP_REG3, argw));

		if ((arg & 0xf0) != SLJIT_UNUSED) {
			EMIT_INSTRUCTION(EMIT_DATA_PROCESS_INS(0x08, TMP_REG3, TMP_REG3, reg_map[(arg >> 4) & 0xf]));
		}

		EMIT_INSTRUCTION(EMIT_DATA_PROCESS_INS(0x08, TMP_REG3, TMP_REG3, reg_map[arg & 0xf]));

		compiler->cache_arg = arg;
		compiler->cache_argw = argw;

		GETPUT_ARG_DATA_TRANSFER(1, 0, reg, TMP_REG3, 0);
		return SLJIT_NO_ERROR;
	}

	TEST_FAIL(load_immediate(compiler, tmp_reg, argw));

	if ((arg & 0xf0) != SLJIT_UNUSED) {
		EMIT_INSTRUCTION(EMIT_DATA_PROCESS_INS(0x08, tmp_reg, tmp_reg, reg_map[(arg >> 4) & 0xf]));
	}

	EMIT_INSTRUCTION(EMIT_DATA_TRANSFER(inp_flags, 1, inp_flags & WRITE_BACK, reg, arg & 0xf, reg_map[tmp_reg] | (max_delta & 0xf00 ? SRC2_IMM : 0)));
	return SLJIT_NO_ERROR;
}

#define ORDER_IND_REGS(arg) \
	SLJIT_ASSERT(arg & SLJIT_MEM_FLAG); \
	if (((arg >> 4) & 0xf) > (arg & 0xf)) \
		arg = SLJIT_MEM_FLAG | ((arg << 4) & 0xf0) | ((arg >> 4) & 0xf)

static int emit_op(struct sljit_compiler *compiler, int op, int inp_flags,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	// arg1 goes to TMP_REG1 or src reg
	// arg2 goes to TMP_REG2, imm or src reg
	// TMP_REG3 can be used for caching
	// result goes to TMP_REG2, so put result can use TMP_REG1 and TMP_REG3

	// We prefers register and simple consts
	int dst_r;
	int src1_r;
	int src2_r = 0;
	int sugg_src2_r = TMP_REG2;
	int flags = 0;

	compiler->cache_arg = 0;
	compiler->cache_argw = 0;

	// Destination check
	if (dst >= SLJIT_TEMPORARY_REG1 && dst <= TMP_REG3) {
		dst_r = dst;
		flags |= REG_DEST;
		if (op >= SLJIT_MOV && op <= SLJIT_MOVU_SI)
			sugg_src2_r = dst_r;
	}
	else if (dst == SLJIT_UNUSED) {
		if (op >= SLJIT_MOV && op <= SLJIT_MOVU_SI && !(src2 & SLJIT_MEM_FLAG))
			return SLJIT_NO_ERROR;
		dst_r = TMP_REG2;
	}
	else {
		SLJIT_ASSERT(dst & SLJIT_MEM_FLAG);
		if (getput_arg_fast(compiler, inp_flags | ARG_TEST, TMP_REG2, dst, dstw)) {
			flags |= FAST_DEST;
			dst_r = TMP_REG2;
		}
		else {
			flags |= SLOW_DEST;
			dst_r = 0;
		}
	}

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
		if ((inp_flags & ALLOW_ANY_IMM) && (src1 & SLJIT_IMM)) {
			// The second check will generate a hit
			src2_r = get_immediate(src1w);
			if (src2_r != 0) {
				flags |= ARGS_SWAPPED;
				src1 = src2;
				src1w = src2w;
			}
			if (inp_flags & ALLOW_INV_IMM) {
				src2_r = get_immediate(~src1w);
				if (src2_r != 0) {
					flags |= ARGS_SWAPPED | INV_IMM;
					src1 = src2;
					src1w = src2w;
				}
			}
		}

		src1_r = 0;
		if (getput_arg_fast(compiler, inp_flags | LOAD_DATA, TMP_REG1, src1, src1w)) {
			TEST_FAIL(compiler->error);
			src1_r = TMP_REG1;
		}
	}

	// Source 2
	if (src2_r == 0) {
		if (src2 >= SLJIT_TEMPORARY_REG1 && src2 <= TMP_REG3) {
			src2_r = src2;
			flags |= REG_SOURCE;
			if (!(flags & REG_DEST) && op >= SLJIT_MOV && op <= SLJIT_MOVU_SI)
				dst_r = src2_r;
		}
		else do { // do { } while(0) is used because of breaks
			if ((inp_flags & ALLOW_ANY_IMM) && (src2 & SLJIT_IMM)) {
				src2_r = get_immediate(src2w);
				if (src2_r != 0)
					break;
				if (inp_flags & ALLOW_INV_IMM) {
					src2_r = get_immediate(~src2w);
					if (src2_r != 0) {
						flags |= INV_IMM;
						break;
					}
				}
			}

			// src2_r is 0
			if (getput_arg_fast(compiler, inp_flags | LOAD_DATA, sugg_src2_r, src2, src2w)) {
				TEST_FAIL(compiler->error);
				src2_r = sugg_src2_r;
			}
		} while (0);
	}

	// src1_r, src2_r and dst_r can be zero (=unprocessed) or non-zero
	// If they are zero, they must not be registers
	if (src1_r == 0 && src2_r == 0 && dst_r == 0) {
		ORDER_IND_REGS(src1);
		ORDER_IND_REGS(src2);
		ORDER_IND_REGS(dst);
		if (!can_cache(src1, src1w, src2, src2w) && can_cache(src1, src1w, dst, dstw)) {
			SLJIT_ASSERT(!(flags & ARGS_SWAPPED));
			flags |= ARGS_SWAPPED;
			TEST_FAIL(getput_arg(compiler, inp_flags | LOAD_DATA, TMP_REG1, src2, src2w, src1, src1w));
			TEST_FAIL(getput_arg(compiler, inp_flags | LOAD_DATA, TMP_REG2, src1, src1w, dst, dstw));
		}
		else {
			TEST_FAIL(getput_arg(compiler, inp_flags | LOAD_DATA, TMP_REG1, src1, src1w, src2, src2w));
			TEST_FAIL(getput_arg(compiler, inp_flags | LOAD_DATA, TMP_REG2, src2, src2w, dst, dstw));
		}
		src1_r = TMP_REG1;
		src2_r = TMP_REG2;
	}
	else if (src1_r == 0 && src2_r == 0) {
		ORDER_IND_REGS(src1);
		ORDER_IND_REGS(src2);
		TEST_FAIL(getput_arg(compiler, inp_flags | LOAD_DATA, TMP_REG1, src1, src1w, src2, src2w));
		src1_r = TMP_REG1;
	}
	else if (src1_r == 0 && dst_r == 0) {
		ORDER_IND_REGS(src1);
		ORDER_IND_REGS(dst);
		TEST_FAIL(getput_arg(compiler, inp_flags | LOAD_DATA, TMP_REG1, src1, src1w, dst, dstw));
		src1_r = TMP_REG1;
	}
	else if (src2_r == 0 && dst_r == 0) {
		ORDER_IND_REGS(src2);
		ORDER_IND_REGS(dst);
		TEST_FAIL(getput_arg(compiler, inp_flags | LOAD_DATA, sugg_src2_r, src2, src2w, dst, dstw));
		src2_r = sugg_src2_r;
	}

	if (dst_r == 0)
		dst_r = TMP_REG2;

	if (src1_r == 0) {
		TEST_FAIL(getput_arg(compiler, inp_flags | LOAD_DATA, TMP_REG1, src1, src1w, 0, 0));
		src1_r = TMP_REG1;
	}

	if (src2_r == 0) {
		TEST_FAIL(getput_arg(compiler, inp_flags | LOAD_DATA, sugg_src2_r, src2, src2w, 0, 0));
		src2_r = sugg_src2_r;
	}

	TEST_FAIL(emit_single_op(compiler, op, flags, dst_r, src1_r, src2_r));

	if (flags & (FAST_DEST | SLOW_DEST)) {
		if (flags & FAST_DEST) {
			TEST_FAIL(getput_arg_fast(compiler, inp_flags, dst_r, dst, dstw));
		}
		else {
			TEST_FAIL(getput_arg(compiler, inp_flags, dst_r, dst, dstw, 0, 0));
		}
	}
	return SLJIT_NO_ERROR;
}

int sljit_emit_op0(struct sljit_compiler *compiler, int op)
{
	FUNCTION_ENTRY();

	SLJIT_ASSERT(GET_OPCODE(op) >= SLJIT_DEBUGGER && GET_OPCODE(op) <= SLJIT_DEBUGGER);
	sljit_emit_op0_verbose();

	op = GET_OPCODE(op);
	switch (op) {
	case SLJIT_DEBUGGER:
		TEST_FAIL(push_inst(compiler));
		compiler->last_type = LIT_INS;
		compiler->last_ins = 0xe1200070;
		break;
	}

	return SLJIT_NO_ERROR;
}

int sljit_emit_op1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
	FUNCTION_ENTRY();

	SLJIT_ASSERT(GET_OPCODE(op) >= SLJIT_MOV && GET_OPCODE(op) <= SLJIT_NEG);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_OP();
	FUNCTION_CHECK_SRC(src, srcw);
	FUNCTION_CHECK_DST(dst, dstw);
	FUNCTION_CHECK_OP1();
#endif
	sljit_emit_op1_verbose();

	op = GET_OPCODE(op);
	switch (op) {
	case SLJIT_MOV:
	case SLJIT_MOV_UI:
	case SLJIT_MOV_SI:
		return emit_op(compiler, SLJIT_MOV, ALLOW_ANY_IMM, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOV_UB:
		return emit_op(compiler, SLJIT_MOV_UB, ALLOW_ANY_IMM | BYTE_DATA, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (unsigned char)srcw : srcw);

	case SLJIT_MOV_SB:
		return emit_op(compiler, SLJIT_MOV_SB, ALLOW_ANY_IMM | SIGNED_DATA | BYTE_DATA, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (char)srcw : srcw);

	case SLJIT_MOV_UH:
		return emit_op(compiler, SLJIT_MOV_UH, ALLOW_ANY_IMM | HALF_DATA, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (unsigned short)srcw : srcw);

	case SLJIT_MOV_SH:
		return emit_op(compiler, SLJIT_MOV_SH, ALLOW_ANY_IMM | SIGNED_DATA | HALF_DATA, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (short)srcw : srcw);

	case SLJIT_MOVU:
	case SLJIT_MOVU_UI:
	case SLJIT_MOVU_SI:
		return emit_op(compiler, SLJIT_MOV, ALLOW_ANY_IMM | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOVU_UB:
		return emit_op(compiler, SLJIT_MOV_UB, ALLOW_ANY_IMM | BYTE_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (unsigned char)srcw : srcw);

	case SLJIT_MOVU_SB:
		return emit_op(compiler, SLJIT_MOV_SB, ALLOW_ANY_IMM | SIGNED_DATA | BYTE_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (char)srcw : srcw);

	case SLJIT_MOVU_UH:
		return emit_op(compiler, SLJIT_MOV_UH, ALLOW_ANY_IMM | HALF_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (unsigned short)srcw : srcw);

	case SLJIT_MOVU_SH:
		return emit_op(compiler, SLJIT_MOV_SH, ALLOW_ANY_IMM | SIGNED_DATA | HALF_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (short)srcw : srcw);

	case SLJIT_NOT:
		return emit_op(compiler, SLJIT_NOT, ALLOW_ANY_IMM, dst, dstw, TMP_REG1, 0, src, srcw);

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

	SLJIT_ASSERT(GET_OPCODE(op) >= SLJIT_ADD && GET_OPCODE(op) <= SLJIT_ASHR);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_OP();
	FUNCTION_CHECK_SRC(src1, src1w);
	FUNCTION_CHECK_SRC(src2, src2w);
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_op2_verbose();

	op = GET_OPCODE(op);
	switch (op) {
	case SLJIT_ADD:
	case SLJIT_ADDC:
	case SLJIT_SUB:
	case SLJIT_SUBC:
	case SLJIT_OR:
	case SLJIT_XOR:
		return emit_op(compiler, op, ALLOW_IMM, dst, dstw, src1, src1w, src2, src2w);
	case SLJIT_MUL:
		return emit_op(compiler, op, 0, dst, dstw, src1, src1w, src2, src2w);
	case SLJIT_AND:
		return emit_op(compiler, op, ALLOW_ANY_IMM, dst, dstw, src1, src1w, src2, src2w);
	case SLJIT_SHL:
	case SLJIT_LSHR:
	case SLJIT_ASHR:
		if (src2 & SLJIT_IMM) {
			compiler->shift_imm = src2w & 0x1f;
			return emit_op(compiler, op, 0, dst, dstw, TMP_REG1, 0, src1, src1w);
		}
		else {
			compiler->shift_imm = 0x20;
			return emit_op(compiler, op, 0, dst, dstw, src1, src1w, src2, src2w);
		}
	}

	return SLJIT_NO_ERROR;
}

// ---------------------------------------------------------------------
//  Floating point operators
// ---------------------------------------------------------------------

#ifdef SLJIT_CONFIG_ARM_V5

// Two ARM fpus are supported: vfp and fpa

// 0 - no fpu
// 1 - vfp
// 2 - fpa
static int arm_fpu_type = -1;

static void init_compiler()
{
	if (arm_fpu_type != -1)
		return;

	// TODO: Only the OS can help to determine the correct fpu type
	arm_fpu_type = 1;
}

int sljit_is_fpu_available(void)
{
	if (arm_fpu_type == -1)
		init_compiler();
	return (arm_fpu_type > 0) ? 1 : 0;
}

#elif defined(SLJIT_CONFIG_ARM_V7)

#define arm_fpu_type 1

int sljit_is_fpu_available(void)
{
	return 1;
}

#else
#error "Unknown ARM architecture"
#endif

#define EMIT_FPU_DATA_TRANSFER(add, load, base, freg, offs) \
	(((arm_fpu_type == 1) ? 0xed000b00 : 0xed008100) | ((add) << 23) | ((load) << 20) | (reg_map[base] << 16) | (freg << 12) | (offs))
#define EMIT_FPU_OPERATION(vfp_opcode, fpa_opcode, dst, src1, src2) \
	(((arm_fpu_type == 1) ? vfp_opcode : fpa_opcode) | ((dst) << 12) | (src1) | ((src2) << 16))

static int emit_fpu_data_transfer(struct sljit_compiler *compiler, int fpu_reg, int load, int arg, sljit_w argw)
{
	SLJIT_ASSERT(arg & SLJIT_MEM_FLAG);

	// Fast loads and stores
	if ((arg & 0xf) != 0 && (arg & 0xf0) == 0 && (argw & 0x3) == 0) {
		if (argw >= 0 && argw <= 0x3ff) {
			EMIT_INSTRUCTION(EMIT_FPU_DATA_TRANSFER(1, load, arg & 0xf, fpu_reg, argw >> 2));
			return SLJIT_NO_ERROR;
		}
		if (argw < 0 && argw >= -0x3ff) {
			EMIT_INSTRUCTION(EMIT_FPU_DATA_TRANSFER(0, load, arg & 0xf, fpu_reg, (-argw) >> 2));
			return SLJIT_NO_ERROR;
		}
		if (argw >= 0 && argw <= 0x3ffff) {
			SLJIT_ASSERT(get_immediate(argw & 0x3fc00) != 0);
			EMIT_INSTRUCTION(EMIT_DATA_PROCESS_INS(0x08, TMP_REG1, arg & 0xf, get_immediate(argw & 0x3fc00)));
			argw &= 0x3ff;
			EMIT_INSTRUCTION(EMIT_FPU_DATA_TRANSFER(1, load, TMP_REG1, fpu_reg, argw >> 2));
			return SLJIT_NO_ERROR;
		}
		if (argw < 0 && argw >= -0x3ffff) {
			argw = -argw;
			SLJIT_ASSERT(get_immediate(argw & 0x3fc00) != 0);
			EMIT_INSTRUCTION(EMIT_DATA_PROCESS_INS(0x04, TMP_REG1, arg & 0xf, get_immediate(argw & 0x3fc00)));
			argw &= 0x3ff;
			EMIT_INSTRUCTION(EMIT_FPU_DATA_TRANSFER(0, load, TMP_REG1, fpu_reg, argw >> 2));
			return SLJIT_NO_ERROR;
		}
	}

	ORDER_IND_REGS(arg);

	if (compiler->cache_arg == arg && ((argw - compiler->cache_argw) & 0x3) == 0) {
		if (((sljit_uw)argw - (sljit_uw)compiler->cache_argw) <= 0x3ff) {
			EMIT_INSTRUCTION(EMIT_FPU_DATA_TRANSFER(1, load, TMP_REG3, fpu_reg, (argw - compiler->cache_argw) >> 2));
			return SLJIT_NO_ERROR;
		}
		if (((sljit_uw)compiler->cache_argw - (sljit_uw)argw) <= 0x3ff) {
			EMIT_INSTRUCTION(EMIT_FPU_DATA_TRANSFER(0, load, TMP_REG3, fpu_reg, (compiler->cache_argw - argw) >> 2));
			return SLJIT_NO_ERROR;
		}
	}

	compiler->cache_arg = arg;
	compiler->cache_argw = 0;
	if ((arg & 0xf0) != 0) {
		EMIT_INSTRUCTION(EMIT_DATA_PROCESS_INS(0x08, TMP_REG3, arg & 0xf, reg_map[(arg >> 4) & 0xf]));

		if ((argw & 0x3) == 0) {
			if (argw >= 0 && argw <= 0x3ff) {
				EMIT_INSTRUCTION(EMIT_FPU_DATA_TRANSFER(1, load, TMP_REG3, fpu_reg, argw >> 2));
				return SLJIT_NO_ERROR;
			}
			if (argw < 0 && argw >= -0x3ff) {
				EMIT_INSTRUCTION(EMIT_FPU_DATA_TRANSFER(0, load, TMP_REG3, fpu_reg, (-argw) >> 2));
				return SLJIT_NO_ERROR;
			}
		}

		compiler->cache_argw = argw;
		TEST_FAIL(load_immediate(compiler, TMP_REG1, argw));

		EMIT_INSTRUCTION(EMIT_DATA_PROCESS_INS(0x08, TMP_REG3, TMP_REG3, reg_map[TMP_REG1]));
	}
	else if ((arg & 0xf) != 0) {
		compiler->cache_argw = argw;
		TEST_FAIL(load_immediate(compiler, TMP_REG1, argw));

		EMIT_INSTRUCTION(EMIT_DATA_PROCESS_INS(0x08, TMP_REG3, arg & 0xf, reg_map[TMP_REG1]));
	}
	else {
		compiler->cache_argw = argw;
		TEST_FAIL(load_immediate(compiler, TMP_REG3, argw));
	}

	EMIT_INSTRUCTION(EMIT_FPU_DATA_TRANSFER(1, load, TMP_REG3, fpu_reg, 0));
	return SLJIT_NO_ERROR;
}

int sljit_emit_fop1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
	int dst_freg;

	FUNCTION_ENTRY();

	SLJIT_ASSERT(sljit_is_fpu_available());
	SLJIT_ASSERT(GET_OPCODE(op) >= SLJIT_FCMP && GET_OPCODE(op) <= SLJIT_FABS);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_OP();
	FUNCTION_FCHECK(src, srcw);
	FUNCTION_FCHECK(dst, dstw);
	FUNCTION_CHECK_FOP();
#endif
	sljit_emit_fop1_verbose();

	compiler->cache_arg = 0;
	compiler->cache_argw = 0;

	if (GET_OPCODE(op) == SLJIT_FCMP) {
		if (dst > SLJIT_FLOAT_REG4) {
			TEST_FAIL(emit_fpu_data_transfer(compiler, TMP_FREG1, 1, dst, dstw));
			dst = TMP_FREG1;
		}
		if (src > SLJIT_FLOAT_REG4) {
			TEST_FAIL(emit_fpu_data_transfer(compiler, TMP_FREG2, 1, src, srcw));
			src = TMP_FREG2;
		}
		if (arm_fpu_type == 1) {
			EMIT_INSTRUCTION(0xeeb40b40 | (dst << 12) | src);
			EMIT_INSTRUCTION(0xeef1fa10);
		}
		else
			EMIT_INSTRUCTION(0xee90f110 | (dst << 16) | src);
		return SLJIT_NO_ERROR;
	}

	dst_freg = (dst > SLJIT_FLOAT_REG4) ? TMP_FREG1 : dst;

	if (src > SLJIT_FLOAT_REG4) {
		TEST_FAIL(emit_fpu_data_transfer(compiler, dst_freg, 1, src, srcw));
		src = dst_freg;
	}

	switch (op) {
		case SLJIT_FMOV:
			if (src != dst_freg && dst_freg != TMP_FREG1)
				EMIT_INSTRUCTION(EMIT_FPU_OPERATION(0xeeb00b40, 0xee008180, dst_freg, src, 0));
			break;
		case SLJIT_FNEG:
			EMIT_INSTRUCTION(EMIT_FPU_OPERATION(0xeeb10b40, 0xee108180, dst_freg, src, 0));
			break;
		case SLJIT_FABS:
			EMIT_INSTRUCTION(EMIT_FPU_OPERATION(0xeeb00bc0, 0xee208180, dst_freg, src, 0));
			break;
	}

	if (dst_freg == TMP_FREG1) {
		TEST_FAIL(emit_fpu_data_transfer(compiler, src, 0, dst, dstw));
	}

	return SLJIT_NO_ERROR;
}

int sljit_emit_fop2(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	int dst_freg;

	FUNCTION_ENTRY();

	SLJIT_ASSERT(sljit_is_fpu_available());
	SLJIT_ASSERT(GET_OPCODE(op) >= SLJIT_FADD && GET_OPCODE(op) <= SLJIT_FDIV);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_OP();
	FUNCTION_FCHECK(src1, src1w);
	FUNCTION_FCHECK(src2, src2w);
	FUNCTION_FCHECK(dst, dstw);
	FUNCTION_CHECK_FOP();
#endif
	sljit_emit_fop2_verbose();

	compiler->cache_arg = 0;
	compiler->cache_argw = 0;

	dst_freg = (dst > SLJIT_FLOAT_REG4) ? TMP_FREG1 : dst;

	if (src2 > SLJIT_FLOAT_REG4) {
		TEST_FAIL(emit_fpu_data_transfer(compiler, TMP_FREG2, 1, src2, src2w));
		src2 = TMP_FREG2;
	}

	if (src1 > SLJIT_FLOAT_REG4) {
		TEST_FAIL(emit_fpu_data_transfer(compiler, TMP_FREG1, 1, src1, src1w));
		src1 = TMP_FREG1;
	}

	switch (op) {
	case SLJIT_FADD:
		EMIT_INSTRUCTION(EMIT_FPU_OPERATION(0xee300b00, 0xee000180, dst_freg, src2, src1));
		break;

	case SLJIT_FSUB:
		EMIT_INSTRUCTION(EMIT_FPU_OPERATION(0xee300b40, 0xee200180, dst_freg, src2, src1));
		break;

	case SLJIT_FMUL:
		EMIT_INSTRUCTION(EMIT_FPU_OPERATION(0xee200b00, 0xee100180, dst_freg, src2, src1));
		break;

	case SLJIT_FDIV:
		EMIT_INSTRUCTION(EMIT_FPU_OPERATION(0xee800b00, 0xee400180, dst_freg, src2, src1));
		break;
	}

	if (dst_freg == TMP_FREG1) {
		TEST_FAIL(emit_fpu_data_transfer(compiler, TMP_FREG1, 0, dst, dstw));
	}

	return SLJIT_NO_ERROR;
}

// ---------------------------------------------------------------------
//  Conditional instructions
// ---------------------------------------------------------------------

static sljit_uw get_cc(int type)
{
	switch (type) {
	case SLJIT_C_EQUAL:
		return 0x00000000;

	case SLJIT_C_NOT_EQUAL:
		return 0x10000000;

	case SLJIT_C_LESS:
		return 0x30000000;

	case SLJIT_C_NOT_LESS:
		return 0x20000000;

	case SLJIT_C_GREATER:
		return 0x80000000;

	case SLJIT_C_NOT_GREATER:
		return 0x90000000;

	case SLJIT_C_SIG_LESS:
		return 0xb0000000;

	case SLJIT_C_SIG_NOT_LESS:
		return 0xa0000000;

	case SLJIT_C_SIG_GREATER:
		return 0xc0000000;

	case SLJIT_C_SIG_NOT_GREATER:
		return 0xd0000000;

	case SLJIT_C_OVERFLOW:
		return 0x60000000;

	case SLJIT_C_NOT_OVERFLOW:
		return 0x70000000;

	default: // SLJIT_JUMP
		return 0xe0000000;
	}
}

struct sljit_label* sljit_emit_label(struct sljit_compiler *compiler)
{
	struct sljit_label *label;

	FUNCTION_ENTRY();

	sljit_emit_label_verbose();

	// Flush the pipe to get the real addr
	if (push_inst(compiler))
		return NULL;

	if (compiler->last_label && compiler->last_label->size == compiler->size)
		return compiler->last_label;

	label = (struct sljit_label*)ensure_abuf(compiler, sizeof(struct sljit_label));
	TEST_MEM_ERROR2(label);

	label->next = NULL;
	label->size = compiler->size;
	if (compiler->last_label)
		compiler->last_label->next = label;
	else
		compiler->labels = label;
	compiler->last_label = label;

	return label;
}

struct sljit_jump* sljit_emit_jump(struct sljit_compiler *compiler, int type)
{
	struct sljit_jump *jump;

	FUNCTION_ENTRY();
	SLJIT_ASSERT((type & ~0x1ff) == 0);
	SLJIT_ASSERT((type & 0xff) >= SLJIT_C_EQUAL && (type & 0xff) <= SLJIT_CALL3);

	sljit_emit_jump_verbose();

	// Flush the pipe to get the real addr
	TEST_FAIL2(push_inst(compiler));

	jump = (struct sljit_jump*)ensure_abuf(compiler, sizeof(struct sljit_jump));
	TEST_MEM_ERROR2(jump);

	jump->next = NULL;
	jump->flags = type & SLJIT_LONG_JUMP;
	type &= 0xff;
	if (compiler->last_jump)
		compiler->last_jump->next = jump;
	else
		compiler->jumps = jump;
	compiler->last_jump = jump;

	// In ARM, we don't need to touch the arguments

	if (!jump->flags) {
		if (type >= SLJIT_CALL0)
			jump->flags |= IS_BL;
		compiler->last_type = LIT_UCINS;
		compiler->last_ins = get_cc(type) | 0x059ff000;
		compiler->last_imm = 0;
	}
	else {
		compiler->last_type = LIT_UCINS;
		compiler->last_ins = get_cc(type) | 0x059ff000;
		compiler->last_imm = 0;
		compiler->patches++;
	}

	if (type >= SLJIT_CALL0)
		TEST_FAIL2(emit_blx(compiler));

	jump->addr = compiler->size;
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

	// Flush the pipe to get the real addr
	TEST_FAIL(push_inst(compiler));

	// In ARM, we don't need to touch the arguments

	if (src & SLJIT_IMM) {
		jump = (struct sljit_jump*)ensure_abuf(compiler, sizeof(struct sljit_jump));
		TEST_MEM_ERROR(jump);

		jump->next = NULL;
		jump->flags = JUMP_ADDR | ((type >= SLJIT_CALL0) ? IS_BL : 0);
		jump->target = srcw;
		if (compiler->last_jump)
			compiler->last_jump->next = jump;
		else
			compiler->jumps = jump;
		compiler->last_jump = jump;

		compiler->last_type = LIT_CINS;
		compiler->last_ins = EMIT_DATA_TRANSFER(WORD_DATA | LOAD_DATA, 1, 0, TMP_PC, TMP_PC, 0);
		compiler->last_imm = srcw;

		if (type >= SLJIT_CALL0)
			TEST_FAIL(emit_blx(compiler));
		jump->addr = compiler->size;
	}
	else {
		TEST_FAIL(emit_op(compiler, SLJIT_MOV, ALLOW_ANY_IMM, TMP_REG2, 0, TMP_REG1, 0, src, srcw));
		SLJIT_ASSERT((compiler->last_ins & 0x0c00f000) == (sljit_uw)(0x00000000 | (reg_map[TMP_REG2] << 12)) ||
			(compiler->last_ins & 0x0c00f000) == (sljit_uw)(0x04000000 | (reg_map[TMP_REG2] << 12)));

		if (type < SLJIT_CALL0) {
			// Changing the Rd to PC
			compiler->last_ins |= 0x0000f000;
		} else {
			TEST_FAIL(push_inst(compiler));
			compiler->last_type = LIT_INS;
			compiler->last_ins = EMIT_BLX(TMP_REG2);
		}
	}

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

	if (dst == SLJIT_UNUSED)
		return SLJIT_NO_ERROR;

	if (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_NO_REGISTERS)
		reg = dst;
	else
		reg = TMP_REG2;

	EMIT_INSTRUCTION(0xe3a00000 | (reg_map[reg] << 12));
	EMIT_INSTRUCTION(0x03a00001 | (reg_map[reg] << 12) | get_cc(type));

	if (reg == TMP_REG2)
		return emit_op(compiler, SLJIT_MOV, ALLOW_ANY_IMM, dst, dstw, TMP_REG1, 0, TMP_REG2, 0);
	else
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

	// Flush the pipe to get the real addr
	if (push_inst(compiler))
		return NULL;

	const_ = (struct sljit_const*)ensure_abuf(compiler, sizeof(struct sljit_const));
	TEST_MEM_ERROR2(const_);

	const_->next = NULL;
	const_->addr = compiler->size;
	if (compiler->last_const)
		compiler->last_const->next = const_;
	else
		compiler->consts = const_;
	compiler->last_const = const_;

	reg = (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_NO_REGISTERS) ? dst : TMP_REG2;

	compiler->last_type = LIT_UCINS;
	compiler->last_ins = EMIT_DATA_TRANSFER(WORD_DATA | LOAD_DATA, 1, 0, reg, TMP_PC, 0);
	compiler->last_imm = initval;
	compiler->patches++;

	if (reg == TMP_REG2 && dst != SLJIT_UNUSED)
		if (emit_op(compiler, SLJIT_MOV, ALLOW_ANY_IMM, dst, dstw, TMP_REG1, 0, TMP_REG2, 0))
			return NULL;
	return const_;
}

void sljit_set_jump_addr(sljit_uw addr, sljit_uw new_addr)
{
	sljit_uw *ptr = (sljit_uw*)addr;
	sljit_uw *inst = (sljit_uw*)ptr[0];
	sljit_uw mov_pc = ptr[1];
	sljit_w diff = (sljit_w)(((sljit_w)new_addr - (sljit_w)(inst + 2)) >> 2);

	if (diff <= 0x7fffff && diff >= -0x800000) {
		// Turn to branch
		*inst = (mov_pc & 0xf0000000) | 0x0a000000 | (diff & 0xffffff);
		SLJIT_CACHE_FLUSH(inst, inst + 1);
	} else {
		// Get the position of the constant
		if (mov_pc & (1 << 23))
			ptr = inst + ((mov_pc & 0xfff) >> 2) + 2;
		else
			ptr = inst + 1;

		if (*inst != mov_pc) {
			*inst = mov_pc;
			SLJIT_CACHE_FLUSH(inst, inst + 1);
		}
		*ptr = new_addr;
	}
}

void sljit_set_const(sljit_uw addr, sljit_w new_constant)
{
	sljit_uw *ptr = (sljit_uw*)addr;
	sljit_uw *inst = (sljit_uw*)ptr[0];
	sljit_uw mov_pc = ptr[1];
	sljit_uw src2;

	src2 = get_immediate(new_constant);
	if (src2 != 0) {
		*inst = 0xe3a00000 | (mov_pc & 0xf000) | src2;
		SLJIT_CACHE_FLUSH(inst, inst + 1);
		return;
	}

	src2 = get_immediate(~new_constant);
	if (src2 != 0) {
		*inst = 0xe3e00000 | (mov_pc & 0xf000) | src2;
		SLJIT_CACHE_FLUSH(inst, inst + 1);
		return;
	}

	if (mov_pc & (1 << 23))
		ptr = inst + ((mov_pc & 0xfff) >> 2) + 2;
	else
		ptr = inst + 1;

	if (*inst != mov_pc) {
		*inst = mov_pc;
		SLJIT_CACHE_FLUSH(inst, inst + 1);
	}
	*ptr = new_constant;
}
