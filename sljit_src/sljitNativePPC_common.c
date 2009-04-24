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

// Length of an instruction word
// Both for ppc-32 and ppc-64
typedef unsigned int sljit_i;

#define TMP_REG1	(SLJIT_LOCALS_REG + 1)
#define TMP_REG2	(SLJIT_LOCALS_REG + 2)
#define TMP_REG3	(SLJIT_LOCALS_REG + 3)
#define REAL_STACK_PTR	(SLJIT_LOCALS_REG + 4)

#define INS_FORM_IMM(opcode, D, A, IMM) \
		(((opcode) << 26) | (reg_map[D] << 21) | (reg_map[A] << 16) | (IMM))
#define INS_FORM_OP0(opcode, D, opcode2) \
		(((opcode) << 26) | (reg_map[D] << 21) | (opcode2))
#define INS_FORM_OP1(opcode, D, A, opcode2) \
		(((opcode) << 26) | (reg_map[D] << 21) | (reg_map[A] << 16) | (opcode2))
#define INS_FORM_OP2(opcode, D, A, B, opcode2) \
		(((opcode) << 26) | (reg_map[D] << 21) | (reg_map[A] << 16) | (reg_map[B] << 11) | (opcode2))

#define SIMM_MAX	(0x7fff)
#define SIMM_MIN	(-0x8000)
#define UIMM_MAX	(0xffff)

// SLJIT_LOCALS_REG is not the real stack register, since it must
// point to the head of the stack chain
static sljit_ub reg_map[SLJIT_NO_REGISTERS + 5] = {
  0, 3, 4, 5, 30, 29, 28, 31, 6, 7, 8, 1
};

static int push_inst(struct sljit_compiler *compiler, sljit_i ins)
{
	sljit_i *ptr;

	ptr = (sljit_i*)ensure_buf(compiler, sizeof(sljit_i));
	if (!ptr) {
		compiler->error = SLJIT_MEMORY_ERROR;
		return SLJIT_MEMORY_ERROR;
	}
	*ptr = ins;
	compiler->size++;
	return SLJIT_NO_ERROR;
}

void* sljit_generate_code(struct sljit_compiler *compiler)
{
	struct sljit_memory_fragment *buf;
	sljit_i *code;
	sljit_i *code_ptr;
	sljit_i *buf_ptr;
	sljit_i *buf_end;

	FUNCTION_ENTRY();

	SLJIT_ASSERT(compiler->size > 0);
	reverse_buf(compiler);

	code = SLJIT_MALLOC_EXEC(compiler->size * sizeof(sljit_uw));

	buf = compiler->buf;

	code_ptr = code;
	do {
		buf_ptr = (sljit_i*)buf->memory;
		buf_end = buf_ptr + (buf->used_size >> 2);
		do {
			*code_ptr++ = *buf_ptr++;
		} while (buf_ptr < buf_end);

		buf = buf->next;
	} while (buf);

	SLJIT_ASSERT(code_ptr - code <= compiler->size);

	SLJIT_CACHE_FLUSH((char *)code, (char *)code_ptr);

	compiler->error = SLJIT_CODE_GENERATED;
	return code;
}

void sljit_free_code(void* code)
{
	SLJIT_FREE_EXEC(code);
}

static int load_immediate(struct sljit_compiler *compiler, int reg, sljit_w imm);

int sljit_emit_enter(struct sljit_compiler *compiler, int args, int general, int local_size)
{
	FUNCTION_ENTRY();
	SLJIT_ASSERT(args >= 0 && args <= SLJIT_NO_GEN_REGISTERS);
	SLJIT_ASSERT(general >= 0 && general <= SLJIT_NO_GEN_REGISTERS);
	SLJIT_ASSERT(args <= general);
	SLJIT_ASSERT(local_size >= 0 && local_size <= SLJIT_MAX_LOCAL_SIZE);
	SLJIT_ASSERT(compiler->general == -1);

	sljit_emit_enter_verbose();

	compiler->general = general;

	TEST_FAIL(push_inst(compiler, INS_FORM_OP0(31, 0, 0x802a6)));
#ifdef SLJIT_CONFIG_PPC_32
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(36, SLJIT_LOCALS_REG, REAL_STACK_PTR, (-(int)(sizeof(sljit_w))) & 0xffff )));
	if (general >= 1)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(36, SLJIT_GENERAL_REG1, REAL_STACK_PTR, (-2 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (general >= 2)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(36, SLJIT_GENERAL_REG2, REAL_STACK_PTR, (-3 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (general >= 3)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(36, SLJIT_GENERAL_REG3, REAL_STACK_PTR, (-4 * (int)(sizeof(sljit_w))) & 0xffff )));
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(36, 0, REAL_STACK_PTR, sizeof(sljit_w))));
#else
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(62, SLJIT_LOCALS_REG, REAL_STACK_PTR, (-(int)(sizeof(sljit_w))) & 0xffff )));
	if (general >= 1)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(62, SLJIT_GENERAL_REG1, REAL_STACK_PTR, (-2 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (general >= 2)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(62, SLJIT_GENERAL_REG2, REAL_STACK_PTR, (-3 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (general >= 3)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(62, SLJIT_GENERAL_REG3, REAL_STACK_PTR, (-4 * (int)(sizeof(sljit_w))) & 0xffff )));
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(62, 0, REAL_STACK_PTR, 2 * sizeof(sljit_w))));
#endif

	if (args >= 1)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, SLJIT_TEMPORARY_REG1, SLJIT_GENERAL_REG1, SLJIT_TEMPORARY_REG1, 444 << 1)));
	if (args >= 2)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, SLJIT_TEMPORARY_REG2, SLJIT_GENERAL_REG2, SLJIT_TEMPORARY_REG2, 444 << 1)));
	if (args >= 3)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, SLJIT_TEMPORARY_REG3, SLJIT_GENERAL_REG3, SLJIT_TEMPORARY_REG3, 444 << 1)));

#ifdef SLJIT_CONFIG_PPC_32
	compiler->local_size = (general + 1 + 1) * sizeof(sljit_w) + local_size;
#else
	compiler->local_size = (general + 1 + 7) * sizeof(sljit_w) + local_size;
#endif
	compiler->local_size = (compiler->local_size + 15) & ~0xf;

#ifdef SLJIT_CONFIG_PPC_32
	if (compiler->local_size <= 0x8000) {
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(37, REAL_STACK_PTR, REAL_STACK_PTR, ((-compiler->local_size) & 0xffff))));
	}
	else {
		TEST_FAIL(load_immediate(compiler, 0, -compiler->local_size));
		TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, REAL_STACK_PTR, REAL_STACK_PTR, 0, 183 << 1)));
	}
	TEST_FAIL(push_inst(compiler, INS_FORM_IMM(14, SLJIT_LOCALS_REG, REAL_STACK_PTR, 2 * sizeof(sljit_w))));
