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

// 32b register indexes:
//   0 - EAX
//   1 - ECX
//   2 - EDX
//   3 - EBX
//   4 - none
//   5 - EBP
//   6 - ESI
//   7 - EDI

// 64b register indexes:
//   0 - RAX
//   1 - RCX
//   2 - RDX
//   3 - RBX
//   4 - none
//   5 - RBP
//   6 - RSI
//   7 - RDI
//   8 - R8   // From now on REX prefix is required
//   9 - R9
//  10 - R10
//  11 - R11
//  12 - R12
//  13 - R13
//  14 - R14
//  15 - R15

#ifdef SLJIT_CONFIG_X86_32

// Last register + 1
#define TMP_REGISTER	(SLJIT_GENERAL_REG3 + 1)

static sljit_ub reg_map[SLJIT_NO_REGISTERS + 2] = {
   0, 0, 2, 1, 3, 6, 7, 5
};

#else

// Last register + 1
#define TMP_REGISTER	(SLJIT_GENERAL_REG3 + 1)
#define TMP_REG2	(SLJIT_GENERAL_REG3 + 2)
#define TMP_REG3	(SLJIT_GENERAL_REG3 + 3)

static sljit_ub reg_map[SLJIT_NO_REGISTERS + 4] = {
   0, 0, 6, 1, 3, 14, 15, 7, 8, 9
};

// re low-map. reg_map & 0x7
static sljit_ub reg_lmap[SLJIT_NO_REGISTERS + 4] = {
   0, 0, 6, 1, 3, 6, 7, 7, 0, 1
};

#define REX_W		0x48
#define REX_R		0x44
#define REX_X		0x42
#define REX_B		0x41

typedef unsigned int sljit_uhw;
typedef int sljit_hw;

#define NOT_HALFWORD(x)		((x) > 0x7fffffffl || (x) < -0x80000000l)

#endif

#define INC_SIZE(s)			(*buf++ = (s), compiler->size += (s))

#define PUSH_REG(r)			(*buf++ = (0x50 + (r)))
#define POP_REG(r)			(*buf++ = (0x58 + (r)))
#define RET()				(*buf++ = (0xc3))
#define RETN(n)				(*buf++ = (0xc2), *buf++ = n, *buf++ = 0)
// r32, r/m32
#define MOV_RM(mod, reg, rm)		(*buf++ = (0x8b), *buf++ = (mod) << 6 | (reg) << 3 | (rm))

static sljit_ub get_jump_code(int type)
{
	switch (type) {
	case SLJIT_C_EQUAL:
		return 0x84;

	case SLJIT_C_NOT_EQUAL:
		return 0x85;

	case SLJIT_C_LESS:
		return 0x82;

	case SLJIT_C_NOT_LESS:
		return 0x83;

	case SLJIT_C_GREATER:
		return 0x87;

	case SLJIT_C_NOT_GREATER:
		return 0x86;

	case SLJIT_C_SIG_LESS:
		return 0x8c;

	case SLJIT_C_SIG_NOT_LESS:
		return 0x8d;

	case SLJIT_C_SIG_GREATER:
		return 0x8f;

	case SLJIT_C_SIG_NOT_GREATER:
		return 0x8e;

	case SLJIT_C_CARRY:
		return 0x82;

	case SLJIT_C_NOT_CARRY:
		return 0x83;

	case SLJIT_C_ZERO:
		return 0x84;

	case SLJIT_C_NOT_ZERO:
		return 0x85;

	case SLJIT_C_OVERFLOW:
		return 0x80;

	case SLJIT_C_NOT_OVERFLOW:
		return 0x81;
	}
	return 0;
}

static sljit_ub* generate_jump_code(struct sljit_jump *jump, sljit_ub *code_ptr, sljit_ub *code, int type)
{
	int short_jump;
	sljit_uw label_addr;

	if (!(jump->flags & SLJIT_LONG_JUMP)) {
		SLJIT_ASSERT(jump->flags & JUMP_LABEL);

		label_addr = (sljit_uw)(code + jump->label->size);
		short_jump = (sljit_w)(label_addr - (jump->addr + 2)) >= -128 && (sljit_w)(label_addr - (jump->addr + 2)) <= 127;

		if (type == SLJIT_JUMP) {
			if (short_jump)
				*code_ptr++ = 0xeb;
			else
				*code_ptr++ = 0xe9;
			jump->addr++;
		}
		else if (short_jump) {
			*code_ptr++ = get_jump_code(type) - 0x10;
			jump->addr++;
		}
		else {
			*code_ptr++ = 0x0f;
			*code_ptr++ = get_jump_code(type);
			jump->addr += 2;
		}

		if (short_jump) {
			jump->flags |= PATCH_MB;
			code_ptr ++;
		} else {
			jump->flags |= PATCH_MW;
			code_ptr += 4;
		}
	}
	else {
		SLJIT_ASSERT(jump->flags & (JUMP_LABEL | JUMP_ADDR));

		if (type == SLJIT_JUMP) {
			*code_ptr++ = 0xe9;
			jump->addr++;
		}
		else {
			*code_ptr++ = 0x0f;
			*code_ptr++ = get_jump_code(type);
			jump->addr += 2;
		}

		if (jump->flags & JUMP_LABEL)
			jump->flags |= PATCH_MW;
		else
			*(sljit_w*)code_ptr = jump->target - (jump->addr + 4); 
		code_ptr += 4;
	}
	return code_ptr;
}

