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

// See sljit_emit_enter if you want to change them
static SLJIT_CONST sljit_ub reg_map[SLJIT_NO_REGISTERS + 5] = {
  0, 0, 1, 2, 12, 14, 6, 7, 8, 10, 11, 13, 3, 4, 5, 15
};

#define COPY_BITS(src, from, to, bits) \
	((from >= to ? (src >> (from - to)) : (src << (to - from))) & (((1 << bits) - 1) << to))

// Thumb16 encodings
#define RD3(rd) (reg_map[rd])
#define RN3(rn) (reg_map[rn] << 3)
#define RM3(rm) (reg_map[rm] << 6)
#define RDN3(rdn) (reg_map[rdn] << 8)
#define IMM3(imm) (imm << 6)
#define IMM8(imm) (imm)

// Thumb16 helpers
#define SET_REGS44(rd, rn) \
	((reg_map[rn] << 3) | (reg_map[rd] & 0x7) | ((reg_map[rd] & 0x8) << 4))
#define IS_2_LO_REGS(reg1, reg2) \
	(reg_map[reg1] <= 7 && reg_map[reg2] <= 7)
#define IS_3_LO_REGS(reg1, reg2, reg3) \
	(reg_map[reg1] <= 7 && reg_map[reg2] <= 7 && reg_map[reg3] <= 7)

// Thumb32 encodings
#define RD4(rd) (reg_map[rd] << 8)
#define RN4(rn) (reg_map[rn] << 16)
#define RM4(rm) (reg_map[rm])
#define IMM5(imm) \
	(COPY_BITS(imm, 2, 12, 3) | ((imm & 0x3) << 6))
#define IMM12(imm) \
	(COPY_BITS(imm, 11, 26, 1) | COPY_BITS(imm, 8, 12, 3) | (imm & 0xff))

typedef unsigned int sljit_i;
typedef unsigned short sljit_uh;

static int push_inst16(struct sljit_compiler *compiler, sljit_i inst)
{
	SLJIT_ASSERT(!(inst & 0xffff0000));

	sljit_uh *ptr = (sljit_uh*)ensure_buf(compiler, sizeof(sljit_uh));
	FAIL_IF(!ptr);
	*ptr = inst;
	compiler->size++;
	return SLJIT_NO_ERROR;
}

static int push_inst32(struct sljit_compiler *compiler, sljit_i inst)
{
	sljit_uh *ptr = (sljit_uh*)ensure_buf(compiler, sizeof(sljit_i));
	FAIL_IF(!ptr);
	*ptr++ = inst >> 16;
	*ptr = inst;
	compiler->size += 2;
	return SLJIT_NO_ERROR;
}

void* sljit_generate_code(struct sljit_compiler *compiler)
{
	struct sljit_memory_fragment *buf;
	sljit_uh *code;
	sljit_uh *code_ptr;
	sljit_uh *buf_ptr;
	sljit_uh *buf_end;
	sljit_uw half_count;

	FUNCTION_ENTRY();

	SLJIT_ASSERT(compiler->size > 0);
	reverse_buf(compiler);

	code = (sljit_uh*)SLJIT_MALLOC_EXEC(compiler->size * sizeof(sljit_uh));
	PTR_FAIL_IF_NULL(code);
	buf = compiler->buf;

	code_ptr = code;
	half_count = 0;

	do {
		buf_ptr = (sljit_uh*)buf->memory;
		buf_end = buf_ptr + (buf->used_size >> 1);
		do {
			*code_ptr = *buf_ptr++;
			code_ptr ++;
			half_count ++;
		} while (buf_ptr < buf_end);

		buf = buf->next;
	} while (buf);

	SLJIT_ASSERT(code_ptr - code <= (int)compiler->size);

	// Set thumb mode flag
	return (void*)((sljit_uw)code | 0x1);
}

