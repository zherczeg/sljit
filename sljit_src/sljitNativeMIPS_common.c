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

char* sljit_get_platform_name()
{
#ifdef SLJIT_CONFIG_MIPS_32
	return "mips-32";
#else
#error "mips-64 is not yet supported"
#endif
}

// Length of an instruction word
// Both for mips-32 and mips-64
typedef unsigned int sljit_i;

#define TMP_REG1	(SLJIT_NO_REGISTERS + 1)
#define TMP_REG2	(SLJIT_NO_REGISTERS + 2)
#define TMP_REG3	(SLJIT_NO_REGISTERS + 3)
#define REAL_STACK_PTR	(SLJIT_NO_REGISTERS + 4)

// TMP_EREG1 is used mainly for literal encoding on 64 bit
#define TMP_EREG1		24
#define TMP_EREG2		25

// Flags are keept in volatile registers
#define EQUAL_FLAG	7
// And carry flag as well
#define ULESS_FLAG	11
#define UGREATER_FLAG	12
#define LESS_FLAG	13
#define GREATER_FLAG	14
#define OVERFLOW_FLAG	15

#define UNORD_BIT	1
#define EQUAL_BIT	2
#define LESS_BIT	3
#define GREATER_BIT	4

#define TMP_FREG1	(SLJIT_FLOAT_REG4 + 1)
#define TMP_FREG2	(SLJIT_FLOAT_REG4 + 2)

// ---------------------------------------------------------------------
//  Instrucion forms
// ---------------------------------------------------------------------

#define S(s)		(reg_map[s] << 21)
#define T(t)		(reg_map[t] << 16)
#define D(d)		(reg_map[d] << 11)
// Absolute registers
#define SA(s)		((s) << 21)
#define TA(t)		((t) << 16)
#define DA(d)		((d) << 11)
#define FT(t)		((t) << (16 + 1))
#define FS(s)		((s) << (11 + 1))
#define FD(d)		((d) << (6 + 1))
#define IMM(imm)	((imm) & 0xffff)
#define SH_IMM(imm)	((imm & 0x1f) << 6)

#define DR(dr)		(reg_map[dr])
#define HI(opcode)	((opcode) << 26)
#define LO(opcode)	(opcode)
#define FMT_D		(17 << 21)

#define ABS_D		(HI(17) | FMT_D | LO(5))
#define ADD_D		(HI(17) | FMT_D | LO(0))
#define ADDU		(HI(0) | LO(33))
#define ADDIU		(HI(9))
#define AND		(HI(0) | LO(36))
#define ANDI		(HI(12))
#define B		(HI(4))
#define BAL		(HI(1) | (17 << 16))
#define BC1F		(HI(17) | (8 << 21))
#define BC1T		(HI(17) | (8 << 21) | (1 << 16))
#define BEQ		(HI(4))
#define BNE		(HI(5))
#define BREAK		(HI(0) | LO(13))
#define C_UN_D		(HI(17) | FMT_D | LO(49))
#define C_UEQ_D		(HI(17) | FMT_D | LO(51))
#define C_ULT_D		(HI(17) | FMT_D | LO(53))
#define DIV_D		(HI(17) | FMT_D | LO(3))
#define EXT		(HI(31) | LO(0))
#define J		(HI(2))
#define JAL		(HI(3))
#define JALR		(HI(0) | LO(9))
#define JR		(HI(0) | LO(8))
#define LD		(HI(55))
#define LDC1		(HI(53))
#define LUI		(HI(15))
#define LW		(HI(35))
#define NEG_D		(HI(17) | FMT_D | LO(7))
#define MFHI		(HI(0) | LO(16))
#define MFLO		(HI(0) | LO(18))
#define MOV_D		(HI(17) | FMT_D | LO(6))
#define CFC1		(HI(17) | (2 << 21))
#define MOVN		(HI(0) | LO(11))
#define MOVZ		(HI(0) | LO(10))
#define MUL		(HI(28) | LO(2))
#define MUL_D		(HI(17) | FMT_D | LO(2))
#define MULT		(HI(0) | LO(24))
#define NOP		(HI(0) | LO(0))
#define NOR		(HI(0) | LO(39))
#define OR		(HI(0) | LO(37))
#define ORI		(HI(13))
#define SD		(HI(63))
#define SDC1		(HI(61))
#define SEB		(HI(31) | (16 << 6) | LO(32))
#define SEH		(HI(31) | (24 << 6) | LO(32))
#define SLT		(HI(0) | LO(42))
#define SLTIU		(HI(11))
#define SLTU		(HI(0) | LO(43))
#define SLL		(HI(0) | LO(0))
#define SLLV		(HI(0) | LO(4))
#define SRL		(HI(0) | LO(2))
#define SRLV		(HI(0) | LO(6))
#define SRA		(HI(0) | LO(3))
#define SRAV		(HI(0) | LO(7))
#define SUB_D		(HI(17) | FMT_D | LO(1))
#define SUBU		(HI(0) | LO(35))
#define SW		(HI(43))
#define XOR		(HI(0) | LO(38))
#define XORI		(HI(14))

#ifdef SLJIT_CONFIG_MIPS_32
#define ADDU_W		ADDU
#define ADDIU_W		ADDIU
#define EXT_W		EXT
#define SLL_W		SLL
#define SUBU_W		SUBU
#else
#define ADDU_W		DADDU
#define ADDIU_W		DADDIU
#define EXT_W		DEXT
#define SLL_W		DSLL
#define SUBU_W		DSUBU
#endif

#define SIMM_MAX	(0x7fff)
#define SIMM_MIN	(-0x8000)
#define UIMM_MAX	(0xffff)

static SLJIT_CONST sljit_ub reg_map[SLJIT_NO_REGISTERS + 6] = {
  0, 2, 3, 6, 4, 5, 17, 18, 19, 20, 21, 16, 8, 9, 10, 29
};

// dest_reg is the absolute name of the register
// Useful for reordering instructions in the delay slot
static int push_inst(struct sljit_compiler *compiler, sljit_i ins, int delay_slot)
{
	sljit_i *ptr = (sljit_i*)ensure_buf(compiler, sizeof(sljit_i));
	FAIL_IF(!ptr);
	*ptr = ins;
	compiler->size++;
	compiler->delay_slot = delay_slot;
	return SLJIT_SUCCESS;
}

static SLJIT_INLINE sljit_i invert_branch(int flags)
{
	return (flags & IS_ARIT_COND) ? (1 << 26) : (1 << 16);
}

