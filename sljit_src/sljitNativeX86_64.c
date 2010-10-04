/*
 *    Stack-less Just-In-Time compiler
 *
 *    Copyright 2009-2010 Zoltan Herczeg (hzmester@freemail.hu). All rights reserved.
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

// x86 64-bit arch dependent functions

static sljit_ub* generate_far_jump_code(struct sljit_jump *jump, sljit_ub *code_ptr, int type)
{
	SLJIT_ASSERT(jump->flags & (JUMP_LABEL | JUMP_ADDR));

	if (type < SLJIT_JUMP) {
		*code_ptr++ = get_jump_code(type ^ 0x1) - 0x10;
		*code_ptr++ = 10 + 3;
	}

	SLJIT_ASSERT(reg_map[TMP_REG3] == 9);
	*code_ptr++ = REX_W | REX_B;
	*code_ptr++ = 0xb8 + 1;
	jump->addr = (sljit_uw)code_ptr;

	if (jump->flags & JUMP_LABEL)
		jump->flags |= PATCH_MD;
	else
		*(sljit_w*)code_ptr = jump->target;

	code_ptr += sizeof(sljit_w);
	*code_ptr++ = REX_B;
	*code_ptr++ = 0xff;
	*code_ptr++ = (type >= SLJIT_CALL0) ? 0xd1 /* call */ : 0xe1 /* jmp */;

	return code_ptr;
}

static sljit_ub* generate_fixed_jump(sljit_ub *code_ptr, sljit_w addr, int type)
{
	sljit_w delta = addr - ((sljit_w)code_ptr + 1 + sizeof(sljit_hw));

	if (delta <= 0x7fffffffl && delta >= -0x80000000l) {
		*code_ptr++ = (type == 2) ? 0xe8 /* call */ : 0xe9 /* jmp */;
		*(sljit_w*)code_ptr = delta;
	}
	else {
		SLJIT_ASSERT(reg_map[TMP_REG3] == 9);
		*code_ptr++ = REX_W | REX_B;
		*code_ptr++ = 0xb8 + 1;
		*(sljit_w*)code_ptr = addr;
		code_ptr += sizeof(sljit_w);
		*code_ptr++ = REX_B;
		*code_ptr++ = 0xff;
		*code_ptr++ = (type == 2) ? 0xd1 /* call */ : 0xe1 /* jmp */;
	}

	return code_ptr;
}

int sljit_emit_enter(struct sljit_compiler *compiler, int args, int temporaries, int generals, int local_size)
{
	int size;
	sljit_ub *buf;

	FUNCTION_ENTRY();
	// TODO: support the others
	SLJIT_ASSERT(args >= 0 && args <= 3);
	SLJIT_ASSERT(temporaries >= 0 && temporaries <= SLJIT_NO_TMP_REGISTERS);
	SLJIT_ASSERT(generals >= 0 && generals <= SLJIT_NO_GEN_REGISTERS);
	SLJIT_ASSERT(args <= generals);
	SLJIT_ASSERT(local_size >= 0 && local_size <= SLJIT_MAX_LOCAL_SIZE);

	sljit_emit_enter_verbose();

	compiler->temporaries = temporaries;
	compiler->generals = generals;
	compiler->flags_saved = 0;

	size = generals;
	if (generals >= 2)
		size += generals - 1;
	size += args * 3;
	if (size > 0) {
		buf = (sljit_ub*)ensure_buf(compiler, 1 + size);
		FAIL_IF(!buf);

		INC_SIZE(size);
		if (generals > 4) {
			SLJIT_ASSERT(reg_map[SLJIT_GENERAL_EREG2] >= 8);
			*buf++ = REX_B;
			PUSH_REG(reg_lmap[SLJIT_GENERAL_EREG2]);
		}
		if (generals > 3) {
			SLJIT_ASSERT(reg_map[SLJIT_GENERAL_EREG1] >= 8);
			*buf++ = REX_B;
			PUSH_REG(reg_lmap[SLJIT_GENERAL_EREG1]);
		}
		if (generals > 2) {
			SLJIT_ASSERT(reg_map[SLJIT_GENERAL_REG3] >= 8);
			*buf++ = REX_B;
			PUSH_REG(reg_lmap[SLJIT_GENERAL_REG3]);
		}
		if (generals > 1) {
			SLJIT_ASSERT(reg_map[SLJIT_GENERAL_REG2] >= 8);
			*buf++ = REX_B;
			PUSH_REG(reg_lmap[SLJIT_GENERAL_REG2]);
		}
		if (generals > 0) {
			SLJIT_ASSERT(reg_map[SLJIT_GENERAL_REG1] < 8);
			PUSH_REG(reg_map[SLJIT_GENERAL_REG1]);
		}

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
	}

	local_size = (local_size + sizeof(sljit_uw) - 1) & ~(sizeof(sljit_uw) - 1);
	compiler->local_size = local_size;
	if (local_size > 0) {
		compiler->mode32 = 0;
		return emit_non_cum_binary(compiler, 0x2b, 0x29, 0x5 << 3, 0x2d,
			SLJIT_LOCALS_REG, 0, SLJIT_LOCALS_REG, 0, SLJIT_IMM, local_size);
	}

	// Mov arguments to general registers
	return SLJIT_SUCCESS;
}