void* sljit_generate_code(struct sljit_compiler *compiler)
{
	struct sljit_memory_fragment *buf;
	sljit_ub *code;
	sljit_ub *code_ptr;
	sljit_ub *buf_ptr;
	sljit_ub *buf_end;
	sljit_ub len;

	struct sljit_label *label;
	struct sljit_jump *jump;
	struct sljit_const *const_;

	FUNCTION_ENTRY();
	SLJIT_ASSERT(compiler->size > 0);
	reverse_buf(compiler);

	// Second code generation pass
	code = SLJIT_MALLOC_EXEC(compiler->size);
	if (!code) {
		compiler->error = SLJIT_MEMORY_ERROR;
		return NULL;
	}
	buf = compiler->buf;

	code_ptr = code;
	label = compiler->labels;
	jump = compiler->jumps;
	const_ = compiler->consts;
	do {
		buf_ptr = buf->memory;
		buf_end = buf_ptr + buf->used_size;
		while (buf_ptr < buf_end) {
			len = *buf_ptr++;
			if (len > 0) {
				// The code is already generated
				SLJIT_MEMMOVE(code_ptr, buf_ptr, len);
				code_ptr += len;
				buf_ptr += len;
			}
			else {
				if (*buf_ptr >= 3) {
					jump->addr = (sljit_uw)code_ptr;
					code_ptr = generate_jump_code(jump, code_ptr, code, *buf_ptr - 3);
					jump = jump->next;
				}
				else if (*buf_ptr == 0) {
					label->addr = (sljit_uw)code_ptr;
					label->size = code_ptr - code;
					label = label->next;
				}
				else if (*buf_ptr == 1) {
					const_->addr = ((sljit_uw)code_ptr) - sizeof(sljit_w);
					const_ = const_->next;
				}
				else
					*(sljit_w*)(code_ptr - 4) -= (sljit_w)code_ptr; 
				buf_ptr++;
			}
		}
		SLJIT_ASSERT(buf_ptr == buf_end);
		buf = buf->next;
	} while (buf != NULL);

	jump = compiler->jumps;
	while (jump) {
		if (jump->flags & PATCH_MB) {
			SLJIT_ASSERT((sljit_w)(jump->label->addr - (jump->addr + 1)) >= -128 && (sljit_w)(jump->label->addr - (jump->addr + 1)) <= 127);
			*(sljit_ub*)jump->addr = jump->label->addr - (jump->addr + 1);
		} else if (jump->flags & PATCH_MW)
			*(sljit_w*)jump->addr = jump->label->addr - (jump->addr + 4);
		jump = jump->next;
	}

	// Maybe we waste some space because of short jumps
	SLJIT_ASSERT(code_ptr <= code + compiler->size);
	compiler->error = SLJIT_CODE_GENERATED;
	return (void*)code;
}

void sljit_free_code(void* code)
{
	SLJIT_FREE_EXEC(code);
}

// ---------------------------------------------------------------------
//  Operators
// ---------------------------------------------------------------------

#ifdef SLJIT_CONFIG_X86_32
#include "sljitNativeX86_32.c"
#elif defined(SLJIT_CONFIG_X86_64)
#include "sljitNativeX86_64.c"
#endif

static sljit_ub* emit_x86_bin_instruction(struct sljit_compiler *compiler, int size,
	// The register or immediate operand
	int a, sljit_w imma,
	// The general operand (not immediate)
	int b, sljit_w immb)
{
	sljit_ub *buf;
	sljit_ub *buf_ptr;
	int total_size = size;
	int byte = 0;

	// Calculate size of b
	total_size += 1; // mod r/m byte
	if (b & SLJIT_MEM_FLAG) {
		if ((b & 0xf0) != 0)
			total_size += 1; // SIB byte

		if ((b & 0x0f) == SLJIT_NO_REG)
			total_size += 4;
		else if (immb != 0) {
			// Immediate operand
			if (immb <= 127 && immb >= -128)
				total_size += 1;
			else
				total_size += 4;
		}
	}

	// Calculate size of a
	if (a & SLJIT_IMM) {
		if (imma <= 127 && imma >= -128) {
			total_size += 1;
			byte = 1;
		} else
			total_size += 4;
	}

	buf = ensure_buf(compiler, 1 + total_size);
	TEST_MEM_ERROR2(buf);

	// Encoding the byte
	INC_SIZE(total_size);
	buf_ptr = buf + size;

	// Encode mod/rm byte
	if (a & SLJIT_IMM) {
		*buf = byte ? 0x83 : 0x81;
		*buf_ptr = 0;
	}
	else if (a == 0)
		*buf_ptr = 0;
	else 
		*buf_ptr = reg_map[a] << 3;

	if (!(b & SLJIT_MEM_FLAG))
		*buf_ptr++ |= 0xc0 + reg_map[b];
	else if ((b & 0x0f) != SLJIT_NO_REG) {
		if (immb != 0) {
			if (immb <= 127 && immb >= -128)
				*buf_ptr |= 0x40;
			else
				*buf_ptr |= 0x80;
		}

		if ((b & 0xf0) == 0) {
			*buf_ptr++ |= reg_map[b & 0x0f];
		} else {
			*buf_ptr++ |= 0x04;
			*buf_ptr++ = reg_map[b & 0x0f] | (reg_map[(b >> 4) & 0x0f] << 3);
		}

		if (immb != 0) {
			if (immb <= 127 && immb >= -128)
				*buf_ptr++ = immb; // 8 bit displacement
			else {
				*(sljit_w*)buf_ptr = immb; // 32 bit displacement
				buf_ptr += sizeof(sljit_w);
			}
		}
	}
	else {
		*buf_ptr++ |= 0x05;
		*(sljit_w*)buf_ptr = immb; // 32 bit displacement
	}

	if (a & SLJIT_IMM) {
		if (byte)
			*buf_ptr = imma;
		else
			*(sljit_w*)buf_ptr = imma;
	}

	return buf;
}

static sljit_ub* emit_x86_shift_instruction(struct sljit_compiler *compiler,
	// Imm or SLJIT_PREF_SHIFT_REG
	int a, sljit_w imma,
	// The general operand (not immediate)
	int b, sljit_w immb)
{
	sljit_ub *buf;
	sljit_ub *buf_ptr;
	int total_size = 1;

	// Calculate size of b
	total_size += 1; // mod r/m byte
	if (b & SLJIT_MEM_FLAG) {
		if ((b & 0xf0) != 0)
			total_size += 1; // SIB byte

		if ((b & 0x0f) == SLJIT_NO_REG)
			total_size += 4;
		else if (immb != 0) {
			// Immediate operand
			if (immb <= 127 && immb >= -128)
				total_size += 1;
			else
				total_size += 4;
		}
	}

	// Calculate size of a
	if (a & SLJIT_IMM) {
		imma &= 0x1f;
		if (imma != 1)
			total_size ++;
	}
	else
		SLJIT_ASSERT(a == SLJIT_PREF_SHIFT_REG);

	buf = ensure_buf(compiler, 1 + total_size);
	TEST_MEM_ERROR2(buf);

	// Encoding the byte
	INC_SIZE(total_size);
	buf_ptr = buf;

	// Encode mod/rm byte
	if (a & SLJIT_IMM) {
		if (imma == 1)
			*buf_ptr++ = 0xd1;
		else
			*buf_ptr++ = 0xc1;
	} else
		*buf_ptr++ = 0xd3;
	*buf_ptr = 0;

	if (!(b & SLJIT_MEM_FLAG))
		*buf_ptr++ |= 0xc0 + reg_map[b];
	else if ((b & 0x0f) != SLJIT_NO_REG) {
		if (immb != 0) {
			if (immb <= 127 && immb >= -128)
				*buf_ptr |= 0x40;
			else
				*buf_ptr |= 0x80;
		}

		if ((b & 0xf0) == 0) {
			*buf_ptr++ |= reg_map[b & 0x0f];
		} else {
			*buf_ptr++ |= 0x04;
			*buf_ptr++ = reg_map[b & 0x0f] | (reg_map[(b >> 4) & 0x0f] << 3);
		}

		if (immb != 0) {
			if (immb <= 127 && immb >= -128)
				*buf_ptr++ = immb; // 8 bit displacement
			else {
				*(sljit_w*)buf_ptr = immb; // 32 bit displacement
				buf_ptr += sizeof(sljit_w);
			}
		}
	}
	else {
		*buf_ptr++ |= 0x05;
		*(sljit_w*)buf_ptr = immb; // 32 bit displacement
	}

	if (a & SLJIT_IMM && (imma != 1))
		*buf_ptr = imma;

	return buf + 1;
}