#define INVALID_IMM	0x80000000
static sljit_uw get_imm(sljit_uw imm)
{
	int counter;

	if (imm <= 0xff)
		return imm;

	if ((imm & 0xffff) == (imm >> 16)) {
		// Some special cases
		if (!(imm & 0xff00))
			return (1 << 12) | (imm & 0xff);
		if (!(imm & 0xff))
			return (2 << 12) | ((imm >> 8) & 0xff);
		if ((imm & 0xff00) == ((imm & 0xff) << 8))
			return (3 << 12) | (imm & 0xff);
	}

	// Count leading zero?
	counter = 8;
	if (!(imm & 0xffff0000)) {
		counter += 16;
		imm <<= 16;
	}
	if (!(imm & 0xff000000)) {
		counter += 8;
		imm <<= 8;
	}
	if (!(imm & 0xf0000000)) {
		counter += 4;
		imm <<= 4;
	}
	if (!(imm & 0xc0000000)) {
		counter += 2;
		imm <<= 2;
	}
	if (!(imm & 0x80000000)) {
		counter += 1;
		imm <<= 1;
	}
	// Since imm >= 128, this must be true
	SLJIT_ASSERT(counter <= 31);

	if (imm & 0x00ffffff)
		return INVALID_IMM; // Cannot be encoded

	return ((imm >> 24) & 0x7f) | COPY_BITS(counter, 4, 26, 1) | COPY_BITS(counter, 1, 12, 3) | COPY_BITS(counter, 0, 7, 1);
}

static int load_immediate(struct sljit_compiler *compiler, int reg, sljit_uw imm)
{
	sljit_uw tmp;

	if (imm >= 0x10000) {
		tmp = get_imm(imm);
		if (tmp != INVALID_IMM)
			return push_inst32(compiler, 0xf04f0000 | RD4(reg) | tmp);
		tmp = get_imm(~imm);
		if (tmp != INVALID_IMM)
			return push_inst32(compiler, 0xf06f0000 | RD4(reg) | tmp);
	}

	// set low 16 bits, set hi 16 bits to 0
	FAIL_IF(push_inst32(compiler, 0xf2400000 | RD4(reg) |
		COPY_BITS(imm, 12, 16, 4) | COPY_BITS(imm, 11, 26, 1) | COPY_BITS(imm, 8, 12, 3) | (imm & 0xff)));

	// set hi 16 bit if needed
	if (imm >= 0x10000)
		return push_inst32(compiler, 0xf2c00000 | RD4(reg) |
			COPY_BITS(imm, 12 + 16, 16, 4) | COPY_BITS(imm, 11 + 16, 26, 1) | COPY_BITS(imm, 8 + 16, 12, 3) | ((imm & 0xff0000) >> 16));
	return SLJIT_NO_ERROR;
}

