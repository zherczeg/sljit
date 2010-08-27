/*
 *    Stack-less Just-In-Time compiler
 *
 *    Copyright 2009-2010 Zoltan Herczeg. All rights reserved.
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

#include "sljitLir.h"

#ifndef SLJIT_CONFIG_UNSUPPORTED

#define FUNCTION_ENTRY() \
	SLJIT_ASSERT(compiler->error == SLJIT_SUCCESS)

#define FAIL_IF(expr) \
	do { \
		if (SLJIT_UNLIKELY(expr)) \
			return compiler->error; \
	} while (0)

#define PTR_FAIL_IF(expr) \
	do { \
		if (SLJIT_UNLIKELY(expr)) \
			return NULL; \
	} while (0)

#define FAIL_IF_NULL(ptr) \
	do { \
		if (SLJIT_UNLIKELY(!(ptr))) { \
			compiler->error = SLJIT_ERR_ALLOC_FAILED; \
			return SLJIT_ERR_ALLOC_FAILED; \
		} \
	} while (0)

#define PTR_FAIL_IF_NULL(ptr) \
	do { \
		if (SLJIT_UNLIKELY(!(ptr))) { \
			compiler->error = SLJIT_ERR_ALLOC_FAILED; \
			return NULL; \
		} \
	} while (0)

#define PTR_FAIL_WITH_EXEC_IF(ptr) \
	do { \
		if (SLJIT_UNLIKELY(!(ptr))) { \
			compiler->error = SLJIT_ERR_EX_ALLOC_FAILED; \
			return NULL; \
		} \
	} while (0)

#define GET_OPCODE(op) \
	((op) & ~(SLJIT_INT_OP | SLJIT_SET_E | SLJIT_SET_S | SLJIT_SET_U | SLJIT_SET_O | SLJIT_SET_C))

#define GET_FLAGS(op) \
	((op) & (SLJIT_SET_E | SLJIT_SET_S | SLJIT_SET_U | SLJIT_SET_O | SLJIT_SET_C))

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

#if defined(SLJIT_CONFIG_ARM_V5) || defined(SLJIT_CONFIG_ARM_V7)
	#define IS_BL		0x4
	#define PATCH_B		0x8

#define CPOOL_SIZE	512
#define LIT_NONE	0
// Normal instruction
#define LIT_INS		1
// Instruction with a constant
#define LIT_CINS	2
// Instruction with a unique constant
#define LIT_UCINS	3
#endif

#if defined(SLJIT_CONFIG_ARM_THUMB2)
	#define IS_CONDITIONAL	0x04
	#define IS_BL		0x08
#endif

#if defined(SLJIT_CONFIG_PPC_32) || defined(SLJIT_CONFIG_PPC_64)
	#define UNCOND_ADDR	0x04
	#define PATCH_B		0x08
	#define ABSOLUTE_B	0x10
#endif

#if defined(SLJIT_CONFIG_X86_64) || defined(SLJIT_CONFIG_PPC_64)
#include <sys/mman.h>

static void* sljit_malloc_exec(sljit_w size)
{
	void* ptr;

	size += sizeof(sljit_w);
	ptr = mmap(0, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANON, -1, 0);
	if (ptr == MAP_FAILED)
		return NULL;

	*(sljit_w*)ptr = size;
	ptr = (void*)(((sljit_ub*)ptr) + sizeof(sljit_w));
	return ptr;
}

static void sljit_free_exec(void* ptr)
{
	ptr = (void*)(((sljit_ub*)ptr) - sizeof(sljit_w));
	munmap(ptr, *(sljit_w*)ptr);
}

#endif

#if defined(SLJIT_SSE2_AUTO) && !defined(SLJIT_SSE2)
#error SLJIT_SSE2_AUTO cannot be enabled without SLJIT_SSE2
#endif

// ---------------------------------------------------------------------
//  Public functions
// ---------------------------------------------------------------------

#if defined(SLJIT_CONFIG_ARM_V5) || (defined(SLJIT_SSE2) && (defined(SLJIT_CONFIG_X86_32) || defined(SLJIT_CONFIG_X86_64)))
#define NEEDS_COMPILER_INIT
static int compiler_initialized = 0;
// A thread safe initialization
static void init_compiler();
#endif

struct sljit_compiler* sljit_create_compiler(void)
{
	struct sljit_compiler *compiler = (struct sljit_compiler*)SLJIT_MALLOC(sizeof(struct sljit_compiler));

	if (!compiler)
		return NULL;

	compiler->error = SLJIT_SUCCESS;

	compiler->labels = NULL;
	compiler->jumps = NULL;
	compiler->consts = NULL;
	compiler->last_label = NULL;
	compiler->last_jump = NULL;
	compiler->last_const = NULL;

	compiler->buf = (struct sljit_memory_fragment*)SLJIT_MALLOC(BUF_SIZE);
	compiler->abuf = (struct sljit_memory_fragment*)SLJIT_MALLOC(ABUF_SIZE);

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

	compiler->temporaries = -1;
	compiler->generals = -1;
	compiler->local_size = 0;
	compiler->size = 0;

#ifdef SLJIT_CONFIG_X86_32
	compiler->args = -1;
#endif

#if defined(SLJIT_CONFIG_ARM_V5) || defined(SLJIT_CONFIG_ARM_V7)
	compiler->cpool = (sljit_uw*)SLJIT_MALLOC(CPOOL_SIZE * sizeof(sljit_uw) + CPOOL_SIZE * sizeof(sljit_ub));
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
#endif

#ifdef SLJIT_VERBOSE
	compiler->verbose = NULL;
#endif

#ifdef NEEDS_COMPILER_INIT
	if (!compiler_initialized) {
		init_compiler();
		compiler_initialized = 1;
	}
#endif

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

#if defined(SLJIT_CONFIG_ARM_V5) || defined(SLJIT_CONFIG_ARM_V7)
	SLJIT_FREE(compiler->cpool);
#endif
	SLJIT_FREE(compiler);
}

#if defined(SLJIT_CONFIG_ARM_THUMB2)
void sljit_free_code(void* code)
{
	// Remove thumb mode flag
	SLJIT_FREE_EXEC((void*)((sljit_uw)code & ~0x1));
}
#elif defined(SLJIT_CONFIG_PPC_64)
void sljit_free_code(void* code)
{
	// Resolve indirection
	code = (void*)(*(sljit_uw*)code);
	SLJIT_FREE_EXEC(code);
}
#else
void sljit_free_code(void* code)
{
	SLJIT_FREE_EXEC(code);
}
#endif

void sljit_set_label(struct sljit_jump *jump, struct sljit_label* label)
{
	jump->flags &= ~JUMP_ADDR;
	jump->flags |= JUMP_LABEL;
	jump->label = label;
}

void sljit_set_target(struct sljit_jump *jump, sljit_uw target)
{
	SLJIT_ASSERT(jump->flags & SLJIT_REWRITABLE_JUMP);

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

	if (compiler->buf->used_size + size <= (int)(BUF_SIZE - sizeof(int) - sizeof(void*))) {
		ret = compiler->buf->memory + compiler->buf->used_size;
		compiler->buf->used_size += size;
		return ret;
	}
	new_frag = (struct sljit_memory_fragment*)SLJIT_MALLOC(BUF_SIZE);
	PTR_FAIL_IF_NULL(new_frag);
	new_frag->next = compiler->buf;
	compiler->buf = new_frag;
	new_frag->used_size = size;
	return new_frag->memory;
}

static void* ensure_abuf(struct sljit_compiler *compiler, int size)
{
	sljit_ub *ret;
	struct sljit_memory_fragment *new_frag;

	if (compiler->abuf->used_size + size <= (int)(ABUF_SIZE - sizeof(int) - sizeof(void*))) {
		ret = compiler->abuf->memory + compiler->abuf->used_size;
		compiler->abuf->used_size += size;
		return ret;
	}
	new_frag = (struct sljit_memory_fragment*)SLJIT_MALLOC(ABUF_SIZE);
	PTR_FAIL_IF_NULL(new_frag);
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
	(((exp) & SLJIT_MEM) && (((exp) & 0xf) == reg || (((exp) >> 4) & 0xf) == reg))

#ifdef SLJIT_DEBUG
#define FUNCTION_CHECK_OP() \
	switch (GET_OPCODE(op)) { \
	case SLJIT_NOT: \
	case SLJIT_AND: \
	case SLJIT_OR: \
	case SLJIT_XOR: \
	case SLJIT_SHL: \
	case SLJIT_LSHR: \
	case SLJIT_ASHR: \
		SLJIT_ASSERT(!(op & (SLJIT_SET_S | SLJIT_SET_U | SLJIT_SET_O | SLJIT_SET_C))); \
		break; \
	case SLJIT_NEG: \
		SLJIT_ASSERT(!(op & (SLJIT_SET_S | SLJIT_SET_U | SLJIT_SET_C))); \
		break; \
	case SLJIT_FCMP: \
		SLJIT_ASSERT(!(op & (SLJIT_SET_S | SLJIT_SET_O | SLJIT_SET_C))); \
		SLJIT_ASSERT((op & (SLJIT_SET_E | SLJIT_SET_U))); \
		break; \
	case SLJIT_ADD: \
	case SLJIT_ADDC: \
		SLJIT_ASSERT(!(op & (SLJIT_SET_S | SLJIT_SET_U))); \
		break; \
	case SLJIT_SUB: \
	case SLJIT_SUBC: \
		break; \
	default: \
		/* Nothing */ \
		SLJIT_ASSERT(!(op & (SLJIT_SET_E | SLJIT_SET_S | SLJIT_SET_U | SLJIT_SET_O | SLJIT_SET_C))); \
		break; \
	}