static SLJIT_INLINE sljit_i* optimize_jump(struct sljit_jump *jump, sljit_i *code_ptr, sljit_i *code)
{
	sljit_w diff;
	sljit_uw target_addr;
	sljit_i *inst;
	sljit_i saved_inst;

	if (jump->flags & SLJIT_REWRITABLE_JUMP)
		return code_ptr;

	if (jump->flags & JUMP_ADDR)
		target_addr = jump->target;
	else {
		SLJIT_ASSERT(jump->flags & JUMP_LABEL);
		target_addr = (sljit_uw)(code + jump->label->size);
	}
	inst = (sljit_i*)jump->addr;
	if (jump->flags & IS_COND)
		inst--;

	// B instructions
	if (jump->flags & IS_MOVABLE) {
		diff = ((sljit_w)target_addr - (sljit_w)(inst)) >> 2;
		if (diff <= SIMM_MAX && diff >= SIMM_MIN) {
			jump->flags |= PATCH_B;

			if (!(jump->flags & IS_COND)) {
				inst[0] = inst[-1];
				inst[-1] = (jump->flags & IS_JAL) ? BAL : B;
				jump->addr -= sizeof(sljit_i);
				return inst;
			}
			saved_inst = inst[0];
			inst[0] = inst[-1];
			inst[-1] = saved_inst ^ invert_branch(jump->flags);
			jump->addr -= 2 * sizeof(sljit_i);
			return inst;
		}
	}

	diff = ((sljit_w)target_addr - (sljit_w)(inst + 1)) >> 2;
	if (diff <= SIMM_MAX && diff >= SIMM_MIN) {
		jump->flags |= PATCH_B;

		if (!(jump->flags & IS_COND)) {
			inst[0] = (jump->flags & IS_JAL) ? BAL : B;
			inst[1] = NOP;
			return inst + 1;
		}
		inst[0] = inst[0] ^ invert_branch(jump->flags);
		inst[1] = NOP;
		jump->addr -= sizeof(sljit_i);
		return inst + 1;
	}

	if (jump->flags & IS_COND) {
		if ((target_addr & ~0xfffffff) == ((jump->addr + 3 * sizeof(sljit_i)) & ~0xfffffff)) {
			jump->flags |= PATCH_J;
			inst[0] = (inst[0] & 0xffff0000) | 3;
			inst[1] = NOP;
			inst[2] = J;
			inst[3] = NOP;
			jump->addr += sizeof(sljit_i);
			return inst + 3;
		}
		return code_ptr;
	}

	// J instuctions
	if (jump->flags & IS_MOVABLE) {
		if ((target_addr & ~0xfffffff) == (jump->addr & ~0xfffffff)) {
			jump->flags |= PATCH_J;
			inst[0] = inst[-1];
			inst[-1] = (jump->flags & IS_JAL) ? JAL : J;
			jump->addr -= sizeof(sljit_i);
			return inst;
		}
	}

	if ((target_addr & ~0xfffffff) == ((jump->addr + sizeof(sljit_i)) & ~0xfffffff)) {
		jump->flags |= PATCH_J;
		inst[0] = (jump->flags & IS_JAL) ? JAL : J;
		inst[1] = NOP;
		return inst + 1;
	}

	return code_ptr;
}

#ifdef __GNUC__
static __attribute__ ((noinline)) void sljit_cache_flush(void* code, void* code_ptr)
{
	SLJIT_CACHE_FLUSH(code, code_ptr);
}
#endif