#else
	if (compiler->local_size <= 0x8000) {
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(62, REAL_STACK_PTR, REAL_STACK_PTR, ((-compiler->local_size) & 0xffff) | 1 )));
	}
	else {
		TEST_FAIL(load_immediate(compiler, 0, -compiler->local_size));
		TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, REAL_STACK_PTR, REAL_STACK_PTR, 0, 181 << 1)));
	}
	TEST_FAIL(push_inst(compiler, INS_FORM_IMM(14, SLJIT_LOCALS_REG, REAL_STACK_PTR, 7 * sizeof(sljit_w))));
#endif

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
}

int sljit_emit_return(struct sljit_compiler *compiler, int reg)
{
	FUNCTION_ENTRY();
	SLJIT_ASSERT(reg >= 0 && reg <= SLJIT_NO_REGISTERS);
	SLJIT_ASSERT(compiler->general >= 0);

	sljit_emit_return_verbose();

	if (reg != SLJIT_PREF_RET_REG && reg != SLJIT_NO_REG)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, reg, SLJIT_PREF_RET_REG, reg, 444 << 1)));

	if (compiler->local_size <= SIMM_MAX) {
		TEST_FAIL(push_inst(compiler, INS_FORM_IMM(14, REAL_STACK_PTR, REAL_STACK_PTR, compiler->local_size & 0xffff)));
	}
	else {
		TEST_FAIL(load_immediate(compiler, 0, compiler->local_size));
		TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, REAL_STACK_PTR, REAL_STACK_PTR, 0, 266 << 1)));
	}

#ifdef SLJIT_CONFIG_PPC_32
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(32, 0, REAL_STACK_PTR, sizeof(sljit_w))));
	if (compiler->general >= 3)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(32, SLJIT_GENERAL_REG3, REAL_STACK_PTR, (-4 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (compiler->general >= 2)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(32, SLJIT_GENERAL_REG2, REAL_STACK_PTR, (-3 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (compiler->general >= 1)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(32, SLJIT_GENERAL_REG1, REAL_STACK_PTR, (-2 * (int)(sizeof(sljit_w))) & 0xffff )));
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(32, SLJIT_LOCALS_REG, REAL_STACK_PTR, (-(int)(sizeof(sljit_w))) & 0xffff )));
#else
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(58, 0, REAL_STACK_PTR, 2 * sizeof(sljit_w))));
	if (compiler->general >= 3)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(58, SLJIT_GENERAL_REG3, REAL_STACK_PTR, (-4 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (compiler->general >= 2)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(58, SLJIT_GENERAL_REG2, REAL_STACK_PTR, (-3 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (compiler->general >= 1)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(58, SLJIT_GENERAL_REG1, REAL_STACK_PTR, (-2 * (int)(sizeof(sljit_w))) & 0xffff )));
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(58, SLJIT_LOCALS_REG, REAL_STACK_PTR, (-(int)(sizeof(sljit_w))) & 0xffff )));
#endif

	TEST_FAIL(push_inst(compiler, INS_FORM_OP0(31, 0, 0x803a6)));
	TEST_FAIL(push_inst(compiler, 0x4e800020));

	return SLJIT_NO_ERROR;
}

// ---------------------------------------------------------------------
//  Operators
// ---------------------------------------------------------------------

#define OP1_OFFSET	(SLJIT_ASHR + 1)

// inp_flags:

// Creates an index in data_transfer_insts array
#define WORD_DATA	0x00
#define BYTE_DATA	0x01
#define HALF_DATA	0x02
#define INT_DATA	0x03
#define SIGNED_DATA	0x04
#define LOAD_DATA	0x08
#define WRITE_BACK	0x10
#define INDEXED		0x20

#define MEM_MASK	0x3f

// Other inp_flags

#define ARG_TEST	0x100
#define ALT_FORM1	0x200
#define ALT_FORM2	0x400
#define ALT_INT_SF	0x800

// i/x - immediate/indexed form
// n/w - no write-back / write-back (1 bit)
// s/l - store/load (1 bit)
// u/s - signed/unsigned (1 bit)
// w/b/h/i - word/byte/half/int allowed (2 bit)
// It contans 32 items, but not all are different

// 64 bit only: [reg+imm] must be aligned to 4 bytes
#define ADDR_MODE2	0x10000
// 64-bit only: there is no lwau instruction
#define UPDATE_REQ	0x20000

#ifdef SLJIT_CONFIG_PPC_32
#define ARCH_DEPEND(a, b)	a
#define GET_INST_CODE(inst)	(inst)
#else
#define ARCH_DEPEND(a, b)	b
#define GET_INST_CODE(index)	((inst) & ~(ADDR_MODE2 | UPDATE_REQ))
#endif