#define FUNCTION_CHECK_IS_REG(r) \
	((r) == SLJIT_UNUSED || (r) == SLJIT_LOCALS_REG || \
	((r) >= SLJIT_TEMPORARY_REG1 && (r) <= SLJIT_TEMPORARY_REG3 && (r) <= SLJIT_TEMPORARY_REG1 - 1 + compiler->temporaries) || \
	((r) >= SLJIT_GENERAL_REG1 && (r) <= SLJIT_GENERAL_REG3 && (r) <= SLJIT_GENERAL_REG1 - 1 + compiler->generals)) \

#define FUNCTION_CHECK_SRC(p, i) \
	SLJIT_ASSERT(compiler->temporaries != -1 && compiler->generals != -1); \
	if (((p) >= SLJIT_TEMPORARY_REG1 && (p) <= SLJIT_TEMPORARY_REG1 - 1 + compiler->temporaries) || \
			((p) >= SLJIT_GENERAL_REG1 && (p) <= SLJIT_GENERAL_REG1 - 1 + compiler->generals) || \
			(p) == SLJIT_LOCALS_REG) \
		SLJIT_ASSERT(i == 0); \
	else if ((p) == SLJIT_IMM) \
		; \
	else if ((p) & SLJIT_MEM) { \
		SLJIT_ASSERT(FUNCTION_CHECK_IS_REG((p) & 0xf)); \
		if (((p) & 0xf) != 0) { \
			SLJIT_ASSERT(FUNCTION_CHECK_IS_REG(((p) >> 4) & 0xf)); \
			SLJIT_ASSERT((((p) >> 4) & 0xf) != SLJIT_LOCALS_REG || ((p) & 0xf) != SLJIT_LOCALS_REG); \
		} else \
			SLJIT_ASSERT((((p) >> 4) & 0xf) == 0); \
		SLJIT_ASSERT(((p) >> 9) == 0); \
	} \
	else \
		SLJIT_ASSERT_STOP();