#define ARG1_IMM	0x10000
#define ARG2_IMM	0x20000
#define SET_FLAGS	0x40000
static int emit_op_imm(struct sljit_compiler *compiler, int flags, int dst, sljit_uw arg1, sljit_uw arg2)
{
	// dst must be register, TMP_REG1
	// arg1 must be register, TMP_REG1, imm
	// arg2 must be register, TMP_REG2, imm
	int reg;
	sljit_uw imm;

	if (SLJIT_UNLIKELY((flags & (ARG1_IMM | ARG2_IMM)) == (ARG1_IMM | ARG2_IMM))) {
		// Both are immediates
		flags &= ~ARG1_IMM;
		FAIL_IF(load_immediate(compiler, TMP_REG1, arg1));
		arg1 = TMP_REG1;
	}

	if (flags & (ARG1_IMM | ARG2_IMM)) {
		reg = (flags & ARG2_IMM) ? arg1 : arg2;
		imm = (flags & ARG2_IMM) ? arg2 : arg1;

		switch (flags & 0xffff) {
		case SLJIT_MOV:
			SLJIT_ASSERT(!(flags & SET_FLAGS) && (flags & ARG2_IMM) && arg1 == TMP_REG1);
			return load_immediate(compiler, dst, imm);
		case SLJIT_NOT:
			// Since the flags should be set, we just fall back to the register mode
			// Although I can do some clever things here, "NOT IMM" does not worth the efforts
			break;
		case SLJIT_ADD:
			if (IS_2_LO_REGS(reg, dst)) {
				if (imm <= 0x7)
					return push_inst16(compiler, 0x1c00 | IMM3(imm) | RD3(dst) | RN3(reg));
				if (reg == dst && imm <= 0xff)
					return push_inst16(compiler, 0x3000 | IMM8(imm) | RDN3(dst));
			}
			if (imm <= 0xfff && !(flags & SET_FLAGS))
				return push_inst32(compiler, 0xf2000000 | RD4(dst) | RN4(reg) | IMM12(imm));
			imm = get_imm(imm);
			if (imm != INVALID_IMM)
				return push_inst32(compiler, 0xf1100000 | RD4(dst) | RN4(reg) | imm);
			break;
		case SLJIT_ADDC:
			imm = get_imm(imm);
			if (imm != INVALID_IMM)
				return push_inst32(compiler, 0xf1500000 | RD4(dst) | RN4(reg) | imm);
			break;
		case SLJIT_SUB:
			if (flags & ARG2_IMM) {
				if (IS_2_LO_REGS(reg, dst)) {
					if (imm <= 0x7)
						return push_inst16(compiler, 0x1e00 | IMM3(imm) | RD3(dst) | RN3(reg));
					if (reg == dst && imm <= 0xff)
						return push_inst16(compiler, 0x3800 | IMM8(imm) | RDN3(dst));
				}
				if (imm <= 0xfff && !(flags & SET_FLAGS))
					return push_inst32(compiler, 0xf2a00000 | RD4(dst) | RN4(reg) | IMM12(imm));
				imm = get_imm(imm);
				if (imm != INVALID_IMM)
					return push_inst32(compiler, 0xf1b00000 | RD4(dst) | RN4(reg) | imm);
			}
			else {
				if (imm == 0 && IS_2_LO_REGS(reg, dst))
					return push_inst16(compiler, 0x4240 | RD3(dst) | RN3(reg));
				imm = get_imm(imm);
				if (imm != INVALID_IMM)
					return push_inst32(compiler, 0xf1d00000 | RD4(dst) | RN4(reg) | imm);
			}
			break;
		case SLJIT_SUBC:
			if (flags & ARG2_IMM) {
				imm = get_imm(imm);
				if (imm != INVALID_IMM)
					return push_inst32(compiler, 0xf1700000 | RD4(dst) | RN4(reg) | imm);
			}
			break;
		case SLJIT_AND:
			imm = get_imm(imm);
			if (imm != INVALID_IMM)
				return push_inst32(compiler, 0xf0100000 | RD4(dst) | RN4(reg) | imm);
			break;
		case SLJIT_OR:
			imm = get_imm(imm);
			if (imm != INVALID_IMM)
				return push_inst32(compiler, 0xf0500000 | RD4(dst) | RN4(reg) | imm);
			imm = get_imm(~((flags & ARG2_IMM) ? arg2 : arg1));
			if (imm != INVALID_IMM)
				return push_inst32(compiler, 0xf0700000 | RD4(dst) | RN4(reg) | imm);
			break;
		case SLJIT_XOR:
			imm = get_imm(imm);
			if (imm != INVALID_IMM)
				return push_inst32(compiler, 0xf0900000 | RD4(dst) | RN4(reg) | imm);
			break;
		case SLJIT_SHL:
			if (flags & ARG2_IMM) {
				imm &= 0x1f;
				if (IS_2_LO_REGS(dst, reg))
					return push_inst16(compiler, 0x0000 | RD3(dst) | RN3(reg) | (imm << 6));
				return push_inst32(compiler, 0xea5f0000 | RD4(dst) | RM4(reg) | IMM5(imm));
			}
			break;
		case SLJIT_LSHR:
			if (flags & ARG2_IMM) {
				imm &= 0x1f;
				if (IS_2_LO_REGS(dst, reg))
					return push_inst16(compiler, 0x0800 | RD3(dst) | RN3(reg) | (imm << 6));
				return push_inst32(compiler, 0xea5f0010 | RD4(dst) | RM4(reg) | IMM5(imm));
			}
			break;
		case SLJIT_ASHR:
			if (flags & ARG2_IMM) {
				imm &= 0x1f;
				if (IS_2_LO_REGS(dst, reg))
					return push_inst16(compiler, 0x1000 | RD3(dst) | RN3(reg) | (imm << 6));
				return push_inst32(compiler, 0xea5f0020 | RD4(dst) | RM4(reg) | IMM5(imm));
			}
			break;
		default:
			SLJIT_ASSERT_STOP();
			break;
		}

		if (flags & ARG2_IMM) {
			FAIL_IF(load_immediate(compiler, TMP_REG2, arg2));
			arg2 = TMP_REG2;
		}
		else {
			FAIL_IF(load_immediate(compiler, TMP_REG1, arg1));
			arg1 = TMP_REG1;
		}
	}

	// Both arguments are registers
	switch (flags & 0xffff) {
	case SLJIT_MOV:
		SLJIT_ASSERT(!(flags & SET_FLAGS) && arg1 == TMP_REG1);
		return push_inst16(compiler, 0x4600 | SET_REGS44(dst, arg2));
	case SLJIT_NOT:
		SLJIT_ASSERT(arg1 == TMP_REG1);
		if (IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, 0x43c0 | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, 0xea7f0000 | RD4(dst) | RM4(arg2));
	case SLJIT_ADD:
		if (IS_3_LO_REGS(dst, arg1, arg2))
			return push_inst16(compiler, 0x1800 | RD3(dst) | RN3(arg1) | RM3(arg2));
		if (dst == arg1 && !(flags & SET_FLAGS))
			return push_inst16(compiler, 0x4400 | SET_REGS44(dst, arg2));
		return push_inst32(compiler, 0xeb100000 | RD4(dst) | RN4(arg1) | RM4(arg2));
	case SLJIT_ADDC:
		if (dst == arg1 && IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, 0x4140 | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, 0xeb500000 | RD4(dst) | RN4(arg1) | RM4(arg2));
	case SLJIT_SUB:
		if (IS_3_LO_REGS(dst, arg1, arg2))
			return push_inst16(compiler, 0x1a00 | RD3(dst) | RN3(arg1) | RM3(arg2));
		return push_inst32(compiler, 0xebb00000 | RD4(dst) | RN4(arg1) | RM4(arg2));
	case SLJIT_SUBC:
		if (dst == arg1 && IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, 0x4180 | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, 0xeb700000 | RD4(dst) | RN4(arg1) | RM4(arg2));
	case SLJIT_AND:
		if (dst == arg1 && IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, 0x4000 | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, 0xea100000 | RD4(dst) | RN4(arg1) | RM4(arg2));
	case SLJIT_OR:
		if (dst == arg1 && IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, 0x4300 | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, 0xea500000 | RD4(dst) | RN4(arg1) | RM4(arg2));
	case SLJIT_XOR:
		if (dst == arg1 && IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, 0x4040 | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, 0xea900000 | RD4(dst) | RN4(arg1) | RM4(arg2));
	case SLJIT_SHL:
		if (dst == arg1 && IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, 0x4080 | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, 0xfa10f000 | RD4(dst) | RN4(arg1) | RM4(arg2));
	case SLJIT_LSHR:
		if (dst == arg1 && IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, 0x40c0 | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, 0xfa30f000 | RD4(dst) | RN4(arg1) | RM4(arg2));
	case SLJIT_ASHR:
		if (dst == arg1 && IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, 0x4100 | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, 0xfa50f000 | RD4(dst) | RN4(arg1) | RM4(arg2));
	}

	SLJIT_ASSERT_STOP();
	return SLJIT_NO_ERROR;
}