static int emit_mov(struct sljit_compiler *compiler,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
	sljit_ub* code;

	if (dst == SLJIT_NO_REG) {
		// No destination, doesn't need to setup flags
		if (src & SLJIT_MEM_FLAG) {
			code = emit_x86_instruction(compiler, 1, TMP_REGISTER, 0, src, srcw);
			TEST_MEM_ERROR(code);
			*code = 0x8b;
		}
		return SLJIT_NO_ERROR;
	}
	if (src >= SLJIT_TEMPORARY_REG1 && src <= TMP_REGISTER) {
		code = emit_x86_instruction(compiler, 1, src, 0, dst, dstw);
		TEST_MEM_ERROR(code);
		*code = 0x89;
		return SLJIT_NO_ERROR;
	}
	if (src & SLJIT_IMM) {
		if (dst >= SLJIT_TEMPORARY_REG1 && dst <= TMP_REGISTER) {
#ifdef SLJIT_CONFIG_X86_32
			return emit_do_imm(compiler, 0xb8 + reg_map[dst], srcw);
#else
			if (!compiler->mode32) {
				if (NOT_HALFWORD(srcw))
					return emit_load_imm64(compiler, dst, srcw);
			}
			else
				return emit_do_imm32(compiler, (reg_map[dst] >= 8) ? REX_B : 0, 0xb8 + reg_lmap[dst], srcw);
#endif
		}
#ifdef SLJIT_CONFIG_X86_64
		if (!compiler->mode32 && NOT_HALFWORD(srcw)) {
			if (emit_load_imm64(compiler, TMP_REG2, srcw))
				return compiler->error;
			code = emit_x86_instruction(compiler, 1, TMP_REG2, 0, dst, dstw);
			TEST_MEM_ERROR(code);
			*code = 0x89;
			return SLJIT_NO_ERROR;
		}
#endif
		code = emit_x86_instruction(compiler, 1, SLJIT_IMM, srcw, dst, dstw);
		TEST_MEM_ERROR(code);
		*code = 0xc7;
		return SLJIT_NO_ERROR;
	}
	if (dst >= SLJIT_TEMPORARY_REG1 && dst <= TMP_REGISTER) {
		code = emit_x86_instruction(compiler, 1, dst, 0, src, srcw);
		TEST_MEM_ERROR(code);
		*code = 0x8b;
		return SLJIT_NO_ERROR;
	}

	// Memory to memory move. Requires two instruction
	code = emit_x86_instruction(compiler, 1, TMP_REGISTER, 0, src, srcw);
	TEST_MEM_ERROR(code);
	*code = 0x8b;
	code = emit_x86_instruction(compiler, 1, TMP_REGISTER, 0, dst, dstw);
	TEST_MEM_ERROR(code);
	*code = 0x89;
	return SLJIT_NO_ERROR;
}

#define EMIT_MOV(compiler, dst, dstw, src, srcw) \
	do { \
		if (emit_mov(compiler, dst, dstw, src, srcw)) \
			return compiler->error; \
	} while (0)

static INLINE int sljit_emit_unary(struct sljit_compiler *compiler, int un_index,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
	sljit_ub* code;

	if (dst == SLJIT_NO_REG) {
		EMIT_MOV(compiler, TMP_REGISTER, 0, src, srcw);
		code = emit_x86_instruction(compiler, 1, 0, 0, TMP_REGISTER, 0);
		TEST_MEM_ERROR(code);
		*code++ = 0xf7;
		*code |= (un_index) << 3;
		return SLJIT_NO_ERROR;
	}
	if (dst == src && dstw == srcw) {
		/* Same input and output */
		code = emit_x86_instruction(compiler, 1, 0, 0, dst, dstw);
		TEST_MEM_ERROR(code);
		*code++ = 0xf7;
		*code |= (un_index) << 3;
		return SLJIT_NO_ERROR;
	}
	if (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_GENERAL_REG3) {
		EMIT_MOV(compiler, dst, 0, src, srcw);
		code = emit_x86_instruction(compiler, 1, 0, 0, dst, dstw);
		TEST_MEM_ERROR(code);
		*code++ = 0xf7;
		*code |= (un_index) << 3;
		return SLJIT_NO_ERROR;
	}
	EMIT_MOV(compiler, TMP_REGISTER, 0, src, srcw);
	code = emit_x86_instruction(compiler, 1, 0, 0, TMP_REGISTER, 0);
	TEST_MEM_ERROR(code);
	*code++ = 0xf7;
	*code |= (un_index) << 3;
	EMIT_MOV(compiler, dst, dstw, TMP_REGISTER, 0);
	return SLJIT_NO_ERROR;
}

int sljit_emit_op1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
	FUNCTION_ENTRY();

	SLJIT_ASSERT((op & ~SLJIT_32BIT_OPERATION) >= SLJIT_MOV && (op & ~SLJIT_32BIT_OPERATION) <= SLJIT_NEG);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_SRC(src, srcw);
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_op1_verbose();

#ifdef SLJIT_CONFIG_X86_64
	compiler->mode32 = op & SLJIT_32BIT_OPERATION;