#define FUNCTION_CHECK_DST(p, i) \
	SLJIT_ASSERT(compiler->temporaries != -1 && compiler->generals != -1); \
	if (((p) >= SLJIT_TEMPORARY_REG1 && (p) <= SLJIT_TEMPORARY_REG1 - 1 + compiler->temporaries) || \
			((p) >= SLJIT_GENERAL_REG1 && (p) <= SLJIT_GENERAL_REG1 - 1 + compiler->generals) || \
			(p) == SLJIT_UNUSED) \
		SLJIT_ASSERT(i == 0); \
	else if ((p) & SLJIT_MEM) { \
		SLJIT_ASSERT(FUNCTION_CHECK_IS_REG((p) & 0xf)); \
		if (((p) & 0xf) != 0) { \
			SLJIT_ASSERT(FUNCTION_CHECK_IS_REG(((p) >> 4) & 0xf)); \
			SLJIT_ASSERT((((p) >> 4) & 0xf) != SLJIT_LOCALS_REG || ((p) & 0xf) != SLJIT_LOCALS_REG); \
		} else \
			SLJIT_ASSERT((((p) >> 4) & 0xf) == 0); \
		SLJIT_ASSERT(((p) >> 9) == 0); \
	} \
	else \
		SLJIT_ASSERT_STOP();

