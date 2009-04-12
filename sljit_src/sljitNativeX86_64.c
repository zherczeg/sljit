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

// x86 64-bit arch dependent functions

static sljit_ub* generate_far_jump_code(struct sljit_compiler *compiler, struct sljit_jump *jump, sljit_ub *code_ptr, int type)
{
	SLJIT_ASSERT(jump->flags & (JUMP_LABEL | JUMP_ADDR));

	if (type == SLJIT_JUMP) {
		*code_ptr++ = 0xff;
		*code_ptr++ = 0x25;
		*(sljit_hw*)code_ptr = (sljit_uw)compiler->addr_ptr - ((sljit_uw)code_ptr + sizeof(sljit_hw));
		code_ptr += sizeof(sljit_hw);
	}
	else if (type >= SLJIT_CALL0) {
		*code_ptr++ = 0xff;
		*code_ptr++ = 0x15;
		*(sljit_hw*)code_ptr = (sljit_uw)compiler->addr_ptr - ((sljit_uw)code_ptr + sizeof(sljit_hw));
		code_ptr += sizeof(sljit_hw);
	}
	else {
		// Inverted conditional
		*code_ptr++ = get_jump_code(type ^ 0x1) - 0x10;
		*code_ptr++ = 6;
		// And a jump
		*code_ptr++ = 0xff;
		*code_ptr++ = 0x25;
		*(sljit_hw*)code_ptr = (sljit_uw)compiler->addr_ptr - ((sljit_uw)code_ptr + sizeof(sljit_hw));
		code_ptr += sizeof(sljit_hw);
	}

	jump->addr = (sljit_uw)compiler->addr_ptr;
	if (jump->flags & JUMP_LABEL)
		jump->flags |= PATCH_MD;
	else
		*compiler->addr_ptr = jump->target;
	compiler->addr_ptr++;

	return code_ptr;
}

static sljit_ub* generate_fixed_jump(struct sljit_compiler *compiler, sljit_ub *code_ptr, sljit_w addr, int type)
{
	sljit_w delta = addr - ((sljit_w)code_ptr + 1 + sizeof(sljit_hw));

	if (delta <= 0x7fffffffl && delta >= -0x80000000l) {
		*code_ptr++ = (type == 2) ? 0xe8 /* call */ : 0xe9 /* jmp */;
		*(sljit_w*)code_ptr = delta;
	}
	else {
		*code_ptr++ = 0xff;
		*code_ptr++ = (type == 2) ? 0x15 /* call */ : 0x25 /* jmp */;
		*(sljit_hw*)code_ptr = (sljit_uw)compiler->addr_ptr - ((sljit_uw)code_ptr + sizeof(sljit_hw));
		code_ptr += sizeof(sljit_hw);
		*compiler->addr_ptr++ = addr;
	}

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

	sljit_emit_enter_verbose();

	compiler->general = general;

	size = general;
	if (general >= 2)
		size += general - 1;
	size += args * 3;
	if (size == 0)
		return SLJIT_NO_ERROR;
	buf = ensure_buf(compiler, 1 + size);
	TEST_MEM_ERROR(buf);

	INC_SIZE(size);
	if (general > 2) {
		*buf++ = REX_B;
		PUSH_REG(reg_lmap[SLJIT_GENERAL_REG3]);
	}
	if (general > 1) {
		*buf++ = REX_B;
		PUSH_REG(reg_lmap[SLJIT_GENERAL_REG2]);
	}
	if (general > 0)
		PUSH_REG(reg_map[SLJIT_GENERAL_REG1]);

	if (args > 0) {
		*buf++ = REX_W;
		*buf++ = 0x8b;
		*buf++ = 0xc0 | (reg_map[SLJIT_GENERAL_REG1] << 3) | 0x7;
	}
	if (args > 1) {
		*buf++ = REX_W | REX_R;
		*buf++ = 0x8b;
		*buf++ = 0xc0 | (reg_lmap[SLJIT_GENERAL_REG2] << 3) | 0x6;
	}
	if (args > 2) {
		*buf++ = REX_W | REX_R;
		*buf++ = 0x8b;
		*buf++ = 0xc0 | (reg_lmap[SLJIT_GENERAL_REG3] << 3) | 0x2;
	}

	local_size = (local_size + sizeof(sljit_uw) - 1) & ~(sizeof(sljit_uw) - 1);
	compiler->local_size = local_size;
	if (local_size > 0) {
		compiler->mode32 = 0;
		return emit_non_cum_binary(compiler, 0x2b, 0x29, 0x5 << 3, 0x2d,
			SLJIT_LOCALS_REG, 0, SLJIT_LOCALS_REG, 0, SLJIT_IMM, local_size);
	}

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

	sljit_fake_enter_verbose();

	compiler->general = general;
	compiler->local_size = (local_size + sizeof(sljit_uw) - 1) & ~(sizeof(sljit_uw) - 1);
}

