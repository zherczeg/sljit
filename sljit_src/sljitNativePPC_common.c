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
#define ZERO_REG	(SLJIT_LOCALS_REG + 4)
#define REAL_STACK_PTR	(SLJIT_LOCALS_REG + 5)

#define TMP_FREG1       (SLJIT_FLOAT_REG4 + 1)
#define TMP_FREG2       (SLJIT_FLOAT_REG4 + 2)

#define INS_FORM_IMM(opcode, D, A, IMM) \
		(((opcode) << 26) | (reg_map[D] << 21) | (reg_map[A] << 16) | (IMM))
#define INS_FORM_OP0(opcode, D, opcode2) \
		(((opcode) << 26) | (reg_map[D] << 21) | (opcode2))
#define INS_FORM_OP1(opcode, D, A, opcode2) \
		(((opcode) << 26) | (reg_map[D] << 21) | (reg_map[A] << 16) | (opcode2))
#define INS_FORM_OP2(opcode, D, A, B, opcode2) \
		(((opcode) << 26) | (reg_map[D] << 21) | (reg_map[A] << 16) | (reg_map[B] << 11) | (opcode2))
#define INS_FORM_CMP1(opcode, cr, A, opcode2) \
		(((opcode) << 26) | ((cr) << 21) | (reg_map[A] << 16) | (opcode2))
#define INS_FORM_CMP2(opcode, cr, A, B, opcode2) \
		(((opcode) << 26) | ((cr) << 21) | (reg_map[A] << 16) | (reg_map[B] << 11) | (opcode2))

#define INS_FORM_FOP(opcode, D, A, B, C, opcode2) \
		(((opcode) << 26) | ((D) << 21) | ((A) << 16) | ((B) << 11) | ((C) << 6) | ((opcode2) << 1))
#define INS_FORM_FOP1(opcode, D, A, opcode2) \
		(((opcode) << 26) | ((D) << 21) | (reg_map[A] << 16) | (opcode2))
#define INS_FORM_FOP2(opcode, D, A, B, opcode2) \
		(((opcode) << 26) | ((D) << 21) | (reg_map[A] << 16) | (reg_map[B] << 11) | (opcode2))

#define SIMM_MAX	(0x7fff)
#define SIMM_MIN	(-0x8000)
#define UIMM_MAX	(0xffff)