#define FUNCTION_FCHECK(p, i) \
	if ((p) >= SLJIT_FLOAT_REG1 && (p) <= SLJIT_FLOAT_REG4) \
		SLJIT_ASSERT(i == 0); \
	else if ((p) & SLJIT_MEM) { \
		SLJIT_ASSERT(((p) & 0xf) <= SLJIT_LOCALS_REG); \
		if (((p) & 0xf) != 0) { \
			SLJIT_ASSERT((((p) >> 4) & 0xf) <= SLJIT_LOCALS_REG); \
			SLJIT_ASSERT((((p) >> 4) & 0xf) != SLJIT_LOCALS_REG || ((p) & 0xf) != SLJIT_LOCALS_REG); \
		} else \
			SLJIT_ASSERT((((p) >> 4) & 0xf) == 0); \
		SLJIT_ASSERT(((p) >> 9) == 0); \
	} \
	else \
		SLJIT_ASSERT_STOP();

#define FUNCTION_CHECK_OP1() \
	if (GET_OPCODE(op) >= SLJIT_MOV && GET_OPCODE(op) <= SLJIT_MOVU_SH) { \
		if (GET_OPCODE(op) != SLJIT_MOV && GET_OPCODE(op) != SLJIT_MOVU) \
			SLJIT_ASSERT(!(op & SLJIT_INT_OP)); \
	} \
        if (GET_OPCODE(op) >= SLJIT_MOVU && GET_OPCODE(op) <= SLJIT_MOVU_SH) { \
		if ((src & SLJIT_MEM) && (src & 0xf)) { \
			SLJIT_ASSERT((src & 0xf) != SLJIT_LOCALS_REG); \
			SLJIT_ASSERT((dst & 0xf) != (src & 0xf) && ((dst >> 4) & 0xf) != (src & 0xf)); \
		} \
	}

#define FUNCTION_CHECK_FOP() \
	SLJIT_ASSERT(!(op & SLJIT_INT_OP));

#endif

#ifdef SLJIT_VERBOSE

void sljit_compiler_verbose(struct sljit_compiler *compiler, FILE* verbose)
{
	compiler->verbose = verbose;
}

static char* reg_names[] = {
	(char*)"<noreg>", (char*)"tmp_r1", (char*)"tmp_r2", (char*)"tmp_r3",
	(char*)"tmp_er1", (char*)"tmp_er2", (char*)"gen_r1", (char*)"gen_r2",
	(char*)"gen_r3", (char*)"gen_er1", (char*)"gen_er2", (char*)"stack_r"
};

