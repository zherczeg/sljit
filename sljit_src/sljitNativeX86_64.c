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

int sljit_emit_enter(struct sljit_compiler *compiler, int args, int general)
{
	int size;
	sljit_ub *buf;

	FUNCTION_ENTRY();
	// TODO: support the others
	SLJIT_ASSERT(args >= 0 && args <= SLJIT_NO_GEN_REGISTERS);
	SLJIT_ASSERT(general >= 0 && general <= SLJIT_NO_GEN_REGISTERS);
	SLJIT_ASSERT(args <= general);
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
		PUSH_REG((reg_map[SLJIT_GENERAL_REG3] & 0x7));
	}
	if (general > 1) {
		*buf++ = REX_B;
		PUSH_REG((reg_map[SLJIT_GENERAL_REG2] & 0x7));
	}
	if (general > 0)
		PUSH_REG(reg_map[SLJIT_GENERAL_REG1]);

	if (args > 0) {
		*buf++ = REX_W;
		*buf++ = 0x8b;
		*buf++ = 0xC0 | (reg_map[SLJIT_GENERAL_REG1] << 3) | 0x7;
	}
	if (args > 1) {
		*buf++ = REX_W | REX_R;
		*buf++ = 0x8b;
		*buf++ = 0xC0 | ((reg_map[SLJIT_GENERAL_REG2] & 0x7) << 3) | 0x6;
	}
	if (args > 2) {
		*buf++ = REX_W | REX_R;
		*buf++ = 0x8b;
		*buf++ = 0xC0 | ((reg_map[SLJIT_GENERAL_REG3] & 0x7) << 3) | 0x2;
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

	size = 1 + compiler->general;
	if (compiler->general >= 2)
		size += compiler->general - 1;
	if (reg != SLJIT_PREF_RET_REG && reg != SLJIT_NO_REG)
		size += 3;
	buf = ensure_buf(compiler, 1 + size);
	TEST_MEM_ERROR(buf);

	INC_SIZE(size);
	if (reg != SLJIT_PREF_RET_REG && reg != SLJIT_NO_REG) {
		*buf++ = REX_W | ((reg_map[reg] & 0x8) ? REX_B : 0);
		*buf++ = 0x8b;
		*buf++ = 0xC0 | (reg_map[SLJIT_PREF_RET_REG] << 3) | (reg_map[reg] & 0x7);
	}

	if (compiler->general > 0)
		POP_REG(reg_map[SLJIT_GENERAL_REG1]);
	if (compiler->general > 1) {
		*buf++ = REX_B;
		POP_REG(reg_map[SLJIT_GENERAL_REG2] & 0x7);
	}
	if (compiler->general > 2) {
		*buf++ = REX_B;
		POP_REG(reg_map[SLJIT_GENERAL_REG3] & 0x7);
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
	int total_size = size;

	SLJIT_ASSERT(!(a & SLJIT_IMM) || compiler->mode32 || !NOT_HALFWORD(imma));

	if (b & SLJIT_MEM_FLAG && NOT_HALFWORD(immb)) {
		if (emit_load_imm64(compiler, TMP_REG3, immb))
			return NULL;
		immb = 0;
		if ((b & 0x0f) != SLJIT_NO_REG) {
			if ((b & 0xf0) == SLJIT_NO_REG)
				b |= TMP_REG3 << 4;
			else {
				// We need to replace the upper word
				buf = ensure_buf(compiler, 1 + 4);
				TEST_MEM_ERROR2(buf);
				INC_SIZE(4);
				*buf++ = REX_W | REX_X | REX_R | ((reg_map[(b >> 4) & 0x0f] >= 8) ? REX_B : 0);
				*buf++ = 0x8d;
				*buf++ = 0x04 | (reg_lmap[TMP_REG3] << 3);
				*buf++ = 0x04 | (reg_lmap[TMP_REG3] << 3) | reg_lmap[(b >> 4) & 0x0f];
				b = (b & ~0xf0) | (TMP_REG3 << 4);
			}
		}
		else
			b |= TMP_REG3;
	}

	if (!compiler->mode32)
		rex |= REX_W;

	// Calculate size of b
	total_size += 1; // mod r/m byte
	if (b & SLJIT_MEM_FLAG) {
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

	// Calculate size of a
	if (a & SLJIT_IMM)
		total_size += sizeof(sljit_hw);
	else if (reg_map[a] >= 8)
		rex |= REX_R;

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
	if ((a & SLJIT_IMM) || (a == 0))
		*buf_ptr = 0;
	else
		*buf_ptr = reg_lmap[a] << 3;

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
	}

	if (a & SLJIT_IMM)
		*(sljit_hw*)buf_ptr = imma;

	return buf;
}