static sljit_i data_transfer_insts[64] = {

// No write-back

/* i n s u w */ ARCH_DEPEND(36 << 26, (62 << 26) | ADDR_MODE2 | 0x0),
/* i n s u b */ 38 << 26,
/* i n s u h */ 44 << 26,
/* i n s u i */ 36 << 26,

/* i n s s w */ ARCH_DEPEND(36 << 26, (62 << 26) | ADDR_MODE2 | 0x0),
/* i n s s b */ 38 << 26,
/* i n s s h */ 44 << 26,
/* i n s s i */ 36 << 26,

/* i n l u w */ ARCH_DEPEND(32 << 26, (58 << 26) | ADDR_MODE2 | 0x0),
/* i n l u b */ 34 << 26,
/* i n l u h */ 40 << 26,
/* i n l u i */ 32 << 26,

/* i n l s w */ ARCH_DEPEND(32 << 26, (58 << 26) | ADDR_MODE2 | 0x0),
/* i n l s b */ (34 << 26) /* EXTS_REQ */,
/* i n l s h */ 42 << 26,
/* i n l s i */ ARCH_DEPEND(32 << 26, (58 << 26) | ADDR_MODE2 | 0x2),

// Write-back

/* i w s u w */ ARCH_DEPEND(37 << 26, (62 << 26) | ADDR_MODE2 | 0x1),
/* i w s u b */ 39 << 26,
/* i w s u h */ 45 << 26,
/* i w s u i */ 37 << 26,

/* i w s s w */ ARCH_DEPEND(37 << 26, (62 << 26) | ADDR_MODE2 | 0x1),
/* i w s s b */ 39 << 26,
/* i w s s h */ 45 << 26,
/* i w s s i */ 37 << 26,

/* i w l u w */ ARCH_DEPEND(33 << 26, (58 << 26) | ADDR_MODE2 | 0x1),
/* i w l u b */ 35 << 26,
/* i w l u h */ 41 << 26,
/* i w l u i */ 33 << 26,

/* i w l s w */ ARCH_DEPEND(33 << 26, (58 << 26) | ADDR_MODE2 | 0x1),
/* i w l s b */ (35 << 26) /* EXTS_REQ */,
/* i w l s h */ 43 << 26,
/* i w l s i */ ARCH_DEPEND(33 << 26, (58 << 26) | ADDR_MODE2 | UPDATE_REQ | 0x2),

// ----------
//  Indexed
// ---------

// No write-back

/* x n s u w */ ARCH_DEPEND((31 << 26) | (151 << 1), (31 << 26) | (149 << 1)),
/* x n s u b */ (31 << 26) | (215 << 1),
/* x n s u h */ (31 << 26) | (407 << 1),
/* x n s u i */ (31 << 26) | (151 << 1),

/* x n s s w */ ARCH_DEPEND((31 << 26) | (151 << 1), (31 << 26) | (149 << 1)),
/* x n s s b */ (31 << 26) | (215 << 1),
/* x n s s h */ (31 << 26) | (407 << 1),
/* x n s s i */ (31 << 26) | (151 << 1),

/* x n l u w */ ARCH_DEPEND((31 << 26) | (23 << 1), (31 << 26) | (21 << 1)),
/* x n l u b */ (31 << 26) | (87 << 1),
/* x n l u h */ (31 << 26) | (279 << 1),
/* x n l u i */ (31 << 26) | (23 << 1),

/* x n l s w */ ARCH_DEPEND((31 << 26) | (23 << 1), (31 << 26) | (21 << 1)),
/* x n l s b */ (31 << 26) | (87 << 1) /* EXTS_REQ */,
/* x n l s h */ (31 << 26) | (343 << 1),
/* x n l s i */ ARCH_DEPEND((31 << 26) | (23 << 1), (31 << 26) | (341 << 1)),

// Write-back

/* x w s u w */ ARCH_DEPEND((31 << 26) | (183 << 1), (31 << 26) | (181 << 1)),
/* x w s u b */ (31 << 26) | (247 << 1),
/* x w s u h */ (31 << 26) | (439 << 1),
/* x w s u i */ (31 << 26) | (183 << 1),

/* x w s s w */ ARCH_DEPEND((31 << 26) | (183 << 1), (31 << 26) | (181 << 1)),
/* x w s s b */ (31 << 26) | (247 << 1),
/* x w s s h */ (31 << 26) | (439 << 1),
/* x w s s i */ (31 << 26) | (183 << 1),

/* x w l u w */ ARCH_DEPEND((31 << 26) | (55 << 1), (31 << 26) | (53 << 1)),
/* x w l u b */ (31 << 26) | (119 << 1),
/* x w l u h */ (31 << 26) | (311 << 1),
/* x w l u i */ (31 << 26) | (55 << 1),

/* x w l s w */ ARCH_DEPEND((31 << 26) | (55 << 1), (31 << 26) | (53 << 1)),
/* x w l s b */ (31 << 26) | (119 << 1) /* EXTS_REQ */,
/* x w l s h */ (31 << 26) | (375 << 1),
/* x w l s i */ ARCH_DEPEND((31 << 26) | (55 << 1), (31 << 26) | (373 << 1))

};

#undef ARCH_DEPEND

  // Source and destination is register
#define REG_DEST	0x001
#define REG1_SOURCE	0x002
#define REG2_SOURCE	0x004
  // getput_arg_fast returned true
#define FAST_DEST	0x008
  // Multiple instructions are required
#define SLOW_DEST	0x010
// ALT_FORM1		0x200
// ALT_FORM2		0x400
// ALT_INT_SF		0x800

#ifdef SLJIT_CONFIG_PPC_32
#include "sljitNativePPC_32.c"
#else
#include "sljitNativePPC_64.c"
#endif

// Simple cases, (no caching is required)
static int getput_arg_fast(struct sljit_compiler *compiler, int inp_flags, int reg, int arg, sljit_w argw)
{
	sljit_i inst;
	int tmp_reg;

	SLJIT_ASSERT(arg & SLJIT_MEM_FLAG);

	if ((arg & 0xf) == SLJIT_NO_REG) {
#ifdef SLJIT_CONFIG_PPC_32
		if (argw <= SIMM_MAX && argw >= SIMM_MIN) {
			if (inp_flags & ARG_TEST)
				return 1;

			inst = data_transfer_insts[(inp_flags & ~WRITE_BACK) & MEM_MASK];
			SLJIT_ASSERT(!(inst & (ADDR_MODE2 | UPDATE_REQ)));
			push_inst(compiler, GET_INST_CODE(inst) | (reg_map[reg] << 21) | (argw & 0xffff));
			return -1;
		}
#else
		inst = data_transfer_insts[(inp_flags & ~WRITE_BACK) & MEM_MASK];
		if (argw <= SIMM_MAX && argw >= SIMM_MIN &&
				(!(inst & ADDR_MODE2) || (argw & 0x3) == 0)) {
			if (inp_flags & ARG_TEST)
				return 1;

			push_inst(compiler, GET_INST_CODE(inst) | (reg_map[reg] << 21) | (argw & 0xffff));
			return -1;
		}
#endif
		return (inp_flags & ARG_TEST) ? SLJIT_NO_ERROR : 0;
	}

	if ((arg & 0xf0) == SLJIT_NO_REG) {
#ifdef SLJIT_CONFIG_PPC_32
		if (argw <= SIMM_MAX && argw >= SIMM_MIN) {
			if (inp_flags & ARG_TEST)
				return 1;

			inst = data_transfer_insts[inp_flags & MEM_MASK];
			SLJIT_ASSERT(!(inst & (ADDR_MODE2 | UPDATE_REQ)));
			push_inst(compiler, GET_INST_CODE(inst) | (reg_map[reg] << 21) | (reg_map[arg & 0xf] << 16) | (argw & 0xffff));
			return -1;
		}
#else
		inst = data_transfer_insts[inp_flags & MEM_MASK];
		if (argw <= SIMM_MAX && argw >= SIMM_MIN && (!(inst & ADDR_MODE2) || (argw & 0x3) == 0)) {
			if (inp_flags & ARG_TEST)
				return 1;

			if ((inp_flags & WRITE_BACK) && (inst & UPDATE_REQ)) {
				tmp_reg = (inp_flags & LOAD_DATA) ? (arg & 0xf) : TMP_REG3;
				if (push_inst(compiler, INS_FORM_OP1(14, tmp_reg, arg & 0xf, argw & 0xffff)))
					return -1;
				arg = tmp_reg | SLJIT_MEM_FLAG;
				argw = 0;
			}
			push_inst(compiler, GET_INST_CODE(inst) | (reg_map[reg] << 21) | (reg_map[arg & 0xf] << 16) | (argw & 0xffff));
			return -1;
		}
#endif
	}
	else if (argw == 0) {
		if (inp_flags & ARG_TEST)
			return 1;
		inst = data_transfer_insts[(inp_flags | INDEXED) & MEM_MASK];
		SLJIT_ASSERT(!(inst & (ADDR_MODE2 | UPDATE_REQ)));
		push_inst(compiler, GET_INST_CODE(inst) | (reg_map[reg] << 21) | (reg_map[arg & 0xf] << 16) | ((reg_map[(arg >> 4) & 0xf] << 11)));
		return -1;
	}
	return (inp_flags & ARG_TEST) ? SLJIT_NO_ERROR : 0;
}