static char* freg_names[] = {
	(char*)"<noreg>", (char*)"fr1", (char*)"fr2", (char*)"fr3", (char*)"fr4"
};

#if defined(SLJIT_CONFIG_X86_64) || defined(SLJIT_CONFIG_PPC_64)
	#define SLJIT_PRINT_D	"l"
#else
	#define SLJIT_PRINT_D	""
#endif

#define sljit_emit_enter_verbose() \
	if (compiler->verbose) fprintf(compiler->verbose, "  enter args=%d temporaries=%d generals=%d local_size=%d\n", args, temporaries, generals, local_size);
#define sljit_fake_enter_verbose() \
	if (compiler->verbose) fprintf(compiler->verbose, "  fake enter args=%d temporaries=%d generals=%d local_size=%d\n", args, temporaries, generals, local_size);
#define sljit_verbose_param(p, i) \
	if ((p) & SLJIT_IMM) \
		fprintf(compiler->verbose, "#%"SLJIT_PRINT_D"d", (i)); \
	else if ((p) & SLJIT_MEM) { \
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
	if ((p) & SLJIT_MEM) { \
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
#define sljit_emit_return_verbose() \
	if (compiler->verbose) { \
		fprintf(compiler->verbose, "  return "); \
		sljit_verbose_param(src, srcw); \
		fprintf(compiler->verbose, "\n"); \
	}

static char* op0_names[] = {
	(char*)"debugger"
};
#define sljit_emit_op0_verbose() \
	if (compiler->verbose) \
		fprintf(compiler->verbose, "  %s\n", op0_names[GET_OPCODE(op)]); \

static char* op1_names[] = {
	(char*)"mov", (char*)"mov_ub", (char*)"mov_sb", (char*)"mov_uh",
	(char*)"mov_sh", (char*)"mov_ui", (char*)"mov_si", (char*)"movu",
	(char*)"movu_ub", (char*)"movu_sb", (char*)"movu_uh", (char*)"movu_sh",
	(char*)"movu_ui", (char*)"movu_si", (char*)"not", (char*)"neg"
};
#define sljit_emit_op1_verbose() \
	if (compiler->verbose) { \
		fprintf(compiler->verbose, "  %s%s%s%s%s%s%s ", !(op & SLJIT_INT_OP) ? "" : "i", op1_names[GET_OPCODE(op) - SLJIT_MOV], \
			!(op & SLJIT_SET_E) ? "" : "E", !(op & SLJIT_SET_S) ? "" : "S", !(op & SLJIT_SET_U) ? "" : "U", !(op & SLJIT_SET_O) ? "" : "O", !(op & SLJIT_SET_C) ? "" : "C"); \
		sljit_verbose_param(dst, dstw); \
		fprintf(compiler->verbose, ", "); \
		sljit_verbose_param(src, srcw); \
		fprintf(compiler->verbose, "\n"); \
	}

static char* op2_names[] = {
	(char*)"add", (char*)"addc", (char*)"sub", (char*)"subc",
	(char*)"mul", (char*)"and", (char*)"or", (char*)"xor",
	(char*)"shl", (char*)"lshr", (char*)"ashr"
};
#define sljit_emit_op2_verbose() \
	if (compiler->verbose) { \
		fprintf(compiler->verbose, "  %s%s%s%s%s%s%s ", !(op & SLJIT_INT_OP) ? "" : "i", op2_names[GET_OPCODE(op) - SLJIT_ADD], \
			!(op & SLJIT_SET_E) ? "" : "E", !(op & SLJIT_SET_S) ? "" : "S", !(op & SLJIT_SET_U) ? "" : "U", !(op & SLJIT_SET_O) ? "" : "O", !(op & SLJIT_SET_C) ? "" : "C"); \
		sljit_verbose_param(dst, dstw); \
		fprintf(compiler->verbose, ", "); \
		sljit_verbose_param(src1, src1w); \
		fprintf(compiler->verbose, ", "); \
		sljit_verbose_param(src2, src2w); \
		fprintf(compiler->verbose, "\n"); \
	}

static char* fop1_names[] = {
	(char*)"fcmp", (char*)"fmov", (char*)"fneg", (char*)"fabs"
};
#define sljit_emit_fop1_verbose() \
	if (compiler->verbose) { \
		fprintf(compiler->verbose, "  %s ", fop1_names[GET_OPCODE(op) - SLJIT_FCMP]); \
		sljit_verbose_fparam(dst, dstw); \
		fprintf(compiler->verbose, ", "); \
		sljit_verbose_fparam(src, srcw); \
		fprintf(compiler->verbose, "\n"); \
	}

static char* fop2_names[] = {
	(char*)"fadd", (char*)"fsub", (char*)"fmul", (char*)"fdiv"
};
#define sljit_emit_fop2_verbose() \
	if (compiler->verbose) { \
		fprintf(compiler->verbose, "  %s ", fop2_names[GET_OPCODE(op) - SLJIT_FADD]); \
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
	(char*)"c_equal", (char*)"c_not_equal",
	(char*)"c_less", (char*)"c_not_less",
	(char*)"c_greater", (char*)"c_not_greater",
	(char*)"c_sig_less", (char*)"c_sig_not_less",
	(char*)"c_sig_greater", (char*)"c_sig_not_greater",
	(char*)"c_carry", (char*)"c_not_carry",
	(char*)"c_zero", (char*)"c_not_zero",
	(char*)"c_overflow", (char*)"c_not_overflow",
	(char*)"jump",
	(char*)"call0", (char*)"call1", (char*)"call2", (char*)"call3"
};
#define sljit_emit_jump_verbose() \
	if (compiler->verbose) fprintf(compiler->verbose, "  jump <%s> %s\n", jump_names[type & 0xff], (type & SLJIT_REWRITABLE_JUMP) ? "(rewritable)" : "");
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
#define sljit_emit_op0_verbose()
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
#elif defined(SLJIT_CONFIG_ARM_V5) || defined(SLJIT_CONFIG_ARM_V7)
	#include "sljitNativeARM_v5.c"
#elif defined(SLJIT_CONFIG_ARM_THUMB2)
	#include "sljitNativeARM_Thumb2.c"
#elif defined(SLJIT_CONFIG_PPC_32)
	#include "sljitNativePPC_common.c"
#elif defined(SLJIT_CONFIG_PPC_64)
	#include "sljitNativePPC_common.c"
#endif

#else /* SLJIT_CONFIG_UNSUPPORTED */

// Empty function bodies for those machines, which are not (yet) supported

struct sljit_compiler* sljit_create_compiler(void)
{
	SLJIT_ASSERT_STOP();
	return NULL;
}

void sljit_free_compiler(struct sljit_compiler *compiler)
{
	(void)compiler;
	SLJIT_ASSERT_STOP();
}

#ifdef SLJIT_VERBOSE
void sljit_compiler_verbose(struct sljit_compiler *compiler, FILE* verbose)
{
	(void)compiler;
	(void)verbose;
	SLJIT_ASSERT_STOP();
}
#endif

void* sljit_generate_code(struct sljit_compiler *compiler)
{
	(void)compiler;
	SLJIT_ASSERT_STOP();
	return NULL;
}

void sljit_free_code(void* code)
{
	(void)code;
	SLJIT_ASSERT_STOP();
}

int sljit_emit_enter(struct sljit_compiler *compiler, int args, int temporaries, int generals, int local_size)
{
	(void)compiler;
	(void)args;
	(void)temporaries;
	(void)generals;
	(void)local_size;
	SLJIT_ASSERT_STOP();
	return SLJIT_ERR_UNSUPPORTED;
}

void sljit_fake_enter(struct sljit_compiler *compiler, int args, int temporaries, int generals, int local_size)
{
	(void)compiler;
	(void)args;
	(void)temporaries;
	(void)generals;
	(void)local_size;
	SLJIT_ASSERT_STOP();
}

int sljit_emit_return(struct sljit_compiler *compiler, int src, sljit_w srcw)
{
	(void)compiler;
	(void)src;
	(void)srcw;
	SLJIT_ASSERT_STOP();
	return SLJIT_ERR_UNSUPPORTED;
}

int sljit_emit_op1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
	(void)compiler;
	(void)op;
	(void)dst;
	(void)dstw;
	(void)src;
	(void)srcw;
	SLJIT_ASSERT_STOP();
	return SLJIT_ERR_UNSUPPORTED;
}

int sljit_emit_op2(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	(void)compiler;
	(void)op;
	(void)dst;
	(void)dstw;
	(void)src1;
	(void)src1w;
	(void)src2;
	(void)src2w;
	SLJIT_ASSERT_STOP();
	return SLJIT_ERR_UNSUPPORTED;
}

int sljit_is_fpu_available(void)
{
	SLJIT_ASSERT_STOP();
	return 0;
}

int sljit_emit_fop1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
	(void)compiler;
	(void)op;
	(void)dst;
	(void)dstw;
	(void)src;
	(void)srcw;
	SLJIT_ASSERT_STOP();
	return SLJIT_ERR_UNSUPPORTED;
}

