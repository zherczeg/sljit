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

#include "sljitLir.h"

#define FUNCTION_ENTRY() \
	SLJIT_ASSERT(compiler->error == SLJIT_NO_ERROR)

#define TEST_MEM_ERROR(ptr) \
	if (!(ptr)) { \
		compiler->error = SLJIT_MEMORY_ERROR; \
		return SLJIT_MEMORY_ERROR; \
	}

#define TEST_MEM_ERROR2(ptr) \
	if (!(ptr)) { \
		compiler->error = SLJIT_MEMORY_ERROR; \
		return NULL; \
	}

#define TEST_FAIL(expr) \
	if (expr) \
		return compiler->error;

#define BUF_SIZE	2048
#define ABUF_SIZE	512

// Max local_size. This could be increased if it is really necessary.
#define SLJIT_MAX_LOCAL_SIZE	65536

// Jump flags
#define JUMP_LABEL	0x1
#define JUMP_ADDR	0x2

#if defined(SLJIT_CONFIG_X86_32) || defined(SLJIT_CONFIG_X86_64)
	#define PATCH_MB	0x4
	#define PATCH_MW	0x8
#ifdef SLJIT_CONFIG_X86_64
	#define PATCH_MD	0x10
#endif
#endif

#ifdef SLJIT_CONFIG_ARM
	#define PATCH_B		0x4
	#define IS_BL		0x8

#define CPOOL_SIZE	512
#define LIT_NONE	0
// Normal instruction
#define LIT_INS		1
// Instruction with a constant
#define LIT_CINS	2
// Instruction with a unique constant
#define LIT_UCINS	3
#endif

#if defined(SLJIT_CONFIG_X86_64) || defined(SLJIT_CONFIG_PPC_64)
#include <sys/mman.h>

void* sljit_malloc_exec(int size)
{
	void* ptr;

	size += sizeof(int);
	ptr = mmap(0, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANON, -1, 0);
	if (ptr == MAP_FAILED)
		return NULL;

	*(int*)ptr = size;
	ptr = (void*)(((sljit_ub*)ptr) + sizeof(int));
	return ptr;
}

void sljit_free_exec(void* ptr)
{
	ptr = (void*)(((sljit_ub*)ptr) - sizeof(int));
	munmap(ptr, *(int*)ptr);
}

#endif

// ---------------------------------------------------------------------
//  Public functions
// ---------------------------------------------------------------------

#ifdef SLJIT_CONFIG_ARM
static void detect_cpu_features();
#else
#define detect_cpu_features()
#endif

struct sljit_compiler* sljit_create_compiler(void)
{
	struct sljit_compiler *compiler = SLJIT_MALLOC(sizeof(struct sljit_compiler));

	if (!compiler)
		return NULL;

	compiler->error = SLJIT_NO_ERROR;

	compiler->labels = NULL;
	compiler->jumps = NULL;
	compiler->consts = NULL;
	compiler->last_label = NULL;
	compiler->last_jump = NULL;
	compiler->last_const = NULL;

	compiler->buf = SLJIT_MALLOC(BUF_SIZE);
	compiler->abuf = SLJIT_MALLOC(ABUF_SIZE);

	if (!compiler->buf || !compiler->abuf) {
		if (compiler->buf)
			SLJIT_FREE(compiler->buf);
		if (compiler->abuf)
			SLJIT_FREE(compiler->abuf);
		SLJIT_FREE(compiler);
		return NULL;
	}

	compiler->buf->next = NULL;
	compiler->buf->used_size = 0;
	compiler->abuf->next = NULL;
	compiler->abuf->used_size = 0;

