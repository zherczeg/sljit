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
#define TMP_REGISTER	(SLJIT_LOCALS_REG + 1)

static sljit_ub reg_map[SLJIT_NO_REGISTERS + 2] = {
  0, 0, 2, 1, 3, 6, 7, 4, 5
};

#else // SLJIT_CONFIG_X86_32

// Last register + 1
#define TMP_REGISTER	(SLJIT_LOCALS_REG + 1)
#define TMP_REG2	(SLJIT_LOCALS_REG + 2)
#define TMP_REG3	(SLJIT_LOCALS_REG + 3)

static sljit_ub reg_map[SLJIT_NO_REGISTERS + 4] = {
  0, 0, 6, 1, 3, 14, 15, 4, 7, 8, 9
};

// re low-map. reg_map & 0x7
static sljit_ub reg_lmap[SLJIT_NO_REGISTERS + 4] = {
  0, 0, 6, 1, 3, 6, 7, 4, 7, 0, 1
};

#define REX_W		0x48
#define REX_R		0x44
#define REX_X		0x42
#define REX_B		0x41
#define REX		0x40

typedef unsigned int sljit_uhw;
typedef int sljit_hw;

#define IS_HALFWORD(x)		((x) <= 0x7fffffffl && (x) >= -0x80000000l)
#define NOT_HALFWORD(x)		((x) > 0x7fffffffl || (x) < -0x80000000l)

#endif // SLJIT_CONFIG_X86_32

// Size flags for emit_x86_instruction:
#define EX86_BIN_INS		0x010
#define EX86_SHIFT_INS		0x020
#define EX86_REX		0x040
#define EX86_NO_REXW		0x080
#define EX86_BYTE_ARG		0x100
#define EX86_HALF_ARG		0x200
#define EX86_PREF_66		0x400

#define INC_SIZE(s)			(*buf++ = (s), compiler->size += (s))
#define INC_CSIZE(s)			(*code++ = (s), compiler->size += (s))

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

static sljit_ub* generate_near_jump_code(struct sljit_jump *jump, sljit_ub *code_ptr, sljit_ub *code, int type)
{
	int short_jump;
	sljit_uw label_addr;

	// SLJIT probably never generates code > 4G
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
	else if (type >= SLJIT_CALL0) {
		short_jump = 0;
		*code_ptr++ = 0xe8;
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
		code_ptr += sizeof(sljit_b);
	} else {
		jump->flags |= PATCH_MW;
#ifdef SLJIT_CONFIG_X86_32
		code_ptr += sizeof(sljit_w);
#else
		code_ptr += sizeof(sljit_hw);
#endif
	}

	return code_ptr;
}

#ifdef SLJIT_CONFIG_X86_32
static sljit_ub* generate_far_jump_code(struct sljit_jump *jump, sljit_ub *code_ptr, int type);
#else
static sljit_ub* generate_far_jump_code(struct sljit_compiler *compiler, struct sljit_jump *jump, sljit_ub *code_ptr, int type);
static sljit_ub* generate_fixed_jump(struct sljit_compiler *compiler, sljit_ub *code_ptr, sljit_w addr, int type);
#endif

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
#ifdef SLJIT_CONFIG_X86_32
	code = (sljit_ub*)SLJIT_MALLOC_EXEC(compiler->size);
#else
	if (compiler->addrs > 0)
		code = (sljit_ub*)SLJIT_MALLOC_EXEC(compiler->size + compiler->addrs * sizeof(sljit_w) + (sizeof(sljit_w) - 1 /* alignment */));
	else
		code = (sljit_ub*)SLJIT_MALLOC_EXEC(compiler->size);
#endif
	if (!code) {
		compiler->error = SLJIT_MEMORY_ERROR;
		return NULL;
	}
	buf = compiler->buf;
#ifdef SLJIT_CONFIG_X86_64
	if (compiler->addrs > 0)
		compiler->addr_ptr = (sljit_uw*)((sljit_uw)(code + compiler->size + sizeof(sljit_w) - 1) & ~(sizeof(sljit_w) - 1));
