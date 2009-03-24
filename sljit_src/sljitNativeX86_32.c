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

// x86 32-bit arch dependent functions

static sljit_ub* generate_far_jump_code(struct sljit_jump *jump, sljit_ub *code_ptr, int type)
{
	SLJIT_ASSERT(jump->flags & (JUMP_LABEL | JUMP_ADDR));

	if (type == SLJIT_JUMP) {
		*code_ptr++ = 0xe9;
		jump->addr++;
	}
	else if (type >= SLJIT_CALL0) {
		*code_ptr++ = 0xe8;
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

	return code_ptr;
}

int sljit_emit_enter(struct sljit_compiler *compiler, int args, int general, int local_size)
{
	int size;
	sljit_ub *buf;

	FUNCTION_ENTRY();
	// TODO: support the others
	SLJIT_ASSERT(args >= 0 && args <= SLJIT_NO_GEN_REGISTERS);
	SLJIT_ASSERT(general >= 0 && general <= SLJIT_NO_GEN_REGISTERS);
	SLJIT_ASSERT(args <= general);
	SLJIT_ASSERT(local_size >= 0 && local_size <= SLJIT_MAX_LOCAL_SIZE);
	SLJIT_ASSERT(compiler->general == -1);
	SLJIT_ASSERT(compiler->args == -1);

	sljit_emit_enter_verbose();

	compiler->general = general;
	compiler->args = args;

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

	local_size = (local_size + sizeof(sljit_uw) - 1) & ~(sizeof(sljit_uw) - 1);
	compiler->local_size = local_size;
	if (local_size > 0)
		return emit_non_cum_binary(compiler, 0x2b, 0x29, 0x5 << 3, 0x2d,
			SLJIT_STACK_PTR_REG, 0, SLJIT_STACK_PTR_REG, 0, SLJIT_IMM, local_size);

	// Mov arguments to general registers
	return SLJIT_NO_ERROR;
}

void sljit_fake_enter(struct sljit_compiler *compiler, int args, int general, int local_size)
{
	SLJIT_ASSERT(args >= 0 && args <= SLJIT_NO_GEN_REGISTERS);
	SLJIT_ASSERT(general >= 0 && general <= SLJIT_NO_GEN_REGISTERS);
	SLJIT_ASSERT(args <= general);
	SLJIT_ASSERT(local_size >= 0 && local_size <= SLJIT_MAX_LOCAL_SIZE);
	SLJIT_ASSERT(compiler->general == -1);
	SLJIT_ASSERT(compiler->args == -1);

	sljit_fake_enter_verbose();

	compiler->general = general;
	compiler->args = args;
	compiler->local_size = (local_size + sizeof(sljit_uw) - 1) & ~(sizeof(sljit_uw) - 1);
}

int sljit_emit_return(struct sljit_compiler *compiler, int reg)
{
	int size;
	sljit_ub *buf;

	FUNCTION_ENTRY();
	SLJIT_ASSERT(reg >= 0 && reg <= SLJIT_NO_REGISTERS);
	SLJIT_ASSERT(compiler->general >= 0);
	SLJIT_ASSERT(compiler->args >= 0);

	sljit_emit_return_verbose();

	if (compiler->local_size > 0)
		TEST_FAIL(emit_cum_binary(compiler, 0x03, 0x01, 0x0 << 3, 0x05,
				SLJIT_STACK_PTR_REG, 0, SLJIT_STACK_PTR_REG, 0, SLJIT_IMM, compiler->local_size));

	size = 2 + compiler->general;
	if (reg != SLJIT_PREF_RET_REG && reg != SLJIT_NO_REG)
		size += 2;
	if (compiler->args > 0)
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
	if (compiler->args > 0)
		RETN(compiler->args * sizeof(sljit_w));
	else
		RET();

	return SLJIT_NO_ERROR;
}

// ---------------------------------------------------------------------
//  Operators
// ---------------------------------------------------------------------

static int emit_do_imm(struct sljit_compiler *compiler, sljit_ub opcode, sljit_w imm)
{
	sljit_ub *buf;

	buf = ensure_buf(compiler, 1 + 1 + sizeof(sljit_w));
	TEST_MEM_ERROR(buf);
	INC_SIZE(1 + sizeof(sljit_w));
	*buf++ = opcode;
	*(sljit_w*)buf = imm;
	return SLJIT_NO_ERROR;
}

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
		if ((b & 0xf) == SLJIT_STACK_PTR_REG && (b & 0xf0) == 0)
			b |= SLJIT_STACK_PTR_REG << 4;
		else if ((b & 0xf0) == (SLJIT_STACK_PTR_REG << 4))
			b = ((b & 0xf) << 4) | SLJIT_STACK_PTR_REG | SLJIT_MEM_FLAG;

		if ((b & 0xf0) != SLJIT_NO_REG)
			total_size += 1; // SIB byte

		if ((b & 0x0f) == SLJIT_NO_REG)
			total_size += sizeof(sljit_w);
		else if (immb != 0) {
			// Immediate operand
			if (immb <= 127 && immb >= -128)
				total_size += sizeof(sljit_b);
			else
				total_size += sizeof(sljit_w);
		}
	}

	// Calculate size of a
	if (a & SLJIT_IMM)
		total_size += sizeof(sljit_w);

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

		if ((b & 0xf0) == SLJIT_NO_REG) {
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
		buf_ptr += sizeof(sljit_w);
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
		if ((b & 0xf) == SLJIT_STACK_PTR_REG && (b & 0xf0) == 0)
			b |= SLJIT_STACK_PTR_REG << 4;
		else if ((b & 0xf0) == (SLJIT_STACK_PTR_REG << 4))
			b = ((b & 0xf) << 4) | SLJIT_STACK_PTR_REG | SLJIT_MEM_FLAG;
	
		if ((b & 0xf0) != SLJIT_NO_REG)
			total_size += 1; // SIB byte

		if ((b & 0x0f) == SLJIT_NO_REG)
			total_size += sizeof(sljit_w);
		else if (immb != 0) {
			// Immediate operand
			if (immb <= 127 && immb >= -128)
				total_size += sizeof(sljit_b);
			else
				total_size += sizeof(sljit_w);
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
		buf_ptr += sizeof(sljit_w);
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
		if ((b & 0xf) == SLJIT_STACK_PTR_REG && (b & 0xf0) == 0)
			b |= SLJIT_STACK_PTR_REG << 4;
		else if ((b & 0xf0) == (SLJIT_STACK_PTR_REG << 4))
			b = ((b & 0xf) << 4) | SLJIT_STACK_PTR_REG | SLJIT_MEM_FLAG;
	
		if ((b & 0xf0) != 0)
			total_size += 1; // SIB byte

		if ((b & 0x0f) == SLJIT_NO_REG)
			total_size += sizeof(sljit_w);
		else if (immb != 0) {
			// Immediate operand
			if (immb <= 127 && immb >= -128)
				total_size += sizeof(sljit_b);
			else
				total_size += sizeof(sljit_w);
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
		buf_ptr += sizeof(sljit_w);
	}

	if (a & SLJIT_IMM && (imma != 1))
		*buf_ptr = imma;

	return buf + 1;
}

// ---------------------------------------------------------------------
//  Conditional instructions
// ---------------------------------------------------------------------

static int call_with_args(struct sljit_compiler *compiler, int type)
{
	sljit_ub *buf;

	buf = ensure_buf(compiler, type - SLJIT_CALL0 + 1);
	TEST_MEM_ERROR(buf);
	INC_SIZE(type - SLJIT_CALL0);
	if (type >= SLJIT_CALL3)
		PUSH_REG(reg_map[SLJIT_TEMPORARY_REG3]);
	if (type >= SLJIT_CALL2)
		PUSH_REG(reg_map[SLJIT_TEMPORARY_REG2]);
	PUSH_REG(reg_map[SLJIT_TEMPORARY_REG1]);
	return SLJIT_NO_ERROR;
}