int sljit_emit_return(struct sljit_compiler *compiler, int reg)
{
	int size;
	sljit_ub *buf;

	FUNCTION_ENTRY();
	SLJIT_ASSERT(reg >= 0 && reg <= SLJIT_NO_REGISTERS);
	SLJIT_ASSERT(compiler->general >= 0);

	sljit_emit_return_verbose();

	if (compiler->local_size > 0) {
		compiler->mode32 = 0;
		TEST_FAIL(emit_cum_binary(compiler, 0x03, 0x01, 0x0 << 3, 0x05,
				SLJIT_LOCALS_REG, 0, SLJIT_LOCALS_REG, 0, SLJIT_IMM, compiler->local_size));
	}

	size = 1 + compiler->general;
	if (compiler->general >= 2)
		size += compiler->general - 1;
	if (reg != SLJIT_PREF_RET_REG && reg != SLJIT_NO_REG)
		size += 3;
	buf = ensure_buf(compiler, 1 + size);
	TEST_MEM_ERROR(buf);

	INC_SIZE(size);
	if (reg != SLJIT_PREF_RET_REG && reg != SLJIT_NO_REG) {
		*buf++ = REX_W | ((reg_map[reg] >= 8) ? REX_B : 0);
		*buf++ = 0x8b;
		*buf++ = 0xc0 | (reg_lmap[SLJIT_PREF_RET_REG] << 3) | reg_lmap[reg];
	}

	if (compiler->general > 0)
		POP_REG(reg_map[SLJIT_GENERAL_REG1]);
	if (compiler->general > 1) {
		*buf++ = REX_B;
		POP_REG(reg_lmap[SLJIT_GENERAL_REG2]);
	}
	if (compiler->general > 2) {
		*buf++ = REX_B;
		POP_REG(reg_lmap[SLJIT_GENERAL_REG3]);
	}

	RET();
	return SLJIT_NO_ERROR;
}

// ---------------------------------------------------------------------
//  Operators
// ---------------------------------------------------------------------

static int emit_do_imm32(struct sljit_compiler *compiler, sljit_ub rex, sljit_ub opcode, sljit_w imm)
{
	sljit_ub *buf;

	if (rex != 0) {
		buf = ensure_buf(compiler, 1 + 2 + sizeof(sljit_hw));
		TEST_MEM_ERROR(buf);
		INC_SIZE(2 + sizeof(sljit_hw));
		*buf++ = rex;
		*buf++ = opcode;
		*(sljit_hw*)buf = imm;
	}
	else {
		buf = ensure_buf(compiler, 1 + 1 + sizeof(sljit_hw));
		TEST_MEM_ERROR(buf);
		INC_SIZE(1 + sizeof(sljit_hw));
		*buf++ = opcode;
		*(sljit_hw*)buf = imm;
	}
	return SLJIT_NO_ERROR;
}

