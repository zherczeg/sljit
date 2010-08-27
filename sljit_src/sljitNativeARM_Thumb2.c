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
#define TMP_REG1	(SLJIT_NO_REGISTERS + 1)
#define TMP_REG2	(SLJIT_NO_REGISTERS + 2)
#define TMP_REG3	(SLJIT_NO_REGISTERS + 3)
#define TMP_PC		(SLJIT_NO_REGISTERS + 4)

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
#define RT4(rt) (reg_map[rt] << 12)
#define DD4(dd) ((dd) << 12)
#define DN4(dn) ((dn) << 16)
#define DM4(dm) (dm)
#define IMM5(imm) \
	(COPY_BITS(imm, 2, 12, 3) | ((imm & 0x3) << 6))
#define IMM12(imm) \
	(COPY_BITS(imm, 11, 26, 1) | COPY_BITS(imm, 8, 12, 3) | (imm & 0xff))

typedef unsigned int sljit_i;
typedef unsigned short sljit_uh;

// ---------------------------------------------------------------------
//  Instrucion forms
// ---------------------------------------------------------------------

// dot '.' changed to _
// I immediate form (possibly followed by number of immediate bits)
#define ADCS		0x4140
#define ADCSI		0xf1500000
#define ADCS_W		0xeb500000
#define ADD		0x4400
#define ADDS		0x1800
#define ADDSI3		0x1c00
#define ADDSI8		0x3000
#define ADDS_W		0xeb100000
#define ADDS_WI		0xf1100000
#define ADDWI		0xf2000000
#define ADD_SP		0xb000
#define ADD_W		0xeb000000
#define ADD_WI		0xf1000000
#define ANDS		0x4000
#define ANDSI		0xf0100000
#define ANDS_W		0xea100000
#define ASRS		0x4100
#define ASRSI		0x1000
#define ASRS_W		0xfa50f000
#define ASRS_WI		0xea5f0020
#define BICSI		0xf0300000
#define BKPT		0xbe00
#define BLX		0x4780
#define BX		0x4700
#define EORS		0x4040
#define EORSI		0xf0900000
#define EORS_W		0xea900000
#define IT		0xbf00
#define LSLS		0x4080
#define LSLSI		0x0000
#define LSLS_W		0xfa10f000
#define LSLS_WI		0xea5f0000
#define LSRS		0x40c0
#define LSRSI		0x0800
#define LSRS_W		0xfa30f000
#define LSRS_WI		0xea5f0010
#define MOV		0x4600
#define MOVSI		0x2000
#define MOVT		0xf2c00000
#define MOVW		0xf2400000
#define MOV_WI		0xf04f0000
#define MUL		0xfb00f000
#define MVNS		0x43c0
#define MVNS_W		0xea7f0000
#define MVN_WI		0xf06f0000
#define ORNSI		0xf0700000
#define ORRS		0x4300
#define ORRSI		0xf0500000
#define ORRS_W		0xea500000
#define POP		0xbd00
#define POP_W		0xe8bd0000
#define PUSH		0xb500
#define PUSH_W		0xe92d0000
#define RSBSI		0x4240
#define RSBS_WI		0xf1d00000
#define SBCS		0x4180
#define SBCSI		0xf1700000
#define SBCS_W		0xeb700000
#define STR_SP		0x9000
#define SUBS		0x1a00
#define SUBSI3		0x1e00
#define SUBSI8		0x3800
#define SUBS_W		0xebb00000
#define SUBS_WI		0xf1b00000
#define SUBWI		0xf2a00000
#define SUB_SP		0xb080
#define SUB_WI		0xf1a00000
#define VABS_F64	0xeeb00bc0
#define VADD_F64	0xee300b00
#define VCMP_F64	0xeeb40b40
#define VDIV_F64	0xee800b00
#define VMOV_F64	0xeeb00b40
#define VMRS		0xeef1fa10
#define VMUL_F64	0xee200b00
#define VNEG_F64	0xeeb10b40
#define VSTR		0xed000b00
#define VSUB_F64	0xee300b40

static int push_inst16(struct sljit_compiler *compiler, sljit_i inst)
{
	SLJIT_ASSERT(!(inst & 0xffff0000));

	sljit_uh *ptr = (sljit_uh*)ensure_buf(compiler, sizeof(sljit_uh));
	FAIL_IF(!ptr);
	*ptr = inst;
	compiler->size++;
	return SLJIT_SUCCESS;
}

static int push_inst32(struct sljit_compiler *compiler, sljit_i inst)
{
	sljit_uh *ptr = (sljit_uh*)ensure_buf(compiler, sizeof(sljit_i));
	FAIL_IF(!ptr);
	*ptr++ = inst >> 16;
	*ptr = inst;
	compiler->size += 2;
	return SLJIT_SUCCESS;
}

static SLJIT_INLINE int emit_imm32_const(struct sljit_compiler *compiler, int dst, sljit_uw imm)
{
	FAIL_IF(push_inst32(compiler, MOVW | RD4(dst) |
		COPY_BITS(imm, 12, 16, 4) | COPY_BITS(imm, 11, 26, 1) | COPY_BITS(imm, 8, 12, 3) | (imm & 0xff)));
	return push_inst32(compiler, MOVT | RD4(dst) |
		COPY_BITS(imm, 12 + 16, 16, 4) | COPY_BITS(imm, 11 + 16, 26, 1) | COPY_BITS(imm, 8 + 16, 12, 3) | ((imm & 0xff0000) >> 16));
}