void* sljit_generate_code(struct sljit_compiler *compiler)
{
	struct sljit_memory_fragment *buf;
	sljit_i *code;
	sljit_i *code_ptr;
	sljit_i *buf_ptr;
	sljit_i *buf_end;
	sljit_uw word_count;
	sljit_uw addr;

	struct sljit_label *label;
	struct sljit_jump *jump;
	struct sljit_const *const_;

	FUNCTION_ENTRY();

	SLJIT_ASSERT(compiler->size > 0);
	reverse_buf(compiler);

	code = (sljit_i*)SLJIT_MALLOC_EXEC(compiler->size * sizeof(sljit_uw));
	PTR_FAIL_WITH_EXEC_IF(code);
	buf = compiler->buf;

	code_ptr = code;
	word_count = 0;
	label = compiler->labels;
	jump = compiler->jumps;
	const_ = compiler->consts;
	do {
		buf_ptr = (sljit_i*)buf->memory;
		buf_end = buf_ptr + (buf->used_size >> 2);
		do {
			*code_ptr = *buf_ptr++;
			SLJIT_ASSERT(!label || label->size >= word_count);
			SLJIT_ASSERT(!jump || jump->addr >= word_count);
			SLJIT_ASSERT(!const_ || const_->addr >= word_count);
			// These structures are ordered by their address
			if (label && label->size == word_count) {
				// Just recording the address
				label->addr = (sljit_uw)code_ptr;
				label->size = code_ptr - code;
				label = label->next;
			}
			if (jump && jump->addr == word_count) {
				SLJIT_ASSERT(jump->flags & (JUMP_LABEL | JUMP_ADDR));
#ifdef SLJIT_CONFIG_MIPS_32
				jump->addr = (sljit_uw)(code_ptr - 3);
#else
				jump->addr = (sljit_uw)(code_ptr - 6);
#endif
				code_ptr = optimize_jump(jump, code_ptr, code);
				jump = jump->next;
			}
			if (const_ && const_->addr == word_count) {
				// Just recording the address
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
	SLJIT_ASSERT(code_ptr - code <= (int)compiler->size);

	jump = compiler->jumps;
	while (jump) {
		do {
			addr = (jump->flags & JUMP_LABEL) ? jump->label->addr : jump->target;
			buf_ptr = (sljit_i*)jump->addr;

			if (jump->flags & PATCH_B) {
				addr = (sljit_w)(addr - (jump->addr + sizeof(sljit_i))) >> 2;
				SLJIT_ASSERT((sljit_w)addr <= SIMM_MAX && (sljit_w)addr >= SIMM_MIN);
				buf_ptr[0] = (buf_ptr[0] & 0xffff0000) | (addr & 0xffff);
				break;
			}
			if (jump->flags & PATCH_J) {
				SLJIT_ASSERT((addr & ~0xfffffff) == ((jump->addr + sizeof(sljit_i)) & ~0xfffffff));
				buf_ptr[0] |= (addr >> 2) & 0x0cffffff;
				break;
			}

			// Set the fields of immediate loads
#ifdef SLJIT_CONFIG_MIPS_32
			buf_ptr[0] = (buf_ptr[0] & 0xffff0000) | ((addr >> 16) & 0xffff);
			buf_ptr[1] = (buf_ptr[1] & 0xffff0000) | (addr & 0xffff);
#else
			buf_ptr[0] = (buf_ptr[0] & 0xffff0000) | ((addr >> 48) & 0xffff);
			buf_ptr[1] = (buf_ptr[1] & 0xffff0000) | ((addr >> 32) & 0xffff);
			buf_ptr[3] = (buf_ptr[3] & 0xffff0000) | ((addr >> 16) & 0xffff);
			buf_ptr[4] = (buf_ptr[4] & 0xffff0000) | (addr & 0xffff);
#endif
		} while (0);
		jump = jump->next;
	}

	compiler->error = SLJIT_ERR_COMPILED;
#ifndef __GNUC__
	SLJIT_CACHE_FLUSH(code, code_ptr);
#else
	// GCC workaround for invalid code generation with -O2
	sljit_cache_flush(code, code_ptr);
#endif
	return code;
}

// Creates an index in data_transfer_insts array
#define WORD_DATA	0x00
#define BYTE_DATA	0x01
#define HALF_DATA	0x02
#define INT_DATA	0x03
#define SIGNED_DATA	0x04
#define LOAD_DATA	0x08

#define MEM_MASK	0x0f

#define WRITE_BACK	0x0010
#define CUMULATIVE_OP	0x0020
#define LOGICAL_OP	0x0040
#define IMM_OP		0x0080
#define SRC2_IMM	0x0100

#define UNUSED_DEST	0x0200
#define REG_DEST	0x0400
#define REG1_SOURCE	0x0800
#define REG2_SOURCE	0x1000

// Only these flags are set. UNUSED_DEST is not set when no flags should be set
#define CHECK_FLAGS(list) \
	(!(flags & UNUSED_DEST) || (op & GET_FLAGS(~(list))))

#ifdef SLJIT_CONFIG_MIPS_32
#include "sljitNativeMIPS_32.c"
#else
#include "sljitNativeMIPS_64.c"
#endif

#ifdef SLJIT_CONFIG_MIPS_32
#define STACK_STORE	SW
#define STACK_LOAD	LW
#else
#define STACK_STORE	SD
#define STACK_LOAD	LD
#endif

static int emit_op(struct sljit_compiler *compiler, int op, int inp_flags,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w);

int sljit_emit_enter(struct sljit_compiler *compiler, int args, int temporaries, int generals, int local_size)
{
	sljit_i base;

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

	local_size += (generals + 2 + 4) * sizeof(sljit_w);
	local_size = (local_size + 15) & ~0xf;
	compiler->local_size = local_size;

	if (local_size <= SIMM_MAX) {
		// Frequent case
		FAIL_IF(push_inst(compiler, ADDIU_W | S(REAL_STACK_PTR) | T(REAL_STACK_PTR) | IMM(-local_size), DR(REAL_STACK_PTR)));
		base = S(REAL_STACK_PTR);
	}
	else {
		FAIL_IF(load_immediate(compiler, DR(TMP_REG1), local_size));
		FAIL_IF(push_inst(compiler, ADDU_W | S(REAL_STACK_PTR) | TA(0) | D(TMP_REG2), DR(TMP_REG2)));
		FAIL_IF(push_inst(compiler, SUBU_W | S(REAL_STACK_PTR) | T(TMP_REG1) | D(REAL_STACK_PTR), DR(REAL_STACK_PTR)));
		base = S(TMP_REG2);
		local_size = 0;
	}

	FAIL_IF(push_inst(compiler, STACK_STORE | base | TA(31) | IMM(local_size - 1 * (int)sizeof(sljit_w)), MOVABLE_INS));
	FAIL_IF(push_inst(compiler, STACK_STORE | base | T(SLJIT_LOCALS_REG) | IMM(local_size - 2 * (int)sizeof(sljit_w)), MOVABLE_INS));
	if (generals >= 1)
		FAIL_IF(push_inst(compiler, STACK_STORE | base | T(SLJIT_GENERAL_REG1) | IMM(local_size - 3 * (int)sizeof(sljit_w)), MOVABLE_INS));
	if (generals >= 2)
		FAIL_IF(push_inst(compiler, STACK_STORE | base | T(SLJIT_GENERAL_REG2) | IMM(local_size - 4 * (int)sizeof(sljit_w)), MOVABLE_INS));
	if (generals >= 3)
		FAIL_IF(push_inst(compiler, STACK_STORE | base | T(SLJIT_GENERAL_REG3) | IMM(local_size - 5 * (int)sizeof(sljit_w)), MOVABLE_INS));
	if (generals >= 4)
		FAIL_IF(push_inst(compiler, STACK_STORE | base | T(SLJIT_GENERAL_EREG1) | IMM(local_size - 6 * (int)sizeof(sljit_w)), MOVABLE_INS));
	if (generals >= 5)
		FAIL_IF(push_inst(compiler, STACK_STORE | base | T(SLJIT_GENERAL_EREG2) | IMM(local_size - 7 * (int)sizeof(sljit_w)), MOVABLE_INS));

	FAIL_IF(push_inst(compiler, ADDIU_W | S(REAL_STACK_PTR) | T(SLJIT_LOCALS_REG) | IMM(4 * sizeof(sljit_w)), DR(SLJIT_LOCALS_REG)));

	if (args >= 1)
		FAIL_IF(push_inst(compiler, ADDU_W | SA(4) | TA(0) | D(SLJIT_GENERAL_REG1), DR(SLJIT_GENERAL_REG1)));
	if (args >= 2)
		FAIL_IF(push_inst(compiler, ADDU_W | SA(5) | TA(0) | D(SLJIT_GENERAL_REG2), DR(SLJIT_GENERAL_REG2)));
	if (args >= 3)
		FAIL_IF(push_inst(compiler, ADDU_W | SA(6) | TA(0) | D(SLJIT_GENERAL_REG3), DR(SLJIT_GENERAL_REG3)));

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

	local_size += (generals + 2 + 4) * sizeof(sljit_w);
	compiler->local_size = (local_size + 15) & ~0xf;
}

int sljit_emit_return(struct sljit_compiler *compiler, int src, sljit_w srcw)
{
	int local_size;
	sljit_i base;

	FUNCTION_ENTRY();
#ifdef SLJIT_DEBUG
	if (src != SLJIT_UNUSED) {
		FUNCTION_CHECK_SRC(src, srcw);
	}
	else
		SLJIT_ASSERT(srcw == 0);
#endif

	sljit_emit_return_verbose();

	local_size = compiler->local_size;

	if (src != SLJIT_PREF_RET_REG && src != SLJIT_UNUSED)
		FAIL_IF(emit_op(compiler, SLJIT_MOV, WORD_DATA, SLJIT_PREF_RET_REG, 0, TMP_REG1, 0, src, srcw));

	if (local_size <= SIMM_MAX)
		base = S(REAL_STACK_PTR);
	else {
		FAIL_IF(load_immediate(compiler, DR(TMP_REG1), local_size));
		FAIL_IF(push_inst(compiler, ADDU_W | S(REAL_STACK_PTR) | T(TMP_REG1) | D(TMP_REG1), DR(TMP_REG1)));
		base = S(TMP_REG1);
		local_size = 0;
	}

	FAIL_IF(push_inst(compiler, STACK_LOAD | base | TA(31) | IMM(local_size - 1 * (int)sizeof(sljit_w)), 31));
	if (compiler->generals >= 5)
		FAIL_IF(push_inst(compiler, STACK_LOAD | base | T(SLJIT_GENERAL_EREG2) | IMM(local_size - 7 * (int)sizeof(sljit_w)), DR(SLJIT_GENERAL_EREG2)));
	if (compiler->generals >= 4)
		FAIL_IF(push_inst(compiler, STACK_LOAD | base | T(SLJIT_GENERAL_EREG1) | IMM(local_size - 6 * (int)sizeof(sljit_w)), DR(SLJIT_GENERAL_EREG1)));
	if (compiler->generals >= 3)
		FAIL_IF(push_inst(compiler, STACK_LOAD | base | T(SLJIT_GENERAL_REG3) | IMM(local_size - 5 * (int)sizeof(sljit_w)), DR(SLJIT_GENERAL_REG3)));
	if (compiler->generals >= 2)
		FAIL_IF(push_inst(compiler, STACK_LOAD | base | T(SLJIT_GENERAL_REG2) | IMM(local_size - 4 * (int)sizeof(sljit_w)), DR(SLJIT_GENERAL_REG2)));
	if (compiler->generals >= 1)
		FAIL_IF(push_inst(compiler, STACK_LOAD | base | T(SLJIT_GENERAL_REG1) | IMM(local_size - 3 * (int)sizeof(sljit_w)), DR(SLJIT_GENERAL_REG1)));
	FAIL_IF(push_inst(compiler, STACK_LOAD | base | T(SLJIT_LOCALS_REG) | IMM(local_size - 2 * (int)sizeof(sljit_w)), DR(SLJIT_LOCALS_REG)));

	FAIL_IF(push_inst(compiler, JR | SA(31), UNMOVABLE_INS));
	if (compiler->local_size <= SIMM_MAX)
		return push_inst(compiler, ADDIU_W | S(REAL_STACK_PTR) | T(REAL_STACK_PTR) | IMM(compiler->local_size), UNMOVABLE_INS);
	else
		return push_inst(compiler, ADDU_W | S(TMP_REG1) | TA(0) | D(REAL_STACK_PTR), UNMOVABLE_INS);
}

#undef STACK_STORE
#undef STACK_LOAD

// ---------------------------------------------------------------------
//  Operators
// ---------------------------------------------------------------------

#ifdef SLJIT_CONFIG_MIPS_32
#define ARCH_DEPEND(a, b)	a
#else
#define ARCH_DEPEND(a, b)	b
#endif

static SLJIT_CONST sljit_i data_transfer_insts[16] = {
/* s u w */ ARCH_DEPEND(HI(43) /* sw */, HI(63) /* sd */),
/* s u b */ HI(40) /* sb */,
/* s u h */ HI(41) /* sh*/,
/* s u i */ HI(43) /* sw */,

/* s s w */ ARCH_DEPEND(HI(43) /* sw */, HI(63) /* sd */),
/* s s b */ HI(40) /* sb */,
/* s s h */ HI(41) /* sh*/,
/* s s i */ HI(43) /* sw */,

/* l u w */ ARCH_DEPEND(HI(35) /* lw */, HI(55) /* ld */),
/* l u b */ HI(36) /* lbu */,
/* l u h */ HI(37) /* lhu */,
/* l u i */ ARCH_DEPEND(HI(35) /* lw */, HI(39) /* lwu */),

/* l s w */ ARCH_DEPEND(HI(35) /* lw */, HI(55) /* ld */),
/* l s b */ HI(32) /* lb */,
/* l s h */ HI(33) /* lh */,
/* l s i */ HI(35) /* lw */,
};

// dsta is an absoulute dst!
static int emit_op_mem(struct sljit_compiler *compiler, int inp_flags, int dst_ar, int base, sljit_w offset)
{
	int tmp_ar;
	int hi_reg;

	SLJIT_ASSERT(base & SLJIT_MEM);
	tmp_ar = (inp_flags & LOAD_DATA) ? dst_ar : DR(TMP_REG3);
	hi_reg = (base >> 4) & 0xf;
	base &= 0xf;

	if (SLJIT_UNLIKELY(hi_reg)) {
		offset &= 0x3;
		if (SLJIT_UNLIKELY(offset))
			FAIL_IF(push_inst(compiler, SLL_W | T(hi_reg) | DA(TMP_EREG1) | SH_IMM(offset), TMP_EREG1));

		if (!(inp_flags & WRITE_BACK)) {
			FAIL_IF(push_inst(compiler, ADDU_W | S(base) | (!offset ? T(hi_reg) : TA(TMP_EREG1)) | DA(tmp_ar), tmp_ar));
			return push_inst(compiler, data_transfer_insts[inp_flags & MEM_MASK] | SA(tmp_ar) | TA(dst_ar), (inp_flags & LOAD_DATA) ? dst_ar : MOVABLE_INS);
		}

		if (dst_ar == DR(base)) {
			SLJIT_ASSERT(!(inp_flags & LOAD_DATA));
			FAIL_IF(push_inst(compiler, ADDU_W | SA(dst_ar) | TA(0) | D(TMP_REG1), DR(TMP_REG1)));
			dst_ar = DR(TMP_REG1);
		}

		FAIL_IF(push_inst(compiler, ADDU_W | S(base) | (!offset ? T(hi_reg) : TA(TMP_EREG1)) | D(base), DR(base)));
		return push_inst(compiler, data_transfer_insts[inp_flags & MEM_MASK] | S(base) | TA(dst_ar), (inp_flags & LOAD_DATA) ? dst_ar : MOVABLE_INS);
	}

	if (!(inp_flags & WRITE_BACK) || !(base)) {
		if (offset <= SIMM_MAX && offset >= SIMM_MIN)
			return push_inst(compiler, data_transfer_insts[inp_flags & MEM_MASK] | S(base) | TA(dst_ar) | IMM(offset), (inp_flags & LOAD_DATA) ? dst_ar : MOVABLE_INS);

		if (!(offset & 0x8000))
			FAIL_IF(load_immediate(compiler, tmp_ar, offset & ~0xffff));
		else
			FAIL_IF(load_immediate(compiler, tmp_ar, (offset + 0x10000) & ~0xffff));

		if (base)
			FAIL_IF(push_inst(compiler, ADDU_W | S(base) | TA(tmp_ar) | DA(tmp_ar), tmp_ar));

		return push_inst(compiler, data_transfer_insts[inp_flags & MEM_MASK] | SA(tmp_ar) | TA(dst_ar) | IMM(offset), (inp_flags & LOAD_DATA) ? dst_ar : MOVABLE_INS);
	}

	if (dst_ar == DR(base)) {
		SLJIT_ASSERT(!(inp_flags & LOAD_DATA));
		FAIL_IF(push_inst(compiler, ADDU_W | SA(dst_ar) | TA(0) | D(TMP_REG1), DR(TMP_REG1)));
		dst_ar = DR(TMP_REG1);
	}

	if (offset <= SIMM_MAX && offset >= SIMM_MIN)
		FAIL_IF(push_inst(compiler, ADDIU_W | S(base) | T(base) | IMM(offset), DR(base)));
	else {
		FAIL_IF(load_immediate(compiler, tmp_ar, offset));
		FAIL_IF(push_inst(compiler, ADDU_W | S(base) | TA(tmp_ar) | D(base), DR(base)));
	}
	return push_inst(compiler, data_transfer_insts[inp_flags & MEM_MASK] | S(base) | TA(dst_ar), (inp_flags & LOAD_DATA) ? dst_ar : MOVABLE_INS);
}

static int emit_op(struct sljit_compiler *compiler, int op, int flags,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	// arg1 goes to TMP_REG1 or src reg
	// arg2 goes to TMP_REG2, imm or src reg
	// TMP_REG3 can be used for caching
	// result goes to TMP_REG2, so put result can use TMP_REG1 and TMP_REG3
	int dst_r;
	int src1_r;
	sljit_w src2_r = 0;
	int sugg_src2_r = TMP_REG2;

	compiler->cache_arg = 0;
	compiler->cache_argw = 0;

	if (dst >= SLJIT_TEMPORARY_REG1 && dst <= TMP_REG3) {
		dst_r = dst;
		flags |= REG_DEST;
		if (GET_OPCODE(op) >= SLJIT_MOV && GET_OPCODE(op) <= SLJIT_MOVU_SI)
			sugg_src2_r = dst_r;
	}
	else if (dst == SLJIT_UNUSED) {
		if (op >= SLJIT_MOV && op <= SLJIT_MOVU_SI && !(src2 & SLJIT_MEM))
			return SLJIT_SUCCESS;
		if (GET_FLAGS(op))
			flags |= UNUSED_DEST;
		dst_r = TMP_REG2;
	}
	else
		dst_r = TMP_REG2;

	if (flags & IMM_OP) {
		if ((src2 & SLJIT_IMM) && src2w) {
			if ((!(flags & LOGICAL_OP) && (src2w <= SIMM_MAX && src2w >= SIMM_MIN))
				|| ((flags & LOGICAL_OP) && !(src2w & ~UIMM_MAX))) {
				flags |= SRC2_IMM;
				src2_r = src2w;
			}
		}
		if ((src1 & SLJIT_IMM) && src1w && (flags & CUMULATIVE_OP) && !(flags & SRC2_IMM)) {
			if ((!(flags & LOGICAL_OP) && (src1w <= SIMM_MAX && src1w >= SIMM_MIN))
				|| ((flags & LOGICAL_OP) && !(src1w & ~UIMM_MAX))) {
				flags |= SRC2_IMM;
				src2_r = src1w;

				// And swap arguments
				src1 = src2;
				src1w = src2w;
				src2 = SLJIT_IMM;
				// src2w = src2_r unneeded
			}
		}
	}

	// Source 1
	if (src1 >= SLJIT_TEMPORARY_REG1 && src1 <= TMP_REG3) {
		src1_r = src1;
		flags |= REG1_SOURCE;
	}
	else if (src1 & SLJIT_IMM) {
		if (src1w) {
			FAIL_IF(load_immediate(compiler, DR(TMP_REG1), src1w));
			src1_r = TMP_REG1;
		}
		else
			src1_r = 0;
	}
	else {
		FAIL_IF(emit_op_mem(compiler, flags | LOAD_DATA, DR(TMP_REG1), src1, src1w));
		src1_r = TMP_REG1;
	}

	// Source 2
	if (src2 >= SLJIT_TEMPORARY_REG1 && src2 <= TMP_REG3) {
		src2_r = src2;
		flags |= REG2_SOURCE;
		if (!(flags & REG_DEST) && GET_OPCODE(op) >= SLJIT_MOV && GET_OPCODE(op) <= SLJIT_MOVU_SI)
			dst_r = src2_r;
	}
	else if (src2 & SLJIT_IMM) {
		if (!(flags & SRC2_IMM)) {
			if (src2w || (GET_OPCODE(op) >= SLJIT_MOV && GET_OPCODE(op) <= SLJIT_MOVU_SI)) {
				FAIL_IF(load_immediate(compiler, DR(sugg_src2_r), src2w));
				src2_r = sugg_src2_r;
			}
			else
				src2_r = 0;
		}
	}
	else {
		FAIL_IF(emit_op_mem(compiler, flags | LOAD_DATA, DR(sugg_src2_r), src2, src2w));
		src2_r = sugg_src2_r;
	}

	FAIL_IF(emit_single_op(compiler, op, flags, dst_r, src1_r, src2_r));

	if (dst & SLJIT_MEM)
		return emit_op_mem(compiler, flags, DR(dst_r), dst, dstw);

	return SLJIT_SUCCESS;
}

int sljit_emit_op0(struct sljit_compiler *compiler, int op)
{
	FUNCTION_ENTRY();

	SLJIT_ASSERT(GET_OPCODE(op) >= SLJIT_DEBUGGER && GET_OPCODE(op) <= SLJIT_NOP);
	sljit_emit_op0_verbose();

	op = GET_OPCODE(op);
	switch (op) {
	case SLJIT_DEBUGGER:
		return push_inst(compiler, BREAK, UNMOVABLE_INS);
	case SLJIT_NOP:
		return push_inst(compiler, NOP, UNMOVABLE_INS);
	}

	return SLJIT_SUCCESS;
}

int sljit_emit_op1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
#ifdef SLJIT_CONFIG_MIPS_32
	#define inp_flags 0
#endif

	FUNCTION_ENTRY();

	SLJIT_ASSERT(GET_OPCODE(op) >= SLJIT_MOV && GET_OPCODE(op) <= SLJIT_NEG);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_OP();
	FUNCTION_CHECK_SRC(src, srcw);
	FUNCTION_CHECK_DST(dst, dstw);
	FUNCTION_CHECK_OP1();
#endif
	sljit_emit_op1_verbose();

	SLJIT_ASSERT(SLJIT_MOV + 7 == SLJIT_MOVU);

	switch (GET_OPCODE(op)) {
	case SLJIT_MOV:
		return emit_op(compiler, SLJIT_MOV, inp_flags | WORD_DATA, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOV_UI:
		return emit_op(compiler, SLJIT_MOV_UI, inp_flags | INT_DATA, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOV_SI:
		return emit_op(compiler, SLJIT_MOV_SI, inp_flags | INT_DATA | SIGNED_DATA, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOV_UB:
		return emit_op(compiler, SLJIT_MOV_UB, inp_flags | BYTE_DATA, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (unsigned char)srcw : srcw);

	case SLJIT_MOV_SB:
		return emit_op(compiler, SLJIT_MOV_SB, inp_flags | BYTE_DATA | SIGNED_DATA, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (signed char)srcw : srcw);

	case SLJIT_MOV_UH:
		return emit_op(compiler, SLJIT_MOV_UH, inp_flags | HALF_DATA, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (unsigned short)srcw : srcw);

	case SLJIT_MOV_SH:
		return emit_op(compiler, SLJIT_MOV_SH, inp_flags | HALF_DATA | SIGNED_DATA, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (signed short)srcw : srcw);

	case SLJIT_MOVU:
		return emit_op(compiler, SLJIT_MOV, inp_flags | WORD_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOVU_UI:
		return emit_op(compiler, SLJIT_MOV_UI, inp_flags | INT_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOVU_SI:
		return emit_op(compiler, SLJIT_MOV_SI, inp_flags | INT_DATA | SIGNED_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOVU_UB:
		return emit_op(compiler, SLJIT_MOV_UB, inp_flags | BYTE_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (unsigned char)srcw : srcw);

	case SLJIT_MOVU_SB:
		return emit_op(compiler, SLJIT_MOV_SB, inp_flags | BYTE_DATA | SIGNED_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (signed char)srcw : srcw);

	case SLJIT_MOVU_UH:
		return emit_op(compiler, SLJIT_MOV_UH, inp_flags | HALF_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (unsigned short)srcw : srcw);

	case SLJIT_MOVU_SH:
		return emit_op(compiler, SLJIT_MOV_SH, inp_flags | HALF_DATA | SIGNED_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (signed short)srcw : srcw);

	case SLJIT_NOT:
		return emit_op(compiler, op, inp_flags, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_NEG:
		return emit_op(compiler, SLJIT_SUB | GET_ALL_FLAGS(op), inp_flags | IMM_OP, dst, dstw, SLJIT_IMM, 0, src, srcw);
	}

	return SLJIT_SUCCESS;
#ifdef SLJIT_CONFIG_MIPS_32
	#undef inp_flags
#endif
}

int sljit_emit_op2(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
#ifdef SLJIT_CONFIG_MIPS_32
	#define inp_flags 0
#endif

	FUNCTION_ENTRY();

	SLJIT_ASSERT(GET_OPCODE(op) >= SLJIT_ADD && GET_OPCODE(op) <= SLJIT_ASHR);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_OP();
	FUNCTION_CHECK_SRC(src1, src1w);
	FUNCTION_CHECK_SRC(src2, src2w);
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_op2_verbose();

	switch (GET_OPCODE(op)) {
	case SLJIT_ADD:
	case SLJIT_ADDC:
		return emit_op(compiler, op, inp_flags | CUMULATIVE_OP | IMM_OP, dst, dstw, src1, src1w, src2, src2w);

	case SLJIT_SUB:
	case SLJIT_SUBC:
		return emit_op(compiler, op, inp_flags | IMM_OP, dst, dstw, src1, src1w, src2, src2w);

	case SLJIT_MUL:
		return emit_op(compiler, op, inp_flags | CUMULATIVE_OP, dst, dstw, src1, src1w, src2, src2w);

	case SLJIT_AND:
	case SLJIT_OR:
	case SLJIT_XOR:
		return emit_op(compiler, op, inp_flags | CUMULATIVE_OP | LOGICAL_OP | IMM_OP, dst, dstw, src1, src1w, src2, src2w);

	case SLJIT_SHL:
	case SLJIT_LSHR:
	case SLJIT_ASHR:
#ifdef SLJIT_CONFIG_MIPS_32
		if (src2 & SLJIT_IMM)
			src2w &= 0x1f;
#else
		if (src2 & SLJIT_IMM)
			src2w &= 0x3f;
#endif
		return emit_op(compiler, op, inp_flags | IMM_OP, dst, dstw, src1, src1w, src2, src2w);
	}

	return SLJIT_SUCCESS;
#ifdef SLJIT_CONFIG_MIPS_32
	#undef inp_flags
#endif
}

// ---------------------------------------------------------------------
//  Floating point operators
// ---------------------------------------------------------------------

int sljit_is_fpu_available(void)
{
#if 0
	sljit_w fir;
	asm ("cfc1 %0, $0" : "=r"(fir));
	return (fir >> 22) & 0x1;
#endif
	// Qemu says fir is 0 by default
	return 1;
}

static int emit_fpu_data_transfer(struct sljit_compiler *compiler, int fpu_reg, int load, int arg, sljit_w argw)
{
	int hi_reg;

	SLJIT_ASSERT(arg & SLJIT_MEM);

	// Fast loads and stores
	if (!(arg & 0xf0)) {
		// Both for (arg & 0xf) == SLJIT_UNUSED and (arg & 0xf) != SLJIT_UNUSED
		if (argw <= SIMM_MAX && argw >= SIMM_MIN)
			return push_inst(compiler, (load ? LDC1 : SDC1) | S(arg & 0xf) | FT(fpu_reg) | IMM(argw), MOVABLE_INS);
	}

	if (arg & 0xf0) {
		argw &= 0x3;
		hi_reg = (arg >> 4) & 0xf;
		if (argw) {
			FAIL_IF(push_inst(compiler, SLL_W | T(hi_reg) | D(TMP_REG1) | SH_IMM(argw), DR(TMP_REG1)));
			hi_reg = TMP_REG1;
		}
		FAIL_IF(push_inst(compiler, ADDU_W | S(hi_reg) | T(arg & 0xf) | D(TMP_REG1), DR(TMP_REG1)));
		return push_inst(compiler, (load ? LDC1 : SDC1) | S(TMP_REG1) | FT(fpu_reg) | IMM(0), MOVABLE_INS);
	}

	// Use cache
	if (compiler->cache_arg == arg && argw - compiler->cache_argw <= SIMM_MAX && argw - compiler->cache_argw >= SIMM_MIN)
		return push_inst(compiler, (load ? LDC1 : SDC1) | S(TMP_REG3) | FT(fpu_reg) | IMM(argw - compiler->cache_argw), MOVABLE_INS);

	// Put value to cache
	compiler->cache_arg = arg;
	compiler->cache_argw = argw;

	FAIL_IF(load_immediate(compiler, DR(TMP_REG3), argw));
	if (arg & 0xf)
		FAIL_IF(push_inst(compiler, ADDU_W | S(TMP_REG3) | T(arg & 0xf) | D(TMP_REG3), DR(TMP_REG3)));
	return push_inst(compiler, (load ? LDC1 : SDC1) | S(TMP_REG3) | FT(fpu_reg) | IMM(0), MOVABLE_INS);
}

int sljit_emit_fop1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
	int dst_fr;

	FUNCTION_ENTRY();

	SLJIT_ASSERT(sljit_is_fpu_available());
	SLJIT_ASSERT(GET_OPCODE(op) >= SLJIT_FCMP && GET_OPCODE(op) <= SLJIT_FABS);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_OP();
	FUNCTION_FCHECK(src, srcw);
	FUNCTION_FCHECK(dst, dstw);
#endif
	sljit_emit_fop1_verbose();

	compiler->cache_arg = 0;
	compiler->cache_argw = 0;

	if (GET_OPCODE(op) == SLJIT_FCMP) {
		if (dst > SLJIT_FLOAT_REG4) {
			FAIL_IF(emit_fpu_data_transfer(compiler, TMP_FREG1, 1, dst, dstw));
			dst = TMP_FREG1;
		}
		if (src > SLJIT_FLOAT_REG4) {
			FAIL_IF(emit_fpu_data_transfer(compiler, TMP_FREG2, 1, src, srcw));
			src = TMP_FREG2;
		}

		// src and dst swapped
		if (op & SLJIT_SET_E)
			FAIL_IF(push_inst(compiler, C_UEQ_D | FT(src) | FS(dst) | (EQUAL_BIT << 8), FCSR_FCC + EQUAL_BIT));
		if (op & SLJIT_SET_S) {
			FAIL_IF(push_inst(compiler, C_ULT_D | FT(src) | FS(dst) | (LESS_BIT << 8), FCSR_FCC + LESS_BIT));
			FAIL_IF(push_inst(compiler, C_ULT_D | FT(dst) | FS(src) | (GREATER_BIT << 8), FCSR_FCC + GREATER_BIT));
		}
		return push_inst(compiler, C_UN_D | FT(src) | FS(dst) | (UNORD_BIT << 8), FCSR_FCC + UNORD_BIT);
	}

	dst_fr = (dst > SLJIT_FLOAT_REG4) ? TMP_FREG1 : dst;

	if (src > SLJIT_FLOAT_REG4) {
		FAIL_IF(emit_fpu_data_transfer(compiler, dst_fr, 1, src, srcw));
		src = dst_fr;
	}

	switch (op) {
		case SLJIT_FMOV:
			if (src != dst_fr && dst_fr != TMP_FREG1)
				FAIL_IF(push_inst(compiler, MOV_D | FS(src) | FD(dst_fr), MOVABLE_INS));
			break;
		case SLJIT_FNEG:
			FAIL_IF(push_inst(compiler, NEG_D | FS(src) | FD(dst_fr), MOVABLE_INS));
			break;
		case SLJIT_FABS:
			FAIL_IF(push_inst(compiler, ABS_D | FS(src) | FD(dst_fr), MOVABLE_INS));
			break;
	}

	if (dst_fr == TMP_FREG1)
		FAIL_IF(emit_fpu_data_transfer(compiler, src, 0, dst, dstw));

	return SLJIT_SUCCESS;
}

int sljit_emit_fop2(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	int dst_fr;

	FUNCTION_ENTRY();

	SLJIT_ASSERT(sljit_is_fpu_available());
	SLJIT_ASSERT(GET_OPCODE(op) >= SLJIT_FADD && GET_OPCODE(op) <= SLJIT_FDIV);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_OP();
	FUNCTION_FCHECK(src1, src1w);
	FUNCTION_FCHECK(src2, src2w);
	FUNCTION_FCHECK(dst, dstw);
#endif
	sljit_emit_fop2_verbose();

	compiler->cache_arg = 0;
	compiler->cache_argw = 0;

	dst_fr = (dst > SLJIT_FLOAT_REG4) ? TMP_FREG1 : dst;

	if (src2 > SLJIT_FLOAT_REG4) {
		FAIL_IF(emit_fpu_data_transfer(compiler, TMP_FREG2, 1, src2, src2w));
		src2 = TMP_FREG2;
	}

	if (src1 > SLJIT_FLOAT_REG4) {
		FAIL_IF(emit_fpu_data_transfer(compiler, TMP_FREG1, 1, src1, src1w));
		src1 = TMP_FREG1;
	}

	switch (op) {
	case SLJIT_FADD:
		FAIL_IF(push_inst(compiler, ADD_D | FT(src2) | FS(src1) | FD(dst_fr), MOVABLE_INS));
		break;

	case SLJIT_FSUB:
		FAIL_IF(push_inst(compiler, SUB_D | FT(src2) | FS(src1) | FD(dst_fr), MOVABLE_INS));
		break;

	case SLJIT_FMUL:
		FAIL_IF(push_inst(compiler, MUL_D | FT(src2) | FS(src1) | FD(dst_fr), MOVABLE_INS));
		break;

	case SLJIT_FDIV:
		FAIL_IF(push_inst(compiler, DIV_D | FT(src2) | FS(src1) | FD(dst_fr), MOVABLE_INS));
		break;
	}

	if (dst_fr == TMP_FREG1)
		FAIL_IF(emit_fpu_data_transfer(compiler, TMP_FREG1, 0, dst, dstw));

	return SLJIT_SUCCESS;
}

// ---------------------------------------------------------------------
//  Conditional instructions
// ---------------------------------------------------------------------

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
	compiler->delay_slot = UNMOVABLE_INS;
	return label;
}

#ifdef SLJIT_CONFIG_MIPS_32
#define JUMP_LENGTH	4
#else
#define JUMP_LENGTH	7
#endif

#define BR_Z(src) \
	inst = BEQ | SA(src) | TA(0) | JUMP_LENGTH; \
	flags = IS_ARIT_COND; \
	delay_check = src;

#define BR_NZ(src) \
	inst = BNE | SA(src) | TA(0) | JUMP_LENGTH; \
	flags = IS_ARIT_COND; \
	delay_check = src;

#define BR_T(bit) \
	inst = BC1T | (bit << 18) | JUMP_LENGTH; \
	flags = IS_FLOAT_COND; \
	delay_check = FCSR_FCC + bit;

#define BR_F(bit) \
	inst = BC1F | (bit << 18) | JUMP_LENGTH; \
	flags = IS_FLOAT_COND; \
	delay_check = FCSR_FCC + bit;

struct sljit_jump* sljit_emit_jump(struct sljit_compiler *compiler, int type)
{
	struct sljit_jump *jump;
	sljit_i inst;
	int flags = 0;
	int delay_check = UNMOVABLE_INS;

	FUNCTION_ENTRY();
	SLJIT_ASSERT((type & ~0x1ff) == 0);
	SLJIT_ASSERT((type & 0xff) >= SLJIT_C_EQUAL && (type & 0xff) <= SLJIT_CALL3);

	sljit_emit_jump_verbose();

	jump = (struct sljit_jump*)ensure_abuf(compiler, sizeof(struct sljit_jump));
	PTR_FAIL_IF(!jump);

	jump->next = NULL;
	jump->flags = (type & SLJIT_REWRITABLE_JUMP);
	type &= 0xff;
	if (compiler->last_jump)
		compiler->last_jump->next = jump;
	else
		compiler->jumps = jump;
	compiler->last_jump = jump;

	switch (type) {
	case SLJIT_C_EQUAL:
		BR_NZ(EQUAL_FLAG);
		break;
	case SLJIT_C_NOT_EQUAL:
		BR_Z(EQUAL_FLAG);
		break;
	case SLJIT_C_LESS:
		BR_Z(ULESS_FLAG);
		break;
	case SLJIT_C_NOT_LESS:
		BR_NZ(ULESS_FLAG);
		break;
	case SLJIT_C_GREATER:
		BR_Z(UGREATER_FLAG);
		break;
	case SLJIT_C_NOT_GREATER:
		BR_NZ(UGREATER_FLAG);
		break;
	case SLJIT_C_SIG_LESS:
		BR_Z(LESS_FLAG);
		break;
	case SLJIT_C_SIG_NOT_LESS:
		BR_NZ(LESS_FLAG);
		break;
	case SLJIT_C_SIG_GREATER:
		BR_Z(GREATER_FLAG);
		break;
	case SLJIT_C_SIG_NOT_GREATER:
		BR_NZ(GREATER_FLAG);
		break;
	case SLJIT_C_OVERFLOW:
	case SLJIT_C_MUL_OVERFLOW:
		BR_Z(OVERFLOW_FLAG);
		break;
	case SLJIT_C_NOT_OVERFLOW:
	case SLJIT_C_MUL_NOT_OVERFLOW:
		BR_NZ(OVERFLOW_FLAG);
		break;
	case SLJIT_C_FLOAT_EQUAL:
		BR_F(EQUAL_BIT);
		break;
	case SLJIT_C_FLOAT_NOT_EQUAL:
		BR_T(EQUAL_BIT);
		break;
	case SLJIT_C_FLOAT_LESS:
		BR_F(LESS_BIT);
		break;
	case SLJIT_C_FLOAT_NOT_LESS:
		BR_T(LESS_BIT);
		break;
	case SLJIT_C_FLOAT_GREATER:
		BR_F(GREATER_BIT);
		break;
	case SLJIT_C_FLOAT_NOT_GREATER:
		BR_T(GREATER_BIT);
		break;
	case SLJIT_C_FLOAT_NAN:
		BR_F(UNORD_BIT);
		break;
	case SLJIT_C_FLOAT_NOT_NAN:
		BR_T(UNORD_BIT);
		break;
	default:
		// Not conditional branch
		inst = 0;
		break;
	}

	jump->flags |= flags;
	if (compiler->delay_slot == MOVABLE_INS || (compiler->delay_slot != UNMOVABLE_INS && compiler->delay_slot != delay_check))
		jump->flags |= IS_MOVABLE;

	if (inst)
		PTR_FAIL_IF(push_inst(compiler, inst, UNMOVABLE_INS));

	if (type >= SLJIT_CALL1)
		PTR_FAIL_IF(push_inst(compiler, ADDU_W | S(SLJIT_TEMPORARY_REG1) | TA(0) | DA(4), 4));
	if (type >= SLJIT_CALL2)
		PTR_FAIL_IF(push_inst(compiler, ADDU_W | S(SLJIT_TEMPORARY_REG2) | TA(0) | DA(5), 5));

	PTR_FAIL_IF(emit_const(compiler, TMP_REG2, 0));
	if (type <= SLJIT_JUMP)
		PTR_FAIL_IF(push_inst(compiler, JR | S(TMP_REG2), UNMOVABLE_INS));
	else {
		jump->flags |= IS_JAL;
		PTR_FAIL_IF(push_inst(compiler, JALR | S(TMP_REG2) | DA(31), UNMOVABLE_INS));
	}
	jump->addr = compiler->size;
	PTR_FAIL_IF(push_inst(compiler, NOP, UNMOVABLE_INS));
	return jump;
}

#undef JUMP_LENGTH
#undef BR_Z
#undef BR_NZ
#undef BR_T
#undef BR_F

int sljit_emit_ijump(struct sljit_compiler *compiler, int type, int src, sljit_w srcw)
{
	int src_r = TMP_REG2;
	struct sljit_jump *jump = NULL;

	FUNCTION_ENTRY();
	SLJIT_ASSERT(type >= SLJIT_JUMP && type <= SLJIT_CALL3);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_SRC(src, srcw);
#endif
	sljit_emit_ijump_verbose();

	if (src >= SLJIT_TEMPORARY_REG1 && src <= SLJIT_NO_REGISTERS) {
		if (DR(src) < 4 || DR(src) > 6)
			src_r = src;
		else
			FAIL_IF(push_inst(compiler, ADDU_W | S(src) | TA(0) | D(TMP_REG2), DR(TMP_REG2)));
	}

	if (type >= SLJIT_CALL1)
		FAIL_IF(push_inst(compiler, ADDU_W | S(SLJIT_TEMPORARY_REG1) | TA(0) | DA(4), 4));
	if (type >= SLJIT_CALL2)
		FAIL_IF(push_inst(compiler, ADDU_W | S(SLJIT_TEMPORARY_REG2) | TA(0) | DA(5), 5));

	if (src & SLJIT_IMM) {
		jump = (struct sljit_jump*)ensure_abuf(compiler, sizeof(struct sljit_jump));
		FAIL_IF(!jump);

		jump->next = NULL;
		jump->flags = JUMP_ADDR | ((type >= SLJIT_CALL0) ? IS_JAL : 0);
		jump->target = srcw;
		if (compiler->last_jump)
			compiler->last_jump->next = jump;
		else
			compiler->jumps = jump;
		compiler->last_jump = jump;

		if (compiler->delay_slot != UNMOVABLE_INS)
			jump->flags |= IS_MOVABLE;

		FAIL_IF(emit_const(compiler, TMP_REG2, 0));
	}
	else if (src & SLJIT_MEM)
		FAIL_IF(emit_op(compiler, SLJIT_MOV, WORD_DATA, TMP_REG2, 0, TMP_REG1, 0, src, srcw));

	if (type <= SLJIT_JUMP)
		FAIL_IF(push_inst(compiler, JR | S(src_r), UNMOVABLE_INS));
	else
		FAIL_IF(push_inst(compiler, JALR | S(src_r) | DA(31), UNMOVABLE_INS));
	if (jump)
		jump->addr = compiler->size;
	FAIL_IF(push_inst(compiler, NOP, UNMOVABLE_INS));
	return SLJIT_SUCCESS;
}

int sljit_emit_cond_set(struct sljit_compiler *compiler, int dst, sljit_w dstw, int type)
{
	int sugg_dst_ar, dst_ar;

	FUNCTION_ENTRY();
	SLJIT_ASSERT(type >= SLJIT_C_EQUAL && type < SLJIT_JUMP);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_set_cond_verbose();

	if (dst == SLJIT_UNUSED)
		return SLJIT_SUCCESS;

	sugg_dst_ar = DR((dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_NO_REGISTERS) ? dst : TMP_REG2);

	switch (type) {
	case SLJIT_C_EQUAL:
	case SLJIT_C_NOT_EQUAL:
		FAIL_IF(push_inst(compiler, SLTIU | SA(EQUAL_FLAG) | TA(sugg_dst_ar) | IMM(1), sugg_dst_ar));
		dst_ar = sugg_dst_ar;
		break;
	case SLJIT_C_LESS:
	case SLJIT_C_NOT_LESS:
		dst_ar = ULESS_FLAG;
		break;
	case SLJIT_C_GREATER:
	case SLJIT_C_NOT_GREATER:
		dst_ar = UGREATER_FLAG;
		break;
	case SLJIT_C_SIG_LESS:
	case SLJIT_C_SIG_NOT_LESS:
		dst_ar = LESS_FLAG;
		break;
	case SLJIT_C_SIG_GREATER:
	case SLJIT_C_SIG_NOT_GREATER:
		dst_ar = GREATER_FLAG;
		break;
	case SLJIT_C_OVERFLOW:
	case SLJIT_C_NOT_OVERFLOW:
		dst_ar = OVERFLOW_FLAG;
		break;
	case SLJIT_C_MUL_OVERFLOW:
	case SLJIT_C_MUL_NOT_OVERFLOW:
		FAIL_IF(push_inst(compiler, SLTIU | SA(OVERFLOW_FLAG) | TA(sugg_dst_ar) | IMM(1), sugg_dst_ar));
		dst_ar = sugg_dst_ar;
		type ^= 0x1; // Flip type bit for the XORI below
		break;
	default:
		if (type >= SLJIT_C_FLOAT_EQUAL && type <= SLJIT_C_FLOAT_NOT_NAN) {
			FAIL_IF(push_inst(compiler, CFC1 | TA(sugg_dst_ar) | DA(31), sugg_dst_ar));
			switch (type) {
			case SLJIT_C_FLOAT_EQUAL:
			case SLJIT_C_FLOAT_NOT_EQUAL:
				dst_ar = EQUAL_BIT + 24;
				break;
			case SLJIT_C_FLOAT_LESS:
			case SLJIT_C_FLOAT_NOT_LESS:
				dst_ar = LESS_BIT + 24;
				break;
			case SLJIT_C_FLOAT_GREATER:
			case SLJIT_C_FLOAT_NOT_GREATER:
				dst_ar = GREATER_BIT + 24;
				break;
			case SLJIT_C_FLOAT_NAN:
			case SLJIT_C_FLOAT_NOT_NAN:
				dst_ar = UNORD_BIT + 24;
				break;
			}
			FAIL_IF(push_inst(compiler, EXT_W | SA(sugg_dst_ar) | TA(sugg_dst_ar) | (dst_ar << 6), sugg_dst_ar));
		}
		dst_ar = sugg_dst_ar;
		break;
	}

	if (type & 0x1) {
		FAIL_IF(push_inst(compiler, XORI | SA(dst_ar) | TA(sugg_dst_ar) | IMM(1), sugg_dst_ar));
		dst_ar = sugg_dst_ar;
	}

	if (dst & SLJIT_MEM) {
		compiler->cache_arg = 0;
		compiler->cache_argw = 0;
		return emit_op_mem(compiler, WORD_DATA, dst_ar, dst, dstw);
	}

	if (sugg_dst_ar != dst_ar)
		return push_inst(compiler, ADDU_W | SA(dst_ar) | TA(0) | DA(sugg_dst_ar), sugg_dst_ar);
	return SLJIT_SUCCESS;
}

struct sljit_const* sljit_emit_const(struct sljit_compiler *compiler, int dst, sljit_w dstw, sljit_w initval)
{
	struct sljit_const *const_;
	int reg;

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

	reg = (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_NO_REGISTERS) ? dst : TMP_REG2;

	PTR_FAIL_IF(emit_const(compiler, reg, initval));

	if (dst & SLJIT_MEM)
		PTR_FAIL_IF(emit_op(compiler, SLJIT_MOV, WORD_DATA, dst, dstw, TMP_REG1, 0, TMP_REG2, 0));
	return const_;
}
