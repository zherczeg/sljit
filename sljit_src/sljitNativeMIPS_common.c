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

// ---------------------------------------------------------------------
//  Instrucion forms
// ---------------------------------------------------------------------

void* sljit_generate_code(struct sljit_compiler *compiler)
{
	FUNCTION_ENTRY();

	SLJIT_ASSERT(compiler->size > 0);
	reverse_buf(compiler);

	SLJIT_ASSERT_STOP();
	return NULL;
}

#ifdef SLJIT_CONFIG_MIPS_32
#include "sljitNativeMIPS_32.c"
#else
#include "sljitNativeMIPS_64.c"
#endif

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

	SLJIT_ASSERT_STOP();
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

	SLJIT_ASSERT_STOP();
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

	SLJIT_ASSERT_STOP();
	return SLJIT_SUCCESS;
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
		break;
	}

	SLJIT_ASSERT_STOP();
	return SLJIT_SUCCESS;
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
	return SLJIT_SUCCESS;
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
	return SLJIT_SUCCESS;
}

// ---------------------------------------------------------------------
//  Floating point operators
// ---------------------------------------------------------------------

int sljit_is_fpu_available(void)
{
	SLJIT_ASSERT_STOP();
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
#endif
	sljit_emit_fop1_verbose();

	SLJIT_ASSERT_STOP();
	return SLJIT_SUCCESS;
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
#endif
	sljit_emit_fop2_verbose();

	SLJIT_ASSERT_STOP();
	return SLJIT_SUCCESS;
}

// ---------------------------------------------------------------------
//  Conditional instructions
// ---------------------------------------------------------------------

struct sljit_label* sljit_emit_label(struct sljit_compiler *compiler)
{
	//struct sljit_label *label;

	FUNCTION_ENTRY();

	sljit_emit_label_verbose();

	SLJIT_ASSERT_STOP();
	return NULL;
}

struct sljit_jump* sljit_emit_jump(struct sljit_compiler *compiler, int type)
{
	//struct sljit_jump *jump;

	FUNCTION_ENTRY();
	SLJIT_ASSERT((type & ~0x1ff) == 0);
	SLJIT_ASSERT((type & 0xff) >= SLJIT_C_EQUAL && (type & 0xff) <= SLJIT_CALL3);

	sljit_emit_jump_verbose();

	SLJIT_ASSERT_STOP();
	return NULL;
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
	return SLJIT_SUCCESS;
}

int sljit_emit_cond_set(struct sljit_compiler *compiler, int dst, sljit_w dstw, int type)
{
	int reg;

	FUNCTION_ENTRY();
	SLJIT_ASSERT(type >= SLJIT_C_EQUAL && type < SLJIT_JUMP);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_set_cond_verbose();

	if (dst == SLJIT_UNUSED)
		return SLJIT_SUCCESS;

	SLJIT_ASSERT_STOP();
	return SLJIT_SUCCESS;
}

struct sljit_const* sljit_emit_const(struct sljit_compiler *compiler, int dst, sljit_w dstw, sljit_w initval)
{
	//struct sljit_const *const_;

	FUNCTION_ENTRY();
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_const_verbose();

	SLJIT_ASSERT_STOP();
	return NULL;
}