static SLJIT_INLINE void modify_imm32_const(sljit_uh* inst, sljit_uw new_imm)
{
	int dst = inst[1] & 0x0f00;
	SLJIT_ASSERT(((inst[0] & 0xfbf0) == (MOVW >> 16)) && ((inst[2] & 0xfbf0) == (MOVT >> 16)) && dst == (inst[3] & 0x0f00));
	inst[0] = (MOVW >> 16) | COPY_BITS(new_imm, 12, 0, 4) | COPY_BITS(new_imm, 11, 10, 1);
	inst[1] = dst | COPY_BITS(new_imm, 8, 12, 3) | (new_imm & 0xff);
	inst[2] = (MOVT >> 16) | COPY_BITS(new_imm, 12 + 16, 0, 4) | COPY_BITS(new_imm, 11 + 16, 10, 1);
	inst[3] = dst | COPY_BITS(new_imm, 8 + 16, 12, 3) | ((new_imm & 0xff0000) >> 16);
}

void* sljit_generate_code(struct sljit_compiler *compiler)
{
	struct sljit_memory_fragment *buf;
	sljit_uh *code;
	sljit_uh *code_ptr;
	sljit_uh *buf_ptr;
	sljit_uh *buf_end;
	sljit_uw half_count;

	struct sljit_label *label;
	struct sljit_jump *jump;
	struct sljit_const *const_;

	FUNCTION_ENTRY();

	SLJIT_ASSERT(compiler->size > 0);
	reverse_buf(compiler);

	code = (sljit_uh*)SLJIT_MALLOC_EXEC(compiler->size * sizeof(sljit_uh));
	PTR_FAIL_WITH_EXEC_IF(code);
	buf = compiler->buf;

	code_ptr = code;
	half_count = 0;
	label = compiler->labels;
	jump = compiler->jumps;
	const_ = compiler->consts;

	do {
		buf_ptr = (sljit_uh*)buf->memory;
		buf_end = buf_ptr + (buf->used_size >> 1);
		do {
			*code_ptr = *buf_ptr++;
			// These structures are ordered by their address
			if (label && label->size == half_count) {
				label->addr = ((sljit_uw)code_ptr) | 0x1;
				label->size = code_ptr - code;
				label = label->next;
			}
			if (jump && jump->addr == half_count) {
					SLJIT_ASSERT(jump->flags & (JUMP_LABEL | JUMP_ADDR));

					jump->addr = (sljit_uw)code_ptr - ((jump->flags & IS_CONDITIONAL) ? 12 : 10);
					jump = jump->next;
			}
			if (const_ && const_->addr == half_count) {
				const_->addr = (sljit_uw)code_ptr;
				const_ = const_->next;
			}
			code_ptr ++;
			half_count ++;
		} while (buf_ptr < buf_end);

		buf = buf->next;
	} while (buf);

	SLJIT_ASSERT(!label);
	SLJIT_ASSERT(!jump);
	SLJIT_ASSERT(!const_);
	SLJIT_ASSERT(code_ptr - code <= (int)compiler->size);

	jump = compiler->jumps;
	while (jump) {
		sljit_set_jump_addr(jump->addr, (jump->flags & JUMP_LABEL) ? jump->label->addr : jump->target);
		jump = jump->next;
	}

	compiler->error = SLJIT_ERR_COMPILED;
	// Set thumb mode flag
	return (void*)((sljit_uw)code | 0x1);
}