#endif

	switch (op & ~SLJIT_32BIT_OPERATION) {
	case SLJIT_MOV:
		return emit_mov(compiler, dst, dstw, src, srcw);

	case SLJIT_NOT:
		return sljit_emit_unary(compiler, 0x2, dst, dstw, src, srcw);

	case SLJIT_NEG:
		return sljit_emit_unary(compiler, 0x3, dst, dstw, src, srcw);
	}

	return SLJIT_NO_ERROR;
}

static INLINE int sljit_emit_cum_binary(struct sljit_compiler *compiler,
	sljit_ub op_rm, sljit_ub op_mr, sljit_ub op_imm, sljit_ub op_eax_imm,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	sljit_ub* code;
	sljit_ub* buf;

	if (dst == SLJIT_NO_REG) {
		EMIT_MOV(compiler, TMP_REGISTER, 0, src1, src1w);
		if (src2 & SLJIT_IMM) {
			code = emit_x86_bin_instruction(compiler, 1, src2, src2w, TMP_REGISTER, 0);
			TEST_MEM_ERROR(code);
			*(code + 1) |= op_imm;
		}
		else {
			code = emit_x86_bin_instruction(compiler, 1, TMP_REGISTER, 0, src2, src2w);
			TEST_MEM_ERROR(code);
			*code = op_rm;
		}
		return SLJIT_NO_ERROR;
	}

	if (dst == src1 && dstw == src1w) {
		if (src2 & SLJIT_IMM) {
			if ((dst == SLJIT_TEMPORARY_REG1) && (src2w > 127 || src2w < -128)) {
				buf = ensure_buf(compiler, 1 + 5);
				TEST_MEM_ERROR(buf);
				INC_SIZE(5);
				*buf++ = op_eax_imm;
				*(sljit_w*)buf = src2w;
			}
			else {
				code = emit_x86_bin_instruction(compiler, 1, src2, src2w, dst, dstw);
				TEST_MEM_ERROR(code);
				*(code + 1) |= op_imm;
			}
		}
		else if (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_GENERAL_REG3) {
			code = emit_x86_bin_instruction(compiler, 1, dst, dstw, src2, src2w);
			TEST_MEM_ERROR(code);
			*code = op_rm;
		}
		else if (src2 >= SLJIT_TEMPORARY_REG1 && src2 <= SLJIT_GENERAL_REG3) {
			code = emit_x86_bin_instruction(compiler, 1, src2, src2w, dst, dstw);
			TEST_MEM_ERROR(code);
			*code = op_mr;
		}
		else {
			EMIT_MOV(compiler, TMP_REGISTER, 0, src2, src2w);
			code = emit_x86_bin_instruction(compiler, 1, TMP_REGISTER, 0, dst, dstw);
			TEST_MEM_ERROR(code);
			*code = op_mr;
		}
		return SLJIT_NO_ERROR;
	}

	// Only for cumulative operations
	if (dst == src2 && dstw == src2w) {
		if (src1 & SLJIT_IMM) {
			if ((dst == SLJIT_TEMPORARY_REG1) && (src1w > 127 || src1w < -128)) {
				buf = ensure_buf(compiler, 1 + 5);
				TEST_MEM_ERROR(buf);
				INC_SIZE(5);
				*buf++ = op_eax_imm;
				*(sljit_w*)buf = src1w;
			}
			else {
				code = emit_x86_bin_instruction(compiler, 1, src1, src1w, dst, dstw);
				TEST_MEM_ERROR(code);
				*(code + 1) |= op_imm;
			}
		}
		else if (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_GENERAL_REG3) {
			code = emit_x86_bin_instruction(compiler, 1, dst, dstw, src1, src1w);
			TEST_MEM_ERROR(code);
			*code = op_rm;
		}
		else if (src1 >= SLJIT_TEMPORARY_REG1 && src1 <= SLJIT_GENERAL_REG3) {
			code = emit_x86_bin_instruction(compiler, 1, src1, src1w, dst, dstw);
			TEST_MEM_ERROR(code);
			*code = op_mr;
		}
		else {
			EMIT_MOV(compiler, TMP_REGISTER, 0, src1, src1w);
			code = emit_x86_bin_instruction(compiler, 1, TMP_REGISTER, 0, dst, dstw);
			TEST_MEM_ERROR(code);
			*code = op_mr;
		}
		return SLJIT_NO_ERROR;
	}

	// General version
	if (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_GENERAL_REG3) {
		EMIT_MOV(compiler, dst, 0, src1, src1w);
		if (src2 & SLJIT_IMM) {
			code = emit_x86_bin_instruction(compiler, 1, src2, src2w, dst, 0);
			TEST_MEM_ERROR(code);
			*(code + 1) |= op_imm;
		}
		else {
			code = emit_x86_bin_instruction(compiler, 1, dst, 0, src2, src2w);
			TEST_MEM_ERROR(code);
			*code = op_rm;
		}
	}
	else {
		// This version requires less memory writing
		EMIT_MOV(compiler, TMP_REGISTER, 0, src1, src1w);
		if (src2 & SLJIT_IMM) {
			code = emit_x86_bin_instruction(compiler, 1, src2, src2w, TMP_REGISTER, 0);
			TEST_MEM_ERROR(code);
			*(code + 1) |= op_imm;
		}
		else {
			code = emit_x86_bin_instruction(compiler, 1, TMP_REGISTER, 0, src2, src2w);
			TEST_MEM_ERROR(code);
			*code = op_rm;
		}
		EMIT_MOV(compiler, dst, dstw, TMP_REGISTER, 0);
	}

	return SLJIT_NO_ERROR;
}