int sljit_emit_fop2(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	(void)compiler;
	(void)op;
	(void)dst;
	(void)dstw;
	(void)src1;
	(void)src1w;
	(void)src2;
	(void)src2w;
	SLJIT_ASSERT_STOP();
	return SLJIT_ERR_UNSUPPORTED;
}

struct sljit_label* sljit_emit_label(struct sljit_compiler *compiler)
{
	(void)compiler;
	SLJIT_ASSERT_STOP();
	return NULL;
}

struct sljit_jump* sljit_emit_jump(struct sljit_compiler *compiler, int type)
{
	(void)compiler;
	(void)type;
	SLJIT_ASSERT_STOP();
	return NULL;
}

void sljit_set_label(struct sljit_jump *jump, struct sljit_label* label)
{
	(void)jump;
	(void)label;
	SLJIT_ASSERT_STOP();
}

void sljit_set_target(struct sljit_jump *jump, sljit_uw target)
{
	(void)jump;
	(void)target;
	SLJIT_ASSERT_STOP();
}

int sljit_emit_ijump(struct sljit_compiler *compiler, int type, int src, sljit_w srcw)
{
	(void)compiler;
	(void)type;
	(void)src;
	(void)srcw;
	SLJIT_ASSERT_STOP();
	return SLJIT_ERR_UNSUPPORTED;
}

int sljit_emit_cond_set(struct sljit_compiler *compiler, int dst, sljit_w dstw, int type)
{
	(void)compiler;
	(void)dst;
	(void)dstw;
	(void)type;
	SLJIT_ASSERT_STOP();
	return SLJIT_ERR_UNSUPPORTED;
}

struct sljit_const* sljit_emit_const(struct sljit_compiler *compiler, int dst, sljit_w dstw, sljit_w initval)
{
	(void)compiler;
	(void)dst;
	(void)dstw;
	(void)initval;
	SLJIT_ASSERT_STOP();
	return NULL;
}

void sljit_set_jump_addr(sljit_uw addr, sljit_uw new_addr)
{
	(void)addr;
	(void)new_addr;
	SLJIT_ASSERT_STOP();
}

void sljit_set_const(sljit_uw addr, sljit_w new_constant)
{
	(void)addr;
	(void)new_constant;
	SLJIT_ASSERT_STOP();
}

#endif