#define INVALID_IMM	0x80000000
static sljit_uw get_imm(sljit_uw imm)
{
	// Thumb immediate form
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

static int load_immediate(struct sljit_compiler *compiler, int dst, sljit_uw imm)
{
	sljit_uw tmp;

	if (imm >= 0x10000) {
		tmp = get_imm(imm);
		if (tmp != INVALID_IMM)
			return push_inst32(compiler, MOV_WI | RD4(dst) | tmp);
		tmp = get_imm(~imm);
		if (tmp != INVALID_IMM)
			return push_inst32(compiler, MVN_WI | RD4(dst) | tmp);
	}

	// set low 16 bits, set hi 16 bits to 0
	FAIL_IF(push_inst32(compiler, MOVW | RD4(dst) |
		COPY_BITS(imm, 12, 16, 4) | COPY_BITS(imm, 11, 26, 1) | COPY_BITS(imm, 8, 12, 3) | (imm & 0xff)));

	// set hi 16 bit if needed
	if (imm >= 0x10000)
		return push_inst32(compiler, MOVT | RD4(dst) |
			COPY_BITS(imm, 12 + 16, 16, 4) | COPY_BITS(imm, 11 + 16, 26, 1) | COPY_BITS(imm, 8 + 16, 12, 3) | ((imm & 0xff0000) >> 16));
	return SLJIT_SUCCESS;
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
			if (!(flags & SET_FLAGS))
				return load_immediate(compiler, dst, ~imm);
			// Since the flags should be set, we just fall back to the register mode
			// Although I can do some clever things here, "NOT IMM" does not worth the efforts
			break;
		case SLJIT_ADD:
			if (IS_2_LO_REGS(reg, dst)) {
				if (imm <= 0x7)
					return push_inst16(compiler, ADDSI3 | IMM3(imm) | RD3(dst) | RN3(reg));
				if (reg == dst && imm <= 0xff)
					return push_inst16(compiler, ADDSI8 | IMM8(imm) | RDN3(dst));
			}
			if (imm <= 0xfff && !(flags & SET_FLAGS))
				return push_inst32(compiler, ADDWI | RD4(dst) | RN4(reg) | IMM12(imm));
			imm = get_imm(imm);
			if (imm != INVALID_IMM)
				return push_inst32(compiler, ADDS_WI | RD4(dst) | RN4(reg) | imm);
			break;
		case SLJIT_ADDC:
			imm = get_imm(imm);
			if (imm != INVALID_IMM)
				return push_inst32(compiler, ADCSI | RD4(dst) | RN4(reg) | imm);
			break;
		case SLJIT_SUB:
			if (flags & ARG2_IMM) {
				if (IS_2_LO_REGS(reg, dst)) {
					if (imm <= 0x7)
						return push_inst16(compiler, SUBSI3 | IMM3(imm) | RD3(dst) | RN3(reg));
					if (reg == dst && imm <= 0xff)
						return push_inst16(compiler, SUBSI8 | IMM8(imm) | RDN3(dst));
				}
				if (imm <= 0xfff && !(flags & SET_FLAGS))
					return push_inst32(compiler, SUBWI | RD4(dst) | RN4(reg) | IMM12(imm));
				imm = get_imm(imm);
				if (imm != INVALID_IMM)
					return push_inst32(compiler, SUBS_WI | RD4(dst) | RN4(reg) | imm);
			}
			else {
				if (imm == 0 && IS_2_LO_REGS(reg, dst))
					return push_inst16(compiler, RSBSI | RD3(dst) | RN3(reg));
				imm = get_imm(imm);
				if (imm != INVALID_IMM)
					return push_inst32(compiler, RSBS_WI | RD4(dst) | RN4(reg) | imm);
			}
			break;
		case SLJIT_SUBC:
			if (flags & ARG2_IMM) {
				imm = get_imm(imm);
				if (imm != INVALID_IMM)
					return push_inst32(compiler, SBCSI | RD4(dst) | RN4(reg) | imm);
			}
			break;
		case SLJIT_MUL:
			// No form with immediate operand
			break;
		case SLJIT_AND:
			imm = get_imm(imm);
			if (imm != INVALID_IMM)
				return push_inst32(compiler, ANDSI | RD4(dst) | RN4(reg) | imm);
			imm = get_imm(~((flags & ARG2_IMM) ? arg2 : arg1));
			if (imm != INVALID_IMM)
				return push_inst32(compiler, BICSI | RD4(dst) | RN4(reg) | imm);
			break;
		case SLJIT_OR:
			imm = get_imm(imm);
			if (imm != INVALID_IMM)
				return push_inst32(compiler, ORRSI | RD4(dst) | RN4(reg) | imm);
			imm = get_imm(~((flags & ARG2_IMM) ? arg2 : arg1));
			if (imm != INVALID_IMM)
				return push_inst32(compiler, ORNSI | RD4(dst) | RN4(reg) | imm);
			break;
		case SLJIT_XOR:
			imm = get_imm(imm);
			if (imm != INVALID_IMM)
				return push_inst32(compiler, EORSI | RD4(dst) | RN4(reg) | imm);
			break;
		case SLJIT_SHL:
			if (flags & ARG2_IMM) {
				imm &= 0x1f;
				if (IS_2_LO_REGS(dst, reg))
					return push_inst16(compiler, LSLSI | RD3(dst) | RN3(reg) | (imm << 6));
				return push_inst32(compiler, LSLS_WI | RD4(dst) | RM4(reg) | IMM5(imm));
			}
			break;
		case SLJIT_LSHR:
			if (flags & ARG2_IMM) {
				imm &= 0x1f;
				if (IS_2_LO_REGS(dst, reg))
					return push_inst16(compiler, LSRSI | RD3(dst) | RN3(reg) | (imm << 6));
				return push_inst32(compiler, LSRS_WI | RD4(dst) | RM4(reg) | IMM5(imm));
			}
			break;
		case SLJIT_ASHR:
			if (flags & ARG2_IMM) {
				imm &= 0x1f;
				if (IS_2_LO_REGS(dst, reg))
					return push_inst16(compiler, ASRSI | RD3(dst) | RN3(reg) | (imm << 6));
				return push_inst32(compiler, ASRS_WI | RD4(dst) | RM4(reg) | IMM5(imm));
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
		return push_inst16(compiler, MOV | SET_REGS44(dst, arg2));
	case SLJIT_NOT:
		SLJIT_ASSERT(arg1 == TMP_REG1);
		if (IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, MVNS | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, MVNS_W | RD4(dst) | RM4(arg2));
	case SLJIT_ADD:
		if (IS_3_LO_REGS(dst, arg1, arg2))
			return push_inst16(compiler, ADDS | RD3(dst) | RN3(arg1) | RM3(arg2));
		if (dst == arg1 && !(flags & SET_FLAGS))
			return push_inst16(compiler, ADD | SET_REGS44(dst, arg2));
		return push_inst32(compiler, ADDS_W | RD4(dst) | RN4(arg1) | RM4(arg2));
	case SLJIT_ADDC:
		if (dst == arg1 && IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, ADCS | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, ADCS_W | RD4(dst) | RN4(arg1) | RM4(arg2));
	case SLJIT_SUB:
		if (IS_3_LO_REGS(dst, arg1, arg2))
			return push_inst16(compiler, SUBS | RD3(dst) | RN3(arg1) | RM3(arg2));
		return push_inst32(compiler, SUBS_W | RD4(dst) | RN4(arg1) | RM4(arg2));
	case SLJIT_SUBC:
		if (dst == arg1 && IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, SBCS | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, SBCS_W | RD4(dst) | RN4(arg1) | RM4(arg2));
	case SLJIT_MUL:
		if (!(flags & SET_FLAGS))
			return push_inst32(compiler, MUL | RD4(dst) | RN4(arg1) | RM4(arg2));
		SLJIT_ASSERT_STOP();
		//FAIL_IF(push_inst16(compiler, MULS 0x4340 | RD3(dst) | RN3(arg2)));
		return SLJIT_SUCCESS;
	case SLJIT_AND:
		if (dst == arg1 && IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, ANDS | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, ANDS_W | RD4(dst) | RN4(arg1) | RM4(arg2));
	case SLJIT_OR:
		if (dst == arg1 && IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, ORRS | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, ORRS_W | RD4(dst) | RN4(arg1) | RM4(arg2));
	case SLJIT_XOR:
		if (dst == arg1 && IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, EORS | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, EORS_W | RD4(dst) | RN4(arg1) | RM4(arg2));
	case SLJIT_SHL:
		if (dst == arg1 && IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, LSLS | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, LSLS_W | RD4(dst) | RN4(arg1) | RM4(arg2));
	case SLJIT_LSHR:
		if (dst == arg1 && IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, LSRS | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, LSRS_W | RD4(dst) | RN4(arg1) | RM4(arg2));
	case SLJIT_ASHR:
		if (dst == arg1 && IS_2_LO_REGS(dst, arg2))
			return push_inst16(compiler, ASRS | RD3(dst) | RN3(arg2));
		return push_inst32(compiler, ASRS_W | RD4(dst) | RN4(arg1) | RM4(arg2));
	}

	SLJIT_ASSERT_STOP();
	return SLJIT_SUCCESS;
}

#define STORE		0x01
#define SIGNED		0x02

#define WORD_SIZE	0x00
#define BYTE_SIZE	0x04
#define HALF_SIZE	0x08

#define UPDATE		0x10

#define IS_WORD_SIZE(flags)		(!(flags & (BYTE_SIZE | HALF_SIZE)))
#define OFFSET_CHECK(imm, shift)	(!(offset & ~(imm << shift)))

/*
  1st letter:
  w = word
  b = byte
  h = half

  2nd letter:
  s = signed
  u = unsigned

  3rd letter:
  l = load
  s = store
*/

static SLJIT_CONST sljit_uw sljit_mem16[12] = {
/* w u l */ 0x5800 /* ldr */,
/* w u s */ 0x5000 /* str */,
/* w s l */ 0x5800 /* ldr */,
/* w s s */ 0x5000 /* str */,

/* b u l */ 0x5c00 /* ldrb */,
/* b u s */ 0x5400 /* strb */,
/* b s l */ 0x5600 /* ldrsb */,
/* b s s */ 0x5400 /* strb */,

/* h u l */ 0x5a00 /* ldrh */,
/* h u s */ 0x5200 /* strh */,
/* h s l */ 0x5e00 /* ldrsh */,
/* h s s */ 0x5200 /* strh */,
};

static SLJIT_CONST sljit_uw sljit_mem16_imm5[12] = {
/* w u l */ 0x6800 /* ldr imm5 */,
/* w u s */ 0x6000 /* str imm5 */,
/* w s l */ 0x6800 /* ldr imm5 */,
/* w s s */ 0x6000 /* str imm5 */,

/* b u l */ 0x7800 /* ldrb imm5 */,
/* b u s */ 0x7000 /* strb imm5 */,
/* b s l */ 0x0000 /* not allowed */,
/* b s s */ 0x7000 /* strb imm5 */,

/* h u l */ 0x8800 /* ldrh imm5 */,
/* h u s */ 0x8000 /* strh imm5 */,
/* h s l */ 0x0000 /* not allowed */,
/* h s s */ 0x8000 /* strh imm5 */,
};

#define MEM_IMM8	0xc00
#define MEM_IMM12	0x800000
static SLJIT_CONST sljit_uw sljit_mem32[12] = {
/* w u l */ 0xf8500000 /* ldr.w */,
/* w u s */ 0xf8400000 /* str.w */,
/* w s l */ 0xf8500000 /* ldr.w */,
/* w s s */ 0xf8400000 /* str.w */,

/* b u l */ 0xf8100000 /* ldrb.w */,
/* b u s */ 0xf8000000 /* strb.w */,
/* b s l */ 0xf9100000 /* ldrsb.w */,
/* b s s */ 0xf8000000 /* strb.w */,

/* h u l */ 0xf8300000 /* ldrh.w */,
/* h u s */ 0xf8200000 /* strsh.w */,
/* h s l */ 0xf9300000 /* ldrsh.w */,
/* h s s */ 0xf8200000 /* strsh.w */,
};

// Helper function. Dst should be reg + value, using at most 1 instruction, flags does not set
static int emit_set_delta(struct sljit_compiler *compiler, int dst, int reg, sljit_w value)
{
	if (value >= 0) {
		if (value <= 0xfff)
			return push_inst32(compiler, ADDWI | RD4(dst) | RN4(reg) | IMM12(value));
		value = get_imm(value);
		if (value != INVALID_IMM)
			return push_inst32(compiler, ADD_WI | RD4(dst) | RN4(reg) | value);
	}
	else {
		value = -value;
		if (value <= 0xfff)
			return push_inst32(compiler, SUBWI | RD4(dst) | RN4(reg) | IMM12(value));
		value = get_imm(value);
		if (value != INVALID_IMM)
			return push_inst32(compiler, SUB_WI | RD4(dst) | RN4(reg) | value);
	}
	return SLJIT_ERR_UNSUPPORTED;
}

static int emit_op_mem(struct sljit_compiler *compiler, int flags, int dst, int base, sljit_uw offset)
{
	int update = 0;
	int high_reg = (base >> 4) & 0xf;
	sljit_w tmp;

	SLJIT_ASSERT(base & SLJIT_MEM);
	base &= 0xf;
	if (flags & UPDATE) {
		flags &= ~UPDATE;
		// Update only applies if a base register exists
		if (base)
			update = 1;
	}

	if (!update) {
		if (high_reg && offset) {
			if (emit_set_delta(compiler, TMP_REG2, high_reg, offset)) {
				FAIL_IF(compiler->error);
				FAIL_IF(load_immediate(compiler, TMP_REG2, offset));
				FAIL_IF(push_inst16(compiler, ADD | SET_REGS44(TMP_REG2, high_reg)));
			}
			offset = 0;
			high_reg = TMP_REG2;
		}

		tmp = (sljit_w)offset;
		if (tmp > 0xfff || tmp < -0xff) {
			if (tmp > 0)
				tmp &= ~0xfff;
			else
				tmp = -(-tmp & ~0xff);
			if (emit_set_delta(compiler, TMP_REG2, base, tmp)) {
				FAIL_IF(compiler->error);
				FAIL_IF(load_immediate(compiler, TMP_REG2, offset));
				offset = 0;
			}
			else
				offset -= tmp;
			base = TMP_REG2;
		}
	}
	else {
		if (high_reg) {
			FAIL_IF(push_inst16(compiler, ADD | SET_REGS44(base, high_reg)));
			high_reg = 0;
		}

		tmp = (sljit_w)offset;
		if (tmp > 0xff || tmp < -0xff) {
			if (tmp > 0)
				tmp &= ~0xff;
			else
				tmp = -(-tmp & ~0xff);

			if (emit_set_delta(compiler, base, base, tmp)) {
				FAIL_IF(compiler->error);
				FAIL_IF(load_immediate(compiler, TMP_REG2, tmp));
				FAIL_IF(push_inst16(compiler, ADD | SET_REGS44(base, TMP_REG2)));
			}
			offset -= tmp;
		}
	}

	if (SLJIT_UNLIKELY(high_reg && base)) {
		SLJIT_ASSERT(offset == 0 && !update);
		if (IS_3_LO_REGS(dst, base, high_reg))
			return push_inst16(compiler, sljit_mem16[flags] | RD3(dst) | RN3(base) | RM3(high_reg));
		return push_inst32(compiler, sljit_mem32[flags] | RT4(dst) | RN4(base) | RM4(high_reg));
	}

	// Thumb 16 bit encodings
	if (!update) {
		if (IS_2_LO_REGS(dst, base) && sljit_mem16_imm5[flags]) {
			tmp = 3;
			if (IS_WORD_SIZE(flags)) {
				if (OFFSET_CHECK(0x1f, 2))
					tmp = 2;
			}
			else if ((flags & BYTE_SIZE) && offset <= 0x1f)
			{
				if (offset <= 0x1f)
					tmp = 0;
			}
			else {
				SLJIT_ASSERT(flags & HALF_SIZE);
				if (OFFSET_CHECK(0x1f, 1))
					tmp = 1;
			}

			if (tmp != 3)
				return push_inst16(compiler, sljit_mem16_imm5[flags] | RD3(dst) | RN3(base) | (offset << (6 - tmp)));
		}

		// SP based immediate
		if (SLJIT_UNLIKELY(base == SLJIT_LOCALS_REG) && OFFSET_CHECK(0xff, 2) && IS_WORD_SIZE(flags) && reg_map[dst] <= 7)
			return push_inst16(compiler, STR_SP | ((flags & STORE) ? 0 : 0x800) | RDN3(dst) | (offset >> 2));

		if (offset <= 0xfff)
			return push_inst32(compiler, sljit_mem32[flags] | MEM_IMM12 | RT4(dst) | RN4(base) | offset);
	}

	if (offset <= 0xff)
		offset |= 0x200;
	else {
		offset = -(sljit_w)offset;
		SLJIT_ASSERT(offset <= 0xff);
	}
	if (update)
		offset |= 0x100;

	return push_inst32(compiler, sljit_mem32[flags] | MEM_IMM8 | RT4(dst) | RN4(base) | offset);
}

static int emit_fop_mem(struct sljit_compiler *compiler, int flags, int dst, int base, sljit_uw offset)
{
	sljit_w tmp;

	SLJIT_ASSERT(base & SLJIT_MEM);
	if (!(base & 0xf0)) {
		if (base & 0xf)
			base &= 0xf;
		else {
			FAIL_IF(load_immediate(compiler, TMP_REG2, offset));
			base = TMP_REG2;
			offset = 0;
		}
	} else {
		base &= ~SLJIT_MEM;
		FAIL_IF(push_inst32(compiler, ADD_W | RD4(TMP_REG2) | RN4(base & 0xf) | RM4(base >> 4)));
		base = TMP_REG2;
	}

	tmp = offset;
	if (tmp > 0)
		tmp &= ~0x3fc;
	else
		tmp = -((-tmp) & ~0x3fc);

	if (tmp) {
		if (emit_set_delta(compiler, TMP_REG2, base, tmp)) {
			FAIL_IF(compiler->error);
			FAIL_IF(load_immediate(compiler, TMP_REG2, tmp));
			FAIL_IF(push_inst16(compiler, ADD | SET_REGS44(TMP_REG2, base)));
		}
		offset -= tmp;
		base = TMP_REG2;
	}

	tmp = offset;
	if (tmp >= 0) {
		SLJIT_ASSERT(!(tmp & ~0x3fc));
		tmp = (tmp >> 2) | 0x00800000;
	} else {
		SLJIT_ASSERT(!((-tmp) & ~0x3fc));
		tmp = (-tmp) >> 2;
	}

	if (!(flags & STORE))
		tmp |= 0x00100000;

	return push_inst32(compiler, VSTR | RN4(base) | (dst << 12) | tmp);
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
		? push_inst32(compiler, PUSH_W | (1 << 14) | push)
		: push_inst16(compiler, PUSH | push));

	// Stack must be aligned to 8 bytes:
	size = (3 + generals) * sizeof(sljit_uw);
	local_size += size;
	local_size = (local_size + 7) & ~7;
	local_size -= size;
	compiler->local_size = local_size;
	if (local_size > 0) {
		if (local_size <= (127 << 2))
			FAIL_IF(push_inst16(compiler, SUB_SP | (local_size >> 2)));
		else
			FAIL_IF(emit_op_imm(compiler, SLJIT_SUB | ARG2_IMM, SLJIT_LOCALS_REG, SLJIT_LOCALS_REG, local_size));
	}

	if (args >= 1)
		FAIL_IF(push_inst16(compiler, MOV | SET_REGS44(SLJIT_GENERAL_REG1, SLJIT_TEMPORARY_REG1)));
	if (args >= 2)
		FAIL_IF(push_inst16(compiler, MOV | SET_REGS44(SLJIT_GENERAL_REG2, SLJIT_TEMPORARY_REG2)));
	if (args >= 3)
		FAIL_IF(push_inst16(compiler, MOV | SET_REGS44(SLJIT_GENERAL_REG3, SLJIT_TEMPORARY_REG3)));

	return SLJIT_SUCCESS;
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

	if (src != SLJIT_PREF_RET_REG && src != SLJIT_UNUSED) {
		if (src >= SLJIT_TEMPORARY_REG1 && src <= TMP_REG3)
			FAIL_IF(push_inst16(compiler, MOV | SET_REGS44(SLJIT_PREF_RET_REG, src)));
		else {
			compiler->cache_arg = 0;
			compiler->cache_argw = 0;
			FAIL_IF(emit_op_mem(compiler, WORD_SIZE, SLJIT_PREF_RET_REG, src, srcw));
		}
	}

	if (compiler->local_size > 0) {
		if (compiler->local_size <= (127 << 2))
			FAIL_IF(push_inst16(compiler, ADD_SP | (compiler->local_size >> 2)));
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
		? push_inst32(compiler, POP_W | (1 << 15) | pop)
		: push_inst16(compiler, POP | pop);
}

// ---------------------------------------------------------------------
//  Operators
// ---------------------------------------------------------------------

int sljit_emit_op0(struct sljit_compiler *compiler, int op)
{
	FUNCTION_ENTRY();

	SLJIT_ASSERT(GET_OPCODE(op) >= SLJIT_DEBUGGER && GET_OPCODE(op) <= SLJIT_DEBUGGER);
	sljit_emit_op0_verbose();

	op = GET_OPCODE(op);
	switch (op) {
	case SLJIT_DEBUGGER:
		push_inst16(compiler, BKPT);
		break;
	}

	return SLJIT_SUCCESS;
}

int sljit_emit_op1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
	int op_type, dst_r, flags;

	FUNCTION_ENTRY();

	SLJIT_ASSERT(GET_OPCODE(op) >= SLJIT_MOV && GET_OPCODE(op) <= SLJIT_NEG);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_OP();
	FUNCTION_CHECK_SRC(src, srcw);
	FUNCTION_CHECK_DST(dst, dstw);
	FUNCTION_CHECK_OP1();
#endif
	sljit_emit_op1_verbose();

	compiler->cache_arg = 0;
	compiler->cache_argw = 0;

	op_type = GET_OPCODE(op);
	dst_r = (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_NO_REGISTERS) ? dst : TMP_REG1;

	if (op_type >= SLJIT_MOV && op_type <= SLJIT_MOVU_SI) {
		switch (op_type) {
		case SLJIT_MOV:
		case SLJIT_MOV_UI:
		case SLJIT_MOV_SI:
			flags = WORD_SIZE;
			break;
		case SLJIT_MOV_UB:
			flags = BYTE_SIZE;
			if (src & SLJIT_IMM)
				srcw = (unsigned char)srcw;
			break;
		case SLJIT_MOV_SB:
			flags = BYTE_SIZE | SIGNED;
			if (src & SLJIT_IMM)
				srcw = (char)srcw;
			break;
		case SLJIT_MOV_UH:
			flags = HALF_SIZE;
			if (src & SLJIT_IMM)
				srcw = (unsigned short)srcw;
			break;
		case SLJIT_MOV_SH:
			flags = HALF_SIZE | SIGNED;
			if (src & SLJIT_IMM)
				srcw = (short)srcw;
			break;
		case SLJIT_MOVU:
		case SLJIT_MOVU_UI:
		case SLJIT_MOVU_SI:
			flags = WORD_SIZE | UPDATE;
			break;
		case SLJIT_MOVU_UB:
			flags = BYTE_SIZE | UPDATE;
			if (src & SLJIT_IMM)
				srcw = (unsigned char)srcw;
			break;
		case SLJIT_MOVU_SB:
			flags = BYTE_SIZE | SIGNED | UPDATE;
			if (src & SLJIT_IMM)
				srcw = (char)srcw;
			break;
		case SLJIT_MOVU_UH:
			flags = HALF_SIZE | UPDATE;
			if (src & SLJIT_IMM)
				srcw = (unsigned short)srcw;
			break;
		case SLJIT_MOVU_SH:
			flags = HALF_SIZE | SIGNED | UPDATE;
			if (src & SLJIT_IMM)
				srcw = (short)srcw;
			break;
		default:
			SLJIT_ASSERT_STOP();
			flags = 0;
			break;
		}

		if (src & SLJIT_IMM)
			FAIL_IF(emit_op_imm(compiler, SLJIT_MOV | ARG2_IMM, dst_r, TMP_REG1, srcw));
		else if (src & SLJIT_MEM)
			FAIL_IF(emit_op_mem(compiler, flags, dst_r, src, srcw));
		else {
			if (dst_r != TMP_REG1 || dst == SLJIT_UNUSED)
				return emit_op_imm(compiler, SLJIT_MOV, dst_r, TMP_REG1, src);
			dst_r = src;
		}

		if (dst & SLJIT_MEM)
			return emit_op_mem(compiler, flags | STORE, dst_r, dst, dstw);
		return SLJIT_SUCCESS;
	}

	if (op_type == SLJIT_NEG)
		return sljit_emit_op2(compiler, GET_FLAGS(op) | SLJIT_SUB, dst, dstw, SLJIT_IMM, 0, src, srcw);

	flags = GET_FLAGS(op) ? SET_FLAGS : 0;
	if (src & SLJIT_MEM) {
		FAIL_IF(emit_op_mem(compiler, WORD_SIZE, TMP_REG2, src, srcw));
		src = TMP_REG2;
	}

	if (src & SLJIT_IMM)
		flags |= ARG2_IMM;
	else
		srcw = src;

	emit_op_imm(compiler, flags | op_type, dst_r, TMP_REG1, srcw);

	if (dst & SLJIT_MEM)
		return emit_op_mem(compiler, WORD_SIZE | STORE, dst_r, dst, dstw);
	return SLJIT_SUCCESS;
}