// see getput_arg below
// Note: can_cache is called only for binary operators. Those operator always
// uses word arguments without write back
static int can_cache(int arg, sljit_w argw, int next_arg, sljit_w next_argw)
{
	SLJIT_ASSERT(arg & SLJIT_MEM_FLAG);
	SLJIT_ASSERT(next_arg & SLJIT_MEM_FLAG);

	if ((arg & 0xf) == 0) {
		if ((next_arg & SLJIT_MEM_FLAG) && ((sljit_uw)argw - (sljit_uw)next_argw <= SIMM_MAX || (sljit_uw)next_argw - (sljit_uw)argw <= SIMM_MAX))
			return 1;
		return 0;
	}

	if (argw <= SIMM_MAX && argw >= SIMM_MIN) {
		if (arg == next_arg && (next_argw >= SIMM_MAX && next_argw <= SIMM_MIN))
			return 1;
	}

	if (arg == next_arg && ((sljit_uw)argw - (sljit_uw)next_argw <= SIMM_MAX || (sljit_uw)next_argw - (sljit_uw)argw <= SIMM_MAX))
		return 1;

	return 0;
}

#define TEST_WRITE_BACK() \
	if (inp_flags & WRITE_BACK) { \
		tmp_reg = arg & 0xf; \
		if (reg == tmp_reg) { \
			/* This can only happen for stores */ \
			/* since ld reg, reg, ? has no meaning */ \
			SLJIT_ASSERT(!(inp_flags & LOAD_DATA)); \
			TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, reg, TMP_REG3, reg, 444 << 1))); \
			reg = TMP_REG3; \
		} \
	}

#ifdef SLJIT_CONFIG_PPC_64
#define ADJUST_CACHED_IMM(imm) \
	if ((inst & ADDR_MODE2) && (imm & 0x3)) { \
		/* Adjust cached value. Fortunately this is really a rare case */ \
		compiler->cache_argw += imm & 0x3; \
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(14, TMP_REG3, TMP_REG3, imm & 0x3))); \
		imm &= ~0x3; \
	}
#else
#define ADJUST_CACHED_IMM(imm)
#endif

// emit the necessary instructions
// see can_cache above
static int getput_arg(struct sljit_compiler *compiler, int inp_flags, int reg, int arg, sljit_w argw, int next_arg, sljit_w next_argw)
{
	int tmp_reg;
	sljit_i inst;

	SLJIT_ASSERT(arg & SLJIT_MEM_FLAG);

	tmp_reg = (inp_flags & LOAD_DATA) ? reg : TMP_REG3;

	if ((arg & 0xf) == SLJIT_NO_REG) {
		inst = data_transfer_insts[(inp_flags & ~WRITE_BACK) & MEM_MASK];
		if ((compiler->cache_arg & SLJIT_IMM) && (((sljit_uw)argw - (sljit_uw)compiler->cache_argw) <= SIMM_MAX || ((sljit_uw)compiler->cache_argw - (sljit_uw)argw) <= SIMM_MAX)) {
			argw = argw - compiler->cache_argw;
			ADJUST_CACHED_IMM(argw);
			SLJIT_ASSERT(!(inst & UPDATE_REQ));
			return push_inst(compiler, GET_INST_CODE(inst) | (reg_map[reg] << 21) | (reg_map[TMP_REG3] << 16) | (argw & 0xffff));
		}

		if ((next_arg & SLJIT_MEM_FLAG) && (argw - next_argw <= SIMM_MAX || next_argw - argw <= SIMM_MAX)) {
			SLJIT_ASSERT(inp_flags & LOAD_DATA);

			compiler->cache_arg = SLJIT_IMM;
			compiler->cache_argw = argw;
			tmp_reg = TMP_REG3;
		}

		TEST_FAIL(load_immediate(compiler, tmp_reg, argw));
		return push_inst(compiler, GET_INST_CODE(inst) | (reg_map[reg] << 21) | (reg_map[tmp_reg] << 16));
	}

	inst = data_transfer_insts[inp_flags & MEM_MASK];

	if (compiler->cache_arg == arg && ((sljit_uw)argw - (sljit_uw)compiler->cache_argw <= SIMM_MAX || (sljit_uw)compiler->cache_argw - (sljit_uw)argw <= SIMM_MAX)) {
		SLJIT_ASSERT(!(inp_flags & WRITE_BACK));
		argw = argw - compiler->cache_argw;
		ADJUST_CACHED_IMM(argw);
		return push_inst(compiler, GET_INST_CODE(inst) | (reg_map[reg] << 21) | (reg_map[TMP_REG3] << 16) | (argw & 0xffff));
	}

	if ((compiler->cache_arg & SLJIT_IMM) && compiler->cache_argw == argw) {
		TEST_WRITE_BACK();
		if (arg & 0xf0) {
			if (inp_flags & (WRITE_BACK | LOAD_DATA)) {
				TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, tmp_reg, arg & 0xf, (arg >> 4) & 0xf, 266 << 1)));
				arg = tmp_reg | SLJIT_MEM_FLAG;
			}
			else {
				SLJIT_ASSERT(tmp_reg == TMP_REG3);
				TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, TMP_REG3, TMP_REG3, (arg >> 4) & 0xf, 266 << 1)));
			}
		}

		inst = data_transfer_insts[(inp_flags | INDEXED) & MEM_MASK];
		SLJIT_ASSERT(!(inst & (ADDR_MODE2 | UPDATE_REQ)));

		return push_inst(compiler, GET_INST_CODE(inst) | (reg_map[reg] << 21) | (reg_map[arg & 0xf] << 16) | ((reg_map[TMP_REG3] << 11)));
	}

	if (argw <= SIMM_MAX && argw >= SIMM_MIN
#ifdef SLJIT_CONFIG_PPC_64
		&& (!(inst & ADDR_MODE2) || (argw & 0x3) == 0)
#endif
	) {
		// Since getput_arg_fast doesn't caught it, that means two arguments is given
		SLJIT_ASSERT(argw != 0);
		SLJIT_ASSERT((arg & 0xf0) != SLJIT_NO_REG);

		TEST_WRITE_BACK();
		if (arg == next_arg && !(inp_flags & WRITE_BACK) && (next_argw <= SIMM_MAX && next_argw >= SIMM_MIN)) {
			SLJIT_ASSERT(inp_flags & LOAD_DATA);

			compiler->cache_arg = arg;
			compiler->cache_argw = 0;
			tmp_reg = TMP_REG3;
		}

		TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, tmp_reg, arg & 0xf, (arg >> 4) & 0xf, 266 << 1)));