static INLINE int sljit_emit_non_cum_binary(struct sljit_compiler *compiler,
	sljit_ub op_rm, sljit_ub op_mr, sljit_ub op_imm, sljit_ub op_eax_imm,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	sljit_ub* code;
	sljit_ub* buf;

	if (dst == SLJIT_NO_REG) {
		EMIT_MOV(compiler, TMP_REGISTER, 0, src1, src1w);
		if (src2 & SLJIT_IMM) {
			code = emit_x86_bin_instruction(compiler, 1, src2, src2w, TMP_REGISTER, 0);
			TEST_MEM_ERROR(code);
			*(code + 1) |= op_imm;
		}
		else {
			code = emit_x86_bin_instruction(compiler, 1, TMP_REGISTER, 0, src2, src2w);
			TEST_MEM_ERROR(code);
			*code = op_rm;
		}
		return SLJIT_NO_ERROR;
	}

	if (dst == src1 && dstw == src1w) {
		if (src2 & SLJIT_IMM) {
			if ((dst == SLJIT_TEMPORARY_REG1) && (src2w > 127 || src2w < -128)) {
				buf = ensure_buf(compiler, 1 + 5);
				TEST_MEM_ERROR(buf);
				INC_SIZE(5);
				*buf++ = op_eax_imm;
				*(sljit_w*)buf = src2w;
			}
			else {
				code = emit_x86_bin_instruction(compiler, 1, src2, src2w, dst, dstw);
				TEST_MEM_ERROR(code);
				*(code + 1) |= op_imm;
			}
		}
		else if (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_GENERAL_REG3) {
			code = emit_x86_bin_instruction(compiler, 1, dst, dstw, src2, src2w);
			TEST_MEM_ERROR(code);
			*code = op_rm;
		}
		else if (src2 >= SLJIT_TEMPORARY_REG1 && src2 <= SLJIT_GENERAL_REG3) {
			code = emit_x86_bin_instruction(compiler, 1, src2, src2w, dst, dstw);
			TEST_MEM_ERROR(code);
			*code = op_mr;
		}
		else {
			EMIT_MOV(compiler, TMP_REGISTER, 0, src2, src2w);
			code = emit_x86_bin_instruction(compiler, 1, TMP_REGISTER, 0, dst, dstw);
			TEST_MEM_ERROR(code);
			*code = op_mr;
		}
		return SLJIT_NO_ERROR;
	}

	// General version
	if (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_GENERAL_REG3) {
		EMIT_MOV(compiler, dst, 0, src1, src1w);
		if (src2 & SLJIT_IMM) {
			code = emit_x86_bin_instruction(compiler, 1, src2, src2w, dst, 0);
			TEST_MEM_ERROR(code);
			*(code + 1) |= op_imm;
		}
		else {
			code = emit_x86_bin_instruction(compiler, 1, dst, 0, src2, src2w);
			TEST_MEM_ERROR(code);
			*code = op_rm;
		}
	}
	else {
		// This version requires less memory writing
		EMIT_MOV(compiler, TMP_REGISTER, 0, src1, src1w);
		if (src2 & SLJIT_IMM) {
			code = emit_x86_bin_instruction(compiler, 1, src2, src2w, TMP_REGISTER, 0);
			TEST_MEM_ERROR(code);
			*(code + 1) |= op_imm;
		}
		else {
			code = emit_x86_bin_instruction(compiler, 1, TMP_REGISTER, 0, src2, src2w);
			TEST_MEM_ERROR(code);
			*code = op_rm;
		}
		EMIT_MOV(compiler, dst, dstw, TMP_REGISTER, 0);
	}

	return SLJIT_NO_ERROR;
}

static INLINE int sljit_emit_mul(struct sljit_compiler *compiler,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	sljit_ub* code;
	sljit_ub* buf;
	int tmp;
	sljit_w tmpw;

	if (dst != SLJIT_PREF_MUL_DST) {
		// This is not a good case in x86, it may requires a lot of
		// operations
		buf = ensure_buf(compiler, 1 + 1);
		TEST_MEM_ERROR(buf);
		INC_SIZE(1);
		PUSH_REG(reg_map[SLJIT_PREF_MUL_DST]);
	}

	// Some swap may increase performance (mul is commutative operation)
	if (src1 != SLJIT_PREF_MUL_DST && src2 == SLJIT_PREF_MUL_DST) {
		src2 = src1;
		src2w = src1w;
		src1 = SLJIT_PREF_MUL_DST;
		src1w = 0;
	}
	else if (((src2 & SLJIT_IMM) && !(src1 & SLJIT_IMM)) || depends_on(src2, SLJIT_PREF_MUL_DST)) {
		tmp = src1;
		src1 = src2;
		src2 = tmp;
		tmpw = src1w;
		src1w = src2w;
		src2w = tmpw;
	}

	if (src1 != SLJIT_PREF_MUL_DST && depends_on(src2, SLJIT_PREF_MUL_DST)) {
		// Cross references
		buf = ensure_buf(compiler, 1 + 1);
		TEST_MEM_ERROR(buf);
		INC_SIZE(1);
		PUSH_REG(reg_map[SLJIT_TEMPORARY_REG2]);

		EMIT_MOV(compiler, TMP_REGISTER, 0, src2, src2w);
		EMIT_MOV(compiler, SLJIT_PREF_MUL_DST, 0, src1, src1w);

		code = emit_x86_instruction(compiler, 1, 0, 0, TMP_REGISTER, 0);
		TEST_MEM_ERROR(code);
		*code = 0xf7;
		*(code + 1) |= 0x4 << 3;

		buf = ensure_buf(compiler, 1 + 1);
		TEST_MEM_ERROR(buf);
		INC_SIZE(1);
		POP_REG(reg_map[SLJIT_TEMPORARY_REG2]);
	}
	else {
		// "mov reg, reg" is faster than "push reg"
		EMIT_MOV(compiler, TMP_REGISTER, 0, SLJIT_TEMPORARY_REG2, 0);

		if (src1 != SLJIT_PREF_MUL_DST)
			EMIT_MOV(compiler, SLJIT_PREF_MUL_DST, 0, src1, src1w);

		if (src2 & SLJIT_IMM) {
			EMIT_MOV(compiler, SLJIT_TEMPORARY_REG2, 0, src2, src2w);
			src2 = SLJIT_TEMPORARY_REG2;
			src2w = 0;
		}

		code = emit_x86_instruction(compiler, 1, 0, 0, src2, src2w);
		TEST_MEM_ERROR(code);
		*code = 0xf7;
		*(code + 1) |= 0x4 << 3;

		EMIT_MOV(compiler, SLJIT_TEMPORARY_REG2, 0, TMP_REGISTER, 0);
	}

	if (dst != SLJIT_PREF_MUL_DST) {
		if (depends_on(dst, SLJIT_PREF_MUL_DST)) {
			EMIT_MOV(compiler, TMP_REGISTER, 0, SLJIT_PREF_MUL_DST, 0);
			buf = ensure_buf(compiler, 1 + 1);
			TEST_MEM_ERROR(buf);
			INC_SIZE(1);
			POP_REG(reg_map[SLJIT_PREF_MUL_DST]);
			EMIT_MOV(compiler, dst, dstw, TMP_REGISTER, 0);
		}
		else {
			EMIT_MOV(compiler, dst, dstw, SLJIT_PREF_MUL_DST, 0);
			buf = ensure_buf(compiler, 1 + 1);
			TEST_MEM_ERROR(buf);
			INC_SIZE(1);
			POP_REG(reg_map[SLJIT_PREF_MUL_DST]);
		}
	}

	return SLJIT_NO_ERROR;
}