// SLJIT_LOCALS_REG is not the real stack register, since it must
// point to the head of the stack chain
static sljit_ub reg_map[SLJIT_NO_REGISTERS + 6] = {
  0, 3, 4, 5, 6, 7, 29, 28, 27, 26, 25, 31, 8, 9, 10, 30, 1
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

static INLINE int optimize_jump(struct sljit_jump *jump, sljit_i *code_ptr, sljit_i *code)
{
	sljit_w diff;
	sljit_uw absolute_addr;

	if (jump->flags & SLJIT_LONG_JUMP)
		return 0;

	if (jump->flags & JUMP_ADDR)
		absolute_addr = jump->target;
	else {
		SLJIT_ASSERT(jump->flags & JUMP_LABEL);
		absolute_addr = (sljit_uw)(code + jump->label->size);
	}
	diff = ((sljit_w)absolute_addr - (sljit_w)(code_ptr)) & ~0x3l;

	if (jump->flags & UNCOND_ADDR) {
		if (diff <= 0x01ffffff && diff >= -0x02000000) {
			jump->flags |= PATCH_B;
			return 1;
		}
		if (absolute_addr <= 0x03ffffff) {
			jump->flags |= PATCH_B | ABSOLUTE_B;
			return 1;
		}
	}
	else {
		if (diff <= 0x7fff && diff >= -0x8000) {
			jump->flags |= PATCH_B;
			return 1;
		}
		if (absolute_addr <= 0xffff) {
			jump->flags |= PATCH_B | ABSOLUTE_B;
			return 1;
		}
	}
	return 0;
}

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
	if (!code) {
		compiler->error = SLJIT_EX_MEMORY_ERROR;
		return NULL;
	}
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
			// These structures are ordered by their address
			if (label && label->size == word_count) {
				label->addr = (sljit_uw)code_ptr;
				label->size = code_ptr - code;
				label = label->next;
			}
			if (jump && jump->addr == word_count) {
				SLJIT_ASSERT(jump->flags & (JUMP_LABEL | JUMP_ADDR));
#ifdef SLJIT_CONFIG_PPC_32
				jump->addr = (sljit_uw)(code_ptr - 3);
#else
				jump->addr = (sljit_uw)(code_ptr - 6);
#endif
				if (optimize_jump(jump, code_ptr, code)) {
#ifdef SLJIT_CONFIG_PPC_32
					code_ptr[-3] = code_ptr[0];
					code_ptr -= 3;
#else
					code_ptr[-6] = code_ptr[0];
					code_ptr -= 6;
#endif
				}
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

	SLJIT_ASSERT(label == NULL);
	SLJIT_ASSERT(jump == NULL);
	SLJIT_ASSERT(const_ == NULL);
	SLJIT_ASSERT(code_ptr - code <= (int)compiler->size);

	jump = compiler->jumps;
	while (jump) {
		do {
			addr = (jump->flags & JUMP_LABEL) ? jump->label->addr : jump->target;
			buf_ptr = (sljit_i*)jump->addr;
			if (!(jump->flags & SLJIT_LONG_JUMP) && (jump->flags & PATCH_B)) {
				if (jump->flags & UNCOND_ADDR) {
					if (!(jump->flags & ABSOLUTE_B)) {
						addr = addr - jump->addr;
						SLJIT_ASSERT((sljit_w)addr <= 0x01ffffff && (sljit_w)addr >= -0x02000000);
						*buf_ptr = (18 << 26) | (addr & 0x03fffffc) | ((*buf_ptr) & 0x1);
					}
					else {
						SLJIT_ASSERT(addr <= 0x03ffffff);
						*buf_ptr = (18 << 26) | (addr & 0x03fffffc) | 0x2 | ((*buf_ptr) & 0x1);
					}
				}
				else {
					if (!(jump->flags & ABSOLUTE_B)) {
						addr = addr - jump->addr;
						SLJIT_ASSERT((sljit_w)addr <= 0x7fff && (sljit_w)addr >= -0x8000);
						*buf_ptr = (16 << 26) | (addr & 0xfffc) | ((*buf_ptr) & 0x03ff0001);
					}
					else {
						addr = addr & ~0x3l;
						SLJIT_ASSERT(addr <= 0xffff);
						*buf_ptr = (16 << 26) | (addr & 0xfffc) | 0x2 | ((*buf_ptr) & 0x03ff0001);
					}

				}
				break;
			}
#ifdef SLJIT_CONFIG_PPC_32
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

	SLJIT_CACHE_FLUSH((char *)code, (char *)code_ptr);

	compiler->error = SLJIT_CODE_GENERATED;
	return code;
}

void sljit_free_code(void* code)
{
	SLJIT_FREE_EXEC(code);
}

static int load_immediate(struct sljit_compiler *compiler, int reg, sljit_w imm);
static int emit_op(struct sljit_compiler *compiler, int op, int inp_flags,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w);

int sljit_emit_enter(struct sljit_compiler *compiler, int args, int temporaries, int generals, int local_size)
{
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

	TEST_FAIL(push_inst(compiler, INS_FORM_OP0(31, 0, 0x802a6)));
#ifdef SLJIT_CONFIG_PPC_32
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(36, SLJIT_LOCALS_REG, REAL_STACK_PTR, (-(int)(sizeof(sljit_w))) & 0xffff )));
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(36, ZERO_REG, REAL_STACK_PTR, (-2 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (generals >= 1)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(36, SLJIT_GENERAL_REG1, REAL_STACK_PTR, (-3 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (generals >= 2)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(36, SLJIT_GENERAL_REG2, REAL_STACK_PTR, (-4 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (generals >= 3)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(36, SLJIT_GENERAL_REG3, REAL_STACK_PTR, (-5 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (generals >= 4)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(36, SLJIT_GENERAL_EREG1, REAL_STACK_PTR, (-6 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (generals >= 5)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(36, SLJIT_GENERAL_EREG2, REAL_STACK_PTR, (-7 * (int)(sizeof(sljit_w))) & 0xffff )));
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(36, 0, REAL_STACK_PTR, sizeof(sljit_w))));
#else
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(62, SLJIT_LOCALS_REG, REAL_STACK_PTR, (-(int)(sizeof(sljit_w))) & 0xffff )));
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(62, ZERO_REG, REAL_STACK_PTR, (-2 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (generals >= 1)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(62, SLJIT_GENERAL_REG1, REAL_STACK_PTR, (-3 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (generals >= 2)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(62, SLJIT_GENERAL_REG2, REAL_STACK_PTR, (-4 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (generals >= 3)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(62, SLJIT_GENERAL_REG3, REAL_STACK_PTR, (-5 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (generals >= 4)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(62, SLJIT_GENERAL_EREG1, REAL_STACK_PTR, (-6 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (generals >= 5)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(62, SLJIT_GENERAL_EREG2, REAL_STACK_PTR, (-7 * (int)(sizeof(sljit_w))) & 0xffff )));
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(62, 0, REAL_STACK_PTR, 2 * sizeof(sljit_w))));
#endif

	TEST_FAIL(push_inst(compiler, INS_FORM_IMM(14, ZERO_REG, 0, 0)));
	if (args >= 1)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, SLJIT_TEMPORARY_REG1, SLJIT_GENERAL_REG1, SLJIT_TEMPORARY_REG1, 444 << 1)));
	if (args >= 2)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, SLJIT_TEMPORARY_REG2, SLJIT_GENERAL_REG2, SLJIT_TEMPORARY_REG2, 444 << 1)));
	if (args >= 3)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, SLJIT_TEMPORARY_REG3, SLJIT_GENERAL_REG3, SLJIT_TEMPORARY_REG3, 444 << 1)));

#ifdef SLJIT_CONFIG_PPC_32
	compiler->local_size = (2 + generals + 1) * sizeof(sljit_w) + local_size;
#else
	compiler->local_size = (2 + generals + 7) * sizeof(sljit_w) + local_size;
#endif
	compiler->local_size = (compiler->local_size + 15) & ~0xf;

#ifdef SLJIT_CONFIG_PPC_32
	if (compiler->local_size <= SIMM_MAX) {
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(37, REAL_STACK_PTR, REAL_STACK_PTR, ((-compiler->local_size) & 0xffff))));
	}
	else {
		TEST_FAIL(load_immediate(compiler, 0, -compiler->local_size));
		TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, REAL_STACK_PTR, REAL_STACK_PTR, 0, 183 << 1)));
	}
	TEST_FAIL(push_inst(compiler, INS_FORM_IMM(14, SLJIT_LOCALS_REG, REAL_STACK_PTR, 2 * sizeof(sljit_w))));