#ifdef SLJIT_CONFIG_PPC_64
		if ((inp_flags & WRITE_BACK) && (inst & UPDATE_REQ)) {
			if (push_inst(compiler, INS_FORM_OP1(14, tmp_reg, arg & 0xf, argw & 0xffff)))
				return -1;
			arg = tmp_reg | SLJIT_MEM_FLAG;
			argw = 0;
		}
#endif
		return push_inst(compiler, GET_INST_CODE(inst) | (reg_map[reg] << 21) | (reg_map[tmp_reg] << 16 | (argw & 0xffff)));
	}

	if (argw == next_argw && (next_arg & SLJIT_MEM_FLAG)) {
		SLJIT_ASSERT(inp_flags & LOAD_DATA);
		TEST_FAIL(load_immediate(compiler, TMP_REG3, argw));

		compiler->cache_arg = SLJIT_IMM;
		compiler->cache_argw = argw;

		TEST_WRITE_BACK();
		if (arg & 0xf0) {
			if (inp_flags & WRITE_BACK) {
				TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, arg & 0xf, arg & 0xf, (arg >> 4) & 0xf, 266 << 1)));
			}
			else {
				TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, tmp_reg, arg & 0xf, (arg >> 4) & 0xf, 266 << 1)));
				arg = tmp_reg | SLJIT_MEM_FLAG;
			}
		}

		inst = data_transfer_insts[(inp_flags | INDEXED) & MEM_MASK];
		SLJIT_ASSERT(!(inst & (ADDR_MODE2 | UPDATE_REQ)));

		return push_inst(compiler, GET_INST_CODE(inst) | (reg_map[reg] << 21) | (reg_map[arg & 0xf] << 16) | ((reg_map[TMP_REG3] << 11)));
	}

	if (arg == next_arg && !(inp_flags & WRITE_BACK) && ((sljit_uw)argw - (sljit_uw)next_argw <= SIMM_MAX || (sljit_uw)next_argw - (sljit_uw)argw <= SIMM_MAX)) {
		SLJIT_ASSERT(inp_flags & LOAD_DATA);
		TEST_FAIL(load_immediate(compiler, TMP_REG3, argw));

		if ((arg & 0xf0) != SLJIT_NO_REG) {
			TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, TMP_REG3, TMP_REG3, (arg >> 4) & 0xf, 266 << 1)));
		}

		TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, TMP_REG3, TMP_REG3, arg & 0xf, 266 << 1)));

		compiler->cache_arg = arg;
		compiler->cache_argw = argw;

		return push_inst(compiler, GET_INST_CODE(inst) | (reg_map[reg] << 21) | (reg_map[TMP_REG3] << 16));
	}

	// Get the indexed version instead of the normal one
	inst = data_transfer_insts[(inp_flags | INDEXED) & MEM_MASK];
	SLJIT_ASSERT(!(inst & (ADDR_MODE2 | UPDATE_REQ)));

	TEST_FAIL(load_immediate(compiler, tmp_reg, argw));

	if ((arg & 0xf0) != SLJIT_NO_REG) {
		TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, tmp_reg, tmp_reg, (arg >> 4) & 0xf, 266 << 1)));
	}

	return push_inst(compiler, GET_INST_CODE(inst) | (reg_map[reg] << 21) | (reg_map[arg & 0xf] << 16) | ((reg_map[tmp_reg] << 11)));
}

#define ORDER_IND_REGS(arg) \
	if ((arg & SLJIT_MEM_FLAG) && ((arg >> 4) & 0xf) > (arg & 0xf)) \
		arg = SLJIT_MEM_FLAG | ((arg << 4) & 0xf0) | ((arg >> 4) & 0xf)