int sljit_emit_op2(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	int dst_r, flags;

	FUNCTION_ENTRY();

	SLJIT_ASSERT(GET_OPCODE(op) >= SLJIT_ADD && GET_OPCODE(op) <= SLJIT_ASHR);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_OP();
	FUNCTION_CHECK_SRC(src1, src1w);
	FUNCTION_CHECK_SRC(src2, src2w);
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_op2_verbose();

	compiler->cache_arg = 0;
	compiler->cache_argw = 0;

	dst_r = (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_NO_REGISTERS) ? dst : TMP_REG1;
	flags = GET_FLAGS(op) ? SET_FLAGS : 0;

	if (src1 & SLJIT_MEM) {
		FAIL_IF(emit_op_mem(compiler, WORD_SIZE, TMP_REG1, src1, src1w));
		src1 = TMP_REG1;
	}
	if (src2 & SLJIT_MEM) {
		FAIL_IF(emit_op_mem(compiler, WORD_SIZE, TMP_REG2, src2, src2w));
		src2 = TMP_REG2;
	}

	if (src1 & SLJIT_IMM)
		flags |= ARG1_IMM;
	else
		src1w = src1;
	if (src2 & SLJIT_IMM)
		flags |= ARG2_IMM;
	else
		src2w = src2;

	emit_op_imm(compiler, flags | GET_OPCODE(op), dst_r, src1w, src2w);

	if (dst & SLJIT_MEM)
		return emit_op_mem(compiler, WORD_SIZE | STORE, dst_r, dst, dstw);
	return SLJIT_SUCCESS;
}