	compiler->general = -1;
	compiler->local_size = 0;
	compiler->size = 0;

#ifdef SLJIT_CONFIG_X86_32
	compiler->args = -1;
#endif

#ifdef SLJIT_CONFIG_X86_64
	compiler->addrs = 0;
#endif

#ifdef SLJIT_CONFIG_ARM
	compiler->cpool = SLJIT_MALLOC(CPOOL_SIZE * sizeof(sljit_uw) + CPOOL_SIZE * sizeof(sljit_ub));
	if (!compiler->cpool) {
		SLJIT_FREE(compiler->buf);
		SLJIT_FREE(compiler->abuf);
		SLJIT_FREE(compiler);
		return NULL;
	}
	compiler->cpool_unique = (sljit_ub*)(compiler->cpool + CPOOL_SIZE);
	compiler->cpool_diff = 0xffffffff;
	compiler->cpool_fill = 0;
	compiler->patches = 0;
	compiler->last_type = LIT_NONE;

#endif

#ifdef SLJIT_VERBOSE
	compiler->verbose = NULL;
#endif
	detect_cpu_features();

	return compiler;
}

void sljit_free_compiler(struct sljit_compiler *compiler)
{
	struct sljit_memory_fragment *buf;
	struct sljit_memory_fragment *curr;

	buf = compiler->buf;
	while (buf) {
		curr = buf;
		buf = buf->next;
		SLJIT_FREE(curr);
	}

	buf = compiler->abuf;
	while (buf) {
		curr = buf;
		buf = buf->next;
		SLJIT_FREE(curr);
	}

#ifdef SLJIT_CONFIG_ARM
	//SLJIT_FREE(compiler->cpool);
#endif
	SLJIT_FREE(compiler);
}

void sljit_set_label(struct sljit_jump *jump, struct sljit_label* label)
{
	jump->flags &= ~JUMP_ADDR;
	jump->flags |= JUMP_LABEL;
	jump->label = label;
}

void sljit_set_target(struct sljit_jump *jump, sljit_uw target)
{
	SLJIT_ASSERT(jump->flags & SLJIT_LONG_JUMP);

	jump->flags &= ~JUMP_LABEL;
	jump->flags |= JUMP_ADDR;
	jump->target = target;
}

// ---------------------------------------------------------------------
//  Private functions
// ---------------------------------------------------------------------

static void* ensure_buf(struct sljit_compiler *compiler, int size)
{
	sljit_ub *ret;
	struct sljit_memory_fragment *new_frag;

	if (compiler->buf->used_size + size <= (BUF_SIZE - sizeof(int) - sizeof(void*))) {
		ret = compiler->buf->memory + compiler->buf->used_size;
		compiler->buf->used_size += size;
		return ret;
	}
	new_frag = SLJIT_MALLOC(BUF_SIZE);
	if (!new_frag)
		return NULL;
	new_frag->next = compiler->buf;
	compiler->buf = new_frag;
	new_frag->used_size = size;
	return new_frag->memory;
}

static void* ensure_abuf(struct sljit_compiler *compiler, int size)
{
	sljit_ub *ret;
	struct sljit_memory_fragment *new_frag;

	if (compiler->abuf->used_size + size <= (ABUF_SIZE - sizeof(int) - sizeof(void*))) {
		ret = compiler->abuf->memory + compiler->abuf->used_size;
		compiler->abuf->used_size += size;
		return ret;
	}
	new_frag = SLJIT_MALLOC(ABUF_SIZE);
	if (!new_frag)
		return NULL;
	new_frag->next = compiler->abuf;
	compiler->abuf = new_frag;
	new_frag->used_size = size;
	return new_frag->memory;
}

static void reverse_buf(struct sljit_compiler *compiler)
{
	struct sljit_memory_fragment *buf = compiler->buf;
	struct sljit_memory_fragment *prev = NULL;
	struct sljit_memory_fragment *tmp;

	do {
		tmp = buf->next;
		buf->next = prev;
		prev = buf;
		buf = tmp;
	} while (buf != NULL);

	compiler->buf = prev;
}

#define depends_on(exp, reg) \
	(((exp) & SLJIT_MEM_FLAG) && (((exp) & 0xf) == reg || (((exp) >> 4) & 0xf) == reg))