void sljit_fake_enter(struct sljit_compiler *compiler, int args, int temporaries, int generals, int local_size)
{
	SLJIT_ASSERT(args >= 0 && args <= 3);
	SLJIT_ASSERT(temporaries >= 0 && temporaries <= SLJIT_NO_TMP_REGISTERS);
	SLJIT_ASSERT(generals >= 0 && generals <= SLJIT_NO_GEN_REGISTERS);
	SLJIT_ASSERT(args <= generals);
	SLJIT_ASSERT(local_size >= 0 && local_size <= SLJIT_MAX_LOCAL_SIZE);

	sljit_fake_enter_verbose();

	compiler->temporaries = temporaries;
	compiler->generals = generals;
	compiler->local_size = (local_size + sizeof(sljit_uw) - 1) & ~(sizeof(sljit_uw) - 1);
}

int sljit_emit_return(struct sljit_compiler *compiler, int src, sljit_w srcw)
{
	int size;
	sljit_ub *buf;

	FUNCTION_ENTRY();
#ifdef SLJIT_DEBUG
	if (src != SLJIT_UNUSED) {
		FUNCTION_CHECK_SRC(src, srcw);
	}
	else
		SLJIT_ASSERT(srcw == 0);
#endif

	sljit_emit_return_verbose();

	compiler->flags_saved = 0;

	if (src != SLJIT_PREF_RET_REG && src != SLJIT_UNUSED)
		emit_mov(compiler, SLJIT_PREF_RET_REG, 0, src, srcw);

	if (compiler->local_size > 0) {
		compiler->mode32 = 0;
		FAIL_IF(emit_cum_binary(compiler, 0x03, 0x01, 0x0 << 3, 0x05,
				SLJIT_LOCALS_REG, 0, SLJIT_LOCALS_REG, 0, SLJIT_IMM, compiler->local_size));
	}

	size = 1 + compiler->generals;
	if (compiler->generals >= 2)
		size += compiler->generals - 1;
	buf = (sljit_ub*)ensure_buf(compiler, 1 + size);
	FAIL_IF(!buf);

	INC_SIZE(size);

	if (compiler->generals > 0)
		POP_REG(reg_map[SLJIT_GENERAL_REG1]);
	if (compiler->generals > 1) {
		*buf++ = REX_B;
		POP_REG(reg_lmap[SLJIT_GENERAL_REG2]);
	}
	if (compiler->generals > 2) {
		*buf++ = REX_B;
		POP_REG(reg_lmap[SLJIT_GENERAL_REG3]);
	}
	if (compiler->generals > 3) {
		*buf++ = REX_B;
		POP_REG(reg_lmap[SLJIT_GENERAL_EREG1]);
	}
	if (compiler->generals > 4) {
		*buf++ = REX_B;
		POP_REG(reg_lmap[SLJIT_GENERAL_EREG2]);
	}

	RET();
	return SLJIT_SUCCESS;
}

// ---------------------------------------------------------------------
//  Operators
// ---------------------------------------------------------------------

static int emit_do_imm32(struct sljit_compiler *compiler, sljit_ub rex, sljit_ub opcode, sljit_w imm)
{
	sljit_ub *buf;

	if (rex != 0) {
		buf = (sljit_ub*)ensure_buf(compiler, 1 + 2 + sizeof(sljit_hw));
		FAIL_IF(!buf);
		INC_SIZE(2 + sizeof(sljit_hw));
		*buf++ = rex;
		*buf++ = opcode;
		*(sljit_hw*)buf = imm;
	}
	else {
		buf = (sljit_ub*)ensure_buf(compiler, 1 + 1 + sizeof(sljit_hw));
		FAIL_IF(!buf);
		INC_SIZE(1 + sizeof(sljit_hw));
		*buf++ = opcode;
		*(sljit_hw*)buf = imm;
	}
	return SLJIT_SUCCESS;
}