// ---------------------------------------------------------------------
//  Floating point operators
// ---------------------------------------------------------------------

int sljit_is_fpu_available(void)
{
	return 1;
}

int sljit_emit_fop1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
	int dst_r;

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
		if (dst & SLJIT_MEM) {
			emit_fop_mem(compiler, 0, TMP_FREG1, dst, dstw);
			dst = TMP_FREG1;
		}
		if (src & SLJIT_MEM) {
			emit_fop_mem(compiler, 0, TMP_FREG2, src, srcw);
			src = TMP_FREG2;
		}
		FAIL_IF(push_inst32(compiler, VCMP_F64 | DD4(dst) | DM4(src)));
		return push_inst32(compiler, VMRS);
	}

	dst_r = (dst >= SLJIT_FLOAT_REG1 && dst <= SLJIT_FLOAT_REG4) ? dst : TMP_FREG1;
	if (src & SLJIT_MEM) {
		emit_fop_mem(compiler, 0, dst_r, src, srcw);
		src = dst_r;
	}

	switch (GET_OPCODE(op)) {
	case SLJIT_FMOV:
		if (src != dst_r)
			FAIL_IF(push_inst32(compiler, VMOV_F64 | DD4(dst_r) | DM4(src)));
		break;
	case SLJIT_FNEG:
		FAIL_IF(push_inst32(compiler, VNEG_F64 | DD4(dst_r) | DM4(src)));
		break;
	case SLJIT_FABS:
		FAIL_IF(push_inst32(compiler, VABS_F64 | DD4(dst_r) | DM4(src)));
		break;
	}

	if (dst & SLJIT_MEM)
		return emit_fop_mem(compiler, STORE, TMP_FREG1, dst, dstw);
	return SLJIT_SUCCESS;
}