static int emit_op(struct sljit_compiler *compiler, int op, int inp_flags,
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
	int src2_r;
	int sugg_src2_r = TMP_REG2;
	int flags = inp_flags & (ALT_FORM1 | ALT_FORM2 | ALT_INT_SF);

	compiler->cache_arg = 0;
	compiler->cache_argw = 0;

	// Destination check
	if (dst >= SLJIT_TEMPORARY_REG1 && dst <= TMP_REG3) {
		dst_r = dst;
		flags |= REG_DEST;
		if (op >= (OP1_OFFSET + SLJIT_MOV) && op <= (OP1_OFFSET + SLJIT_MOVU_SI))
			sugg_src2_r = dst_r;
	}
	else if (dst == SLJIT_NO_REG) {
		if (op >= (OP1_OFFSET + SLJIT_MOV) && op <= (OP1_OFFSET + SLJIT_MOVU_SI) && !(src2 & SLJIT_MEM_FLAG))
			return SLJIT_NO_ERROR;
		dst_r = TMP_REG2;
	}
	else {
		SLJIT_ASSERT(dst & SLJIT_MEM_FLAG);
		if (getput_arg_fast(compiler, inp_flags | ARG_TEST, TMP_REG2, dst, dstw)) {
			flags |= FAST_DEST;
			dst_r = TMP_REG2;
		}
		else {
			flags |= SLOW_DEST;
			dst_r = 0;
		}
	}

	// Source 1
	if (src1 >= SLJIT_TEMPORARY_REG1 && src1 <= TMP_REG3) {
		src1_r = src1;
		flags |= REG1_SOURCE;
	}
	else if (src1 & SLJIT_IMM) {
#ifdef SLJIT_CONFIG_PPC_64
		if (inp_flags & INT_DATA) {
			if (src1w >= 0)
				src1w &= 0x7fffffff;
			else
				src1w |= 0x7fffffff80000000l;
		}
#endif
		TEST_FAIL(load_immediate(compiler, TMP_REG1, src1w));
		src1_r = TMP_REG1;
	}
	else if (getput_arg_fast(compiler, inp_flags | LOAD_DATA, TMP_REG1, src1, src1w)) {
		TEST_FAIL(compiler->error);
		src1_r = TMP_REG1;
	}
	else
		src1_r = 0;

	if (src2 >= SLJIT_TEMPORARY_REG1 && src2 <= TMP_REG3) {
		src2_r = src2;
		flags |= REG2_SOURCE;
		if (!(flags & REG_DEST) && op >= (OP1_OFFSET + SLJIT_MOV) && op <= (OP1_OFFSET + SLJIT_MOVU_SI))
			dst_r = src2_r;
	}
	else if (src2 & SLJIT_IMM) {
#ifdef SLJIT_CONFIG_PPC_64
		if (inp_flags & INT_DATA) {
			if (src2w >= 0)
				src2w &= 0x7fffffff;
			else
				src2w |= 0x7fffffff80000000l;
		}
#endif
		TEST_FAIL(load_immediate(compiler, sugg_src2_r, src2w));
		src2_r = sugg_src2_r;
	}
	else if (getput_arg_fast(compiler, inp_flags | LOAD_DATA, sugg_src2_r, src2, src2w)) {
		TEST_FAIL(compiler->error);
		src2_r = sugg_src2_r;
	}
	else
		src2_r = 0;

	// src1_r, src2_r and dst_r can be zero (=unprocessed)
	// All arguments are complex addressing modes, and it is a binary operator
	if (src1_r == 0 && src2_r == 0 && dst_r == 0) {
		ORDER_IND_REGS(src1);
		ORDER_IND_REGS(src2);
		ORDER_IND_REGS(dst);
		if (!can_cache(src1, src1w, src2, src2w) && can_cache(src1, src1w, dst, dstw)) {
			TEST_FAIL(getput_arg(compiler, inp_flags | LOAD_DATA, TMP_REG2, src2, src2w, src1, src1w));
			TEST_FAIL(getput_arg(compiler, inp_flags | LOAD_DATA, TMP_REG1, src1, src1w, dst, dstw));
		}
		else {
			TEST_FAIL(getput_arg(compiler, inp_flags | LOAD_DATA, TMP_REG1, src1, src1w, src2, src2w));
			TEST_FAIL(getput_arg(compiler, inp_flags | LOAD_DATA, TMP_REG2, src2, src2w, dst, dstw));
		}
		src1_r = TMP_REG1;
		src2_r = TMP_REG2;
	}
	else if (src1_r == 0 && src2_r == 0) {
		ORDER_IND_REGS(src1);
		ORDER_IND_REGS(src2);
		TEST_FAIL(getput_arg(compiler, inp_flags | LOAD_DATA, TMP_REG1, src1, src1w, src2, src2w));
		src1_r = TMP_REG1;
	}
	else if (src1_r == 0 && dst_r == 0) {
		ORDER_IND_REGS(src1);
		ORDER_IND_REGS(dst);
		TEST_FAIL(getput_arg(compiler, inp_flags | LOAD_DATA, TMP_REG1, src1, src1w, dst, dstw));
		src1_r = TMP_REG1;
	}
	else if (src2_r == 0 && dst_r == 0) {
		ORDER_IND_REGS(src2);
		ORDER_IND_REGS(dst);
		TEST_FAIL(getput_arg(compiler, inp_flags | LOAD_DATA, sugg_src2_r, src2, src2w, dst, dstw));
		src2_r = sugg_src2_r;
	}

	if (dst_r == 0)
		dst_r = TMP_REG2;

	if (src1_r == 0) {
		TEST_FAIL(getput_arg(compiler, inp_flags | LOAD_DATA, TMP_REG1, src1, src1w, 0, 0));
		src1_r = TMP_REG1;
	}

	if (src2_r == 0) {
		TEST_FAIL(getput_arg(compiler, inp_flags | LOAD_DATA, sugg_src2_r, src2, src2w, 0, 0));
		src2_r = sugg_src2_r;
	}

	TEST_FAIL(emit_single_op(compiler, op, flags, dst_r, src1_r, src2_r));

	if (flags & (FAST_DEST | SLOW_DEST)) {
		if (flags & FAST_DEST) {
			TEST_FAIL(getput_arg_fast(compiler, inp_flags, dst_r, dst, dstw));
		}
		else {
			TEST_FAIL(getput_arg(compiler, inp_flags, dst_r, dst, dstw, 0, 0));
		}
	}
	return SLJIT_NO_ERROR;
}

int sljit_emit_op1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
#ifdef SLJIT_CONFIG_PPC_64
	int inp_flags = 0;
#else
	#define inp_flags 0
#endif

	FUNCTION_ENTRY();

	SLJIT_ASSERT(GET_OPCODE(op) >= SLJIT_MOV && GET_OPCODE(op) <= SLJIT_NEG);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_SRC(src, srcw);
	FUNCTION_CHECK_DST(dst, dstw);
	FUNCTION_CHECK_OP1();
#endif
	sljit_emit_op1_verbose();

#ifdef SLJIT_CONFIG_PPC_64
	if (op & SLJIT_INT_OPERATION) {
		inp_flags |= INT_DATA | SIGNED_DATA;
		if (src & SLJIT_IMM)
			srcw = (srcw << 32) >> 32;
	}
#endif

	op = GET_OPCODE(op);
#ifdef SLJIT_CONFIG_PPC_64
	if ((op >= SLJIT_MOV_UI && op <= SLJIT_MOV_SH) || (op >= SLJIT_MOVU_UI && op <= SLJIT_MOVU_SH))
		inp_flags = 0;