static int emit_load_imm64(struct sljit_compiler *compiler, int reg, sljit_w imm)
{
	sljit_ub *buf;

	buf = (sljit_ub*)ensure_buf(compiler, 1 + 2 + sizeof(sljit_w));
	FAIL_IF(!buf);
	INC_SIZE(2 + sizeof(sljit_w));
	*buf++ = REX_W | ((reg_map[reg] <= 7) ? 0 : REX_B);
	*buf++ = 0xb8 + (reg_map[reg] & 0x7);
	*(sljit_w*)buf = imm;
	return SLJIT_SUCCESS;
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
#ifdef SLJIT_SSE2
	// SSE2 and immediate is not possible
	SLJIT_ASSERT(!(a & SLJIT_IMM) || !(flags & EX86_SSE2));
#endif

	size &= 0xf;
	total_size = size;

	if ((b & SLJIT_MEM) && !(b & 0xf0) && NOT_HALFWORD(immb)) {
		if (emit_load_imm64(compiler, TMP_REG3, immb))
			return NULL;
		immb = 0;
		if (b & 0xf)
			b |= TMP_REG3 << 4;
		else
			b |= TMP_REG3;
	}

	if (!compiler->mode32 && !(flags & EX86_NO_REXW))
		rex |= REX_W;
	else if (flags & EX86_REX)
		rex |= REX;

#ifdef SLJIT_SSE2
	if (flags & EX86_PREF_F2)
		total_size++;
#endif
	if (flags & EX86_PREF_66)
		total_size++;

	// Calculate size of b
	total_size += 1; // mod r/m byte
	if (b & SLJIT_MEM) {
		if ((b & 0x0f) == SLJIT_UNUSED)
			total_size += 1 + sizeof(sljit_hw); // SIB byte required to avoid RIP based addressing
		else {
			if (reg_map[b & 0x0f] >= 8)
				rex |= REX_B;
			if (immb != 0 && !(b & 0xf0)) {
				// Immediate operand
				if (immb <= 127 && immb >= -128)
					total_size += sizeof(sljit_b);
				else
					total_size += sizeof(sljit_hw);
			}
		}

		if ((b & 0xf) == SLJIT_LOCALS_REG && (b & 0xf0) == 0)
			b |= SLJIT_LOCALS_REG << 4;

		if ((b & 0xf0) != SLJIT_UNUSED) {
			total_size += 1; // SIB byte
			if (reg_map[(b >> 4) & 0x0f] >= 8)
				rex |= REX_X;
		}
	}
#ifdef SLJIT_SSE2
	else if (!(flags & EX86_SSE2) && reg_map[b] >= 8)
		rex |= REX_B;
#else
	else if (reg_map[b] >= 8)
		rex |= REX_B;
#endif

	if (a & SLJIT_IMM) {
		if (flags & EX86_BIN_INS) {
			if (imma <= 127 && imma >= -128) {
				total_size += 1;
				flags |= EX86_BYTE_ARG;
			} else
				total_size += 4;
		}
		else if (flags & EX86_SHIFT_INS) {
			imma &= 0x3f;
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
#ifdef SLJIT_SSE2
		if (!(flags & EX86_SSE2) && reg_map[a] >= 8)
			rex |= REX_R;
#else
		if (reg_map[a] >= 8)
			rex |= REX_R;
#endif
	}

	if (rex)
		total_size++;

	buf = (sljit_ub*)ensure_buf(compiler, 1 + total_size);
	PTR_FAIL_IF(!buf);

	// Encoding the byte
	INC_SIZE(total_size);
#ifdef SLJIT_SSE2
	if (flags & EX86_PREF_F2)
		*buf++ = 0xf2;
#endif
	if (flags & EX86_PREF_66)
		*buf++ = 0x66;
	if (rex)
		*buf++ = rex;
	buf_ptr = buf + size;

	// Encode mod/rm byte
	if (!(flags & EX86_SHIFT_INS)) {
		if ((flags & EX86_BIN_INS) && (a & SLJIT_IMM))
			*buf = (flags & EX86_BYTE_ARG) ? 0x83 : 0x81;

		if ((a & SLJIT_IMM) || (a == 0))
			*buf_ptr = 0;
#ifdef SLJIT_SSE2
		else if (!(flags & EX86_SSE2))
			*buf_ptr = reg_lmap[a] << 3;
		else
			*buf_ptr = a << 3;
#else
		else
			*buf_ptr = reg_lmap[a] << 3;
#endif
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

	if (!(b & SLJIT_MEM))
#ifdef SLJIT_SSE2
		*buf_ptr++ |= 0xc0 + ((!(flags & EX86_SSE2)) ? reg_lmap[b] : b);
#else
		*buf_ptr++ |= 0xc0 + reg_lmap[b];
#endif
	else if ((b & 0x0f) != SLJIT_UNUSED) {
		if ((b & 0xf0) == SLJIT_UNUSED || (b & 0xf0) == (SLJIT_LOCALS_REG << 4)) {
			if (immb != 0) {
				if (immb <= 127 && immb >= -128)
					*buf_ptr |= 0x40;
				else
					*buf_ptr |= 0x80;
			}

			if ((b & 0xf0) == SLJIT_UNUSED)
				*buf_ptr++ |= reg_lmap[b & 0x0f];
			else {
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
			*buf_ptr++ = reg_lmap[b & 0x0f] | (reg_lmap[(b >> 4) & 0x0f] << 3) | (immb << 6);
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

	SLJIT_ASSERT(reg_map[SLJIT_TEMPORARY_REG2] == 6 && reg_map[SLJIT_TEMPORARY_REG1] < 8 && reg_map[SLJIT_TEMPORARY_REG3] < 8);

	buf = (sljit_ub*)ensure_buf(compiler, 1 + ((type < SLJIT_CALL3) ? 3 : 6));
	FAIL_IF(!buf);
	INC_SIZE((type < SLJIT_CALL3) ? 3 : 6);
	if (type >= SLJIT_CALL3) {
		*buf++ = REX_W;
		*buf++ = 0x8b;
		*buf++ = 0xc0 | (0x2 << 3) | reg_lmap[SLJIT_TEMPORARY_REG3];
	}
	*buf++ = REX_W;
	*buf++ = 0x8b;
	*buf++ = 0xc0 | (0x7 << 3) | reg_lmap[SLJIT_TEMPORARY_REG1];
	return SLJIT_SUCCESS;
}

// ---------------------------------------------------------------------
//  Extend input
// ---------------------------------------------------------------------

static int emit_mov_int(struct sljit_compiler *compiler, int sign,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
	sljit_ub* code;
	int dst_r;

	compiler->mode32 = 0;

	if (dst == SLJIT_UNUSED && !(src & SLJIT_MEM))
		return SLJIT_SUCCESS; // Empty instruction

	if (src & SLJIT_IMM) {
		if (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_NO_REGISTERS)
			return emit_load_imm64(compiler, dst, srcw);
		compiler->mode32 = 1;
		code = emit_x86_instruction(compiler, 1, SLJIT_IMM, (sljit_w)(int)srcw, dst, dstw);
		FAIL_IF(!code);
		*code = 0xc7;
		compiler->mode32 = 0;
		return SLJIT_SUCCESS;
	}

	dst_r = (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_GENERAL_REG3) ? dst : TMP_REGISTER;

	if ((dst & SLJIT_MEM) && (src >= SLJIT_TEMPORARY_REG1 && src <= SLJIT_GENERAL_REG3))
		dst_r = src;
	else {
		if (sign) {
			code = emit_x86_instruction(compiler, 1, dst_r, 0, src, srcw);
			FAIL_IF(!code);
			*code++ = 0x63;
		}
		else {
			if (dst_r == src) {
				compiler->mode32 = 1;
				code = emit_x86_instruction(compiler, 1, TMP_REGISTER, 0, src, 0);
				FAIL_IF(!code);
				*code++ = 0x8b;
				compiler->mode32 = 0;
			}
			// xor reg, reg
			code = emit_x86_instruction(compiler, 1, dst_r, 0, dst_r, 0);
			FAIL_IF(!code);
			*code++ = 0x33;
			if (dst_r != src) {
				compiler->mode32 = 1;
				code = emit_x86_instruction(compiler, 1, dst_r, 0, src, srcw);
				FAIL_IF(!code);
				*code++ = 0x8b;
				compiler->mode32 = 0;
			}
			else {
				compiler->mode32 = 1;
				code = emit_x86_instruction(compiler, 1, src, 0, TMP_REGISTER, 0);
				FAIL_IF(!code);
				*code++ = 0x8b;
				compiler->mode32 = 0;
			}
		}
	}

	if (dst & SLJIT_MEM) {
		compiler->mode32 = 1;
		code = emit_x86_instruction(compiler, 1, dst_r, 0, dst, dstw);
		FAIL_IF(!code);
		*code = 0x89;
		compiler->mode32 = 0;
	}

	return SLJIT_SUCCESS;
}