int sljit_emit_fop2(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	int dst_r;

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

	dst_r = (dst >= SLJIT_FLOAT_REG1 && dst <= SLJIT_FLOAT_REG4) ? dst : TMP_FREG1;
	if (src1 & SLJIT_MEM) {
		emit_fop_mem(compiler, 0, TMP_FREG1, src1, src1w);
		src1 = TMP_FREG1;
	}
	if (src2 & SLJIT_MEM) {
		emit_fop_mem(compiler, 0, TMP_FREG2, src2, src2w);
		src2 = TMP_FREG2;
	}

	switch (GET_OPCODE(op)) {
	case SLJIT_FADD:
		FAIL_IF(push_inst32(compiler, VADD_F64 | DD4(dst_r) | DN4(src1) | DM4(src2)));
		break;
	case SLJIT_FSUB:
		FAIL_IF(push_inst32(compiler, VSUB_F64 | DD4(dst_r) | DN4(src1) | DM4(src2)));
		break;
	case SLJIT_FMUL:
		FAIL_IF(push_inst32(compiler, VMUL_F64 | DD4(dst_r) | DN4(src1) | DM4(src2)));
		break;
	case SLJIT_FDIV:
		FAIL_IF(push_inst32(compiler, VDIV_F64 | DD4(dst_r) | DN4(src1) | DM4(src2)));
		break;
	}

	if (dst & SLJIT_MEM)
		return emit_fop_mem(compiler, STORE, TMP_FREG1, dst, dstw);
	return SLJIT_SUCCESS;
}