static INLINE int sljit_emit_cmp_binary(struct sljit_compiler *compiler,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	sljit_ub* code;
	sljit_ub* buf;

	if (src1 == SLJIT_TEMPORARY_REG1 && (src2 & SLJIT_IMM) && (src2w > 127 || src2w < -128)) {
		buf = ensure_buf(compiler, 1 + 5);
		TEST_MEM_ERROR(buf);
		INC_SIZE(5);
		*buf++ = 0x3d;
		*(sljit_w*)buf = src2w;
		return SLJIT_NO_ERROR;
	}

	if (src1 >= SLJIT_TEMPORARY_REG1 && src1 <= SLJIT_GENERAL_REG3) {
		if (src2 & SLJIT_IMM) {
			code = emit_x86_bin_instruction(compiler, 1, src2, src2w, src1, 0);
			TEST_MEM_ERROR(code);
			*(code + 1) |= 0x7 << 3;
		}
		else {
			code = emit_x86_bin_instruction(compiler, 1, src1, 0, src2, src2w);
			TEST_MEM_ERROR(code);
			*code = 0x3b;
		}
		return SLJIT_NO_ERROR;
	}

	if (src2 >= SLJIT_TEMPORARY_REG1 && src2 <= SLJIT_GENERAL_REG3 && !(src1 & SLJIT_IMM)) {
		code = emit_x86_bin_instruction(compiler, 1, src2, 0, src1, src1w);
		TEST_MEM_ERROR(code);
		*code = 0x39;
		return SLJIT_NO_ERROR;
	}

	EMIT_MOV(compiler, TMP_REGISTER, 0, src1, src1w);
	if (src2 & SLJIT_IMM) {
		code = emit_x86_bin_instruction(compiler, 1, src2, src2w, TMP_REGISTER, 0);
		TEST_MEM_ERROR(code);
		*(code + 1) |= 0x7 << 3;
	}
	else {
		code = emit_x86_bin_instruction(compiler, 1, TMP_REGISTER, 0, src2, src2w);
		TEST_MEM_ERROR(code);
		*code = 0x3b;
	}
	return SLJIT_NO_ERROR;
}

static INLINE int sljit_emit_test_binary(struct sljit_compiler *compiler,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	sljit_ub* code;
	sljit_ub* buf;

	if (src1 == SLJIT_TEMPORARY_REG1 && (src2 & SLJIT_IMM) && (src2w > 127 || src2w < -128)) {
		buf = ensure_buf(compiler, 1 + 5);
		TEST_MEM_ERROR(buf);
		INC_SIZE(5);
		*buf++ = 0xa9;
		*(sljit_w*)buf = src2w;
		return SLJIT_NO_ERROR;
	}

	if (src2 == SLJIT_TEMPORARY_REG1 && (src1 & SLJIT_IMM) && (src1w > 127 || src1w < -128)) {
		buf = ensure_buf(compiler, 1 + 5);
		TEST_MEM_ERROR(buf);
		INC_SIZE(5);
		*buf++ = 0xa9;
		*(sljit_w*)buf = src1w;
		return SLJIT_NO_ERROR;
	}

	if (src1 >= SLJIT_TEMPORARY_REG1 && src1 <= SLJIT_GENERAL_REG3) {
		if (src2 & SLJIT_IMM) {
			code = emit_x86_instruction(compiler, 1, src2, src2w, src1, 0);
			TEST_MEM_ERROR(code);
			*code = 0xf7;
		}
		else {
			code = emit_x86_instruction(compiler, 1, src1, 0, src2, src2w);
			TEST_MEM_ERROR(code);
			*code = 0x85;
		}
		return SLJIT_NO_ERROR;
	}

	if (src2 >= SLJIT_TEMPORARY_REG1 && src2 <= SLJIT_GENERAL_REG3) {
		if (src1 & SLJIT_IMM) {
			code = emit_x86_instruction(compiler, 1, src1, src1w, src2, 0);
			TEST_MEM_ERROR(code);
			*code = 0xf7;
		}
		else {
			code = emit_x86_instruction(compiler, 1, src2, 0, src1, src1w);
			TEST_MEM_ERROR(code);
			*code = 0x85;
		}
		return SLJIT_NO_ERROR;
	}

	EMIT_MOV(compiler, TMP_REGISTER, 0, src1, src1w);
	if (src2 & SLJIT_IMM) {
		code = emit_x86_instruction(compiler, 1, src2, src2w, TMP_REGISTER, 0);
		TEST_MEM_ERROR(code);
		*code = 0xf7;
	}
	else {
		code = emit_x86_instruction(compiler, 1, TMP_REGISTER, 0, src2, src2w);
		TEST_MEM_ERROR(code);
		*code = 0x85;
	}
	return SLJIT_NO_ERROR;
}