#ifdef SLJIT_DEBUG
#define FUNCTION_CHECK_SRC(p, i) \
	if ((p) >= SLJIT_TEMPORARY_REG1 && (p) <= SLJIT_LOCALS_REG) \
		SLJIT_ASSERT(i == 0); \
	else if ((p) == SLJIT_IMM) \
		; \
	else if ((p) & SLJIT_MEM_FLAG) { \
		SLJIT_ASSERT(((p) & 0xf) <= SLJIT_LOCALS_REG); \
		if (((p) & 0xf) != 0) { \
			SLJIT_ASSERT((((p) >> 4) & 0xf) <= SLJIT_LOCALS_REG); \
			SLJIT_ASSERT((((p) >> 4) & 0xf) != SLJIT_LOCALS_REG || ((p) & 0xf) != SLJIT_LOCALS_REG); \
		} else \
			SLJIT_ASSERT((((p) >> 4) & 0xf) == 0); \
		SLJIT_ASSERT(((p) >> 9) == 0); \
	} \
	else \
		SLJIT_ASSERT_IMPOSSIBLE();

#define FUNCTION_CHECK_DST(p, i) \
	if ((p) >= SLJIT_NO_REG && (p) <= SLJIT_GENERAL_REG3) \
		SLJIT_ASSERT(i == 0); \
	else if ((p) & SLJIT_MEM_FLAG) { \
		SLJIT_ASSERT(((p) & 0xf) <= SLJIT_LOCALS_REG); \
		if (((p) & 0xf) != 0) { \
			SLJIT_ASSERT((((p) >> 4) & 0xf) <= SLJIT_LOCALS_REG); \
			SLJIT_ASSERT((((p) >> 4) & 0xf) != SLJIT_LOCALS_REG || ((p) & 0xf) != SLJIT_LOCALS_REG); \
		} else \
			SLJIT_ASSERT((((p) >> 4) & 0xf) == 0); \
		SLJIT_ASSERT(((p) >> 9) == 0); \
	} \
	else \
		SLJIT_ASSERT_IMPOSSIBLE();

#define FUNCTION_FCHECK(p, i) \
	if ((p) >= SLJIT_FLOAT_REG1 && (p) <= SLJIT_FLOAT_REG4) \
		SLJIT_ASSERT(i == 0); \
	else if ((p) & SLJIT_MEM_FLAG) { \
		SLJIT_ASSERT(((p) & 0xf) <= SLJIT_LOCALS_REG); \
		if (((p) & 0xf) != 0) { \
			SLJIT_ASSERT((((p) >> 4) & 0xf) <= SLJIT_LOCALS_REG); \
			SLJIT_ASSERT((((p) >> 4) & 0xf) != SLJIT_LOCALS_REG || ((p) & 0xf) != SLJIT_LOCALS_REG); \
		} else \
			SLJIT_ASSERT((((p) >> 4) & 0xf) == 0); \
		SLJIT_ASSERT(((p) >> 9) == 0); \
	} \
	else \
		SLJIT_ASSERT_IMPOSSIBLE();

#endif

#ifdef SLJIT_VERBOSE

void sljit_compiler_verbose(struct sljit_compiler *compiler, FILE* verbose)
{
	compiler->verbose = verbose;
}

static char* reg_names[] = {
	"<noreg>", "tmp_r1", "tmp_r2", "tmp_r3", "gen_r1", "gen_r2", "gen_r3", "stack_r"
};

static char* freg_names[] = {
	"<noreg>", "fr1", "fr2", "fr3", "fr4"
};

#if defined(SLJIT_CONFIG_X86_64) || defined(SLJIT_CONFIG_PPC_64)
	#define SLJIT_PRINT_D	"l"
#else
	#define SLJIT_PRINT_D	""
#endif

#define sljit_emit_enter_verbose() \
	if (compiler->verbose) fprintf(compiler->verbose, "  enter args=%d generals=%d locals=%d\n", args, general, local_size);