#else
	if (compiler->local_size <= SIMM_MAX) {
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

#ifdef SLJIT_CONFIG_PPC_32
	compiler->local_size = (2 + generals + 1) * sizeof(sljit_w) + local_size;
#else
	compiler->local_size = (2 + generals + 7) * sizeof(sljit_w) + local_size;
#endif
	compiler->local_size = (compiler->local_size + 15) & ~0xf;
}

int sljit_emit_return(struct sljit_compiler *compiler, int src, sljit_w srcw)
{
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
		TEST_FAIL(emit_op(compiler, SLJIT_MOV, 0 /* WORD_DATA */, SLJIT_PREF_RET_REG, 0, TMP_REG1, 0, src, srcw));
	}

	if (compiler->local_size <= SIMM_MAX) {
		TEST_FAIL(push_inst(compiler, INS_FORM_IMM(14, REAL_STACK_PTR, REAL_STACK_PTR, compiler->local_size & 0xffff)));
	}
	else {
		TEST_FAIL(load_immediate(compiler, 0, compiler->local_size));
		TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, REAL_STACK_PTR, REAL_STACK_PTR, 0, 266 << 1)));
	}

#ifdef SLJIT_CONFIG_PPC_32
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(32, 0, REAL_STACK_PTR, sizeof(sljit_w))));
	if (compiler->generals >= 5)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(32, SLJIT_GENERAL_EREG2, REAL_STACK_PTR, (-7 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (compiler->generals >= 4)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(32, SLJIT_GENERAL_EREG1, REAL_STACK_PTR, (-6 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (compiler->generals >= 3)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(32, SLJIT_GENERAL_REG3, REAL_STACK_PTR, (-5 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (compiler->generals >= 2)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(32, SLJIT_GENERAL_REG2, REAL_STACK_PTR, (-4 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (compiler->generals >= 1)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(32, SLJIT_GENERAL_REG1, REAL_STACK_PTR, (-3 * (int)(sizeof(sljit_w))) & 0xffff )));
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(32, ZERO_REG, REAL_STACK_PTR, (-2 * (int)(sizeof(sljit_w))) & 0xffff )));
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(32, SLJIT_LOCALS_REG, REAL_STACK_PTR, (-(int)(sizeof(sljit_w))) & 0xffff )));
#else
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(58, 0, REAL_STACK_PTR, 2 * sizeof(sljit_w))));
	if (compiler->generals >= 5)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(58, SLJIT_GENERAL_EREG2, REAL_STACK_PTR, (-7 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (compiler->generals >= 4)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(58, SLJIT_GENERAL_EREG1, REAL_STACK_PTR, (-6 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (compiler->generals >= 3)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(58, SLJIT_GENERAL_REG3, REAL_STACK_PTR, (-5 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (compiler->generals >= 2)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(58, SLJIT_GENERAL_REG2, REAL_STACK_PTR, (-4 * (int)(sizeof(sljit_w))) & 0xffff )));
	if (compiler->generals >= 1)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP1(58, SLJIT_GENERAL_REG1, REAL_STACK_PTR, (-3 * (int)(sizeof(sljit_w))) & 0xffff )));
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(58, ZERO_REG, REAL_STACK_PTR, (-2 * (int)(sizeof(sljit_w))) & 0xffff )));
	TEST_FAIL(push_inst(compiler, INS_FORM_OP1(58, SLJIT_LOCALS_REG, REAL_STACK_PTR, (-(int)(sizeof(sljit_w))) & 0xffff )));
#endif

	TEST_FAIL(push_inst(compiler, INS_FORM_OP0(31, 0, 0x803a6)));
	TEST_FAIL(push_inst(compiler, 0x4e800020));

	return SLJIT_NO_ERROR;
}

// ---------------------------------------------------------------------
//  Operators
// ---------------------------------------------------------------------

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