static INLINE int sljit_emit_shift(struct sljit_compiler *compiler,
	sljit_ub mode,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	sljit_ub* code;
	sljit_ub* buf;

	if ((src2 & SLJIT_IMM) || (src2 == SLJIT_PREF_SHIFT_REG)) {
		if (dst == src1 && dstw == src1w) {
			code = emit_x86_shift_instruction(compiler, src2, src2w, dst, dstw);
			TEST_MEM_ERROR(code);
			*code |= mode;
			return SLJIT_NO_ERROR;
		}
		if (dst == SLJIT_NO_REG) {
			EMIT_MOV(compiler, TMP_REGISTER, 0, src1, src1w);
			code = emit_x86_shift_instruction(compiler, src2, src2w, TMP_REGISTER, 0);
			TEST_MEM_ERROR(code);
			*code |= mode;
			return SLJIT_NO_ERROR;
		}
		if (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_GENERAL_REG3) {
			EMIT_MOV(compiler, dst, 0, src1, src1w);
			code = emit_x86_shift_instruction(compiler, src2, src2w, dst, 0);
			TEST_MEM_ERROR(code);
			*code |= mode;
			return SLJIT_NO_ERROR;
		}

		EMIT_MOV(compiler, TMP_REGISTER, 0, src1, src1w);
		code = emit_x86_shift_instruction(compiler, src2, src2w, TMP_REGISTER, 0);
		TEST_MEM_ERROR(code);
		*code |= mode;
		EMIT_MOV(compiler, dst, dstw, TMP_REGISTER, 0);
		return SLJIT_NO_ERROR;
	}

	if (dst != SLJIT_PREF_SHIFT_REG) {
		// This case is really bad, since ecx can be used for
		// addressing as well, and we must ensure to work even
		// in that case 
		buf = ensure_buf(compiler, 1 + 1);
		TEST_MEM_ERROR(buf);
		INC_SIZE(1);
		PUSH_REG(reg_map[SLJIT_PREF_SHIFT_REG]);

		EMIT_MOV(compiler, TMP_REGISTER, 0, src1, src1w);
		EMIT_MOV(compiler, SLJIT_PREF_SHIFT_REG, 0, src2, src2w);
		code = emit_x86_shift_instruction(compiler, SLJIT_PREF_SHIFT_REG, 0, TMP_REGISTER, 0);
		TEST_MEM_ERROR(code);
		*code |= mode;

		buf = ensure_buf(compiler, 1 + 1);
		TEST_MEM_ERROR(buf);
		INC_SIZE(1);
		POP_REG(reg_map[SLJIT_PREF_SHIFT_REG]);

		EMIT_MOV(compiler, dst, dstw, TMP_REGISTER, 0);
	}
	else {
		EMIT_MOV(compiler, TMP_REGISTER, 0, src1, src1w);
		EMIT_MOV(compiler, SLJIT_PREF_SHIFT_REG, 0, src2, src2w);
		code = emit_x86_shift_instruction(compiler, SLJIT_PREF_SHIFT_REG, 0, TMP_REGISTER, 0);
		TEST_MEM_ERROR(code);
		*code |= mode;
		EMIT_MOV(compiler, SLJIT_PREF_SHIFT_REG, 0, TMP_REGISTER, 0);
	}

	return SLJIT_NO_ERROR;
}

int sljit_emit_op2(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	FUNCTION_ENTRY();

	SLJIT_ASSERT((op & ~SLJIT_32BIT_OPERATION) >= SLJIT_ADD && (op & ~SLJIT_32BIT_OPERATION) <= SLJIT_ASHR);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_SRC(src1, src1w);
	FUNCTION_CHECK_SRC(src2, src2w);
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_op2_verbose();

#ifdef SLJIT_CONFIG_X86_64
	compiler->mode32 = op & SLJIT_32BIT_OPERATION;
#endif

	switch (op & ~SLJIT_32BIT_OPERATION) {
	case SLJIT_ADD:
		return sljit_emit_cum_binary(compiler, 0x03, 0x01, 0x0 << 3, 0x05,
			dst, dstw, src1, src1w, src2, src2w);
	case SLJIT_ADDC:
		return sljit_emit_cum_binary(compiler, 0x13, 0x11, 0x2 << 3, 0x15,
			dst, dstw, src1, src1w, src2, src2w);
	case SLJIT_SUB:
		if (dst == SLJIT_NO_REG)
			return sljit_emit_cmp_binary(compiler, src1, src1w, src2, src2w);
		return sljit_emit_non_cum_binary(compiler, 0x2b, 0x29, 0x5 << 3, 0x2d,
			dst, dstw, src1, src1w, src2, src2w);
	case SLJIT_SUBC:
		return sljit_emit_non_cum_binary(compiler, 0x1b, 0x19, 0x3 << 3, 0x1d,
			dst, dstw, src1, src1w, src2, src2w);
	case SLJIT_MUL:
		return sljit_emit_mul(compiler, dst, dstw, src1, src1w, src2, src2w);
	case SLJIT_AND:
		if (dst == SLJIT_NO_REG)
			return sljit_emit_test_binary(compiler, src1, src1w, src2, src2w);
		return sljit_emit_cum_binary(compiler, 0x23, 0x21, 0x4 << 3, 0x25,
			dst, dstw, src1, src1w, src2, src2w);
	case SLJIT_OR:
		return sljit_emit_cum_binary(compiler, 0x0b, 0x09, 0x1 << 3, 0x0d,
			dst, dstw, src1, src1w, src2, src2w);
	case SLJIT_XOR:
		return sljit_emit_cum_binary(compiler, 0x33, 0x31, 0x6 << 3, 0x35,
			dst, dstw, src1, src1w, src2, src2w);
	case SLJIT_SHL:
		return sljit_emit_shift(compiler, 0x4 << 3,
			dst, dstw, src1, src1w, src2, src2w);
	case SLJIT_LSHR:
		return sljit_emit_shift(compiler, 0x5 << 3,
			dst, dstw, src1, src1w, src2, src2w);
	case SLJIT_ASHR:
		return sljit_emit_shift(compiler, 0x7 << 3,
			dst, dstw, src1, src1w, src2, src2w);
	}

	return SLJIT_NO_ERROR;
}

// ---------------------------------------------------------------------
//  Conditional instructions
// ---------------------------------------------------------------------

struct sljit_label* sljit_emit_label(struct sljit_compiler *compiler)
{
	sljit_ub *buf;
	struct sljit_label *label;

	FUNCTION_ENTRY();

	sljit_emit_label_verbose();

	if (compiler->last_label && compiler->last_label->size == compiler->size)
		return compiler->last_label;

	label = ensure_abuf(compiler, sizeof(struct sljit_label));
	TEST_MEM_ERROR2(label);

	label->next = NULL;
	label->size = compiler->size;
	if (compiler->last_label)
		compiler->last_label->next = label;
	else
		compiler->labels = label;
	compiler->last_label = label;

	buf = ensure_buf(compiler, 2);
	TEST_MEM_ERROR2(buf);

	*buf++ = 0;
	*buf++ = 0;

	return label;
}

struct sljit_jump* sljit_emit_jump(struct sljit_compiler *compiler, int type)
{
	sljit_ub *buf;
	struct sljit_jump *jump;

	FUNCTION_ENTRY();
	SLJIT_ASSERT((type & ~0x1ff) == 0);
	SLJIT_ASSERT((type & 0xff) >= SLJIT_C_EQUAL && (type & 0xff) <= SLJIT_JUMP);

	sljit_emit_jump_verbose();

	jump = ensure_abuf(compiler, sizeof(struct sljit_jump));
	TEST_MEM_ERROR2(jump);

	jump->next = NULL;
	jump->flags = type & SLJIT_LONG_JUMP;
	type &= 0xff;
	if (compiler->last_jump)
		compiler->last_jump->next = jump;
	else
		compiler->jumps = jump;
	compiler->last_jump = jump;

	// Worst case size
	compiler->size += (type == SLJIT_JUMP) ? 5 : 6;