int sljit_emit_enter(struct sljit_compiler *compiler, int args, int temporaries, int generals, int local_size)
{
	int size;
	sljit_i push;

	FUNCTION_ENTRY();
	SLJIT_ASSERT(args >= 0 && args <= 3);
	SLJIT_ASSERT(temporaries >= 0 && temporaries <= SLJIT_NO_TMP_REGISTERS);
	SLJIT_ASSERT(generals >= 0 && generals <= SLJIT_NO_GEN_REGISTERS);
	SLJIT_ASSERT(args <= generals);
	SLJIT_ASSERT(local_size >= 0 && local_size <= SLJIT_MAX_LOCAL_SIZE);

	sljit_emit_enter_verbose();

	compiler->temporaries = temporaries;
	compiler->generals = generals;

	push = (1 << 4) | (1 << 5);
	if (generals >= 5)
		push |= 1 << 11;
	if (generals >= 4)
		push |= 1 << 10;
	if (generals >= 3)
		push |= 1 << 8;
	if (generals >= 2)
		push |= 1 << 7;
	if (generals >= 1)
		push |= 1 << 6;
	FAIL_IF(generals >= 3
		? push_inst32(compiler, 0xe92d0000 | (1 << 14) | push)
		: push_inst16(compiler, 0xb500 | push));

	// Stack must be aligned to 8 bytes:
	size = (3 + generals) * sizeof(sljit_uw);
	local_size += size;
	local_size = (local_size + 7) & ~7;
	local_size -= size;
	compiler->local_size = local_size;
	if (local_size > 0) {
		if (local_size <= (127 << 2))
			FAIL_IF(push_inst16(compiler, 0xb080 | (local_size >> 2)));
		else
			FAIL_IF(emit_op_imm(compiler, SLJIT_SUB | ARG2_IMM, SLJIT_LOCALS_REG, SLJIT_LOCALS_REG, local_size));
	}

	if (args >= 1)
		FAIL_IF(push_inst16(compiler, 0x4600 | SET_REGS44(SLJIT_GENERAL_REG1, SLJIT_TEMPORARY_REG1)));
	if (args >= 2)
		FAIL_IF(push_inst16(compiler, 0x4600 | SET_REGS44(SLJIT_GENERAL_REG2, SLJIT_TEMPORARY_REG2)));
	if (args >= 3)
		FAIL_IF(push_inst16(compiler, 0x4600 | SET_REGS44(SLJIT_GENERAL_REG3, SLJIT_TEMPORARY_REG3)));

	return SLJIT_NO_ERROR;
}

