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

// Register indexes:
//   0 - EAX
//   1 - ECX
//   2 - EDX
//   3 - EBX
//   4 - none
//   5 - EBP
//   6 - ESI
//   7 - EDI

// Last register + 1
#define TMP_REGISTER	(SLJIT_GENERAL_REG3 + 1)

static sljit_ub reg_map[SLJIT_NO_REGISTERS + 2] = {
   0, 0, 1, 2, 3, 6, 7, 5
};

#define INC_SIZE(s)			(*buf++ = (s), compiler->size += (s))

#define MOD(mod, reg, rm)		(*buf++ = (mod) << 6 | (reg) << 3 | (rm))

#define PUSH_REG(r)			(*buf++ = (0x50 + (r)))
#define POP_REG(r)			(*buf++ = (0x58 + (r)))
#define RET()				(*buf++ = (0xC3))
// r32, r/m32
#define MOV_RM(mod, reg, rm)		(*buf++ = (0x8B), MOD(mod, reg, rm))

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
	code = SLJIT_MALLOC(compiler->size);
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
				if (*buf_ptr > 1) {
					jump->addr = (sljit_uw)code_ptr;
					code_ptr = generate_jump_code(jump, code_ptr, code, *buf_ptr - 2);
					jump = jump->next;
				}
				else if (*buf_ptr == 0) {
					label->addr = (sljit_uw)code_ptr;
					label->size = code_ptr - code;
					label = label->next;
				}
				else {
					const_->addr = ((sljit_uw)code_ptr) - sizeof(sljit_w);
					const_ = const_->next;
				}
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
	SLJIT_FREE(code);
}

int sljit_emit_enter(struct sljit_compiler *compiler, int type, int args, int general)
{
	int size;
	sljit_ub *buf;

	FUNCTION_ENTRY();
	// TODO: support the others
	SLJIT_ASSERT(type == CALL_TYPE_CDECL);
	SLJIT_ASSERT(args >= 0 && args <= SLJIT_NO_GEN_REGISTERS);
	SLJIT_ASSERT(general >= 0 && general <= SLJIT_NO_GEN_REGISTERS);
	SLJIT_ASSERT(args <= general);
	SLJIT_ASSERT(compiler->general == -1);

	sljit_emit_enter_verbose();

	compiler->general = general;

	size = 1 + general + ((args > 0) ? (2 + args * 3) : 0);
	buf = ensure_buf(compiler, 1 + size);
	TEST_MEM_ERROR(buf);

	INC_SIZE(size);
	PUSH_REG(reg_map[TMP_REGISTER]);
	if (args > 0) {
		*buf++ = 0x8b;
		*buf++ = 0xc4 | (reg_map[TMP_REGISTER] << 3);
	}
	if (general > 2)
		PUSH_REG(reg_map[SLJIT_GENERAL_REG3]);
	if (general > 1)
		PUSH_REG(reg_map[SLJIT_GENERAL_REG2]);
	if (general > 0)
		PUSH_REG(reg_map[SLJIT_GENERAL_REG1]);

	if (type == CALL_TYPE_CDECL) {
		if (args > 0) {
			*buf++ = 0x8b;
			*buf++ = 0x40 | (reg_map[SLJIT_GENERAL_REG1] << 3) | reg_map[TMP_REGISTER];
			*buf++ = sizeof(sljit_w) * 2;
		}
		if (args > 1) {
			*buf++ = 0x8b;
			*buf++ = 0x40 | (reg_map[SLJIT_GENERAL_REG2] << 3) | reg_map[TMP_REGISTER];
			*buf++ = sizeof(sljit_w) * 3;
		}
		if (args > 2) {
			*buf++ = 0x8b;
			*buf++ = 0x40 | (reg_map[SLJIT_GENERAL_REG3] << 3) | reg_map[TMP_REGISTER];
			*buf++ = sizeof(sljit_w) * 4;
		}
	}

	// Mov arguments to general registers
	return SLJIT_NO_ERROR;
}

int sljit_emit_return(struct sljit_compiler *compiler, int reg)
{
	int size;
	sljit_ub *buf;

	FUNCTION_ENTRY();
	SLJIT_ASSERT(reg >= 0 && reg <= SLJIT_NO_REGISTERS);
	SLJIT_ASSERT(compiler->general >= 0);

	sljit_emit_return_verbose();

	size = 2 + compiler->general;
	if (reg != SLJIT_PREF_RET_REG && reg != SLJIT_NO_REG)
		size += 2;
	buf = ensure_buf(compiler, 1 + size);
	TEST_MEM_ERROR(buf);

	INC_SIZE(size);
	if (reg != SLJIT_PREF_RET_REG && reg != SLJIT_NO_REG)
		MOV_RM(0x3, reg_map[SLJIT_PREF_RET_REG], reg_map[reg]);

	if (compiler->general > 0)
		POP_REG(reg_map[SLJIT_GENERAL_REG1]);
	if (compiler->general > 1)
		POP_REG(reg_map[SLJIT_GENERAL_REG2]);
	if (compiler->general > 2)
		POP_REG(reg_map[SLJIT_GENERAL_REG3]);
	POP_REG(reg_map[TMP_REGISTER]);
	RET();

	return SLJIT_NO_ERROR;
}

// ---------------------------------------------------------------------
//  Operators
// ---------------------------------------------------------------------

static sljit_ub* emit_x86_instruction(struct sljit_compiler *compiler, int size,
	// The register or immediate operand
	int a, sljit_w imma,
	// The general operand (not immediate)
	int b, sljit_w immb)
{
	sljit_ub *buf;
	sljit_ub *buf_ptr;
	int total_size = size;

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
	if (a & SLJIT_IMM)
		total_size += 4;

	buf = ensure_buf(compiler, 1 + total_size);
	TEST_MEM_ERROR2(buf);

	// Encoding the byte
	INC_SIZE(total_size);
	buf_ptr = buf + size;

	// Encode mod/rm byte
	if ((a & SLJIT_IMM) || (a == 0))
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

	if (a & SLJIT_IMM)
		*(sljit_w*)buf_ptr = imma;

	return buf;
}

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
	// Imm or SLJIT_TEMPORARY_REG2
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
		SLJIT_ASSERT(a == SLJIT_TEMPORARY_REG2);

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

	SLJIT_ASSERT(op >= SLJIT_MOV && op <= SLJIT_NEG);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_SRC(src, srcw);
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_op1_verbose();

	if (op == SLJIT_MOV)
		return emit_mov(compiler, dst, dstw, src, srcw);

	if (op == SLJIT_NOT)
		return sljit_emit_unary(compiler, 0x2, dst, dstw, src, srcw);

	if (op == SLJIT_NEG)
		return sljit_emit_unary(compiler, 0x3, dst, dstw, src, srcw);

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

	SLJIT_ASSERT(op >= SLJIT_ADD && op <= SLJIT_ASHR);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_SRC(src1, src1w);
	FUNCTION_CHECK_SRC(src2, src2w);
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_op2_verbose();

	switch (op) {
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
	*buf++ = type + 2;

	return jump;
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