#define sljit_fake_enter_verbose() \
	if (compiler->verbose) fprintf(compiler->verbose, "  fake enter args=%d generals=%d locals=%d\n", args, general, local_size);
#define sljit_emit_return_verbose() \
	if (compiler->verbose) fprintf(compiler->verbose, "  return %s\n", reg_names[reg]);
#define sljit_verbose_param(p, i) \
	if ((p) & SLJIT_IMM) \
		fprintf(compiler->verbose, "#%"SLJIT_PRINT_D"d", (i)); \
	else if ((p) & SLJIT_MEM_FLAG) { \
		if ((p) & 0xF) { \
			if ((i) != 0) { \
				if (((p) >> 4) & 0xF) \
					fprintf(compiler->verbose, "[%s + %s + #%"SLJIT_PRINT_D"d]", reg_names[(p) & 0xF], reg_names[((p) >> 4)& 0xF], (i)); \
				else \
					fprintf(compiler->verbose, "[%s + #%"SLJIT_PRINT_D"d]", reg_names[(p) & 0xF], (i)); \
			} \
			else { \
				if (((p) >> 4) & 0xF) \
					fprintf(compiler->verbose, "[%s + %s]", reg_names[(p) & 0xF], reg_names[((p) >> 4)& 0xF]); \
				else \
					fprintf(compiler->verbose, "[%s]", reg_names[(p) & 0xF]); \
			} \
		} \
		else \
			fprintf(compiler->verbose, "[#%"SLJIT_PRINT_D"d]", (i)); \
	} else \
		fprintf(compiler->verbose, "%s", reg_names[p]);
#define sljit_verbose_fparam(p, i) \
	if ((p) & SLJIT_MEM_FLAG) { \
		if ((p) & 0xF) { \
			if ((i) != 0) { \
				if (((p) >> 4) & 0xF) \
					fprintf(compiler->verbose, "[%s + %s + #%"SLJIT_PRINT_D"d]", reg_names[(p) & 0xF], reg_names[((p) >> 4)& 0xF], (i)); \
				else \
					fprintf(compiler->verbose, "[%s + #%"SLJIT_PRINT_D"d]", reg_names[(p) & 0xF], (i)); \
			} \
			else { \
				if (((p) >> 4) & 0xF) \
					fprintf(compiler->verbose, "[%s + %s]", reg_names[(p) & 0xF], reg_names[((p) >> 4)& 0xF]); \
				else \
					fprintf(compiler->verbose, "[%s]", reg_names[(p) & 0xF]); \
			} \
		} \
		else \
			fprintf(compiler->verbose, "[#%"SLJIT_PRINT_D"d]", (i)); \
	} else \
		fprintf(compiler->verbose, "%s", freg_names[p]);
static char* op1_names[] = {
	"mov", "not", "neg"
};
#define sljit_emit_op1_verbose() \
	if (compiler->verbose) { \
		fprintf(compiler->verbose, "  %s%s ", op1_names[op & ~SLJIT_32BIT_OPERATION], !(op & SLJIT_32BIT_OPERATION) ? "" : "32"); \
		sljit_verbose_param(dst, dstw); \
		fprintf(compiler->verbose, ", "); \
		sljit_verbose_param(src, srcw); \
		fprintf(compiler->verbose, "\n"); \
	}
static char* op2_names[] = {
	"add", "addc", "sub", "subc", "mul",
	"and", "or", "xor", "shl", "lshr", "ashr"
};
#define sljit_emit_op2_verbose() \
	if (compiler->verbose) { \
		fprintf(compiler->verbose, "  %s%s ", op2_names[op & ~SLJIT_32BIT_OPERATION], !(op & SLJIT_32BIT_OPERATION) ? "" : "32"); \
		sljit_verbose_param(dst, dstw); \
		fprintf(compiler->verbose, ", "); \
		sljit_verbose_param(src1, src1w); \
		fprintf(compiler->verbose, ", "); \
		sljit_verbose_param(src2, src2w); \
		fprintf(compiler->verbose, "\n"); \
	}