static int emit_load_imm64(struct sljit_compiler *compiler, int reg, sljit_w imm)
{
	sljit_ub *buf;

	buf = ensure_buf(compiler, 1 + 2 + sizeof(sljit_w));
	TEST_MEM_ERROR(buf);
	INC_SIZE(2 + sizeof(sljit_w));
	*buf++ = REX_W | ((reg_map[reg] <= 7) ? 0 : REX_B);
	*buf++ = 0xb8 + (reg_map[reg] & 0x7);
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
	sljit_ub rex = 0;
	int flags = size & ~0xf;
	int total_size;

	// The immediate operand must be 32 bit
	SLJIT_ASSERT(!(a & SLJIT_IMM) || compiler->mode32 || IS_HALFWORD(imma));
	// Both cannot be switched on
	SLJIT_ASSERT((flags & (EX86_BIN_INS | EX86_SHIFT_INS)) != (EX86_BIN_INS | EX86_SHIFT_INS));
	// Size flags not allowed for typed instructions
	SLJIT_ASSERT(!(flags & (EX86_BIN_INS | EX86_SHIFT_INS)) || (flags & (EX86_BYTE_ARG | EX86_HALF_ARG)) == 0);
	// Both size flags cannot be switched on
	SLJIT_ASSERT((flags & (EX86_BYTE_ARG | EX86_HALF_ARG)) != (EX86_BYTE_ARG | EX86_HALF_ARG));

	size &= 0xf;
	total_size = size;

	if ((b & SLJIT_MEM_FLAG) && NOT_HALFWORD(immb)) {
		if (emit_load_imm64(compiler, TMP_REG3, immb))
			return NULL;
		immb = 0;
		if ((b & 0x0f) != SLJIT_NO_REG) {
			if ((b & 0xf0) == SLJIT_NO_REG)
				b |= TMP_REG3 << 4;
			else {
				// We need to replace the upper word. Rotate if it is the stack pointer
				if ((b & 0xf0) == (SLJIT_LOCALS_REG << 4))
					b = ((b & 0xf) << 4) | SLJIT_LOCALS_REG | SLJIT_MEM_FLAG;
				buf = ensure_buf(compiler, 1 + 4);
				TEST_MEM_ERROR2(buf);
				INC_SIZE(4);
				*buf++ = REX_W | REX_X | REX_R | ((reg_map[(b >> 4) & 0x0f] >= 8) ? REX_B : 0);
				*buf++ = 0x8d;
				*buf++ = 0x04 | (reg_lmap[TMP_REG3] << 3);
				*buf++ = (reg_lmap[TMP_REG3] << 3) | reg_lmap[(b >> 4) & 0x0f];
				b = (b & ~0xf0) | (TMP_REG3 << 4);
			}
		}
		else
			b |= TMP_REG3;
	}

	if (!compiler->mode32)
		rex |= REX_W;
	else if (flags & EX86_REX)
		rex |= REX;

	// Calculate size of b
	total_size += 1; // mod r/m byte
	if (b & SLJIT_MEM_FLAG) {
		if ((b & 0xf) == SLJIT_LOCALS_REG && (b & 0xf0) == 0)
			b |= SLJIT_LOCALS_REG << 4;
		else if ((b & 0xf0) == (SLJIT_LOCALS_REG << 4))
			b = ((b & 0xf) << 4) | SLJIT_LOCALS_REG | SLJIT_MEM_FLAG;

		if ((b & 0xf0) != SLJIT_NO_REG) {
			total_size += 1; // SIB byte
			if (reg_map[(b >> 4) & 0x0f] >= 8)
				rex |= REX_X;
		}

		if ((b & 0x0f) == SLJIT_NO_REG)
			total_size += 1 + sizeof(sljit_hw); // SIB byte required to avoid RIP based addressing
		else {
			if (reg_map[b & 0x0f] >= 8)
				rex |= REX_B;
			if (immb != 0) {
				// Immediate operand
				if (immb <= 127 && immb >= -128)
					total_size += sizeof(sljit_b);
				else
					total_size += sizeof(sljit_hw);
			}
		}
	}
	else if (reg_map[b] >= 8)
		rex |= REX_B;

	if (a & SLJIT_IMM) {
		if (flags & EX86_BIN_INS) {
			if (imma <= 127 && imma >= -128) {
				total_size += 1;
				flags |= EX86_BYTE_ARG;
			} else
				total_size += 4;
		}
		else if (flags & EX86_SHIFT_INS) {
			imma &= 0x1f;
			if (imma != 1) {
				total_size ++;
				flags |= EX86_BYTE_ARG;
			}
		} else if (flags & EX86_BYTE_ARG)
			total_size++;
		else if (flags & EX86_HALF_ARG)
			total_size += sizeof(short);
		else
			total_size += sizeof(sljit_hw);
	}
	else {
		SLJIT_ASSERT(!(flags & EX86_SHIFT_INS) || a == SLJIT_PREF_SHIFT_REG);
		// reg_map[SLJIT_PREF_SHIFT_REG] is less than 8
		if (reg_map[a] >= 8)
			rex |= REX_R;
	}

	if (rex)
		total_size++;

	buf = ensure_buf(compiler, 1 + total_size);
	TEST_MEM_ERROR2(buf);

	// Encoding the byte
	INC_SIZE(total_size);
	if (rex)
		*buf++ = rex;
	buf_ptr = buf + size;

	// Encode mod/rm byte
	if (!(flags & EX86_SHIFT_INS)) {
		if ((flags & EX86_BIN_INS) && (a & SLJIT_IMM))
			*buf = (flags & EX86_BYTE_ARG) ? 0x83 : 0x81;

		if ((a & SLJIT_IMM) || (a == 0))
			*buf_ptr = 0;
		else
			*buf_ptr = reg_lmap[a] << 3;
	}
	else {
		if (a & SLJIT_IMM) {
			if (imma == 1)
				*buf = 0xd1;
			else
				*buf = 0xc1;
		} else
			*buf = 0xd3;
		*buf_ptr = 0;
	}

	if (!(b & SLJIT_MEM_FLAG))
		*buf_ptr++ |= 0xc0 + reg_lmap[b];
	else if ((b & 0x0f) != SLJIT_NO_REG) {
		if (immb != 0) {
			if (immb <= 127 && immb >= -128)
				*buf_ptr |= 0x40;
			else
				*buf_ptr |= 0x80;
		}

		if ((b & 0xf0) == SLJIT_NO_REG) {
			*buf_ptr++ |= reg_lmap[b & 0x0f];
		} else {
			*buf_ptr++ |= 0x04;
			*buf_ptr++ = reg_lmap[b & 0x0f] | (reg_lmap[(b >> 4) & 0x0f] << 3);
		}

		if (immb != 0) {
			if (immb <= 127 && immb >= -128)
				*buf_ptr++ = immb; // 8 bit displacement
			else {
				*(sljit_hw*)buf_ptr = immb; // 32 bit displacement
				buf_ptr += sizeof(sljit_hw);
			}
		}
	}
	else {
		*buf_ptr++ |= 0x04;
		*buf_ptr++ = 0x25;
		*(sljit_hw*)buf_ptr = immb; // 32 bit displacement
		buf_ptr += sizeof(sljit_hw);
	}

	if (a & SLJIT_IMM) {
		if (flags & EX86_BYTE_ARG)
			*buf_ptr = imma;
		else if (flags & EX86_HALF_ARG)
			*(short*)buf_ptr = imma;
		else if (!(flags & EX86_SHIFT_INS))
			*(sljit_hw*)buf_ptr = imma;
	}

	return !(flags & EX86_SHIFT_INS) ? buf : (buf + 1);
}

// ---------------------------------------------------------------------
//  Conditional instructions
// ---------------------------------------------------------------------

static int call_with_args(struct sljit_compiler *compiler, int type)
{
	sljit_ub *buf;

	buf = ensure_buf(compiler, 1 + ((type < SLJIT_CALL3) ? 3 : 6));
	TEST_MEM_ERROR(buf);
	INC_SIZE((type < SLJIT_CALL3) ? 3 : 6);
	if (type >= SLJIT_CALL3) {
		*buf++ = REX_W;
		*buf++ = 0x8b;
		*buf++ = 0xc0 | (0x2 << 3) | reg_lmap[SLJIT_TEMPORARY_REG3];
	}
	*buf++ = REX_W;
	*buf++ = 0x8b;
	*buf++ = 0xc0 | (0x7 << 3) | reg_lmap[SLJIT_TEMPORARY_REG1];
	return SLJIT_NO_ERROR;
}