// ---------------------------------------------------------------------
//  Conditional instructions
// ---------------------------------------------------------------------

static sljit_uw get_cc(int type)
{
	switch (type) {
	case SLJIT_C_EQUAL:
		return 0x0;

	case SLJIT_C_NOT_EQUAL:
		return 0x1;

	case SLJIT_C_LESS:
		return 0x3;

	case SLJIT_C_NOT_LESS:
		return 0x2;

	case SLJIT_C_GREATER:
		return 0x8;

	case SLJIT_C_NOT_GREATER:
		return 0x9;

	case SLJIT_C_SIG_LESS:
		return 0xb;

	case SLJIT_C_SIG_NOT_LESS:
		return 0xa;

	case SLJIT_C_SIG_GREATER:
		return 0xc;

	case SLJIT_C_SIG_NOT_GREATER:
		return 0xd;

	case SLJIT_C_OVERFLOW:
		return 0x6;

	case SLJIT_C_NOT_OVERFLOW:
		return 0x7;

	default: // SLJIT_JUMP
		return 0xe;
	}
}

struct sljit_label* sljit_emit_label(struct sljit_compiler *compiler)
{
	struct sljit_label *label;

	FUNCTION_ENTRY();

	sljit_emit_label_verbose();

	if (compiler->last_label && compiler->last_label->size == compiler->size)
		return compiler->last_label;

	label = (struct sljit_label*)ensure_abuf(compiler, sizeof(struct sljit_label));
	PTR_FAIL_IF(!label);

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
	int cc;