static char* fop1_names[] = {
	"fcmp", "fmov", "fneg", "fabs"
};
#define sljit_emit_fop1_verbose() \
	if (compiler->verbose) { \
		fprintf(compiler->verbose, "  %s ", fop1_names[op]); \
		sljit_verbose_fparam(dst, dstw); \
		fprintf(compiler->verbose, ", "); \
		sljit_verbose_fparam(src, srcw); \
		fprintf(compiler->verbose, "\n"); \
	}
static char* fop2_names[] = {
	"fadd", "fsub", "fmul", "fdiv"
};
#define sljit_emit_fop2_verbose() \
	if (compiler->verbose) { \
		fprintf(compiler->verbose, "  %s ", fop2_names[op]); \
		sljit_verbose_fparam(dst, dstw); \
		fprintf(compiler->verbose, ", "); \
		sljit_verbose_fparam(src1, src1w); \
		fprintf(compiler->verbose, ", "); \
		sljit_verbose_fparam(src2, src2w); \
		fprintf(compiler->verbose, "\n"); \
	}
#define sljit_emit_label_verbose() \
	if (compiler->verbose) fprintf(compiler->verbose, "label:\n");
static char* jump_names[] = {
	"c_equal", "c_not_equal",
	"c_less", "c_not_less",
	"c_greater", "c_not_greater",
	"c_sig_less", "c_sig_not_less",
	"c_sig_greater", "c_sig_not_greater",
	"c_carry", "c_not_carry",
	"c_zero", "c_not_zero",
	"c_overflow", "c_not_overflow",
	"jump",
	"call0", "call1", "call2", "call3"
};
#define sljit_emit_jump_verbose() \
	if (compiler->verbose) fprintf(compiler->verbose, "  jump <%s> %s\n", jump_names[type & 0xff], (type & SLJIT_LONG_JUMP) ? "(long)" : "");
#define sljit_emit_ijump_verbose() \
	if (compiler->verbose) { \
		fprintf(compiler->verbose, "  ijump <%s> ", jump_names[type]); \
		sljit_verbose_param(src, srcw); \
		fprintf(compiler->verbose, "\n"); \
	}
#define sljit_emit_set_cond_verbose() \
	if (compiler->verbose) { \
		fprintf(compiler->verbose, "  cond_set "); \
		sljit_verbose_param(dst, dstw); \
		fprintf(compiler->verbose, ", %s\n", jump_names[type]); \
	}
#define sljit_emit_const_verbose() \
	if (compiler->verbose) { \
		fprintf(compiler->verbose, "  const "); \
		sljit_verbose_param(dst, dstw); \
		fprintf(compiler->verbose, ", #%"SLJIT_PRINT_D"d\n", initval); \
	}

#else

#define sljit_emit_enter_verbose()
#define sljit_fake_enter_verbose()
#define sljit_emit_return_verbose()
#define sljit_emit_op1_verbose()
#define sljit_emit_op2_verbose()
#define sljit_emit_fop1_verbose()
#define sljit_emit_fop2_verbose()
#define sljit_emit_label_verbose()
#define sljit_emit_jump_verbose()
#define sljit_emit_ijump_verbose()
#define sljit_emit_set_cond_verbose()
#define sljit_emit_const_verbose()

#endif

// ---------------------------------------------------------------------
//  Arch dependent
// ---------------------------------------------------------------------

#if defined(SLJIT_CONFIG_X86_32)
	#include "sljitNativeX86_common.c"
#elif defined(SLJIT_CONFIG_X86_64)
	#include "sljitNativeX86_common.c"
#elif defined(SLJIT_CONFIG_ARM)
	#include "sljitNativeARM.c"
#elif defined(SLJIT_CONFIG_PPC_32)
	#include "sljitNativePPC_common.c"
#elif defined(SLJIT_CONFIG_PPC_64)
	#include "sljitNativePPC_common.c"
#else
	#error "Unknown native code generation method"
#endif