#define ARG_TEST	0x0100
#define ALT_FORM1	0x0200
#define ALT_FORM2	0x0400
#define ALT_FORM3	0x0800
#define ALT_FORM4	0x1000
// integer opertion and set flags -> requires exts on 64 bit systems
#define ALT_SIGN_EXT	0x2000

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
#define REG_DEST	0x0001
#define REG1_SOURCE	0x0002
#define REG2_SOURCE	0x0004
  // getput_arg_fast returned true
#define FAST_DEST	0x0008
  // Multiple instructions are required
#define SLOW_DEST	0x0010
// ALT_FORM1		0x0200
// ALT_FORM2		0x0400
// ALT_FORM3		0x0800
// ALT_FORM4		0x1000
// ALT_SIGN_EXT		0x2000

#ifdef SLJIT_CONFIG_PPC_32
#include "sljitNativePPC_32.c"
#else
#include "sljitNativePPC_64.c"
#endif

// Simple cases, (no caching is required)
static int getput_arg_fast(struct sljit_compiler *compiler, int inp_flags, int reg, int arg, sljit_w argw)
{
	sljit_i inst;
#ifdef SLJIT_CONFIG_PPC_64
	int tmp_reg;
#endif

	SLJIT_ASSERT(arg & SLJIT_MEM_FLAG);
	if ((arg & 0xf) == SLJIT_UNUSED) {
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

	if ((arg & 0xf0) == SLJIT_UNUSED) {
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

	if ((arg & 0xf) == SLJIT_UNUSED) {
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
		SLJIT_ASSERT((arg & 0xf0) != SLJIT_UNUSED);

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

		if ((arg & 0xf0) != SLJIT_UNUSED) {
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

	if ((arg & 0xf0) != SLJIT_UNUSED) {
		TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, tmp_reg, tmp_reg, (arg >> 4) & 0xf, 266 << 1)));
	}

	return push_inst(compiler, GET_INST_CODE(inst) | (reg_map[reg] << 21) | (reg_map[arg & 0xf] << 16) | ((reg_map[tmp_reg] << 11)));
}

#define ORDER_IND_REGS(arg) \
	SLJIT_ASSERT(arg & SLJIT_MEM_FLAG); \
	if (((arg >> 4) & 0xf) > (arg & 0xf)) \
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
	int flags = inp_flags & (ALT_FORM1 | ALT_FORM2 | ALT_FORM3 | ALT_FORM4 | ALT_SIGN_EXT);

	compiler->cache_arg = 0;
	compiler->cache_argw = 0;

	// Destination check
	if (dst >= SLJIT_TEMPORARY_REG1 && dst <= TMP_REG3) {
		dst_r = dst;
		flags |= REG_DEST;
		if (op >= SLJIT_MOV && op <= SLJIT_MOVU_SI)
			sugg_src2_r = dst_r;
	}
	else if (dst == SLJIT_UNUSED) {
		if (op >= SLJIT_MOV && op <= SLJIT_MOVU_SI && !(src2 & SLJIT_MEM_FLAG))
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
		if (!(flags & REG_DEST) && op >= SLJIT_MOV && op <= SLJIT_MOVU_SI)
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
	FUNCTION_CHECK_OP();
	FUNCTION_CHECK_SRC(src, srcw);
	FUNCTION_CHECK_DST(dst, dstw);
	FUNCTION_CHECK_OP1();
#endif
	sljit_emit_op1_verbose();

#ifdef SLJIT_CONFIG_PPC_64
	if (op & SLJIT_INT_OP) {
		inp_flags |= INT_DATA | SIGNED_DATA;
		if (src & SLJIT_IMM)
			srcw = (srcw << 32) >> 32;
	}
#endif
	if (op & SLJIT_SET_O)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP0(31, ZERO_REG, (1 << 16) | (467 << 1))));

	op = GET_OPCODE(op);
#ifdef SLJIT_CONFIG_PPC_64
	if ((op >= SLJIT_MOV_UI && op <= SLJIT_MOV_SH) || (op >= SLJIT_MOVU_UI && op <= SLJIT_MOVU_SH))
		inp_flags = 0;