	buf = ensure_buf(compiler, 2);
	TEST_MEM_ERROR2(buf);

	*buf++ = 0;
	*buf++ = type + 3;

	return jump;
}

int sljit_emit_call(struct sljit_compiler *compiler, int src, sljit_w srcw, int args)
{
	sljit_ub *buf;
	sljit_ub *code;

	FUNCTION_ENTRY();
	SLJIT_ASSERT(args >= -1 && args <= 3);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_SRC(src, srcw);
#endif
	sljit_emit_call_verbose();

	if (args > 0) {
		buf = ensure_buf(compiler, args + 1);
		TEST_MEM_ERROR(buf);
		INC_SIZE(args);
		if (args >= 3)
			PUSH_REG(reg_map[SLJIT_TEMPORARY_REG3]);
		if (args >= 2)
			PUSH_REG(reg_map[SLJIT_TEMPORARY_REG2]);
		PUSH_REG(reg_map[SLJIT_TEMPORARY_REG1]);
	}

	if (src == SLJIT_IMM) {
		buf = ensure_buf(compiler, 1 + 5 + 2);
		TEST_MEM_ERROR(buf);

		INC_SIZE(5);
		*buf++ = (args >= 0) ? 0xe8 : 0xe9;
		*(sljit_w*)buf = srcw;
		buf += 4;

		*buf++ = 0;
		*buf++ = 2;
	}
	else {
		code = emit_x86_instruction(compiler, 1, 0, 0, src, srcw);
		TEST_MEM_ERROR(buf);
		*code++ = 0xff;
		*code |= (args >= 0) ? (2 << 3) : (4 << 3);
	}

	return SLJIT_NO_ERROR;
}

int sljit_emit_cond_set(struct sljit_compiler *compiler, int dst, sljit_w dstw, int type)
{
	sljit_ub *buf;
	sljit_ub cond_set = 0;

	FUNCTION_ENTRY();
	SLJIT_ASSERT(type >= SLJIT_C_EQUAL && type <= SLJIT_C_NOT_OVERFLOW);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_set_cond_verbose();

	if (dst == SLJIT_NO_REG)
		return SLJIT_NO_ERROR;

	switch (type) {
	case SLJIT_C_EQUAL:
		cond_set = 0x94;
		break;

	case SLJIT_C_NOT_EQUAL:
		cond_set = 0x95;
		break;

	case SLJIT_C_LESS:
		cond_set = 0x92;
		break;

	case SLJIT_C_NOT_LESS:
		cond_set = 0x93;
		break;

	case SLJIT_C_GREATER:
		cond_set = 0x97;
		break;

	case SLJIT_C_NOT_GREATER:
		cond_set = 0x96;
		break;

	case SLJIT_C_SIG_LESS:
		cond_set = 0x9c;
		break;

	case SLJIT_C_SIG_NOT_LESS:
		cond_set = 0x9d;
		break;

	case SLJIT_C_SIG_GREATER:
		cond_set = 0x9f;
		break;

	case SLJIT_C_SIG_NOT_GREATER:
		cond_set = 0x9e;
		break;

	case SLJIT_C_CARRY:
		cond_set = 0x92;
		break;

	case SLJIT_C_NOT_CARRY:
		cond_set = 0x93;
		break;

	case SLJIT_C_ZERO:
		cond_set = 0x94;
		break;

	case SLJIT_C_NOT_ZERO:
		cond_set = 0x95;
		break;

	case SLJIT_C_OVERFLOW:
		cond_set = 0x90;
		break;

	case SLJIT_C_NOT_OVERFLOW:
		cond_set = 0x91;
		break;
	}

	if (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_TEMPORARY_REG3) {
		buf = ensure_buf(compiler, 1 + 3 + 3);
		TEST_MEM_ERROR(buf);
		INC_SIZE(3 + 3);
		// Set al to conditional flag
		*buf++ = 0x0f;
		*buf++ = cond_set;
		*buf++ = 0xC0 | reg_map[dst];
		*buf++ = 0x0f;
		*buf++ = 0xb6;
		*buf = 0xC0 | (reg_map[dst] << 3) | reg_map[dst];
	}
	else {
		buf = ensure_buf(compiler, 1 + 1 + 3 + 3);
		TEST_MEM_ERROR(buf);
		INC_SIZE(1 + 3 + 3);
		PUSH_REG(reg_map[SLJIT_TEMPORARY_REG1]);
		// Set al to conditional flag
		*buf++ = 0x0f;
		*buf++ = cond_set;
		*buf++ = 0xC0;

		if (dst >= SLJIT_GENERAL_REG1 && dst <= SLJIT_GENERAL_REG3) {
			*buf++ = 0x0f;
			*buf++ = 0xb6;
			*buf = 0xC0 | (reg_map[dst] << 3);
		} else {
			*buf++ = 0x0f;
			*buf++ = 0xb6;
			*buf = 0xC0;
			EMIT_MOV(compiler, dst, dstw, SLJIT_TEMPORARY_REG1, 0);
		}

		buf = ensure_buf(compiler, 1 + 1);
		TEST_MEM_ERROR(buf);
		INC_SIZE(1);
		POP_REG(reg_map[SLJIT_TEMPORARY_REG1]);
	}

	return SLJIT_NO_ERROR;
}

struct sljit_const* sljit_emit_const(struct sljit_compiler *compiler, int dst, sljit_w dstw, sljit_w constant)
{
	sljit_ub *buf;
	struct sljit_const *const_;

	FUNCTION_ENTRY();
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_const_verbose();

	if (dst == SLJIT_NO_REG)
		dst = TMP_REGISTER;

	const_ = ensure_abuf(compiler, sizeof(struct sljit_const));
	TEST_MEM_ERROR2(const_);

	const_->next = NULL;
	if (compiler->last_const)
		compiler->last_const->next = const_;
	else
		compiler->consts = const_;
	compiler->last_const = const_;

	if (emit_mov(compiler, dst, dstw, SLJIT_IMM, constant))
		return NULL;

	buf = ensure_buf(compiler, 2);
	TEST_MEM_ERROR2(buf);

	*buf++ = 0;
	*buf++ = 1;

	return const_;
}

void sljit_set_jump_addr(sljit_uw addr, sljit_uw new_addr)
{
	*(sljit_w*)addr = new_addr - (addr + 4);
}

void sljit_set_const(sljit_uw addr, sljit_w constant)
{
	*(sljit_w*)addr = constant;
}