void sljit_fake_enter(struct sljit_compiler *compiler, int args, int temporaries, int generals, int local_size)
{
	int size;

	SLJIT_ASSERT(args >= 0 && args <= 3);
	SLJIT_ASSERT(temporaries >= 0 && temporaries <= SLJIT_NO_TMP_REGISTERS);
	SLJIT_ASSERT(generals >= 0 && generals <= SLJIT_NO_GEN_REGISTERS);
	SLJIT_ASSERT(local_size >= 0 && local_size <= SLJIT_MAX_LOCAL_SIZE);

	sljit_fake_enter_verbose();

	compiler->temporaries = temporaries;
	compiler->generals = generals;

	size = (3 + generals) * sizeof(sljit_uw);
	local_size += size;
	local_size = (local_size + 7) & ~7;
	local_size -= size;
	compiler->local_size = local_size;
}

int sljit_emit_return(struct sljit_compiler *compiler, int src, sljit_w srcw)
{
	sljit_i pop;

	FUNCTION_ENTRY();
#ifdef SLJIT_DEBUG
	if (src != SLJIT_UNUSED) {
		FUNCTION_CHECK_SRC(src, srcw);
	}
	else
		SLJIT_ASSERT(srcw == 0);
#endif

	sljit_emit_return_verbose();

	if (compiler->local_size > 0) {
		if (compiler->local_size <= (127 << 2))
			FAIL_IF(push_inst16(compiler, 0xb000 | (compiler->local_size >> 2)));
		else
			FAIL_IF(emit_op_imm(compiler, SLJIT_ADD | ARG2_IMM, SLJIT_LOCALS_REG, SLJIT_LOCALS_REG, compiler->local_size));
	}

	pop = (1 << 4) | (1 << 5);
	if (compiler->generals >= 5)
		pop |= 1 << 11;
	if (compiler->generals >= 4)
		pop |= 1 << 10;
	if (compiler->generals >= 3)
		pop |= 1 << 8;
	if (compiler->generals >= 2)
		pop |= 1 << 7;
	if (compiler->generals >= 1)
		pop |= 1 << 6;
	return compiler->generals >= 3
		? push_inst32(compiler, 0xe8bd0000 | (1 << 15) | pop)
		: push_inst16(compiler, 0xbd00 | pop);
}