	FUNCTION_ENTRY();
	SLJIT_ASSERT((type & ~0x1ff) == 0);
	SLJIT_ASSERT((type & 0xff) >= SLJIT_C_EQUAL && (type & 0xff) <= SLJIT_CALL3);

	sljit_emit_jump_verbose();

	jump = (struct sljit_jump*)ensure_abuf(compiler, sizeof(struct sljit_jump));
	PTR_FAIL_IF(!jump);

	jump->next = NULL;
	jump->flags = type & SLJIT_REWRITABLE_JUMP;
	type &= 0xff;
	if (compiler->last_jump)
		compiler->last_jump->next = jump;
	else
		compiler->jumps = jump;
	compiler->last_jump = jump;

	// In ARM, we don't need to touch the arguments
	PTR_FAIL_IF(emit_imm32_const(compiler, TMP_REG1, 0));
	if (type < SLJIT_JUMP) {
		jump->flags |= IS_CONDITIONAL;
		cc = get_cc(type);
		jump->flags |= cc << 12;
		PTR_FAIL_IF(push_inst16(compiler, IT | (cc << 4) | 0x8));
	}

	if (type <= SLJIT_JUMP)
		PTR_FAIL_IF(push_inst16(compiler, BX | RN3(TMP_REG1)));
	else {
		jump->flags |= IS_BL;
		PTR_FAIL_IF(push_inst16(compiler, BLX | RN3(TMP_REG1)));
	}

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

	// In ARM, we don't need to touch the arguments
	if (src & SLJIT_IMM) {
		jump = (struct sljit_jump*)ensure_abuf(compiler, sizeof(struct sljit_jump));
		FAIL_IF(!jump);

		jump->next = NULL;
		jump->flags = JUMP_ADDR | ((type >= SLJIT_CALL0) ? IS_BL : 0);
		jump->target = srcw;
		if (compiler->last_jump)
			compiler->last_jump->next = jump;
		else
			compiler->jumps = jump;
		compiler->last_jump = jump;

		FAIL_IF(emit_imm32_const(compiler, TMP_REG1, 0));
		if (type <= SLJIT_JUMP)
			FAIL_IF(push_inst16(compiler, BX | RN3(TMP_REG1)));
		else
			FAIL_IF(push_inst16(compiler, BLX | RN3(TMP_REG1)));
		jump->addr = compiler->size;
	}
	else {
		if (src >= SLJIT_TEMPORARY_REG1 && src <= SLJIT_NO_REGISTERS) {
			if (type <= SLJIT_JUMP)
				return push_inst16(compiler, BX | RN3(src));
			else
				return push_inst16(compiler, BLX | RN3(src));
		}

		compiler->cache_arg = 0;
		compiler->cache_argw = 0;
		FAIL_IF(emit_op_mem(compiler, WORD_SIZE, type <= SLJIT_JUMP ? TMP_PC : TMP_REG1, src, srcw));
		if (type >= SLJIT_CALL0)
			return push_inst16(compiler, BLX | RN3(TMP_REG1));
	}
	return SLJIT_SUCCESS;
}

int sljit_emit_cond_set(struct sljit_compiler *compiler, int dst, sljit_w dstw, int type)
{
	int dst_r;
	sljit_uw cc;

	FUNCTION_ENTRY();
	SLJIT_ASSERT(type >= SLJIT_C_EQUAL && type <= SLJIT_C_NOT_OVERFLOW);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_set_cond_verbose();

	dst_r = TMP_REG1;
	if (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_NO_REGISTERS && reg_map[dst] <= 7)
		dst_r = dst;

	cc = get_cc(type);
	FAIL_IF(push_inst16(compiler, IT | (cc << 4) | ((1 - (cc & 0x1)) << 3) | 0x4));
	FAIL_IF(push_inst16(compiler, MOVSI | 0x1 | RDN3(dst_r)));
	FAIL_IF(push_inst16(compiler, MOVSI | 0x0 | RDN3(dst_r)));

	if (dst_r == TMP_REG1 && dst != SLJIT_UNUSED) {
		if (dst & SLJIT_MEM) {
			compiler->cache_arg = 0;
			compiler->cache_argw = 0;
			return emit_op_mem(compiler, WORD_SIZE | STORE, dst_r, dst, dstw);
		}
		else
			return push_inst16(compiler, MOV | SET_REGS44(dst, TMP_REG1));
	}

	return SLJIT_SUCCESS;
}

struct sljit_const* sljit_emit_const(struct sljit_compiler *compiler, int dst, sljit_w dstw, sljit_w initval)
{
	struct sljit_const *const_;
	int dst_r;

	FUNCTION_ENTRY();
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_const_verbose();

	const_ = (struct sljit_const*)ensure_abuf(compiler, sizeof(struct sljit_const));
	PTR_FAIL_IF(!const_);

	const_->next = NULL;
	const_->addr = compiler->size;
	if (compiler->last_const)
		compiler->last_const->next = const_;
	else
		compiler->consts = const_;
	compiler->last_const = const_;

	dst_r = (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_NO_REGISTERS) ? dst : TMP_REG1;
	PTR_FAIL_IF(emit_imm32_const(compiler, dst_r, initval));

	if (dst & SLJIT_MEM) {
		compiler->cache_arg = 0;
		compiler->cache_argw = 0;
		PTR_FAIL_IF(emit_op_mem(compiler, WORD_SIZE | STORE, dst_r, dst, dstw));
	}
	return const_;
}

void sljit_set_jump_addr(sljit_uw addr, sljit_uw new_addr)
{
	sljit_uh* inst = (sljit_uh*)addr;
	modify_imm32_const(inst, new_addr);
	SLJIT_CACHE_FLUSH(inst, inst + 3);
}

void sljit_set_const(sljit_uw addr, sljit_w new_constant)
{
	sljit_uh* inst = (sljit_uh*)addr;
	modify_imm32_const(inst, new_constant);
	SLJIT_CACHE_FLUSH(inst, inst + 3);
}