#endif

	switch (op) {
	case SLJIT_MOV:
		return emit_op(compiler, SLJIT_MOV, inp_flags | WORD_DATA, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOV_UI:
		return emit_op(compiler, SLJIT_MOV_UI, inp_flags | INT_DATA, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOV_SI:
		return emit_op(compiler, SLJIT_MOV_SI, inp_flags | INT_DATA | SIGNED_DATA, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOV_UB:
		return emit_op(compiler, SLJIT_MOV_UB, inp_flags | BYTE_DATA, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOV_SB:
		return emit_op(compiler, SLJIT_MOV_SB, inp_flags | BYTE_DATA | SIGNED_DATA, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (char)srcw : srcw);

	case SLJIT_MOV_UH:
		return emit_op(compiler, SLJIT_MOV_UH, inp_flags | HALF_DATA, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (unsigned short)srcw : srcw);

	case SLJIT_MOV_SH:
		return emit_op(compiler, SLJIT_MOV_SH, inp_flags | HALF_DATA | SIGNED_DATA, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (short)srcw : srcw);

	case SLJIT_MOVU:
		return emit_op(compiler, SLJIT_MOV, inp_flags | WORD_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOVU_UI:
		return emit_op(compiler, SLJIT_MOV_UI, inp_flags | INT_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOVU_SI:
		return emit_op(compiler, SLJIT_MOV_SI, inp_flags | INT_DATA | SIGNED_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_MOVU_UB:
		return emit_op(compiler, SLJIT_MOV_UB, inp_flags | BYTE_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (unsigned char)srcw : srcw);

	case SLJIT_MOVU_SB:
		return emit_op(compiler, SLJIT_MOV_SB, inp_flags | BYTE_DATA | SIGNED_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (char)srcw : srcw);

	case SLJIT_MOVU_UH:
		return emit_op(compiler, SLJIT_MOV_UH, inp_flags | HALF_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (unsigned short)srcw : srcw);

	case SLJIT_MOVU_SH:
		return emit_op(compiler, SLJIT_MOV_SH, inp_flags | HALF_DATA | SIGNED_DATA | WRITE_BACK, dst, dstw, TMP_REG1, 0, src, (src & SLJIT_IMM) ? (short)srcw : srcw);

	case SLJIT_NOT:
		return emit_op(compiler, SLJIT_NOT, inp_flags, dst, dstw, TMP_REG1, 0, src, srcw);

	case SLJIT_NEG:
		return emit_op(compiler, SLJIT_NEG, inp_flags, dst, dstw, TMP_REG1, 0, src, srcw);
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
	FUNCTION_CHECK_OP();
	FUNCTION_CHECK_SRC(src1, src1w);
	FUNCTION_CHECK_SRC(src2, src2w);
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_op2_verbose();

#ifdef SLJIT_CONFIG_PPC_64
	if (op & SLJIT_INT_OP) {
		inp_flags |= INT_DATA | SIGNED_DATA;
		if (src1 & SLJIT_IMM)
			src1w = (src1w << 32) >> 32;
		if (src2 & SLJIT_IMM)
			src2w = (src2w << 32) >> 32;
		if (GET_FLAGS(op))
			inp_flags |= ALT_SIGN_EXT;
	}
#endif
	if (op & SLJIT_SET_O)
		TEST_FAIL(push_inst(compiler, INS_FORM_OP0(31, ZERO_REG, (1 << 16) | (467 << 1))));

	switch (GET_OPCODE(op)) {
	case SLJIT_ADD:
		if (!GET_FLAGS(op)) {
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
		if (!(GET_FLAGS(op) & (SLJIT_SET_O | SLJIT_SET_U))) {
			if (TEST_SL_IMM(src2, src2w)) {
				compiler->imm = src2w & 0xffff;
				return emit_op(compiler, SLJIT_ADD, inp_flags | ALT_FORM3, dst, dstw, src1, src1w, TMP_REG2, 0);
			}
			if (TEST_SL_IMM(src1, src1w)) {
				compiler->imm = src1w & 0xffff;
				return emit_op(compiler, SLJIT_ADD, inp_flags | ALT_FORM3, dst, dstw, src2, src2w, TMP_REG2, 0);
			}
		}
		return emit_op(compiler, SLJIT_ADD, inp_flags, dst, dstw, src1, src1w, src2, src2w);

	case SLJIT_SUB:
		if (!GET_FLAGS(op)) {
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
		if (dst == SLJIT_UNUSED && GET_FLAGS(op) == SLJIT_SET_U) {
			// We know ALT_SIGN_EXT is set if it is an SLJIT_INT_OP on 64 bit systems
			if (TEST_UL_IMM(src2, src2w)) {
				compiler->imm = src2w & 0xffff;
				return emit_op(compiler, SLJIT_SUB, inp_flags | ALT_FORM2, dst, dstw, src1, src1w, TMP_REG2, 0);
			}
			return emit_op(compiler, SLJIT_SUB, inp_flags | ALT_FORM3, dst, dstw, src1, src1w, src2, src2w);
		}
		if (!(op & (SLJIT_SET_O | SLJIT_SET_U))) {
			if (TEST_SL_IMM(src2, -src2w)) {
				compiler->imm = (-src2w) & 0xffff;
				return emit_op(compiler, SLJIT_ADD, inp_flags | ALT_FORM3, dst, dstw, src1, src1w, TMP_REG2, 0);
			}
		}
		// We know ALT_SIGN_EXT is set if it is an SLJIT_INT_OP on 64 bit systems
		return emit_op(compiler, SLJIT_SUB, inp_flags | ((op & SLJIT_SET_U) ? ALT_FORM4 : 0), dst, dstw, src1, src1w, src2, src2w);

	case SLJIT_ADDC:
		return emit_op(compiler, SLJIT_ADDC, inp_flags, dst, dstw, src1, src1w, src2, src2w);

	case SLJIT_SUBC:
		return emit_op(compiler, SLJIT_SUBC, inp_flags | ((op & SLJIT_SET_U) ? ALT_FORM4 : 0), dst, dstw, src1, src1w, src2, src2w);

	case SLJIT_MUL:
#ifdef SLJIT_CONFIG_PPC_64
		if (op & SLJIT_INT_OP)
			inp_flags |= ALT_FORM2;
#endif
		if (!GET_FLAGS(op)) {
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
		if (!GET_FLAGS(op) || GET_OPCODE(op) == SLJIT_AND) {
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
		if (op & SLJIT_INT_OP)
			inp_flags |= ALT_FORM2;
#endif
		if (src2 & SLJIT_IMM) {
			compiler->imm = src2w;
			return emit_op(compiler, GET_OPCODE(op), inp_flags | ALT_FORM1, dst, dstw, src1, src1w, TMP_REG2, 0);
		}
		return emit_op(compiler, GET_OPCODE(op), inp_flags, dst, dstw, src1, src1w, src2, src2w);
	}

#ifdef SLJIT_CONFIG_PPC_32
	#undef inp_flags
#endif
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

static int emit_fpu_data_transfer(struct sljit_compiler *compiler, int fpu_reg, int load, int arg, sljit_w argw)
{
	SLJIT_ASSERT(arg & SLJIT_MEM_FLAG);

	// Fast loads and stores
	if ((arg & 0xf0) == SLJIT_UNUSED) {
		// Both for (arg & 0xf) == SLJIT_UNUSED and (arg & 0xf) != SLJIT_UNUSED
		if (argw <= SIMM_MAX && argw >= SIMM_MIN)
			return push_inst(compiler, INS_FORM_FOP1(load ? 50 : 54, fpu_reg, arg & 0xf, argw & 0xffff));
	}

	if ((arg & 0xf0) != SLJIT_UNUSED && argw == 0)
		return push_inst(compiler, INS_FORM_FOP2(31, fpu_reg, arg & 0xf, (arg >> 4) & 0xf, load ? (599 << 1) : (727 << 1)));

	ORDER_IND_REGS(arg);

	// Use cache
	if (compiler->cache_arg == arg && argw - compiler->cache_argw <= SIMM_MAX && argw - compiler->cache_argw >= SIMM_MIN)
		return push_inst(compiler, INS_FORM_FOP1(load ? 50 : 54, fpu_reg, TMP_REG3, (argw - compiler->cache_argw) & 0xffff));

	if (compiler->cache_argw == argw) {
		if ((compiler->cache_arg & 0xf) == SLJIT_UNUSED) {
			if ((arg & 0xf0) == SLJIT_UNUSED)
				return push_inst(compiler, INS_FORM_FOP2(31, fpu_reg, arg & 0xf, TMP_REG3, load ? (599 << 1) : (727 << 1)));
		}
		else if ((compiler->cache_arg & 0xf0) == SLJIT_UNUSED) {
			if ((arg & 0xf0) != SLJIT_UNUSED) {
				if ((compiler->cache_arg & 0xf) == (arg & 0xf))
					return push_inst(compiler, INS_FORM_FOP2(31, fpu_reg, (arg >> 4) & 0xf, TMP_REG3, load ? (599 << 1) : (727 << 1)));
				if ((compiler->cache_arg & 0xf) == ((arg >> 4) & 0xf))
					return push_inst(compiler, INS_FORM_FOP2(31, fpu_reg, arg & 0xf, TMP_REG3, load ? (599 << 1) : (727 << 1)));
			}
		}
	}

	// Put value to cache
	compiler->cache_arg = arg;
	compiler->cache_argw = argw;

	TEST_FAIL(load_immediate(compiler, TMP_REG3, argw));
	if ((arg & 0xf) == SLJIT_UNUSED)
		return push_inst(compiler, INS_FORM_FOP2(31, fpu_reg, 0, TMP_REG3, load ? (599 << 1) : (727 << 1)));

	if ((arg & 0xf0) != SLJIT_UNUSED) {
		TEST_FAIL(push_inst(compiler, INS_FORM_OP2(31, TMP_REG3, TMP_REG3, (arg >> 4) & 0xf, 266 << 1)));
	}

	return push_inst(compiler, INS_FORM_FOP2(31, fpu_reg, TMP_REG3, arg & 0xf, load ? (631 << 1) : (759 << 1)));
}

int sljit_emit_fop1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
	int dst_freg;

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
		if (dst > SLJIT_FLOAT_REG4) {
			TEST_FAIL(emit_fpu_data_transfer(compiler, TMP_FREG1, 1, dst, dstw));
			dst = TMP_FREG1;
		}
		if (src > SLJIT_FLOAT_REG4) {
			TEST_FAIL(emit_fpu_data_transfer(compiler, TMP_FREG2, 1, src, srcw));
			src = TMP_FREG2;
		}
		if (GET_FLAGS(op) == SLJIT_SET_E)
			return push_inst(compiler, INS_FORM_FOP(63, 0, dst, src, 0, 0));
		TEST_FAIL(push_inst(compiler, INS_FORM_FOP(63, 4, dst, src, 0, 0)));
		return (op & SLJIT_SET_E) ? push_inst(compiler, INS_FORM_FOP(19, 2, 4 + 2, 4 + 2, 0, 449)) : SLJIT_NO_ERROR;
	}

	dst_freg = (dst > SLJIT_FLOAT_REG4) ? TMP_FREG1 : dst;

	if (src > SLJIT_FLOAT_REG4) {
		TEST_FAIL(emit_fpu_data_transfer(compiler, dst_freg, 1, src, srcw));
		src = dst_freg;
	}

	switch (op) {
		case SLJIT_FMOV:
			if (src != dst_freg && dst_freg != TMP_FREG1)
				TEST_FAIL(push_inst(compiler, INS_FORM_FOP(63, dst_freg, 0, src, 0, 72)));
			break;
		case SLJIT_FNEG:
			TEST_FAIL(push_inst(compiler, INS_FORM_FOP(63, dst_freg, 0, src, 0, 40)));
			break;
		case SLJIT_FABS:
			TEST_FAIL(push_inst(compiler, INS_FORM_FOP(63, dst_freg, 0, src, 0, 264)));
			break;
	}

	if (dst_freg == TMP_FREG1) {
		TEST_FAIL(emit_fpu_data_transfer(compiler, src, 0, dst, dstw));
	}

	return SLJIT_NO_ERROR;
}

int sljit_emit_fop2(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	int dst_freg;

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

	dst_freg = (dst > SLJIT_FLOAT_REG4) ? TMP_FREG1 : dst;

	if (src2 > SLJIT_FLOAT_REG4) {
		TEST_FAIL(emit_fpu_data_transfer(compiler, TMP_FREG2, 1, src2, src2w));
		src2 = TMP_FREG2;
	}

	if (src1 > SLJIT_FLOAT_REG4) {
		TEST_FAIL(emit_fpu_data_transfer(compiler, TMP_FREG1, 1, src1, src1w));
		src1 = TMP_FREG1;
	}

	switch (op) {
	case SLJIT_FADD:
		TEST_FAIL(push_inst(compiler, INS_FORM_FOP(63, dst_freg, src1, src2, 0, 21)));
		break;

	case SLJIT_FSUB:
		TEST_FAIL(push_inst(compiler, INS_FORM_FOP(63, dst_freg, src1, src2, 0, 20)));
		break;

	case SLJIT_FMUL:
		TEST_FAIL(push_inst(compiler, INS_FORM_FOP(63, dst_freg, src1, 0, src2, 25)));
		break;

	case SLJIT_FDIV:
		TEST_FAIL(push_inst(compiler, INS_FORM_FOP(63, dst_freg, src1, src2, 0, 18)));
		break;
	}

	if (dst_freg == TMP_FREG1) {
		TEST_FAIL(emit_fpu_data_transfer(compiler, TMP_FREG1, 0, dst, dstw));
	}

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

	if (compiler->last_label && compiler->last_label->size == compiler->size)
		return compiler->last_label;

	label = (struct sljit_label*)ensure_abuf(compiler, sizeof(struct sljit_label));
	TEST_MEM_ERROR2(label);

	label->next = NULL;
	label->size = compiler->size;
	if (compiler->last_label)
		compiler->last_label->next = label;
	else
		compiler->labels = label;
	compiler->last_label = label;
	return label;
}

static sljit_i get_bo_bi_flags(struct sljit_compiler *compiler, int type)
{
	switch (type) {
	case SLJIT_C_EQUAL:
		return (12 << 21) | (2 << 16);

	case SLJIT_C_NOT_EQUAL:
		return (4 << 21) | (2 << 16);

	case SLJIT_C_LESS:
		return (12 << 21) | ((4 + 0) << 16);

	case SLJIT_C_NOT_LESS:
		return (4 << 21) | ((4 + 0) << 16);

	case SLJIT_C_GREATER:
		return (12 << 21) | ((4 + 1) << 16);

	case SLJIT_C_NOT_GREATER:
		return (4 << 21) | ((4 + 1) << 16);

	case SLJIT_C_SIG_LESS:
		return (12 << 21) | (0 << 16);

	case SLJIT_C_SIG_NOT_LESS:
		return (4 << 21) | (0 << 16);

	case SLJIT_C_SIG_GREATER:
		return (12 << 21) | (1 << 16);

	case SLJIT_C_SIG_NOT_GREATER:
		return (4 << 21) | (1 << 16);

	case SLJIT_C_OVERFLOW:
		return (12 << 21) | (3 << 16);

	case SLJIT_C_NOT_OVERFLOW:
		return (4 << 21) | (3 << 16);

	default:
		return (20 << 21);
	}
}

struct sljit_jump* sljit_emit_jump(struct sljit_compiler *compiler, int type)
{
	struct sljit_jump *jump;
	sljit_i bo_bi_flags;

	FUNCTION_ENTRY();
	SLJIT_ASSERT((type & ~0x1ff) == 0);
	SLJIT_ASSERT((type & 0xff) >= SLJIT_C_EQUAL && (type & 0xff) <= SLJIT_CALL3);

	sljit_emit_jump_verbose();

	bo_bi_flags = get_bo_bi_flags(compiler, type & 0xff);
	if (!bo_bi_flags)
		return NULL;

	jump = (struct sljit_jump*)ensure_abuf(compiler, sizeof(struct sljit_jump));
	TEST_MEM_ERROR2(jump);

	jump->next = NULL;
	jump->flags = type & SLJIT_LONG_JUMP;
	type &= 0xff;
	if (compiler->last_jump)
		compiler->last_jump->next = jump;
	else
		compiler->jumps = jump;
	compiler->last_jump = jump;

	// In PPC, we don't need to touch the arguments
	if (type >= SLJIT_JUMP)
		jump->flags |= UNCOND_ADDR;

	TEST_FAIL2(emit_const(compiler, TMP_REG1, 0));
	TEST_FAIL2(push_inst(compiler, INS_FORM_OP0(31, TMP_REG1, (9 << 16) | (467 << 1))));
	jump->addr = compiler->size;
	TEST_FAIL2(push_inst(compiler, (19 << 26) | bo_bi_flags | (3 << 11) | (528 << 1) | (type >= SLJIT_CALL0 ? 1 : 0)));
	return jump;
}

int sljit_emit_ijump(struct sljit_compiler *compiler, int type, int src, sljit_w srcw)
{
	sljit_i bo_bi_flags;
	int src_r;

	FUNCTION_ENTRY();
	SLJIT_ASSERT(type >= SLJIT_JUMP && type <= SLJIT_CALL3);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_SRC(src, srcw);
#endif
	sljit_emit_ijump_verbose();

	bo_bi_flags = get_bo_bi_flags(compiler, type);
	TEST_FAIL(!bo_bi_flags);

	if (src >= SLJIT_TEMPORARY_REG1 && src <= SLJIT_NO_REGISTERS)
		src_r = src;
	else if (src & SLJIT_IMM) {
		TEST_FAIL(load_immediate(compiler, TMP_REG2, srcw));
		src_r = TMP_REG2;
	}
	else {
		TEST_FAIL(emit_op(compiler, SLJIT_MOV, WORD_DATA, TMP_REG2, 0, TMP_REG1, 0, src, srcw));
		src_r = TMP_REG2;
	}

	TEST_FAIL(push_inst(compiler, INS_FORM_OP0(31, src_r, (9 << 16) | (467 << 1))));
	return push_inst(compiler, (19 << 26) | bo_bi_flags | (3 << 11) | (528 << 1) | (type >= SLJIT_CALL0 ? 1 : 0));
}

// Get a bit from CR, all other bits are zeroed
#define GET_CR_BIT(bit, dst) \
	TEST_FAIL(push_inst(compiler, INS_FORM_OP0(31, dst, (19 << 1)))); \
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

	if (dst == SLJIT_UNUSED)
		return SLJIT_NO_ERROR;

	if (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_NO_REGISTERS)
		reg = dst;
	else
		reg = TMP_REG2;

	switch (type) {
	case SLJIT_C_EQUAL:
		GET_CR_BIT(2, reg);
		break;

	case SLJIT_C_NOT_EQUAL:
		GET_CR_BIT(2, reg);
		INVERT_BIT(reg);
		break;

	case SLJIT_C_LESS:
		GET_CR_BIT(4 + 0, reg);
		break;

	case SLJIT_C_NOT_LESS:
		GET_CR_BIT(4 + 0, reg);
		INVERT_BIT(reg);
		break;

	case SLJIT_C_GREATER:
		GET_CR_BIT(4 + 1, reg);
		break;

	case SLJIT_C_NOT_GREATER:
		GET_CR_BIT(4 + 1, reg);
		INVERT_BIT(reg);
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
		GET_CR_BIT(3, reg);
		break;

	case SLJIT_C_NOT_OVERFLOW:
		GET_CR_BIT(3, reg);
		INVERT_BIT(reg);
		break;
	}

	if (reg == TMP_REG2)
		return emit_op(compiler, SLJIT_MOV, WORD_DATA, dst, dstw, TMP_REG1, 0, TMP_REG2, 0);
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

	const_ = (struct sljit_const*)ensure_abuf(compiler, sizeof(struct sljit_const));
	TEST_MEM_ERROR2(const_);

	const_->next = NULL;
	const_->addr = compiler->size;
	if (compiler->last_const)
		compiler->last_const->next = const_;
	else
		compiler->consts = const_;
	compiler->last_const = const_;

	reg = (dst >= SLJIT_TEMPORARY_REG1 && dst <= SLJIT_NO_REGISTERS) ? dst : TMP_REG2;

	if (emit_const(compiler, reg, initval))
		return NULL;

	if (reg == TMP_REG2 && dst != SLJIT_UNUSED)
		if (emit_op(compiler, SLJIT_MOV, WORD_DATA, dst, dstw, TMP_REG1, 0, TMP_REG2, 0))
			return NULL;
	return const_;
}