int sljit_emit_op0(struct sljit_compiler *compiler, int op)
{
	FUNCTION_ENTRY();

	SLJIT_ASSERT(GET_OPCODE(op) >= SLJIT_DEBUGGER && GET_OPCODE(op) <= SLJIT_DEBUGGER);
	sljit_emit_op0_verbose();

	op = GET_OPCODE(op);
	switch (op) {
	case SLJIT_DEBUGGER:
		push_inst16(compiler, 0xbe00);
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

	SLJIT_ASSERT_STOP();
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

	SLJIT_ASSERT_STOP();
	return SLJIT_NO_ERROR;
}

int sljit_is_fpu_available(void)
{
	return 1;
}

int sljit_emit_fop1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
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

	SLJIT_ASSERT_STOP();
	return SLJIT_NO_ERROR;
}

int sljit_emit_fop2(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
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

	SLJIT_ASSERT_STOP();
	return SLJIT_NO_ERROR;
}

struct sljit_label* sljit_emit_label(struct sljit_compiler *compiler)
{
	struct sljit_label *label = NULL;

	FUNCTION_ENTRY();

	sljit_emit_label_verbose();

	SLJIT_ASSERT_STOP();
	return label;
}

struct sljit_jump* sljit_emit_jump(struct sljit_compiler *compiler, int type)
{
	struct sljit_jump *jump = NULL;

	FUNCTION_ENTRY();
	SLJIT_ASSERT((type & ~0x1ff) == 0);
	SLJIT_ASSERT((type & 0xff) >= SLJIT_C_EQUAL && (type & 0xff) <= SLJIT_CALL3);

	sljit_emit_jump_verbose();

	SLJIT_ASSERT_STOP();
	return jump;
}

int sljit_emit_ijump(struct sljit_compiler *compiler, int type, int src, sljit_w srcw)
{
	FUNCTION_ENTRY();
	SLJIT_ASSERT(type >= SLJIT_JUMP && type <= SLJIT_CALL3);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_SRC(src, srcw);
#endif
	sljit_emit_ijump_verbose();

	SLJIT_ASSERT_STOP();
	return SLJIT_NO_ERROR;
}

int sljit_emit_cond_set(struct sljit_compiler *compiler, int dst, sljit_w dstw, int type)
{
	FUNCTION_ENTRY();
	SLJIT_ASSERT(type >= SLJIT_C_EQUAL && type <= SLJIT_C_NOT_OVERFLOW);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_set_cond_verbose();

	SLJIT_ASSERT_STOP();
	return SLJIT_NO_ERROR;
}

struct sljit_const* sljit_emit_const(struct sljit_compiler *compiler, int dst, sljit_w dstw, sljit_w initval)
{
	struct sljit_const *const_ = NULL;

	FUNCTION_ENTRY();
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_const_verbose();

	SLJIT_ASSERT_STOP();
	return const_;
}

void sljit_set_jump_addr(sljit_uw addr, sljit_uw new_addr)
{
	SLJIT_ASSERT_STOP();
}

void sljit_set_const(sljit_uw addr, sljit_w new_constant)
{
	SLJIT_ASSERT_STOP();
}