#endif

	switch (op) {
	case SLJIT_MOV:
		return emit_op(compiler, OP1_OFFSET + SLJIT_MOV, inp_flags | WORD_DATA, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOV_UI:
		return emit_op(compiler, OP1_OFFSET + SLJIT_MOV_UI, inp_flags | INT_DATA, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOV_SI:
		return emit_op(compiler, OP1_OFFSET + SLJIT_MOV_SI, inp_flags | INT_DATA | SIGNED_DATA, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOV_UB:
		return emit_op(compiler, OP1_OFFSET + SLJIT_MOV_UB, inp_flags | BYTE_DATA, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOV_SB:
		return emit_op(compiler, OP1_OFFSET + SLJIT_MOV_SB, inp_flags | BYTE_DATA | SIGNED_DATA, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (char)srcw : srcw);

	case SLJIT_MOV_UH:
		return emit_op(compiler, OP1_OFFSET + SLJIT_MOV_UH, inp_flags | HALF_DATA, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (unsigned short)srcw : srcw);

	case SLJIT_MOV_SH:
		return emit_op(compiler, OP1_OFFSET + SLJIT_MOV_SH, inp_flags | HALF_DATA | SIGNED_DATA, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (short)srcw : srcw);

	case SLJIT_MOVU:
		return emit_op(compiler, OP1_OFFSET + SLJIT_MOV, inp_flags | WORD_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOVU_UI:
		return emit_op(compiler, OP1_OFFSET + SLJIT_MOV_UI, inp_flags | INT_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOVU_SI:
		return emit_op(compiler, OP1_OFFSET + SLJIT_MOV_SI, inp_flags | INT_DATA | SIGNED_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOVU_UB:
		return emit_op(compiler, OP1_OFFSET + SLJIT_MOV_UB, inp_flags | BYTE_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (unsigned char)srcw : srcw);

	case SLJIT_MOVU_SB:
		return emit_op(compiler, OP1_OFFSET + SLJIT_MOV_SB, inp_flags | BYTE_DATA | SIGNED_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (char)srcw : srcw);

	case SLJIT_MOVU_UH:
		return emit_op(compiler, OP1_OFFSET + SLJIT_MOV_UH, inp_flags | HALF_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (unsigned short)srcw : srcw);

	case SLJIT_MOVU_SH:
		return emit_op(compiler, OP1_OFFSET + SLJIT_MOV_SH, inp_flags | HALF_DATA | SIGNED_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (short)srcw : srcw);

	case SLJIT_NOT:
		return emit_op(compiler, OP1_OFFSET + SLJIT_NOT, inp_flags, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_NEG:
		return emit_op(compiler, OP1_OFFSET + SLJIT_NEG, inp_flags, dst, dstw, TMP_REG1, 0, src, srcw);
	}

#ifdef SLJIT_CONFIG_PPC_32
	#undef inp_flags
#endif
	return SLJIT_NO_ERROR;
}

#define TEST_SL_IMM(src, srcw) \
	(((src) & SLJIT_IMM) && (srcw) <= SIMM_MAX && (srcw) >= SIMM_MIN)

#define TEST_UL_IMM(src, srcw) \
	(((src) & SLJIT_IMM) && !((srcw) & ~0xffff))

#ifdef SLJIT_CONFIG_PPC_64
#define TEST_SH_IMM(src, srcw) \
	(((src) & SLJIT_IMM) && !((srcw) & 0xffff) && (srcw) <= 0x7fffffff && (srcw) >= -0x80000000l)
#else
#define TEST_SH_IMM(src, srcw) \
	(((src) & SLJIT_IMM) && !((srcw) & 0xffff))
#endif

#define TEST_UH_IMM(src, srcw) \
	(((src) & SLJIT_IMM) && !((srcw) & ~0xffff0000))

int sljit_emit_op2(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
#ifdef SLJIT_CONFIG_PPC_64
	int inp_flags = 0;
#else
	#define inp_flags 0
#endif

	FUNCTION_ENTRY();

	SLJIT_ASSERT(GET_OPCODE(op) >= SLJIT_ADD && GET_OPCODE(op) <= SLJIT_ASHR);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_SRC(src1, src1w);
	FUNCTION_CHECK_SRC(src2, src2w);
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_op2_verbose();

#ifdef SLJIT_CONFIG_PPC_64
	if (op & SLJIT_INT_OPERATION) {
		inp_flags |= INT_DATA | SIGNED_DATA;
		if (src1 & SLJIT_IMM)
			src1w = (src1w << 32) >> 32;
		if (src2 & SLJIT_IMM)
			src2w = (src2w << 32) >> 32;
		if (op & SLJIT_SET_FLAGS)
			inp_flags |= ALT_INT_SF;
	}
#endif

	switch (GET_OPCODE(op)) {
	case SLJIT_ADD:
		if (!(op & SLJIT_SET_FLAGS)) {
			if (TEST_SL_IMM(src2, src2w)) {
				compiler->imm = src2w & 0xffff;
				return emit_op(compiler, SLJIT_ADD, inp_flags | ALT_FORM1, dst, dstw, src1, src1w, TMP_REG2, 0);
			}
			if (TEST_SL_IMM(src1, src1w)) {
				compiler->imm = src1w & 0xffff;
				return emit_op(compiler, SLJIT_ADD, inp_flags | ALT_FORM1, dst, dstw, src2, src2w, TMP_REG2, 0);
			}
			if (TEST_SH_IMM(src2, src2w)) {
				compiler->imm = (src2w >> 16) & 0xffff;
				return emit_op(compiler, SLJIT_ADD, inp_flags | ALT_FORM2, dst, dstw, src1, src1w, TMP_REG2, 0);
			}
			if (TEST_SH_IMM(src1, src1w)) {
				compiler->imm = (src1w >> 16) & 0xffff;
				return emit_op(compiler, SLJIT_ADD, inp_flags | ALT_FORM2, dst, dstw, src2, src2w, TMP_REG2, 0);
			}
		}
		return emit_op(compiler, SLJIT_ADD, inp_flags, dst, dstw, src1, src1w, src2, src2w);

	case SLJIT_SUB:
		if (!(op & SLJIT_SET_FLAGS)) {
			if (TEST_SL_IMM(src2, -src2w)) {
				compiler->imm = (-src2w) & 0xffff;
				return emit_op(compiler, SLJIT_ADD, inp_flags | ALT_FORM1, dst, dstw, src1, src1w, TMP_REG2, 0);
			}
			if (TEST_SL_IMM(src1, src1w)) {
				compiler->imm = src1w & 0xffff;
				return emit_op(compiler, SLJIT_SUB, inp_flags | ALT_FORM1, dst, dstw, src2, src2w, TMP_REG2, 0);
			}
			if (TEST_SH_IMM(src2, -src2w)) {
				compiler->imm = ((-src2w) >> 16) & 0xffff;
				return emit_op(compiler, SLJIT_ADD, inp_flags | ALT_FORM2, dst, dstw, src1, src1w, TMP_REG2, 0);
			}
		}
		return emit_op(compiler, SLJIT_SUB, inp_flags, dst, dstw, src1, src1w, src2, src2w);

	case SLJIT_ADDC:
	case SLJIT_SUBC:
		return emit_op(compiler, GET_OPCODE(op), inp_flags, dst, dstw, src1, src1w, src2, src2w);

	case SLJIT_MUL:
#ifdef SLJIT_CONFIG_PPC_64
		if (op & SLJIT_INT_OPERATION)
			inp_flags |= ALT_FORM2;
#endif
		if (!(op & SLJIT_SET_FLAGS)) {
			if (TEST_SL_IMM(src2, src2w)) {
				compiler->imm = src2w & 0xffff;
				return emit_op(compiler, SLJIT_MUL, inp_flags | ALT_FORM1, dst, dstw, src1, src1w, TMP_REG2, 0);
			}
			if (TEST_SL_IMM(src1, src1w)) {
				compiler->imm = src1w & 0xffff;
				return emit_op(compiler, SLJIT_MUL, inp_flags | ALT_FORM1, dst, dstw, src2, src2w, TMP_REG2, 0);
			}
		}
		return emit_op(compiler, SLJIT_MUL, inp_flags, dst, dstw, src1, src1w, src2, src2w);

	case SLJIT_AND:
	case SLJIT_OR:
	case SLJIT_XOR:
		// Commutative unsigned operations
		if (!(op & SLJIT_SET_FLAGS) || GET_OPCODE(op) == SLJIT_AND) {
			if (TEST_UL_IMM(src2, src2w)) {
				compiler->imm = src2w & 0xffff;
				return emit_op(compiler, GET_OPCODE(op), inp_flags | ALT_FORM1, dst, dstw, src1, src1w, TMP_REG2, 0);
			}
			if (TEST_UL_IMM(src1, src1w)) {
				compiler->imm = src1w & 0xffff;
				return emit_op(compiler, GET_OPCODE(op), inp_flags | ALT_FORM1, dst, dstw, src2, src2w, TMP_REG2, 0);
			}
			if (TEST_UH_IMM(src2, src2w)) {
				compiler->imm = (src2w >> 16) & 0xffff;
				return emit_op(compiler, GET_OPCODE(op), inp_flags | ALT_FORM2, dst, dstw, src1, src1w, TMP_REG2, 0);
			}
			if (TEST_UH_IMM(src1, src1w)) {
				compiler->imm = (src1w >> 16) & 0xffff;
				return emit_op(compiler, GET_OPCODE(op), inp_flags | ALT_FORM2, dst, dstw, src2, src2w, TMP_REG2, 0);
			}
		}
		return emit_op(compiler, GET_OPCODE(op), inp_flags, dst, dstw, src1, src1w, src2, src2w);

	case SLJIT_SHL:
	case SLJIT_LSHR:
	case SLJIT_ASHR:
#ifdef SLJIT_CONFIG_PPC_64
		if (op & SLJIT_INT_OPERATION)
			inp_flags |= ALT_FORM2;
#endif
		if (src2 & SLJIT_IMM) {
			compiler->imm = src2w;
			return emit_op(compiler, GET_OPCODE(op), inp_flags | ALT_FORM1, dst, dstw, src1, src1w, TMP_REG2, 0);
		}
		return emit_op(compiler, GET_OPCODE(op), inp_flags, dst, dstw, src1, src1w, src2, src2w);
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

int sljit_emit_fop1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
	int dst_freg;

	FUNCTION_ENTRY();

	SLJIT_ASSERT(op >= SLJIT_FCMP && op <= SLJIT_FABS);
#ifdef SLJIT_DEBUG
	FUNCTION_FCHECK(src, srcw);
	FUNCTION_FCHECK(dst, dstw);
#endif
	sljit_emit_fop1_verbose();

	return SLJIT_NO_ERROR;
}

int sljit_emit_fop2(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	int dst_freg;

	FUNCTION_ENTRY();

	SLJIT_ASSERT(op >= SLJIT_FADD && op <= SLJIT_FDIV);
#ifdef SLJIT_DEBUG
	FUNCTION_FCHECK(src1, src1w);
	FUNCTION_FCHECK(src2, src2w);
	FUNCTION_FCHECK(dst, dstw);
#endif
	sljit_emit_fop2_verbose();

	return SLJIT_NO_ERROR;
}

// ---------------------------------------------------------------------
//  Conditional instructions
// ---------------------------------------------------------------------

struct sljit_label* sljit_emit_label(struct sljit_compiler *compiler)
{
	struct sljit_label *label;

	FUNCTION_ENTRY();

	sljit_emit_label_verbose();

	label = NULL;

	return label;
}

struct sljit_jump* sljit_emit_jump(struct sljit_compiler *compiler, int type)
{
	struct sljit_jump *jump;

	FUNCTION_ENTRY();
	SLJIT_ASSERT((type & ~0x1ff) == 0);
	SLJIT_ASSERT((type & 0xff) >= SLJIT_C_EQUAL && (type & 0xff) <= SLJIT_CALL3);

	sljit_emit_jump_verbose();

	jump = NULL;

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

	return SLJIT_NO_ERROR;
}

#define GET_CR_BIT(bit, dst) \
	TEST_FAIL(push_inst(compiler, INS_FORM_OP0(31, dst, (19 << 1)))); \
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(21, dst, dst, ((1 + (bit)) << 11) | (31 << 6) | (31 << 1))));

#define GET_XER_BIT(bit, dst) \
	TEST_FAIL(push_inst(compiler, INS_FORM_OP0(31, dst, (1 << 16) | (339 << 1)))); \
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(21, dst, dst, ((1 + (bit)) << 11) | (31 << 6) | (31 << 1))));

#define INVERT_BIT(dst) \
	TEST_FAIL(push_inst(compiler, INS_FORM_IMM(26, dst, dst, 0x1)));

int sljit_emit_cond_set(struct sljit_compiler *compiler, int dst, sljit_w dstw, int type)
{
	int reg;

	FUNCTION_ENTRY();
	SLJIT_ASSERT(type >= SLJIT_C_EQUAL && type <= SLJIT_C_NOT_OVERFLOW);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_set_cond_verbose();

	if (dst == SLJIT_NO_REG)
		return SLJIT_NO_ERROR;

	if (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_GENERAL_REG3)
		reg = dst;
	else
		reg = TMP_REG2;

	switch (type) {
	case SLJIT_C_EQUAL:
	case SLJIT_C_ZERO:
		GET_CR_BIT(2, reg);
		break;

	case SLJIT_C_NOT_EQUAL:
	case SLJIT_C_NOT_ZERO:
		GET_CR_BIT(2, reg);
		INVERT_BIT(reg);
		break;

	case SLJIT_C_LESS:
	case SLJIT_C_NOT_CARRY:
		GET_XER_BIT(2, reg);
		INVERT_BIT(reg);
		break;

	case SLJIT_C_NOT_LESS:
	case SLJIT_C_CARRY:
		GET_XER_BIT(2, reg);
		break;

	case SLJIT_C_GREATER:
		TEST_FAIL(push_inst(compiler, INS_FORM_OP0(31, TMP_REG1, (19 << 1)))); // CR
		TEST_FAIL(push_inst(compiler, INS_FORM_OP0(31, reg, (1 << 16) | (339 << 1)))); // XER
		TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, reg, reg, TMP_REG1, (60 << 1))));
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(21, reg, reg, ((1 + 2) << 11) | (31 << 6) | (31 << 1))));
		break;

	case SLJIT_C_NOT_GREATER:
		TEST_FAIL(push_inst(compiler, INS_FORM_OP0(31, TMP_REG1, (19 << 1)))); // CR
		TEST_FAIL(push_inst(compiler, INS_FORM_OP0(31, reg, (1 << 16) | (339 << 1)))); // XER
		TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, TMP_REG1, reg, reg, (412 << 1))));
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(21, reg, reg, ((1 + 2) << 11) | (31 << 6) | (31 << 1))));
		break;

	case SLJIT_C_SIG_LESS:
		GET_CR_BIT(0, reg);
		break;

	case SLJIT_C_SIG_NOT_LESS:
		GET_CR_BIT(0, reg);
		INVERT_BIT(reg);
		break;

	case SLJIT_C_SIG_GREATER:
		GET_CR_BIT(1, reg);
		break;

	case SLJIT_C_SIG_NOT_GREATER:
		GET_CR_BIT(1, reg);
		INVERT_BIT(reg);
		break;

	case SLJIT_C_OVERFLOW:
		GET_XER_BIT(1, reg);
		break;

	case SLJIT_C_NOT_OVERFLOW:
		GET_XER_BIT(1, reg);
		INVERT_BIT(reg);
		break;
	}

	if (reg == TMP_REG2)
		return emit_op(compiler, OP1_OFFSET + SLJIT_MOV, WORD_DATA, dst, dstw, TMP_REG1, 0, TMP_REG2, 0);
	else
		return SLJIT_NO_ERROR;
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

	const_ = NULL;

	return const_;
}

void sljit_set_jump_addr(sljit_uw addr, sljit_uw new_addr)
{

}

void sljit_set_const(sljit_uw addr, sljit_w constant)
{

}
