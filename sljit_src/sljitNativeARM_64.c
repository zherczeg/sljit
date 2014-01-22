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
	return "ARM-64" SLJIT_CPUINFO;
}

/* Length of an instruction word */
typedef sljit_ui sljit_ins;

#define TMP_REG1	(SLJIT_NO_REGISTERS + 1)
#define TMP_REG2	(SLJIT_NO_REGISTERS + 2)
#define TMP_REG3	(SLJIT_NO_REGISTERS + 3)
#define TMP_REG4	(SLJIT_NO_REGISTERS + 4)
#define TMP_LR		(SLJIT_NO_REGISTERS + 5)
#define TMP_ZERO	(SLJIT_NO_REGISTERS + 6)

static SLJIT_CONST sljit_ub reg_map[SLJIT_NO_REGISTERS + 7] = {
  0, 0, 1, 2, 3, 4, 19, 20, 21, 22, 23, 31, 9, 10, 11, 12, 30, 31
};

#define W_OP (1 << 31)
#define RD(rd) (reg_map[rd])
#define RT(rt) (reg_map[rt])
#define RN(rn) (reg_map[rn] << 5)
#define RT2(rt2) (reg_map[rt2] << 10)
#define RM(rm) (reg_map[rm] << 16)

/* --------------------------------------------------------------------- */
/*  Instrucion forms                                                     */
/* --------------------------------------------------------------------- */

#define ADC 0x9a000000
#define ADD 0x8b000000
#define ADDI 0x91000000
#define AND 0x8a000000
#define ANDI 0x92000000
#define ASRV 0x9ac02800
#define BRK 0xd4200000
#define CLZ 0xdac01000
#define EOR 0xca000000
#define EORI 0xd2000000
#define LDRI 0xf9400000
#define LDP 0xa9400000
#define LDP_PST 0xa8c00000
#define LSLV 0x9ac02000
#define LSRV 0x9ac02400
#define MADD 0x9b000000
#define MOVK 0xf2800000
#define MOVN 0x92800000
#define MOVZ 0xd2800000
#define NOP 0xd503201f
#define ORN 0xaa200000
#define ORR 0xaa000000
#define ORRI 0xb2000000
#define RET 0xd65f0000
#define SBC 0xda000000
#define SBFM 0x93000000
#define SMADDL 0x9b200000
#define SMULH 0x9b400000
#define STP 0xa9000000
#define STRI 0xf9000000
#define STP_PRE 0xa9800000
#define SUB 0xcb000000
#define SUBI 0xd1000000
#define SUBS 0xeb000000
#define UBFM 0xd3000000

/* dest_reg is the absolute name of the register
   Useful for reordering instructions in the delay slot. */