#endif

	code_ptr = code;
	label = compiler->labels;
	jump = compiler->jumps;
	const_ = compiler->consts;
	do {
		buf_ptr = buf->memory;
		buf_end = buf_ptr + buf->used_size;
		do {
			len = *buf_ptr++;
			if (len > 0) {
				// The code is already generated
				SLJIT_MEMMOVE(code_ptr, buf_ptr, len);
				code_ptr += len;
				buf_ptr += len;
			}
			else {
				if (*buf_ptr >= 4) {
					jump->addr = (sljit_uw)code_ptr;
					if (!(jump->flags & SLJIT_LONG_JUMP))
						code_ptr = generate_near_jump_code(jump, code_ptr, code, *buf_ptr - 4);
					else {
#ifdef SLJIT_CONFIG_X86_32
						code_ptr = generate_far_jump_code(jump, code_ptr, *buf_ptr - 4);
#else
						code_ptr = generate_far_jump_code(compiler, jump, code_ptr, *buf_ptr - 4);
#endif
					}
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
				else {
#ifdef SLJIT_CONFIG_X86_32
					*code_ptr++ = (*buf_ptr == 2) ? 0xe8 /* call */ : 0xe9 /* jmp */;
					buf_ptr++;
					*(sljit_w*)code_ptr = *(sljit_w*)buf_ptr - ((sljit_w)code_ptr + sizeof(sljit_w));
					code_ptr += sizeof(sljit_w);
					buf_ptr += sizeof(sljit_w) - 1;
#else
					code_ptr = generate_fixed_jump(compiler, code_ptr, *(sljit_w*)(buf_ptr + 1), *buf_ptr);
					buf_ptr += sizeof(sljit_w);
#endif
				}
				buf_ptr++;
			}
		} while (buf_ptr < buf_end);
		SLJIT_ASSERT(buf_ptr == buf_end);
		buf = buf->next;
	} while (buf);

	jump = compiler->jumps;
	while (jump) {
		if (jump->flags & PATCH_MB) {
			SLJIT_ASSERT((sljit_w)(jump->label->addr - (jump->addr + sizeof(sljit_b))) >= -128 && (sljit_w)(jump->label->addr - (jump->addr + sizeof(sljit_b))) <= 127);
			*(sljit_ub*)jump->addr = jump->label->addr - (jump->addr + sizeof(sljit_b));
		} else if (jump->flags & PATCH_MW) {
#ifdef SLJIT_CONFIG_X86_32
			*(sljit_w*)jump->addr = jump->label->addr - (jump->addr + sizeof(sljit_w));
#else
			SLJIT_ASSERT((sljit_w)(jump->label->addr - (jump->addr + sizeof(sljit_hw))) >= -0x80000000l && (sljit_w)(jump->label->addr - (jump->addr + sizeof(sljit_hw))) <= 0x7fffffffl);
			*(sljit_hw*)jump->addr = jump->label->addr - (jump->addr + sizeof(sljit_hw));
#endif
		}
#ifdef SLJIT_CONFIG_X86_64
		else if (jump->flags & PATCH_MD)
			*(sljit_w*)jump->addr = jump->label->addr;
#endif

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

static int emit_cum_binary(struct sljit_compiler *compiler,
	sljit_ub op_rm, sljit_ub op_mr, sljit_ub op_imm, sljit_ub op_eax_imm,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w);

static int emit_non_cum_binary(struct sljit_compiler *compiler,
	sljit_ub op_rm, sljit_ub op_mr, sljit_ub op_imm, sljit_ub op_eax_imm,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w);

#ifdef SLJIT_CONFIG_X86_32
#include "sljitNativeX86_32.c"
#elif defined(SLJIT_CONFIG_X86_64)
#include "sljitNativeX86_64.c"
#endif

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
			TEST_FAIL(emit_load_imm64(compiler, TMP_REG2, srcw));
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
		TEST_FAIL(emit_mov(compiler, dst, dstw, src, srcw)); \
	} while (0)

#define ENCODE_PREFIX(prefix) \
	do { \
		code = (sljit_ub*)ensure_buf(compiler, 1 + 1); \
		TEST_MEM_ERROR(code); \
		INC_CSIZE(1); \
		*code = (prefix); \
	} while (0)

static int emit_mov_byte(struct sljit_compiler *compiler, int sign,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
	sljit_ub* code;
	int dst_r;
#ifdef SLJIT_CONFIG_X86_32
	int work_r;
#endif

#ifdef SLJIT_CONFIG_X86_64
	compiler->mode32 = 0;
#endif

	if (dst == SLJIT_NO_REG && !(src & SLJIT_MEM_FLAG))
		return SLJIT_NO_ERROR; // Empty instruction

	if (src & SLJIT_IMM) {
		code = emit_x86_instruction(compiler, 1 | EX86_BYTE_ARG | EX86_NO_REXW, SLJIT_IMM, srcw & 0xff, dst, dstw);
		TEST_MEM_ERROR(code);
		*code = 0xc6;
		return SLJIT_NO_ERROR;
	}

	dst_r = (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_GENERAL_REG3) ? dst : TMP_REGISTER;

	if ((dst & SLJIT_MEM_FLAG) && src >= SLJIT_TEMPORARY_REG1 && src <= SLJIT_GENERAL_REG3) {
#ifdef SLJIT_CONFIG_X86_32
		if (reg_map[src] >= 4) {
			SLJIT_ASSERT(dst_r == TMP_REGISTER);
			EMIT_MOV(compiler, TMP_REGISTER, 0, src, 0);
		} else
			dst_r = src;
#else
		dst_r = src;
#endif
	}
#ifdef SLJIT_CONFIG_X86_32
	else if (src >= SLJIT_TEMPORARY_REG1 && src <= SLJIT_GENERAL_REG3 && reg_map[src] >= 4) {
		// src, dst are registers
		SLJIT_ASSERT(dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_GENERAL_REG3);
		if (reg_map[dst] < 4) {
			EMIT_MOV(compiler, dst, 0, src, 0);
			code = emit_x86_instruction(compiler, 2 | EX86_REX, dst, 0, dst, 0);
			TEST_MEM_ERROR(code);
			*code++ = 0x0f;
			*code = sign ? 0xbe : 0xb6;
		}
		else {
			ENCODE_PREFIX(0x90 + reg_map[TMP_REGISTER]);
			EMIT_MOV(compiler, SLJIT_TEMPORARY_REG1, 0, src, 0);
			code = emit_x86_instruction(compiler, 2 | EX86_REX, dst, 0, SLJIT_TEMPORARY_REG1, 0);
			TEST_MEM_ERROR(code);
			*code++ = 0x0f;
			*code = sign ? 0xbe : 0xb6;
			ENCODE_PREFIX(0x90 + reg_map[TMP_REGISTER]);
		}
	}
#endif
	else {
		code = emit_x86_instruction(compiler, 2, dst_r, 0, src, srcw);
		TEST_MEM_ERROR(code);
		*code++ = 0x0f;
		*code = sign ? 0xbe : 0xb6;
	}

	if (dst & SLJIT_MEM_FLAG) {
#ifdef SLJIT_CONFIG_X86_32
		if (dst_r == TMP_REGISTER) {
			// Find a non-used register, whose reg_map[src] < 4
			if ((dst & 0xf) == SLJIT_TEMPORARY_REG1) {
				if ((dst & 0xf0) == (SLJIT_TEMPORARY_REG2 << 4))
					work_r = SLJIT_TEMPORARY_REG3;
				else
					work_r = SLJIT_TEMPORARY_REG2;
			}
			else {
				if ((dst & 0xf0) != (SLJIT_TEMPORARY_REG1 << 4))
					work_r = SLJIT_TEMPORARY_REG1;
				else if ((dst & 0xf) == SLJIT_TEMPORARY_REG2)
					work_r = SLJIT_TEMPORARY_REG3;
				else
					work_r = SLJIT_TEMPORARY_REG2;
			}

			if (work_r == SLJIT_TEMPORARY_REG1) {
				ENCODE_PREFIX(0x90 + reg_map[TMP_REGISTER]);
			}
			else {
				code = emit_x86_instruction(compiler, 1, work_r, 0, dst_r, 0);
				TEST_MEM_ERROR(code);
				*code = 0x87;
			}

			code = emit_x86_instruction(compiler, 1, work_r, 0, dst, dstw);
			TEST_MEM_ERROR(code);
			*code = 0x88;

			if (work_r == SLJIT_TEMPORARY_REG1) {
				ENCODE_PREFIX(0x90 + reg_map[TMP_REGISTER]);
			}
			else {
				code = emit_x86_instruction(compiler, 1, work_r, 0, dst_r, 0);
				TEST_MEM_ERROR(code);
				*code = 0x87;
			}
		}
		else {
			code = emit_x86_instruction(compiler, 1, dst_r, 0, dst, dstw);
			TEST_MEM_ERROR(code);
			*code = 0x88;
		}
#else
		code = emit_x86_instruction(compiler, 1 | EX86_NO_REXW, dst_r, 0, dst, dstw);
		TEST_MEM_ERROR(code);
		*code = 0x88;
#endif
	}

	return SLJIT_NO_ERROR;
}

static int emit_mov_half(struct sljit_compiler *compiler, int sign,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
	sljit_ub* code;
	int dst_r;

#ifdef SLJIT_CONFIG_X86_64
	compiler->mode32 = 0;
#endif

	if (dst == SLJIT_NO_REG && !(src & SLJIT_MEM_FLAG))
		return SLJIT_NO_ERROR; // Empty instruction

	if (src & SLJIT_IMM) {
		code = emit_x86_instruction(compiler, 1 | EX86_HALF_ARG | EX86_NO_REXW | EX86_PREF_66, SLJIT_IMM, srcw & 0xffff, dst, dstw);
		TEST_MEM_ERROR(code);
		*code = 0xc7;
		return SLJIT_NO_ERROR;
	}

	dst_r = (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_GENERAL_REG3) ? dst : TMP_REGISTER;

	if ((dst & SLJIT_MEM_FLAG) && (src >= SLJIT_TEMPORARY_REG1 && src <= SLJIT_GENERAL_REG3))
		dst_r = src;
	else {
		code = emit_x86_instruction(compiler, 2, dst_r, 0, src, srcw);
		TEST_MEM_ERROR(code);
		*code++ = 0x0f;
		*code = sign ? 0xbf : 0xb7;
	}

	if (dst & SLJIT_MEM_FLAG) {
		code = emit_x86_instruction(compiler, 1 | EX86_NO_REXW | EX86_PREF_66, dst_r, 0, dst, dstw);
		TEST_MEM_ERROR(code);
		*code = 0x89;
	}

	return SLJIT_NO_ERROR;
}

static int emit_unary(struct sljit_compiler *compiler, int un_index,
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

static int emit_not_with_flags(struct sljit_compiler *compiler,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
	sljit_ub* code;

	if (dst == SLJIT_NO_REG) {
		EMIT_MOV(compiler, TMP_REGISTER, 0, src, srcw);
		code = emit_x86_instruction(compiler, 1, 0, 0, TMP_REGISTER, 0);
		TEST_MEM_ERROR(code);
		*code++ = 0xf7;
		*code |= 0x2 << 3;
		code = emit_x86_instruction(compiler, 1, TMP_REGISTER, 0, TMP_REGISTER, 0);
		TEST_MEM_ERROR(code);
		*code = 0x0b;
		return SLJIT_NO_ERROR;
	}
	if (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_GENERAL_REG3) {
		EMIT_MOV(compiler, dst, 0, src, srcw);
		code = emit_x86_instruction(compiler, 1, 0, 0, dst, dstw);
		TEST_MEM_ERROR(code);
		*code++ = 0xf7;
		*code |= 0x2 << 3;
		code = emit_x86_instruction(compiler, 1, dst, 0, dst, 0);
		TEST_MEM_ERROR(code);
		*code = 0x0b;
		return SLJIT_NO_ERROR;
	}
	EMIT_MOV(compiler, TMP_REGISTER, 0, src, srcw);
	code = emit_x86_instruction(compiler, 1, 0, 0, TMP_REGISTER, 0);
	TEST_MEM_ERROR(code);
	*code++ = 0xf7;
	*code |= 0x2 << 3;
	code = emit_x86_instruction(compiler, 1, TMP_REGISTER, 0, TMP_REGISTER, 0);
	TEST_MEM_ERROR(code);
	*code = 0x0b;
	EMIT_MOV(compiler, dst, dstw, TMP_REGISTER, 0);
	return SLJIT_NO_ERROR;
}

int sljit_emit_op1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
	sljit_ub* code;

	FUNCTION_ENTRY();

	SLJIT_ASSERT(GET_OPCODE(op) >= SLJIT_MOV && GET_OPCODE(op) <= SLJIT_NEG);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_SRC(src, srcw);
	FUNCTION_CHECK_DST(dst, dstw);
	FUNCTION_CHECK_OP1();
#endif
	sljit_emit_op1_verbose();

#ifdef SLJIT_CONFIG_X86_64
	compiler->mode32 = op & SLJIT_INT_OPERATION;
#endif

	switch (GET_OPCODE(op)) {
	case SLJIT_MOV:
#ifdef SLJIT_CONFIG_X86_32
	case SLJIT_MOV_UI:
	case SLJIT_MOV_SI:
#endif
		return emit_mov(compiler, dst, dstw, src, srcw);

	case SLJIT_MOV_UB:
		return emit_mov_byte(compiler, 0, dst, dstw, src, srcw);

	case SLJIT_MOV_SB:
		return emit_mov_byte(compiler, 1, dst, dstw, src, srcw);

	case SLJIT_MOV_UH:
		return emit_mov_half(compiler, 0, dst, dstw, src, srcw);

	case SLJIT_MOV_SH:
		return emit_mov_half(compiler, 1, dst, dstw, src, srcw);

#ifdef SLJIT_CONFIG_X86_64
	case SLJIT_MOV_UI:
		return emit_mov_int(compiler, 0, dst, dstw, src, srcw);

	case SLJIT_MOV_SI:
		return emit_mov_int(compiler, 0, dst, dstw, src, srcw);
#endif

	case SLJIT_MOVU:
	case SLJIT_MOVU_UB:
	case SLJIT_MOVU_SB:
	case SLJIT_MOVU_UH:
	case SLJIT_MOVU_SH:
	case SLJIT_MOVU_UI:
	case SLJIT_MOVU_SI:
		if ((src & SLJIT_MEM_FLAG) && (src & 0xf) && (srcw != 0 || (src & 0xf0) != 0)) {
#ifdef SLJIT_CONFIG_X86_64
			compiler->mode32 = 0;
#endif
			code = emit_x86_instruction(compiler, 1, src & 0xf, 0, src, srcw);
			TEST_MEM_ERROR(code);
			*code = 0x8d;
#ifdef SLJIT_CONFIG_X86_64
			compiler->mode32 = op & SLJIT_INT_OPERATION;
#endif
			src &= SLJIT_MEM_FLAG | 0xf;

			srcw = 0;
		}
		switch (GET_OPCODE(op)) {
		case SLJIT_MOVU:
#ifdef SLJIT_CONFIG_X86_32
		case SLJIT_MOVU_UI:
		case SLJIT_MOVU_SI:
#endif
			TEST_FAIL(emit_mov(compiler, dst, dstw, src, srcw));
			break;

		case SLJIT_MOVU_UB:
			TEST_FAIL(emit_mov_byte(compiler, 0, dst, dstw, src, srcw));
			break;

		case SLJIT_MOVU_SB:
			TEST_FAIL(emit_mov_byte(compiler, 1, dst, dstw, src, srcw));
			break;

		case SLJIT_MOVU_UH:
			TEST_FAIL(emit_mov_half(compiler, 0, dst, dstw, src, srcw));
			break;

		case SLJIT_MOVU_SH:
			TEST_FAIL(emit_mov_half(compiler, 1, dst, dstw, src, srcw));
			break;

#ifdef SLJIT_CONFIG_X86_64
		case SLJIT_MOVU_UI:
			TEST_FAIL(emit_mov_int(compiler, 0, dst, dstw, src, srcw));
			break;

		case SLJIT_MOVU_SI:
			TEST_FAIL(emit_mov_int(compiler, 1, dst, dstw, src, srcw));
			break;
#endif

		}
		if ((dst & SLJIT_MEM_FLAG) && (dst & 0xf) && (dstw != 0 || (dst & 0xf0) != 0)) {
#ifdef SLJIT_CONFIG_X86_64
			compiler->mode32 = 0;
#endif
			code = emit_x86_instruction(compiler, 1, dst & 0xf, 0, dst, dstw);
			TEST_MEM_ERROR(code);
			*code = 0x8d;
		}
		return SLJIT_NO_ERROR;

	case SLJIT_NOT:
		if (op & SLJIT_SET_FLAGS)
			return emit_not_with_flags(compiler, dst, dstw, src, srcw);
		return emit_unary(compiler, 0x2, dst, dstw, src, srcw);

	case SLJIT_NEG:
		return emit_unary(compiler, 0x3, dst, dstw, src, srcw);
	}

	return SLJIT_NO_ERROR;
}

#ifdef SLJIT_CONFIG_X86_64

#define BINARY_IMM(_op_imm_, _op_mr_, immw, arg, argw) \
	if (IS_HALFWORD(immw) || compiler->mode32) { \
		code = emit_x86_instruction(compiler, 1 | EX86_BIN_INS, SLJIT_IMM, immw, arg, argw); \
		TEST_MEM_ERROR(code); \
		*(code + 1) |= (_op_imm_); \
	} \
	else { \
		TEST_FAIL(emit_load_imm64(compiler, TMP_REG2, immw)); \
		code = emit_x86_instruction(compiler, 1 | EX86_BIN_INS, TMP_REG2, 0, arg, argw); \
		TEST_MEM_ERROR(code); \
		*code = (_op_mr_); \
	}

#define BINARY_EAX_IMM(_op_eax_imm_, immw) \
	emit_do_imm32(compiler, (!compiler->mode32) ? REX_W : 0, (_op_eax_imm_), immw)

#else

#define BINARY_IMM(_op_imm_, _op_mr_, immw, arg, argw) \
	code = emit_x86_instruction(compiler, 1 | EX86_BIN_INS, SLJIT_IMM, immw, arg, argw); \
	TEST_MEM_ERROR(code); \
	*(code + 1) |= (_op_imm_);

#define BINARY_EAX_IMM(_op_eax_imm_, immw) \
	emit_do_imm(compiler, (_op_eax_imm_), immw)

#endif

static int emit_cum_binary(struct sljit_compiler *compiler,
	sljit_ub op_rm, sljit_ub op_mr, sljit_ub op_imm, sljit_ub op_eax_imm,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	sljit_ub* code;

	if (dst == SLJIT_NO_REG) {
		EMIT_MOV(compiler, TMP_REGISTER, 0, src1, src1w);
		if (src2 & SLJIT_IMM) {
			BINARY_IMM(op_imm, op_mr, src2w, TMP_REGISTER, 0);
		}
		else {
			code = emit_x86_instruction(compiler, 1 | EX86_BIN_INS, TMP_REGISTER, 0, src2, src2w);
			TEST_MEM_ERROR(code);
			*code = op_rm;
		}
		return SLJIT_NO_ERROR;
	}

	if (dst == src1 && dstw == src1w) {
		if (src2 & SLJIT_IMM) {
#ifdef SLJIT_CONFIG_X86_64
			if ((dst == SLJIT_TEMPORARY_REG1) && (src2w > 127 || src2w < -128) && (compiler->mode32 || IS_HALFWORD(src2w))) {
#else
			if ((dst == SLJIT_TEMPORARY_REG1) && (src2w > 127 || src2w < -128)) {
#endif
				BINARY_EAX_IMM(op_eax_imm, src2w);
			}
			else {
				BINARY_IMM(op_imm, op_mr, src2w, dst, dstw);
			}
		}
		else if (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_GENERAL_REG3) {
			code = emit_x86_instruction(compiler, 1 | EX86_BIN_INS, dst, dstw, src2, src2w);
			TEST_MEM_ERROR(code);
			*code = op_rm;
		}
		else if (src2 >= SLJIT_TEMPORARY_REG1 && src2 <= SLJIT_LOCALS_REG) {
			code = emit_x86_instruction(compiler, 1 | EX86_BIN_INS, src2, src2w, dst, dstw);
			TEST_MEM_ERROR(code);
			*code = op_mr;
		}
		else {
			EMIT_MOV(compiler, TMP_REGISTER, 0, src2, src2w);
			code = emit_x86_instruction(compiler, 1 | EX86_BIN_INS, TMP_REGISTER, 0, dst, dstw);
			TEST_MEM_ERROR(code);
			*code = op_mr;
		}
		return SLJIT_NO_ERROR;
	}

	// Only for cumulative operations
	if (dst == src2 && dstw == src2w) {
		if (src1 & SLJIT_IMM) {
#ifdef SLJIT_CONFIG_X86_64
			if ((dst == SLJIT_TEMPORARY_REG1) && (src1w > 127 || src1w < -128) && (compiler->mode32 || IS_HALFWORD(src1w))) {
#else
			if ((dst == SLJIT_TEMPORARY_REG1) && (src1w > 127 || src1w < -128)) {
#endif
				BINARY_EAX_IMM(op_eax_imm, src1w);
			}
			else {
				BINARY_IMM(op_imm, op_mr, src1w, dst, dstw);
			}
		}
		else if (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_GENERAL_REG3) {
			code = emit_x86_instruction(compiler, 1 | EX86_BIN_INS, dst, dstw, src1, src1w);
			TEST_MEM_ERROR(code);
			*code = op_rm;
		}
		else if (src1 >= SLJIT_TEMPORARY_REG1 && src1 <= SLJIT_LOCALS_REG) {
			code = emit_x86_instruction(compiler, 1 | EX86_BIN_INS, src1, src1w, dst, dstw);
			TEST_MEM_ERROR(code);
			*code = op_mr;
		}
		else {
			EMIT_MOV(compiler, TMP_REGISTER, 0, src1, src1w);
			code = emit_x86_instruction(compiler, 1 | EX86_BIN_INS, TMP_REGISTER, 0, dst, dstw);
			TEST_MEM_ERROR(code);
			*code = op_mr;
		}
		return SLJIT_NO_ERROR;
	}

	// General version
	if (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_GENERAL_REG3) {
		EMIT_MOV(compiler, dst, 0, src1, src1w);
		if (src2 & SLJIT_IMM) {
			BINARY_IMM(op_imm, op_mr, src2w, dst, 0);
		}
		else {
			code = emit_x86_instruction(compiler, 1 | EX86_BIN_INS, dst, 0, src2, src2w);
			TEST_MEM_ERROR(code);
			*code = op_rm;
		}
	}
	else {
		// This version requires less memory writing
		EMIT_MOV(compiler, TMP_REGISTER, 0, src1, src1w);
		if (src2 & SLJIT_IMM) {
			BINARY_IMM(op_imm, op_mr, src2w, TMP_REGISTER, 0);
		}
		else {
			code = emit_x86_instruction(compiler, 1 | EX86_BIN_INS, TMP_REGISTER, 0, src2, src2w);
			TEST_MEM_ERROR(code);
			*code = op_rm;
		}
		EMIT_MOV(compiler, dst, dstw, TMP_REGISTER, 0);
	}

	return SLJIT_NO_ERROR;
}

static int emit_non_cum_binary(struct sljit_compiler *compiler,
	sljit_ub op_rm, sljit_ub op_mr, sljit_ub op_imm, sljit_ub op_eax_imm,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	sljit_ub* code;

	if (dst == SLJIT_NO_REG) {
		EMIT_MOV(compiler, TMP_REGISTER, 0, src1, src1w);
		if (src2 & SLJIT_IMM) {
			BINARY_IMM(op_imm, op_mr, src2w, TMP_REGISTER, 0);
		}
		else {
			code = emit_x86_instruction(compiler, 1 | EX86_BIN_INS, TMP_REGISTER, 0, src2, src2w);
			TEST_MEM_ERROR(code);
			*code = op_rm;
		}
		return SLJIT_NO_ERROR;
	}

	if (dst == src1 && dstw == src1w) {
		if (src2 & SLJIT_IMM) {
#ifdef SLJIT_CONFIG_X86_64
			if ((dst == SLJIT_TEMPORARY_REG1) && (src2w > 127 || src2w < -128) && (compiler->mode32 || IS_HALFWORD(src2w))) {
#else
			if ((dst == SLJIT_TEMPORARY_REG1) && (src2w > 127 || src2w < -128)) {
#endif
				BINARY_EAX_IMM(op_eax_imm, src2w);
			}
			else {
				BINARY_IMM(op_imm, op_mr, src2w, dst, dstw);
			}
		}
		else if (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_GENERAL_REG3) {
			code = emit_x86_instruction(compiler, 1 | EX86_BIN_INS, dst, dstw, src2, src2w);
			TEST_MEM_ERROR(code);
			*code = op_rm;
		}
		else if (src2 >= SLJIT_TEMPORARY_REG1 && src2 <= SLJIT_LOCALS_REG) {
			code = emit_x86_instruction(compiler, 1 | EX86_BIN_INS, src2, src2w, dst, dstw);
			TEST_MEM_ERROR(code);
			*code = op_mr;
		}
		else {
			EMIT_MOV(compiler, TMP_REGISTER, 0, src2, src2w);
			code = emit_x86_instruction(compiler, 1 | EX86_BIN_INS, TMP_REGISTER, 0, dst, dstw);
			TEST_MEM_ERROR(code);
			*code = op_mr;
		}
		return SLJIT_NO_ERROR;
	}

	// General version
	if ((dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_GENERAL_REG3) && dst != src2) {
		EMIT_MOV(compiler, dst, 0, src1, src1w);
		if (src2 & SLJIT_IMM) {
			BINARY_IMM(op_imm, op_mr, src2w, dst, 0);
		}
		else {
			code = emit_x86_instruction(compiler, 1 | EX86_BIN_INS, dst, 0, src2, src2w);
			TEST_MEM_ERROR(code);
			*code = op_rm;
		}
	}
	else {
		// This version requires less memory writing
		EMIT_MOV(compiler, TMP_REGISTER, 0, src1, src1w);
		if (src2 & SLJIT_IMM) {
			BINARY_IMM(op_imm, op_mr, src2w, TMP_REGISTER, 0);
		}
		else {
			code = emit_x86_instruction(compiler, 1 | EX86_BIN_INS, TMP_REGISTER, 0, src2, src2w);
			TEST_MEM_ERROR(code);
			*code = op_rm;
		}
		EMIT_MOV(compiler, dst, dstw, TMP_REGISTER, 0);
	}

	return SLJIT_NO_ERROR;
}

static int emit_mul(struct sljit_compiler *compiler,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	sljit_ub* code;
	int dst_r;

	dst_r = (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_GENERAL_REG3) ? dst : TMP_REGISTER;

	// Register destination
	if (dst_r == src1 && !(src2 & SLJIT_IMM)) {
		code = emit_x86_instruction(compiler, 2, dst_r, 0, src2, src2w);
		TEST_MEM_ERROR(code);
		*code++ = 0x0f;
		*code = 0xaf;
	}
	else if (dst_r == src2 && !(src1 & SLJIT_IMM)) {
		code = emit_x86_instruction(compiler, 2, dst_r, 0, src1, src1w);
		TEST_MEM_ERROR(code);
		*code++ = 0x0f;
		*code = 0xaf;
	}
	else if (src1 & SLJIT_IMM) {
		if (src2 & SLJIT_IMM) {
			EMIT_MOV(compiler, dst_r, 0, SLJIT_IMM, src2w);
			src2 = dst_r;
			src2w = 0;
		}

		if (src1w <= 127 && src1w >= -128) {
			code = emit_x86_instruction(compiler, 1, dst_r, 0, src2, src2w);
			TEST_MEM_ERROR(code);
			*code = 0x6b;
			code = (sljit_ub*)ensure_buf(compiler, 1 + 1);
			TEST_MEM_ERROR(code);
			INC_CSIZE(1);
			*code = (sljit_b)src1w;
		}
#ifdef SLJIT_CONFIG_X86_32
		else {
			code = emit_x86_instruction(compiler, 1, dst_r, 0, src2, src2w);
			TEST_MEM_ERROR(code);
			*code = 0x69;
			code = (sljit_ub*)ensure_buf(compiler, 1 + 4);
			TEST_MEM_ERROR(code);
			INC_CSIZE(4);
			*(sljit_w*)code = src1w;
		}
#else
		else if (IS_HALFWORD(src1w)) {
			code = emit_x86_instruction(compiler, 1, dst_r, 0, src2, src2w);
			TEST_MEM_ERROR(code);
			*code = 0x69;
			code = (sljit_ub*)ensure_buf(compiler, 1 + 4);
			TEST_MEM_ERROR(code);
			INC_CSIZE(4);
			*(sljit_hw*)code = src1w;
		}
		else {
			EMIT_MOV(compiler, TMP_REG2, 0, SLJIT_IMM, src1w);
			if (dst_r != src2)
				EMIT_MOV(compiler, dst_r, 0, src2, src2w);
			code = emit_x86_instruction(compiler, 2, dst_r, 0, TMP_REG2, 0);
			TEST_MEM_ERROR(code);
			*code++ = 0x0f;
			*code = 0xaf;
		}
#endif
	}
	else if (src2 & SLJIT_IMM) {
		// Note: src1 is NOT immediate

		if (src2w <= 127 && src2w >= -128) {
			code = emit_x86_instruction(compiler, 1, dst_r, 0, src1, src1w);
			TEST_MEM_ERROR(code);
			*code = 0x6b;
			code = (sljit_ub*)ensure_buf(compiler, 1 + 1);
			TEST_MEM_ERROR(code);
			INC_CSIZE(1);
			*code = (sljit_b)src2w;
		}
#ifdef SLJIT_CONFIG_X86_32
		else {
			code = emit_x86_instruction(compiler, 1, dst_r, 0, src1, src1w);
			TEST_MEM_ERROR(code);
			*code = 0x69;
			code = (sljit_ub*)ensure_buf(compiler, 1 + 4);
			TEST_MEM_ERROR(code);
			INC_CSIZE(4);
			*(sljit_w*)code = src2w;
		}
#else
		else if (IS_HALFWORD(src2w)) {
			code = emit_x86_instruction(compiler, 1, dst_r, 0, src1, src1w);
			TEST_MEM_ERROR(code);
			*code = 0x69;
			code = (sljit_ub*)ensure_buf(compiler, 1 + 4);
			TEST_MEM_ERROR(code);
			INC_CSIZE(4);
			*(sljit_hw*)code = src2w;
		}
		else {
			EMIT_MOV(compiler, TMP_REG2, 0, SLJIT_IMM, src1w);
			if (dst_r != src1)
				EMIT_MOV(compiler, dst_r, 0, src1, src1w);
			code = emit_x86_instruction(compiler, 2, dst_r, 0, TMP_REG2, 0);
			TEST_MEM_ERROR(code);
			*code++ = 0x0f;
			*code = 0xaf;
		}
#endif
	}
	else {
		// Neither argument is immediate
		if (depends_on(src2, dst_r))
			dst_r = TMP_REGISTER;
		EMIT_MOV(compiler, dst_r, 0, src1, src1w);
		code = emit_x86_instruction(compiler, 2, dst_r, 0, src2, src2w);
		TEST_MEM_ERROR(code);
		*code++ = 0x0f;
		*code = 0xaf;
	}

	if (dst_r == TMP_REGISTER)
		EMIT_MOV(compiler, dst, dstw, TMP_REGISTER, 0);

	return SLJIT_NO_ERROR;
}

static int emit_cmp_binary(struct sljit_compiler *compiler,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	sljit_ub* code;

#ifdef SLJIT_CONFIG_X86_64
	if (src1 == SLJIT_TEMPORARY_REG1 && (src2 & SLJIT_IMM) && (src2w > 127 || src2w < -128) && (compiler->mode32 || IS_HALFWORD(src2w))) {
#else
	if (src1 == SLJIT_TEMPORARY_REG1 && (src2 & SLJIT_IMM) && (src2w > 127 || src2w < -128)) {
#endif
		BINARY_EAX_IMM(0x3d, src2w);
		return SLJIT_NO_ERROR;
	}

	if (src1 >= SLJIT_TEMPORARY_REG1 && src1 <= SLJIT_LOCALS_REG) {
		if (src2 & SLJIT_IMM) {
			BINARY_IMM(0x7 << 3, 0x39, src2w, src1, 0);
		}
		else {
			code = emit_x86_instruction(compiler, 1 | EX86_BIN_INS, src1, 0, src2, src2w);
			TEST_MEM_ERROR(code);
			*code = 0x3b;
		}
		return SLJIT_NO_ERROR;
	}

	if (src2 >= SLJIT_TEMPORARY_REG1 && src2 <= SLJIT_LOCALS_REG && !(src1 & SLJIT_IMM)) {
		code = emit_x86_instruction(compiler, 1 | EX86_BIN_INS, src2, 0, src1, src1w);
		TEST_MEM_ERROR(code);
		*code = 0x39;
		return SLJIT_NO_ERROR;
	}

	EMIT_MOV(compiler, TMP_REGISTER, 0, src1, src1w);
	if (src2 & SLJIT_IMM) {
		BINARY_IMM(0x7 << 3, 0x39, src2w, TMP_REGISTER, 0);
	}
	else {
		code = emit_x86_instruction(compiler, 1 | EX86_BIN_INS, TMP_REGISTER, 0, src2, src2w);
		TEST_MEM_ERROR(code);
		*code = 0x3b;
	}
	return SLJIT_NO_ERROR;
}

static int emit_test_binary(struct sljit_compiler *compiler,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	sljit_ub* code;

#ifdef SLJIT_CONFIG_X86_64
	if (src1 == SLJIT_TEMPORARY_REG1 && (src2 & SLJIT_IMM) && (src2w > 127 || src2w < -128) && (compiler->mode32 || IS_HALFWORD(src2w))) {
#else
	if (src1 == SLJIT_TEMPORARY_REG1 && (src2 & SLJIT_IMM) && (src2w > 127 || src2w < -128)) {
#endif
		BINARY_EAX_IMM(0xa9, src2w);
		return SLJIT_NO_ERROR;
	}

#ifdef SLJIT_CONFIG_X86_64
	if (src2 == SLJIT_TEMPORARY_REG1 && (src2 & SLJIT_IMM) && (src1w > 127 || src1w < -128) && (compiler->mode32 || IS_HALFWORD(src1w))) {
#else
	if (src2 == SLJIT_TEMPORARY_REG1 && (src1 & SLJIT_IMM) && (src1w > 127 || src1w < -128)) {
#endif
		BINARY_EAX_IMM(0xa9, src1w);
		return SLJIT_NO_ERROR;
	}

	if (src1 >= SLJIT_TEMPORARY_REG1 && src1 <= SLJIT_LOCALS_REG) {
		if (src2 & SLJIT_IMM) {
#ifdef SLJIT_CONFIG_X86_64
			if (IS_HALFWORD(src2w) || compiler->mode32) {
				code = emit_x86_instruction(compiler, 1, SLJIT_IMM, src2w, src1, 0);
				TEST_MEM_ERROR(code);
				*code = 0xf7;
			}
			else {
				TEST_FAIL(emit_load_imm64(compiler, TMP_REG2, src2w));
				code = emit_x86_instruction(compiler, 1 | EX86_BIN_INS, TMP_REG2, 0, src1, 0);
				TEST_MEM_ERROR(code);
				*code = 0x85;
			}
#else
			code = emit_x86_instruction(compiler, 1, SLJIT_IMM, src2w, src1, 0);
			TEST_MEM_ERROR(code);
			*code = 0xf7;
#endif
		}
		else {
			code = emit_x86_instruction(compiler, 1, src1, 0, src2, src2w);
			TEST_MEM_ERROR(code);
			*code = 0x85;
		}
		return SLJIT_NO_ERROR;
	}

	if (src2 >= SLJIT_TEMPORARY_REG1 && src2 <= SLJIT_LOCALS_REG) {
		if (src1 & SLJIT_IMM) {
#ifdef SLJIT_CONFIG_X86_64
			if (IS_HALFWORD(src1w) || compiler->mode32) {
				code = emit_x86_instruction(compiler, 1, SLJIT_IMM, src1w, src2, 0);
				TEST_MEM_ERROR(code);
				*code = 0xf7;
			}
			else {
				TEST_FAIL(emit_load_imm64(compiler, TMP_REG2, src1w));
				code = emit_x86_instruction(compiler, 1 | EX86_BIN_INS, TMP_REG2, 0, src2, 0);
				TEST_MEM_ERROR(code);
				*code = 0x85;
			}
#else
			code = emit_x86_instruction(compiler, 1, src1, src1w, src2, 0);
			TEST_MEM_ERROR(code);
			*code = 0xf7;
#endif
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
#ifdef SLJIT_CONFIG_X86_64
		if (IS_HALFWORD(src2w) || compiler->mode32) {
			code = emit_x86_instruction(compiler, 1, SLJIT_IMM, src2w, TMP_REGISTER, 0);
			TEST_MEM_ERROR(code);
			*code = 0xf7;
		}
		else {
			TEST_FAIL(emit_load_imm64(compiler, TMP_REG2, src2w));
			code = emit_x86_instruction(compiler, 1 | EX86_BIN_INS, TMP_REG2, 0, TMP_REGISTER, 0);
			TEST_MEM_ERROR(code);
			*code = 0x85;
		}
#else
		code = emit_x86_instruction(compiler, 1, SLJIT_IMM, src2w, TMP_REGISTER, 0);
		TEST_MEM_ERROR(code);
		*code = 0xf7;
#endif
	}
	else {
		code = emit_x86_instruction(compiler, 1, TMP_REGISTER, 0, src2, src2w);
		TEST_MEM_ERROR(code);
		*code = 0x85;
	}
	return SLJIT_NO_ERROR;
}

static int emit_shift(struct sljit_compiler *compiler,
	sljit_ub mode,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	sljit_ub* code;

	if ((src2 & SLJIT_IMM) || (src2 == SLJIT_PREF_SHIFT_REG)) {
		if (dst == src1 && dstw == src1w) {
			code = emit_x86_instruction(compiler, 1 | EX86_SHIFT_INS, src2, src2w, dst, dstw);
			TEST_MEM_ERROR(code);
			*code |= mode;
			return SLJIT_NO_ERROR;
		}
		if (dst == SLJIT_NO_REG) {
			EMIT_MOV(compiler, TMP_REGISTER, 0, src1, src1w);
			code = emit_x86_instruction(compiler, 1 | EX86_SHIFT_INS, src2, src2w, TMP_REGISTER, 0);
			TEST_MEM_ERROR(code);
			*code |= mode;
			return SLJIT_NO_ERROR;
		}
		if (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_GENERAL_REG3) {
			EMIT_MOV(compiler, dst, 0, src1, src1w);
			code = emit_x86_instruction(compiler, 1 | EX86_SHIFT_INS, src2, src2w, dst, 0);
			TEST_MEM_ERROR(code);
			*code |= mode;
			return SLJIT_NO_ERROR;
		}

		EMIT_MOV(compiler, TMP_REGISTER, 0, src1, src1w);
		code = emit_x86_instruction(compiler, 1 | EX86_SHIFT_INS, src2, src2w, TMP_REGISTER, 0);
		TEST_MEM_ERROR(code);
		*code |= mode;
		EMIT_MOV(compiler, dst, dstw, TMP_REGISTER, 0);
		return SLJIT_NO_ERROR;
	}

	if (dst == SLJIT_PREF_SHIFT_REG) {
		EMIT_MOV(compiler, TMP_REGISTER, 0, src1, src1w);
		EMIT_MOV(compiler, SLJIT_PREF_SHIFT_REG, 0, src2, src2w);
		code = emit_x86_instruction(compiler, 1 | EX86_SHIFT_INS, SLJIT_PREF_SHIFT_REG, 0, TMP_REGISTER, 0);
		TEST_MEM_ERROR(code);
		*code |= mode;
		EMIT_MOV(compiler, SLJIT_PREF_SHIFT_REG, 0, TMP_REGISTER, 0);
	}
	else if (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_GENERAL_REG3 && dst != src2 && !depends_on(src2, dst)) {
		if (src1 != dst)
			EMIT_MOV(compiler, dst, 0, src1, src1w);
		EMIT_MOV(compiler, TMP_REGISTER, 0, SLJIT_PREF_SHIFT_REG, 0);
		EMIT_MOV(compiler, SLJIT_PREF_SHIFT_REG, 0, src2, src2w);
		code = emit_x86_instruction(compiler, 1 | EX86_SHIFT_INS, SLJIT_PREF_SHIFT_REG, 0, dst, 0);
		TEST_MEM_ERROR(code);
		*code |= mode;
		EMIT_MOV(compiler, SLJIT_PREF_SHIFT_REG, 0, TMP_REGISTER, 0);
	}
	else {
		// This case is really bad, since ecx can be used for
		// addressing as well, and we must ensure to work even
		// in that case 
#ifdef SLJIT_CONFIG_X86_64
		EMIT_MOV(compiler, TMP_REG2, 0, SLJIT_PREF_SHIFT_REG, 0);
#else
		EMIT_MOV(compiler, SLJIT_MEM1(SLJIT_LOCALS_REG), -(int)sizeof(sljit_w), SLJIT_PREF_SHIFT_REG, 0);
#endif

		EMIT_MOV(compiler, TMP_REGISTER, 0, src1, src1w);
		EMIT_MOV(compiler, SLJIT_PREF_SHIFT_REG, 0, src2, src2w);
		code = emit_x86_instruction(compiler, 1 | EX86_SHIFT_INS, SLJIT_PREF_SHIFT_REG, 0, TMP_REGISTER, 0);
		TEST_MEM_ERROR(code);
		*code |= mode;

#ifdef SLJIT_CONFIG_X86_64
		EMIT_MOV(compiler, SLJIT_PREF_SHIFT_REG, 0, TMP_REG2, 0);
#else
		EMIT_MOV(compiler, SLJIT_PREF_SHIFT_REG, 0, SLJIT_MEM1(SLJIT_LOCALS_REG), -(int)sizeof(sljit_w));
#endif
		EMIT_MOV(compiler, dst, dstw, TMP_REGISTER, 0);
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
	FUNCTION_CHECK_SRC(src1, src1w);
	FUNCTION_CHECK_SRC(src2, src2w);
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_op2_verbose();

#ifdef SLJIT_CONFIG_X86_64
	compiler->mode32 = op & SLJIT_INT_OPERATION;
#endif

	switch (GET_OPCODE(op)) {
	case SLJIT_ADD:
		return emit_cum_binary(compiler, 0x03, 0x01, 0x0 << 3, 0x05,
			dst, dstw, src1, src1w, src2, src2w);
	case SLJIT_ADDC:
		return emit_cum_binary(compiler, 0x13, 0x11, 0x2 << 3, 0x15,
			dst, dstw, src1, src1w, src2, src2w);
	case SLJIT_SUB:
		if (dst == SLJIT_NO_REG)
			return emit_cmp_binary(compiler, src1, src1w, src2, src2w);
		return emit_non_cum_binary(compiler, 0x2b, 0x29, 0x5 << 3, 0x2d,
			dst, dstw, src1, src1w, src2, src2w);
	case SLJIT_SUBC:
		return emit_non_cum_binary(compiler, 0x1b, 0x19, 0x3 << 3, 0x1d,
			dst, dstw, src1, src1w, src2, src2w);
	case SLJIT_MUL:
		return emit_mul(compiler, dst, dstw, src1, src1w, src2, src2w);
	case SLJIT_AND:
		if (dst == SLJIT_NO_REG)
			return emit_test_binary(compiler, src1, src1w, src2, src2w);
		return emit_cum_binary(compiler, 0x23, 0x21, 0x4 << 3, 0x25,
			dst, dstw, src1, src1w, src2, src2w);
	case SLJIT_OR:
		return emit_cum_binary(compiler, 0x0b, 0x09, 0x1 << 3, 0x0d,
			dst, dstw, src1, src1w, src2, src2w);
	case SLJIT_XOR:
		return emit_cum_binary(compiler, 0x33, 0x31, 0x6 << 3, 0x35,
			dst, dstw, src1, src1w, src2, src2w);
	case SLJIT_SHL:
		return emit_shift(compiler, 0x4 << 3,
			dst, dstw, src1, src1w, src2, src2w);
	case SLJIT_LSHR:
		return emit_shift(compiler, 0x5 << 3,
			dst, dstw, src1, src1w, src2, src2w);
	case SLJIT_ASHR:
		return emit_shift(compiler, 0x7 << 3,
			dst, dstw, src1, src1w, src2, src2w);
	}

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

static int emit_fld(struct sljit_compiler *compiler,
	int src, sljit_w srcw)
{
	sljit_ub *buf;

	if (src >= SLJIT_FLOAT_REG1 && src <= SLJIT_FLOAT_REG4) {
		buf = (sljit_ub*)ensure_buf(compiler, 1 + 2);
		TEST_MEM_ERROR(buf);
		INC_SIZE(2);
		*buf++ = 0xd9;
		*buf = 0xc0 + src - 1;
		return SLJIT_NO_ERROR;
	}

	buf = emit_x86_instruction(compiler, 1, 0, 0, src, srcw);
	TEST_MEM_ERROR(buf);
	*buf = 0xdd;
	return SLJIT_NO_ERROR;
}

static int emit_fop(struct sljit_compiler *compiler,
	sljit_ub st_arg, sljit_ub st_arg2,
	sljit_ub m64fp_arg, sljit_ub m64fp_arg2,
	int src, sljit_w srcw)
{
	sljit_ub *buf;

	if (src >= SLJIT_FLOAT_REG1 && src <= SLJIT_FLOAT_REG4) {
		buf = (sljit_ub*)ensure_buf(compiler, 1 + 2);
		TEST_MEM_ERROR(buf);
		INC_SIZE(2);
		*buf++ = st_arg;
		*buf = st_arg2 + src;
		return SLJIT_NO_ERROR;
	}

	buf = emit_x86_instruction(compiler, 1, 0, 0, src, srcw);
	TEST_MEM_ERROR(buf);
	*buf++ = m64fp_arg;
	*buf |= m64fp_arg2;
	return SLJIT_NO_ERROR;
}

static int emit_fop_regs(struct sljit_compiler *compiler,
	sljit_ub st_arg, sljit_ub st_arg2,
	int src)
{
	sljit_ub *buf;

	buf = (sljit_ub*)ensure_buf(compiler, 1 + 2);
	TEST_MEM_ERROR(buf);
	INC_SIZE(2);
	*buf++ = st_arg;
	*buf = st_arg2 + src;
	return SLJIT_NO_ERROR;
}

int sljit_emit_fop1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
	FUNCTION_ENTRY();

	SLJIT_ASSERT(op >= SLJIT_FCMP && op <= SLJIT_FABS);
#ifdef SLJIT_DEBUG
	FUNCTION_FCHECK(src, srcw);
	FUNCTION_FCHECK(dst, dstw);
#endif
	sljit_emit_fop1_verbose();

#ifdef SLJIT_CONFIG_X86_64
	compiler->mode32 = 1;
#endif

	TEST_FAIL(emit_fld(compiler, src, srcw));

	switch (op) {
	case SLJIT_FNEG:
		TEST_FAIL(emit_fop_regs(compiler, 0xd9, 0xe0, 0));
		break;
	case SLJIT_FABS:
		TEST_FAIL(emit_fop_regs(compiler, 0xd9, 0xe1, 0));
		break;
	}

	TEST_FAIL(emit_fop(compiler, 0xdd, 0xd8, 0xdd, 0x3 << 3, dst, dstw));

	return SLJIT_NO_ERROR;
}

int sljit_emit_fop2(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	FUNCTION_ENTRY();

	SLJIT_ASSERT(op >= SLJIT_FADD && op <= SLJIT_FDIV);
#ifdef SLJIT_DEBUG
	FUNCTION_FCHECK(src1, src1w);
	FUNCTION_FCHECK(src2, src2w);
	FUNCTION_FCHECK(dst, dstw);
#endif
	sljit_emit_fop2_verbose();

#ifdef SLJIT_CONFIG_X86_64
	compiler->mode32 = 1;
#endif

	if (src1 >= SLJIT_FLOAT_REG1 && src1 <= SLJIT_FLOAT_REG4 && dst == src1) {
		TEST_FAIL(emit_fld(compiler, src2, src2w));

		switch (op) {
		case SLJIT_FADD:
			TEST_FAIL(emit_fop_regs(compiler, 0xde, 0xc0, src1));
			break;
		case SLJIT_FSUB:
			TEST_FAIL(emit_fop_regs(compiler, 0xde, 0xe8, src1));
			break;
		case SLJIT_FMUL:
			TEST_FAIL(emit_fop_regs(compiler, 0xde, 0xc8, src1));
			break;
		case SLJIT_FDIV:
			TEST_FAIL(emit_fop_regs(compiler, 0xde, 0xf8, src1));
			break;
		}
		return SLJIT_NO_ERROR;
	}

	TEST_FAIL(emit_fld(compiler, src1, src1w));

	if (src2 >= SLJIT_FLOAT_REG1 && src2 <= SLJIT_FLOAT_REG4 && dst == src2) {
		switch (op) {
		case SLJIT_FADD:
			TEST_FAIL(emit_fop_regs(compiler, 0xde, 0xc0, src2));
			break;
		case SLJIT_FSUB:
			TEST_FAIL(emit_fop_regs(compiler, 0xde, 0xe0, src2));
			break;
		case SLJIT_FMUL:
			TEST_FAIL(emit_fop_regs(compiler, 0xde, 0xc8, src2));
			break;
		case SLJIT_FDIV:
			TEST_FAIL(emit_fop_regs(compiler, 0xde, 0xf0, src2));
			break;
		}
		return SLJIT_NO_ERROR;
	}

	switch (op) {
	case SLJIT_FADD:
		TEST_FAIL(emit_fop(compiler, 0xd8, 0xc0, 0xdc, 0x0 << 3, src2, src2w));
		break;
	case SLJIT_FSUB:
		TEST_FAIL(emit_fop(compiler, 0xd8, 0xe0, 0xdc, 0x4 << 3, src2, src2w));
		break;
	case SLJIT_FMUL:
		TEST_FAIL(emit_fop(compiler, 0xd8, 0xc8, 0xdc, 0x1 << 3, src2, src2w));
		break;
	case SLJIT_FDIV:
		TEST_FAIL(emit_fop(compiler, 0xd8, 0xf0, 0xdc, 0x6 << 3, src2, src2w));
		break;
	}

	TEST_FAIL(emit_fop(compiler, 0xdd, 0xd8, 0xdd, 0x3 << 3, dst, dstw));

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

	label = (struct sljit_label*)ensure_abuf(compiler, sizeof(struct sljit_label));
	TEST_MEM_ERROR2(label);

	label->next = NULL;
	label->size = compiler->size;
	if (compiler->last_label)
		compiler->last_label->next = label;
	else
		compiler->labels = label;
	compiler->last_label = label;

	buf = (sljit_ub*)ensure_buf(compiler, 2);
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
	SLJIT_ASSERT((type & 0xff) >= SLJIT_C_EQUAL && (type & 0xff) <= SLJIT_CALL3);

	sljit_emit_jump_verbose();

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

	if (type >= SLJIT_CALL1)
		if (call_with_args(compiler, type))
			return NULL;

	// Worst case size
#ifdef SLJIT_CONFIG_X86_32
	compiler->size += (type >= SLJIT_JUMP) ? 5 : 6;
#else
	if (!(jump->flags & SLJIT_LONG_JUMP))
		compiler->size += (type >= SLJIT_JUMP) ? 5 : 6;
	else {
		compiler->addrs++;
		compiler->size += (type >= SLJIT_JUMP) ? 6 : 8;
	}
#endif

	buf = (sljit_ub*)ensure_buf(compiler, 2);
	TEST_MEM_ERROR2(buf);

	*buf++ = 0;
	*buf++ = type + 4;

	return jump;
}

int sljit_emit_ijump(struct sljit_compiler *compiler, int type, int src, sljit_w srcw)
{
	sljit_ub *buf;
	sljit_ub *code;

	FUNCTION_ENTRY();
	SLJIT_ASSERT(type >= SLJIT_JUMP && type <= SLJIT_CALL3);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_SRC(src, srcw);
#endif
	sljit_emit_ijump_verbose();

	if (type >= SLJIT_CALL1)
		TEST_FAIL(call_with_args(compiler, type));

	if (src == SLJIT_IMM) {
		buf = (sljit_ub*)ensure_buf(compiler, 2 + sizeof(sljit_w));
		TEST_MEM_ERROR(buf);

#ifdef SLJIT_CONFIG_X86_32
		compiler->size += 5;
#else
		compiler->addrs++;
		compiler->size += 6;
#endif

		*buf++ = 0;
		*buf++ = (type >= SLJIT_CALL0) ? 2 : 3;
		*(sljit_w*)buf = srcw;
	}
	else {
#ifdef SLJIT_CONFIG_X86_64
		compiler->mode32 = 1; /* dirty trick: REX_W is not necessary with call,
					 and we know that src is not immediate */
#endif
		code = emit_x86_instruction(compiler, 1, 0, 0, src, srcw);
		TEST_MEM_ERROR(code);
		*code++ = 0xff;
		*code |= (type >= SLJIT_CALL0) ? (2 << 3) : (4 << 3);
	}

	return SLJIT_NO_ERROR;
}

int sljit_emit_cond_set(struct sljit_compiler *compiler, int dst, sljit_w dstw, int type)
{
	sljit_ub *buf;
	sljit_ub cond_set = 0;
#ifdef SLJIT_CONFIG_X86_64
	int reg;
#endif

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

#ifdef SLJIT_CONFIG_X86_64
	if (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_GENERAL_REG3)
		reg = dst;
	else
		reg = TMP_REGISTER;

	buf = (sljit_ub*)ensure_buf(compiler, 1 + 4 + 4);
	TEST_MEM_ERROR(buf);
	INC_SIZE(4 + 4);
	// Set al to conditional flag
	*buf++ = (reg_map[reg] <= 7) ? 0x40 : REX_B;
	*buf++ = 0x0f;
	*buf++ = cond_set;
	*buf++ = 0xC0 | reg_lmap[reg];
	*buf++ = REX_W | (reg_map[reg] <= 7 ? 0 : (REX_B | REX_R));
	*buf++ = 0x0f;
	*buf++ = 0xb6;
	*buf = 0xC0 | (reg_lmap[reg] << 3) | reg_lmap[reg];

	if (reg == TMP_REGISTER)
		EMIT_MOV(compiler, dst, dstw, TMP_REGISTER, 0);

#else
	if (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_TEMPORARY_REG3) {
		buf = (sljit_ub*)ensure_buf(compiler, 1 + 3 + 3);
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
		EMIT_MOV(compiler, TMP_REGISTER, 0, SLJIT_TEMPORARY_REG1, 0);

		buf = (sljit_ub*)ensure_buf(compiler, 1 + 3 + 3);
		TEST_MEM_ERROR(buf);
		INC_SIZE(3 + 3);
		// Set al to conditional flag
		*buf++ = 0x0f;
		*buf++ = cond_set;
		*buf++ = 0xC0;

		*buf++ = 0x0f;
		*buf++ = 0xb6;
		if (dst >= SLJIT_GENERAL_REG1 && dst <= SLJIT_GENERAL_REG3)
			*buf = 0xC0 | (reg_map[dst] << 3);
		else {
			*buf = 0xC0;
			EMIT_MOV(compiler, dst, dstw, SLJIT_TEMPORARY_REG1, 0);
		}

		EMIT_MOV(compiler, SLJIT_TEMPORARY_REG1, 0, TMP_REGISTER, 0);
	}
#endif

	return SLJIT_NO_ERROR;
}

struct sljit_const* sljit_emit_const(struct sljit_compiler *compiler, int dst, sljit_w dstw, sljit_w initval)
{
	sljit_ub *buf;
	struct sljit_const *const_;
#ifdef SLJIT_CONFIG_X86_64
	int reg;
#endif

	FUNCTION_ENTRY();
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_const_verbose();

	const_ = (struct sljit_const*)ensure_abuf(compiler, sizeof(struct sljit_const));
	TEST_MEM_ERROR2(const_);

	const_->next = NULL;
	if (compiler->last_const)
		compiler->last_const->next = const_;
	else
		compiler->consts = const_;
	compiler->last_const = const_;

#ifdef SLJIT_CONFIG_X86_64
	compiler->mode32 = 0;
	reg = (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_GENERAL_REG3) ? dst : TMP_REGISTER;

	if (emit_load_imm64(compiler, reg, initval))
		return NULL;
#else
	if (dst == SLJIT_NO_REG)
		dst = TMP_REGISTER;

	if (emit_mov(compiler, dst, dstw, SLJIT_IMM, initval))
		return NULL;
#endif

	buf = (sljit_ub*)ensure_buf(compiler, 2);
	TEST_MEM_ERROR2(buf);

	*buf++ = 0;
	*buf++ = 1;

#ifdef SLJIT_CONFIG_X86_64
	if (reg == TMP_REGISTER && dst != SLJIT_NO_REG)
		if (emit_mov(compiler, dst, dstw, TMP_REGISTER, 0))
			return NULL;
#endif

	return const_;
}

void sljit_set_jump_addr(sljit_uw addr, sljit_uw new_addr)
{
#ifdef SLJIT_CONFIG_X86_32
	*(sljit_w*)addr = new_addr - (addr + 4);
#else
	*(sljit_uw*)addr = new_addr;
#endif
}

void sljit_set_const(sljit_uw addr, sljit_w new_constant)
{
	*(sljit_w*)addr = new_constant;
}