static sljit_si push_inst(struct sljit_compiler *compiler, sljit_ins ins)
{
	sljit_ins *ptr = (sljit_ins*)ensure_buf(compiler, sizeof(sljit_ins));
	FAIL_IF(!ptr);
	*ptr = ins;
	compiler->size++;
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
			/* These structures are ordered by their address. */
			SLJIT_ASSERT(!label || label->size >= word_count);
			SLJIT_ASSERT(!jump || jump->addr >= word_count);
			SLJIT_ASSERT(!const_ || const_->addr >= word_count);
			if (label && label->size == word_count) {
				label->addr = (sljit_uw)code_ptr;
				label->size = code_ptr - code;
				label = label->next;
			}
			if (jump && jump->addr == word_count) {
					// jump->addr = (sljit_uw)code_ptr - ((jump->flags & IS_COND) ? 10 : 8);
					// code_ptr -= detect_jump_type(jump, code_ptr, code);
					jump = jump->next;
			}
			if (const_ && const_->addr == word_count) {
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
	SLJIT_ASSERT(code_ptr - code <= (sljit_sw)compiler->size);

	jump = compiler->jumps;
	while (jump) {
		/* Update jumps. */
		jump = jump->next;
	}

	compiler->error = SLJIT_ERR_COMPILED;
	compiler->executable_size = (code_ptr - code) * sizeof(sljit_ins);
	/* SLJIT_CACHE_FLUSH(code, code_ptr); */
	return code;
}

/* --------------------------------------------------------------------- */
/*  Core code generator functions.                                       */
/* --------------------------------------------------------------------- */

#define COUNT_TRAILING_ZERO(value, result) \
	result = 0; \
	if (!(value & 0xffffffff)) { \
		result += 32; \
		value >>= 32; \
	} \
	if (!(value & 0xffff)) { \
		result += 16; \
		value >>= 16; \
	} \
	if (!(value & 0xff)) { \
		result += 8; \
		value >>= 8; \
	} \
	if (!(value & 0xf)) { \
		result += 4; \
		value >>= 4; \
	} \
	if (!(value & 0x3)) { \
		result += 2; \
		value >>= 2; \
	} \
	if (!(value & 0x1)) { \
		result += 1; \
		value >>= 1; \
	}

#define LOGICAL_IMM_CHECK 0x100

static sljit_ins logical_imm(sljit_sw imm, sljit_si len)
{
	sljit_si negated, ones, right;
	sljit_uw mask, uimm;
	sljit_ins ins;

	if (len & LOGICAL_IMM_CHECK) {
		len &= ~LOGICAL_IMM_CHECK;
		if (len == 32 && (imm == 0 || imm == -1))
			return 0;
		if (len == 16 && ((sljit_si)imm == 0 || (sljit_si)imm == -1))
			return 0;
	}

	SLJIT_ASSERT((len == 32 && imm != 0 && imm != -1)
		|| (len == 16 && (sljit_si)imm != 0 && (sljit_si)imm != -1));
	uimm = (sljit_uw)imm;
	while (1) {
		if (len <= 0) {
			SLJIT_ASSERT_STOP();
			return 0;
		}
		mask = ((sljit_uw)1 << len) - 1;
		if ((uimm & mask) != ((uimm >> len) & mask))
			break;
		len >>= 1;
	}

	len <<= 1;

	negated = 0;
	if (uimm & 0x1) {
		negated = 1;
		uimm = ~uimm;
	}

	if (len < 64)
		uimm &= ((sljit_uw)1 << len) - 1;

	/* Unsigned right shift. */
	COUNT_TRAILING_ZERO(uimm, right);

	/* Signed shift. We also know that the highest bit is set. */
	imm = (sljit_sw)~uimm;
	SLJIT_ASSERT(imm < 0);

	COUNT_TRAILING_ZERO(imm, ones);

	if (~imm)
		return 0;

	if (len == 64)
		ins = 1 << 22;
	else
		ins = (0x3f - ((len << 1) - 1)) << 10;

	if (negated)
		return ins | ((len - ones - 1) << 10) | ((len - ones - right) << 16);

	return ins | ((ones - 1) << 10) | ((len - right) << 16);
}

#undef COUNT_TRAILING_ZERO

static sljit_si load_immediate(struct sljit_compiler *compiler, sljit_si dst, sljit_sw simm)
{
	sljit_uw imm = (sljit_uw)simm;
	sljit_si i, zeros, ones, first;
	sljit_ins bitmask;

	if (imm <= 0xffff)
		return push_inst(compiler, MOVZ | (imm << 5) | RD(dst));

	if (simm >= -0x10000 && simm < 0)
		return push_inst(compiler, MOVN | ((~imm & 0xffff) << 5) | RD(dst));

	if (imm <= 0xffffffffl) {
		if ((imm & 0xffff0000l) == 0xffff0000)
			return push_inst(compiler, (MOVN ^ W_OP) | ((~imm & 0xffff) << 5) | RD(dst));
		if ((imm & 0xffff) == 0xffff)
			return push_inst(compiler, (MOVN ^ W_OP) | (1 << 21) | ((~imm & 0xffff0000l) >> (16 - 5)) | RD(dst));
		bitmask = logical_imm(simm, 16);
		if (bitmask != 0)
			return push_inst(compiler, (ORRI ^ W_OP) | bitmask | RN(TMP_ZERO) | RD(dst));
	}
	else {
		bitmask = logical_imm(simm, 32);
		if (bitmask != 0)
			return push_inst(compiler, ORRI | bitmask | RN(TMP_ZERO) | RD(dst));
	}

	if (imm <= 0xffffffffl) {
		FAIL_IF(push_inst(compiler, MOVZ | ((imm & 0xffff) << 5) | RD(dst)));
		return push_inst(compiler, MOVK | (1 << 21) | ((imm & 0xffff0000l) >> (16 - 5)) | RD(dst));
	}

	if (simm >= -0x100000000l && simm < 0) {
		FAIL_IF(push_inst(compiler, MOVN | ((~imm & 0xffff) << 5) | RD(dst)));
		return push_inst(compiler, MOVK | (1 << 21) | ((imm & 0xffff0000l) >> (16 - 5)) | RD(dst));
	}

	/* A large amount of number can be constructed from ORR and MOVx,
	but computing them is costly. We don't  */

	zeros = 0;
	ones = 0;
	for (i = 4; i > 0; i--) {
		if ((simm & 0xffff) == 0)
			zeros++;
		if ((simm & 0xffff) == 0xffff)
			ones++;
		simm >>= 16;
	}

	simm = (sljit_sw)imm;
	first = 1;
	if (ones > zeros) {
		simm = ~simm;
		for (i = 0; i < 4; i++) {
			if (!(simm & 0xffff)) {
				simm >>= 16;
				continue;
			}
			if (first) {
				first = 0;
				FAIL_IF(push_inst(compiler, MOVN | (i << 21) | ((simm & 0xffff) << 5) | RD(dst)));
			}
			else
				FAIL_IF(push_inst(compiler, MOVK | (i << 21) | ((~simm & 0xffff) << 5) | RD(dst)));
			simm >>= 16;
		}
		return SLJIT_SUCCESS;
	}

	for (i = 0; i < 4; i++) {
		if (!(simm & 0xffff)) {
			simm >>= 16;
			continue;
		}
		if (first) {
			first = 0;
			FAIL_IF(push_inst(compiler, MOVZ | (i << 21) | ((simm & 0xffff) << 5) | RD(dst)));
		}
		else
			FAIL_IF(push_inst(compiler, MOVK | (i << 21) | ((simm & 0xffff) << 5) | RD(dst)));
		simm >>= 16;
	}
	return SLJIT_SUCCESS;
}

#define ARG1_IMM	0x0010000
#define ARG2_IMM	0x0020000
#define WORD_OP		0x0040000
#define SET_FLAGS	0x0080000
#define UNUSED_RETURN	0x0100000
#define SLOW_DEST	0x0200000
#define SLOW_SRC1	0x0400000
#define SLOW_SRC2	0x0800000

static sljit_si emit_op_imm(struct sljit_compiler *compiler, sljit_si flags, sljit_si dst, sljit_sw arg1, sljit_sw arg2)
{
	/* dst must be register, TMP_REG1
	   arg1 must be register, TMP_REG1, imm
	   arg2 must be register, TMP_REG2, imm */
	sljit_ins inv_bits = (flags & WORD_OP) ? (1 << 31) : 0;
	sljit_ins inst_bits;
	sljit_si op = (flags & 0xffff);
	sljit_si reg;
	sljit_sw imm, nimm;

	if (SLJIT_UNLIKELY((flags & (ARG1_IMM | ARG2_IMM)) == (ARG1_IMM | ARG2_IMM))) {
		/* Both are immediates. */
		flags &= ~ARG1_IMM;
		if (arg1 == 0 && op != SLJIT_ADD && op != SLJIT_SUB)
			arg1 = TMP_ZERO;
		else {
			FAIL_IF(load_immediate(compiler, TMP_REG1, arg1));
			arg1 = TMP_REG1;
		}
	}

	if (flags & (ARG1_IMM | ARG2_IMM)) {
		reg = (flags & ARG2_IMM) ? arg1 : arg2;
		imm = (flags & ARG2_IMM) ? arg2 : arg1;

		switch (op) {
		case SLJIT_MUL:
		case SLJIT_NEG:
		case SLJIT_CLZ:
		case SLJIT_ADDC:
		case SLJIT_SUBC:
			/* No form with immediate operand (except imm 0, which
			is represented by a ZERO register). */
			break;
		case SLJIT_MOV:
			SLJIT_ASSERT(!(flags & SET_FLAGS) && (flags & ARG2_IMM) && arg1 == TMP_REG1);
			return load_immediate(compiler, dst, imm);
		case SLJIT_NOT:
			SLJIT_ASSERT(flags & ARG2_IMM);
			FAIL_IF(load_immediate(compiler, dst, (flags & WORD_OP) ? (~imm & 0xffffffff) : ~imm));
			goto set_flags;
		case SLJIT_SUB:
			if (flags & ARG1_IMM)
				break;
			imm = -imm;
			/* Fall through. */
		case SLJIT_ADD:
			if (imm == 0) {
				if (flags & SET_FLAGS)
					inv_bits |= 1 << 29;
				return push_inst(compiler, ((op == SLJIT_ADD ? ADDI : SUBI) ^ inv_bits) | RD(dst) | RN(reg) | (imm << 10));
			}
			if (imm > 0 && imm <= 0xfff) {
				if (flags & SET_FLAGS)
					inv_bits |= 1 << 29;
				return push_inst(compiler, (ADDI ^ inv_bits) | RD(dst) | RN(reg) | (imm << 10));
			}
			nimm = -imm;
			if (nimm > 0 && nimm <= 0xfff) {
				if (flags & SET_FLAGS)
					inv_bits |= 1 << 29;
				return push_inst(compiler, (SUBI ^ inv_bits) | RD(dst) | RN(reg) | (nimm << 10));
			}
			if (imm > 0 && imm <= 0xffffff && !(imm & 0xfff)) {
				if (flags & SET_FLAGS)
					inv_bits |= 1 << 29;
				return push_inst(compiler, (ADDI ^ inv_bits) | RD(dst) | RN(reg) | (1 << 22) | ((imm >> 12) << 10));
			}
			if (nimm > 0 && nimm <= 0xffffff && !(nimm & 0xfff)) {
				if (flags & SET_FLAGS)
					inv_bits |= 1 << 29;
				return push_inst(compiler, (SUBI ^ inv_bits) | RD(dst) | RN(reg) | (1 << 22) | ((nimm >> 12) << 10));
			}
			if (imm > 0 && imm <= 0xffffff && !(flags & SET_FLAGS)) {
				FAIL_IF(push_inst(compiler, (ADDI ^ inv_bits) | RD(dst) | RN(reg) | (1 << 22) | ((imm >> 12) << 10)));
				return push_inst(compiler, (ADDI ^ inv_bits) | RD(dst) | RN(dst) | ((imm & 0xfff) << 10));
			}
			if (nimm > 0 && nimm <= 0xffffff && !(flags & SET_FLAGS)) {
				FAIL_IF(push_inst(compiler, (SUBI ^ inv_bits) | RD(dst) | RN(reg) | ((nimm >> 12) << 10)));
				return push_inst(compiler, (SUBI ^ inv_bits) | RD(dst) | RN(dst) | ((nimm & 0xfff) << 10));
			}
			break;
		case SLJIT_AND:
			inst_bits = logical_imm(imm, LOGICAL_IMM_CHECK | ((flags & WORD_OP) ? 16 : 32));
			if (!inst_bits)
				break;
			if (flags & SET_FLAGS)
				inv_bits |= 3 << 29;
			return push_inst(compiler, (ANDI ^ inv_bits) | RD(dst) | RN(reg) | inst_bits);
		case SLJIT_OR:
		case SLJIT_XOR:
			inst_bits = logical_imm(imm, LOGICAL_IMM_CHECK | ((flags & WORD_OP) ? 16 : 32));
			if (!inst_bits)
				break;
			if (op == SLJIT_OR)
				inst_bits |= ORRI;
			else
				inst_bits |= EORI;
			FAIL_IF(push_inst(compiler, (inst_bits ^ inv_bits) | RD(dst) | RN(reg)));
			goto set_flags;
		case SLJIT_SHL:
			if (flags & ARG1_IMM)
				break;
			if (flags & WORD_OP) {
				imm &= 0x1f;
				FAIL_IF(push_inst(compiler, (UBFM ^ inv_bits) | RD(dst) | RN(arg1) | ((-imm & 0x1f) << 16) | ((31 - imm) << 10)));
			}
			else {
				imm &= 0x3f;
				FAIL_IF(push_inst(compiler, (UBFM ^ inv_bits) | RD(dst) | RN(arg1) | (1 << 22) | ((-imm & 0x3f) << 16) | ((63 - imm) << 10)));
			}
			goto set_flags;
		case SLJIT_LSHR:
		case SLJIT_ASHR:
			if (flags & ARG1_IMM)
				break;
			if (op == SLJIT_ASHR)
				inv_bits |= 1 << 30;
			if (flags & WORD_OP) {
				imm &= 0x1f;
				FAIL_IF(push_inst(compiler, (UBFM ^ inv_bits) | RD(dst) | RN(arg1) | (imm << 16) | (31 << 10)));
			}
			else {
				imm &= 0x3f;
				FAIL_IF(push_inst(compiler, (UBFM ^ inv_bits) | RD(dst) | RN(arg1) | (1 << 22) | (imm << 16) | (63 << 10)));
			}
			goto set_flags;
		default:
			SLJIT_ASSERT_STOP();
			break;
		}

		if (flags & ARG2_IMM) {
			if (arg2 == 0)
				arg2 = TMP_ZERO;
			else {
				FAIL_IF(load_immediate(compiler, TMP_REG2, arg2));
				arg2 = TMP_REG2;
			}
		}
		else {
			if (arg1 == 0)
				arg1 = TMP_ZERO;
			else {
				FAIL_IF(load_immediate(compiler, TMP_REG1, arg1));
				arg1 = TMP_REG1;
			}
		}
	}

	/* Both arguments are registers. */
	switch (op) {
	case SLJIT_MOV:
	case SLJIT_MOV_P:
	case SLJIT_MOVU:
	case SLJIT_MOVU_P:
		SLJIT_ASSERT(!(flags & SET_FLAGS) && arg1 == TMP_REG1);
		if (dst == arg2)
			return SLJIT_SUCCESS;
		return push_inst(compiler, ORR | RD(dst) | RN(TMP_ZERO) | RM(arg2));
	case SLJIT_MOV_UB:
	case SLJIT_MOVU_UB:
		SLJIT_ASSERT(!(flags & SET_FLAGS) && arg1 == TMP_REG1);
		return push_inst(compiler, (UBFM ^ (1 << 31)) | RD(dst) | RN(arg2) | (7 << 10));
	case SLJIT_MOV_SB:
	case SLJIT_MOVU_SB:
		SLJIT_ASSERT(!(flags & SET_FLAGS) && arg1 == TMP_REG1);
		if (!(flags & WORD_OP))
			inv_bits |= 1 << 22;
		return push_inst(compiler, (SBFM ^ inv_bits) | RD(dst) | RN(arg2) | (7 << 10));
	case SLJIT_MOV_UH:
	case SLJIT_MOVU_UH:
		SLJIT_ASSERT(!(flags & SET_FLAGS) && arg1 == TMP_REG1);
		return push_inst(compiler, (UBFM ^ (1 << 31)) | RD(dst) | RN(arg2) | (15 << 10));
	case SLJIT_MOV_SH:
	case SLJIT_MOVU_SH:
		SLJIT_ASSERT(!(flags & SET_FLAGS) && arg1 == TMP_REG1);
		if (!(flags & WORD_OP))
			inv_bits |= 1 << 22;
		return push_inst(compiler, (SBFM ^ inv_bits) | RD(dst) | RN(arg2) | (15 << 10));
	case SLJIT_MOV_UI:
	case SLJIT_MOVU_UI:
		SLJIT_ASSERT(!(flags & SET_FLAGS) && arg1 == TMP_REG1);
		if ((flags & WORD_OP) && dst == arg2)
			return SLJIT_SUCCESS;
		return push_inst(compiler, (ORR ^ (1 << 31)) | RD(dst) | RN(TMP_ZERO) | RM(arg2));
	case SLJIT_MOV_SI:
	case SLJIT_MOVU_SI:
		SLJIT_ASSERT(!(flags & SET_FLAGS) && arg1 == TMP_REG1);
		if ((flags & WORD_OP) && dst == arg2)
			return SLJIT_SUCCESS;
		return push_inst(compiler, SBFM | (1 << 22) | RD(dst) | RN(arg2) | (31 << 10));
	case SLJIT_NOT:
		SLJIT_ASSERT(arg1 == TMP_REG1);
		FAIL_IF(push_inst(compiler, (ORN ^ inv_bits) | RD(dst) | RN(TMP_ZERO) | RM(arg2)));
		goto set_flags;
	case SLJIT_NEG:
		SLJIT_ASSERT(arg1 == TMP_REG1);
		if (flags & SET_FLAGS)
			inv_bits |= 1 << 29;
		return push_inst(compiler, (SUB ^ inv_bits) | RD(dst) | RN(TMP_ZERO) | RM(arg2));
	case SLJIT_CLZ:
		SLJIT_ASSERT(arg1 == TMP_REG1);
		FAIL_IF(push_inst(compiler, (CLZ ^ inv_bits) | RD(dst) | RN(arg2)));
		goto set_flags;
	case SLJIT_ADD:
		if (flags & SET_FLAGS)
			inv_bits |= 1 << 29;
		return push_inst(compiler, (ADD ^ inv_bits) | RD(dst) | RN(arg1) | RM(arg2));
	case SLJIT_ADDC:
		if (flags & SET_FLAGS)
			inv_bits |= 1 << 29;
		return push_inst(compiler, (ADC ^ inv_bits) | RD(dst) | RN(arg1) | RM(arg2));
	case SLJIT_SUB:
		if (flags & SET_FLAGS)
			inv_bits |= 1 << 29;
		return push_inst(compiler, (SUB ^ inv_bits) | RD(dst) | RN(arg1) | RM(arg2));
	case SLJIT_SUBC:
		if (flags & SET_FLAGS)
			inv_bits |= 1 << 29;
		return push_inst(compiler, (SBC ^ inv_bits) | RD(dst) | RN(arg1) | RM(arg2));
	case SLJIT_MUL:
		if (!(flags & SET_FLAGS))
			return push_inst(compiler, (MADD ^ inv_bits) | RD(dst) | RN(arg1) | RM(arg2) | (31 << 10));
		if (flags & WORD_OP) {
			FAIL_IF(push_inst(compiler, SMADDL | RD(dst) | RN(arg1) | RM(arg2) | (31 << 10)));
			FAIL_IF(push_inst(compiler, SUBS | RD(TMP_REG4) | RN(TMP_ZERO) | RM(dst) | (2 << 22) | (31 << 10)));
			return push_inst(compiler, SUBS | RD(TMP_ZERO) | RN(TMP_REG4) | RM(dst) | (2 << 22) | (63 << 10));
		}
		FAIL_IF(push_inst(compiler, SMULH | RD(TMP_REG4) | RN(arg1) | RM(arg2) | (31 << 10)));
		FAIL_IF(push_inst(compiler, MADD | RD(dst) | RN(arg1) | RM(arg2) | (31 << 10)));
		return push_inst(compiler, SUBS | RD(TMP_ZERO) | RN(TMP_REG4) | RM(dst) | (2 << 22) | (63 << 10));
	case SLJIT_AND:
		if (flags & SET_FLAGS)
			inv_bits |= 3 << 29;
		return push_inst(compiler, (AND ^ inv_bits) | RD(dst) | RN(arg1) | RM(arg2));
	case SLJIT_OR:
		FAIL_IF(push_inst(compiler, (ORR ^ inv_bits) | RD(dst) | RN(arg1) | RM(arg2)));
		goto set_flags;
	case SLJIT_XOR:
		FAIL_IF(push_inst(compiler, (EOR ^ inv_bits) | RD(dst) | RN(arg1) | RM(arg2)));
		goto set_flags;
	case SLJIT_SHL:
		FAIL_IF(push_inst(compiler, (LSLV ^ inv_bits) | RD(dst) | RN(arg1) | RM(arg2)));
		goto set_flags;
	case SLJIT_LSHR:
		FAIL_IF(push_inst(compiler, (LSRV ^ inv_bits) | RD(dst) | RN(arg1) | RM(arg2)));
		goto set_flags;
	case SLJIT_ASHR:
		FAIL_IF(push_inst(compiler, (ASRV ^ inv_bits) | RD(dst) | RN(arg1) | RM(arg2)));
		goto set_flags;
	}

	SLJIT_ASSERT_STOP();
	return SLJIT_SUCCESS;

set_flags:
	if (flags & SET_FLAGS)
		return push_inst(compiler, (SUBS ^ inv_bits) | RD(TMP_ZERO) | RN(dst) | RM(TMP_ZERO));
	return SLJIT_SUCCESS;
}

#define STORE		0x01
#define SIGNED		0x02
#define UPDATE		0x04
#define ARG_TEST	0x08

#define BYTE_SIZE	0x00
#define HALF_SIZE	0x10
#define INT_SIZE	0x20
#define WORD_SIZE	0x30

#define MEM_SIZE(flags) ((flags) >> 4)

/* --------------------------------------------------------------------- */
/*  Entry, exit                                                          */
/* --------------------------------------------------------------------- */

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_enter(struct sljit_compiler *compiler, sljit_si args, sljit_si scratches, sljit_si saveds, sljit_si local_size)
{
	CHECK_ERROR();
	check_sljit_emit_enter(compiler, args, scratches, saveds, local_size);

	compiler->scratches = scratches;
	compiler->saveds = saveds;
#if (defined SLJIT_DEBUG && SLJIT_DEBUG)
	compiler->logical_local_size = local_size;
#endif
	compiler->locals_offset = (2 + saveds) * sizeof(sljit_sw);
	local_size = (compiler->locals_offset + local_size + 15) & ~15;
	compiler->local_size = local_size;

	if (local_size <= (64 << 3))
		FAIL_IF(push_inst(compiler, STP_PRE | 29 | RT2(TMP_LR)
			| RN(SLJIT_LOCALS_REG) | ((-(local_size >> 3) & 0x7f) << 15)));
	else {
		local_size -= (64 << 3);
		if (local_size > 0xfff) {
			FAIL_IF(push_inst(compiler, SUBI | RD(SLJIT_LOCALS_REG) | RN(SLJIT_LOCALS_REG) | ((local_size >> 12) << 10) | (1 << 22)));
			local_size &= 0xfff;
		}
		if (local_size)
			FAIL_IF(push_inst(compiler, SUBI | RD(SLJIT_LOCALS_REG) | RN(SLJIT_LOCALS_REG) | (local_size << 10)));
		FAIL_IF(push_inst(compiler, STP_PRE | 29 | RT2(TMP_LR) | RN(SLJIT_LOCALS_REG) | (0x40 << 15)));
	}

	if (saveds >= 2)
		FAIL_IF(push_inst(compiler, STP | RT(SLJIT_SAVED_REG1) | RT2(SLJIT_SAVED_REG2) | RN(SLJIT_LOCALS_REG) | (2 << 15)));
	if (saveds >= 4)
		FAIL_IF(push_inst(compiler, STP | RT(SLJIT_SAVED_REG3) | RT2(SLJIT_SAVED_EREG1) | RN(SLJIT_LOCALS_REG) | (4 << 15)));
	if (saveds == 1)
		FAIL_IF(push_inst(compiler, STRI | RT(SLJIT_SAVED_REG1) | RN(SLJIT_LOCALS_REG) | (2 << 10)));
	if (saveds == 3)
		FAIL_IF(push_inst(compiler, STRI | RT(SLJIT_SAVED_REG3) | RN(SLJIT_LOCALS_REG) | (4 << 10)));
	if (saveds == 5)
		FAIL_IF(push_inst(compiler, STRI | RT(SLJIT_SAVED_EREG2) | RN(SLJIT_LOCALS_REG) | (6 << 10)));

	if (args >= 1)
		FAIL_IF(push_inst(compiler, ORR | RD(SLJIT_SAVED_REG1) | RN(TMP_ZERO) | RM(SLJIT_SCRATCH_REG1)));
	if (args >= 2)
		FAIL_IF(push_inst(compiler, ORR | RD(SLJIT_SAVED_REG2) | RN(TMP_ZERO) | RM(SLJIT_SCRATCH_REG2)));
	if (args >= 3)
		FAIL_IF(push_inst(compiler, ORR | RD(SLJIT_SAVED_REG3) | RN(TMP_ZERO) | RM(SLJIT_SCRATCH_REG3)));

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE void sljit_set_context(struct sljit_compiler *compiler, sljit_si args, sljit_si scratches, sljit_si saveds, sljit_si local_size)
{
	CHECK_ERROR_VOID();
	check_sljit_set_context(compiler, args, scratches, saveds, local_size);

	compiler->scratches = scratches;
	compiler->saveds = saveds;
#if (defined SLJIT_DEBUG && SLJIT_DEBUG)
	compiler->logical_local_size = local_size;
#endif
	compiler->locals_offset = (2 + saveds) * sizeof(sljit_sw);
	compiler->local_size = (compiler->locals_offset + local_size + 15) & ~15;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_return(struct sljit_compiler *compiler, sljit_si op, sljit_si src, sljit_sw srcw)
{
	sljit_si saveds, local_size;

	CHECK_ERROR();
	check_sljit_emit_return(compiler, op, src, srcw);
	ADJUST_LOCAL_OFFSET(src, srcw);

	saveds = compiler->saveds;

	if (saveds >= 2)
		FAIL_IF(push_inst(compiler, LDP | RT(SLJIT_SAVED_REG1) | RT2(SLJIT_SAVED_REG2) | RN(SLJIT_LOCALS_REG) | (2 << 15)));
	if (saveds >= 4)
		FAIL_IF(push_inst(compiler, LDP | RT(SLJIT_SAVED_REG3) | RT2(SLJIT_SAVED_EREG1) | RN(SLJIT_LOCALS_REG) | (4 << 15)));
	if (saveds == 1)
		FAIL_IF(push_inst(compiler, LDRI | RT(SLJIT_SAVED_REG1) | RN(SLJIT_LOCALS_REG) | (2 << 10)));
	if (saveds == 3)
		FAIL_IF(push_inst(compiler, LDRI | RT(SLJIT_SAVED_REG3) | RN(SLJIT_LOCALS_REG) | (4 << 10)));
	if (saveds == 5)
		FAIL_IF(push_inst(compiler, LDRI | RT(SLJIT_SAVED_EREG2) | RN(SLJIT_LOCALS_REG) | (6 << 10)));

	local_size = compiler->local_size;

	if (local_size <= (62 << 3))
		FAIL_IF(push_inst(compiler, LDP_PST | 29 | RT2(TMP_LR)
			| RN(SLJIT_LOCALS_REG) | (((local_size >> 3) & 0x7f) << 15)));
	else {
		FAIL_IF(push_inst(compiler, LDP_PST | 29 | RT2(TMP_LR) | RN(SLJIT_LOCALS_REG) | (0x3e << 15)));
		local_size -= (62 << 3);
		if (local_size > 0xfff) {
			FAIL_IF(push_inst(compiler, ADDI | RD(SLJIT_LOCALS_REG) | RN(SLJIT_LOCALS_REG) | ((local_size >> 12) << 10) | (1 << 22)));
			local_size &= 0xfff;
		}
		if (local_size)
			FAIL_IF(push_inst(compiler, ADDI | RD(SLJIT_LOCALS_REG) | RN(SLJIT_LOCALS_REG) | (local_size << 10)));
	}

	FAIL_IF(push_inst(compiler, RET | RN(TMP_LR)));
	return SLJIT_SUCCESS;
}

/* --------------------------------------------------------------------- */
/*  Operators                                                            */
/* --------------------------------------------------------------------- */

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_op0(struct sljit_compiler *compiler, sljit_si op)
{
	CHECK_ERROR();
	check_sljit_emit_op0(compiler, op);

	op = GET_OPCODE(op);
	switch (op) {
	case SLJIT_BREAKPOINT:
		push_inst(compiler, BRK);
		break;
	case SLJIT_NOP:
		push_inst(compiler, NOP);
		break;
	}

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_op1(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src, sljit_sw srcw)
{
	sljit_si dst_r, flags;
	sljit_si op_flags = GET_ALL_FLAGS(op);

	CHECK_ERROR();
	check_sljit_emit_op1(compiler, op, dst, dstw, src, srcw);
	ADJUST_LOCAL_OFFSET(dst, dstw);
	ADJUST_LOCAL_OFFSET(src, srcw);

	compiler->cache_arg = 0;
	compiler->cache_argw = 0;

	dst_r = (dst >= SLJIT_SCRATCH_REG1 && dst <= TMP_REG4) ? dst : TMP_REG1;

	op = GET_OPCODE(op);
	if (op >= SLJIT_MOV && op <= SLJIT_MOVU_P) {
		switch (op) {
		case SLJIT_MOV:
		case SLJIT_MOV_P:
			flags = WORD_SIZE;
			break;
		case SLJIT_MOV_UB:
			flags = BYTE_SIZE;
			if (src & SLJIT_IMM)
				srcw = (sljit_ub)srcw;
			break;
		case SLJIT_MOV_SB:
			flags = BYTE_SIZE | SIGNED;
			if (src & SLJIT_IMM)
				srcw = (sljit_sb)srcw;
			break;
		case SLJIT_MOV_UH:
			flags = HALF_SIZE;
			if (src & SLJIT_IMM)
				srcw = (sljit_uh)srcw;
			break;
		case SLJIT_MOV_SH:
			flags = HALF_SIZE | SIGNED;
			if (src & SLJIT_IMM)
				srcw = (sljit_sh)srcw;
			break;
		case SLJIT_MOV_UI:
			flags = INT_SIZE;
			if (src & SLJIT_IMM)
				srcw = (sljit_ui)srcw;
			break;
		case SLJIT_MOV_SI:
			flags = INT_SIZE | SIGNED;
			if (src & SLJIT_IMM)
				srcw = (sljit_si)srcw;
			break;
		case SLJIT_MOVU:
		case SLJIT_MOVU_P:
			flags = WORD_SIZE | UPDATE;
			break;
		case SLJIT_MOVU_UB:
			flags = BYTE_SIZE | UPDATE;
			if (src & SLJIT_IMM)
				srcw = (sljit_ub)srcw;
			break;
		case SLJIT_MOVU_SB:
			flags = BYTE_SIZE | SIGNED | UPDATE;
			if (src & SLJIT_IMM)
				srcw = (sljit_sb)srcw;
			break;
		case SLJIT_MOVU_UH:
			flags = HALF_SIZE | UPDATE;
			if (src & SLJIT_IMM)
				srcw = (sljit_uh)srcw;
			break;
		case SLJIT_MOVU_SH:
			flags = HALF_SIZE | SIGNED | UPDATE;
			if (src & SLJIT_IMM)
				srcw = (sljit_sh)srcw;
			break;
		case SLJIT_MOVU_UI:
			flags = INT_SIZE | UPDATE;
			if (src & SLJIT_IMM)
				srcw = (sljit_ui)srcw;
			break;
		case SLJIT_MOVU_SI:
			flags = INT_SIZE | SIGNED | UPDATE;
			if (src & SLJIT_IMM)
				srcw = (sljit_si)srcw;
			break;
		default:
			SLJIT_ASSERT_STOP();
			flags = 0;
			break;
		}

		if (src & SLJIT_IMM)
			FAIL_IF(emit_op_imm(compiler, SLJIT_MOV | ARG2_IMM, dst_r, TMP_REG1, srcw));
		else if (src & SLJIT_MEM) {
#if 0
			if (getput_arg_fast(compiler, flags, dst_r, src, srcw))
				FAIL_IF(compiler->error);
			else
				FAIL_IF(getput_arg(compiler, flags, dst_r, src, srcw, dst, dstw));
#endif
		} else {
			if (dst_r != TMP_REG1)
				return emit_op_imm(compiler, op | ((op_flags & SLJIT_INT_OP) ? WORD_OP : 0), dst_r, TMP_REG1, src);
			dst_r = src;
		}

		if (dst & SLJIT_MEM) {
#if 0
			if (getput_arg_fast(compiler, flags | STORE, dst_r, dst, dstw))
				return compiler->error;
			else
				return getput_arg(compiler, flags | STORE, dst_r, dst, dstw, 0, 0);
#endif
		}
		return SLJIT_SUCCESS;
	}

	flags = (GET_FLAGS(op_flags) ? SET_FLAGS : 0) | ((op_flags & SLJIT_INT_OP) ? WORD_OP : 0);
	if (src & SLJIT_MEM) {
#if 0
		if (getput_arg_fast(compiler, WORD_SIZE, TMP_REG2, src, srcw))
			FAIL_IF(compiler->error);
		else
			FAIL_IF(getput_arg(compiler, WORD_SIZE, TMP_REG2, src, srcw, dst, dstw));
#endif
		src = TMP_REG2;
	}

	if (src & SLJIT_IMM) {
		flags |= ARG2_IMM;
		if (op_flags & SLJIT_INT_OP)
			srcw = (sljit_si)srcw;
	} else
		srcw = src;

	emit_op_imm(compiler, flags | op, dst_r, TMP_REG1, srcw);

	if (dst & SLJIT_MEM) {
#if 0
		if (getput_arg_fast(compiler, flags | STORE, dst_r, dst, dstw))
			return compiler->error;
		else
			return getput_arg(compiler, flags | STORE, dst_r, dst, dstw, 0, 0);
#endif
	}
	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_op2(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src1, sljit_sw src1w,
	sljit_si src2, sljit_sw src2w)
{
	sljit_si dst_r, flags;

	CHECK_ERROR();
	check_sljit_emit_op2(compiler, op, dst, dstw, src1, src1w, src2, src2w);
	ADJUST_LOCAL_OFFSET(dst, dstw);
	ADJUST_LOCAL_OFFSET(src1, src1w);
	ADJUST_LOCAL_OFFSET(src2, src2w);

	compiler->cache_arg = 0;
	compiler->cache_argw = 0;

	dst_r = (dst >= SLJIT_SCRATCH_REG1 && dst <= TMP_REG4) ? dst : TMP_REG1;
	flags = (GET_FLAGS(op) ? SET_FLAGS : 0) | ((op & SLJIT_INT_OP) ? WORD_OP : 0);

#if 0
	if ((dst & SLJIT_MEM) && !getput_arg_fast(compiler, WORD_SIZE | STORE | ARG_TEST, TMP_REG1, dst, dstw))
		flags |= SLOW_DEST;

	if (src1 & SLJIT_MEM) {
		if (getput_arg_fast(compiler, WORD_SIZE, TMP_REG1, src1, src1w))
			FAIL_IF(compiler->error);
		else
			flags |= SLOW_SRC1;
	}
	if (src2 & SLJIT_MEM) {
		if (getput_arg_fast(compiler, WORD_SIZE, TMP_REG2, src2, src2w))
			FAIL_IF(compiler->error);
		else
			flags |= SLOW_SRC2;
	}

	if ((flags & (SLOW_SRC1 | SLOW_SRC2)) == (SLOW_SRC1 | SLOW_SRC2)) {
		if (!can_cache(src1, src1w, src2, src2w) && can_cache(src1, src1w, dst, dstw)) {
			FAIL_IF(getput_arg(compiler, WORD_SIZE, TMP_REG2, src2, src2w, src1, src1w));
			FAIL_IF(getput_arg(compiler, WORD_SIZE, TMP_REG1, src1, src1w, dst, dstw));
		}
		else {
			FAIL_IF(getput_arg(compiler, WORD_SIZE, TMP_REG1, src1, src1w, src2, src2w));
			FAIL_IF(getput_arg(compiler, WORD_SIZE, TMP_REG2, src2, src2w, dst, dstw));
		}
	}
	else if (flags & SLOW_SRC1)
		FAIL_IF(getput_arg(compiler, WORD_SIZE, TMP_REG1, src1, src1w, dst, dstw));
	else if (flags & SLOW_SRC2)
		FAIL_IF(getput_arg(compiler, WORD_SIZE, TMP_REG2, src2, src2w, dst, dstw));
#endif

	if (src1 & SLJIT_MEM)
		src1 = TMP_REG1;
	if (src2 & SLJIT_MEM)
		src2 = TMP_REG2;

	if (src1 & SLJIT_IMM)
		flags |= ARG1_IMM;
	else
		src1w = src1;
	if (src2 & SLJIT_IMM)
		flags |= ARG2_IMM;
	else
		src2w = src2;

	if (dst == SLJIT_UNUSED)
		flags |= UNUSED_RETURN;

	emit_op_imm(compiler, flags | GET_OPCODE(op), dst_r, src1w, src2w);

#if 0
	if (dst & SLJIT_MEM) {
		if (!(flags & SLOW_DEST)) {
			getput_arg_fast(compiler, WORD_SIZE | STORE, dst_r, dst, dstw);
			return compiler->error;
		}
		return getput_arg(compiler, WORD_SIZE | STORE, TMP_REG1, dst, dstw, 0, 0);
	}
#endif

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_get_register_index(sljit_si reg)
{
	check_sljit_get_register_index(reg);
	return reg_map[reg];
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_get_float_register_index(sljit_si reg)
{
	check_sljit_get_float_register_index(reg);
	return reg;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_op_custom(struct sljit_compiler *compiler,
	void *instruction, sljit_si size)
{
	CHECK_ERROR();
	check_sljit_emit_op_custom(compiler, instruction, size);
	SLJIT_ASSERT(size == 4);

	return push_inst(compiler, *(sljit_ins*)instruction);
}

/* --------------------------------------------------------------------- */
/*  Floating point operators                                             */
/* --------------------------------------------------------------------- */

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_is_fpu_available(void)
{
	return 1;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_fop1(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src, sljit_sw srcw)
{
	CHECK_ERROR();
	check_sljit_emit_fop1(compiler, op, dst, dstw, src, srcw);

	compiler->cache_arg = 0;
	compiler->cache_argw = 0;

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_fop2(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src1, sljit_sw src1w,
	sljit_si src2, sljit_sw src2w)
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

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_fast_enter(struct sljit_compiler *compiler, sljit_si dst, sljit_sw dstw)
{
	CHECK_ERROR();
	check_sljit_emit_fast_enter(compiler, dst, dstw);
	ADJUST_LOCAL_OFFSET(dst, dstw);

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_fast_return(struct sljit_compiler *compiler, sljit_si src, sljit_sw srcw)
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
	return label;
}

SLJIT_API_FUNC_ATTRIBUTE struct sljit_jump* sljit_emit_jump(struct sljit_compiler *compiler, sljit_si type)
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

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_ijump(struct sljit_compiler *compiler, sljit_si type, sljit_si src, sljit_sw srcw)
{
	CHECK_ERROR();
	check_sljit_emit_ijump(compiler, type, src, srcw);
	ADJUST_LOCAL_OFFSET(src, srcw);

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_si sljit_emit_op_flags(struct sljit_compiler *compiler, sljit_si op,
	sljit_si dst, sljit_sw dstw,
	sljit_si src, sljit_sw srcw,
	sljit_si type)
{
	CHECK_ERROR();
	check_sljit_emit_op_flags(compiler, op, dst, dstw, src, srcw, type);
	ADJUST_LOCAL_OFFSET(dst, dstw);
	ADJUST_LOCAL_OFFSET(src, srcw);

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE struct sljit_const* sljit_emit_const(struct sljit_compiler *compiler, sljit_si dst, sljit_sw dstw, sljit_sw init_value)
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

SLJIT_API_FUNC_ATTRIBUTE void sljit_set_jump_addr(sljit_uw addr, sljit_uw new_addr)
{
}

SLJIT_API_FUNC_ATTRIBUTE void sljit_set_const(sljit_uw addr, sljit_sw new_constant)
{
}
