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

/* Must be the first one. Must not depend on any other include. */
#include "sljitLir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined _WIN32 || defined _WIN64
#define COLOR_RED
#define COLOR_GREEN
#define COLOR_ARCH
#define COLOR_DEFAULT
#else
#define COLOR_RED "\33[31m"
#define COLOR_GREEN "\33[32m"
#define COLOR_ARCH "\33[33m"
#define COLOR_DEFAULT "\33[0m"
#endif

union executable_code {
	void* code;
	sljit_sw (SLJIT_CALL *func0)(void);
	sljit_sw (SLJIT_CALL *func1)(sljit_sw a);
	sljit_sw (SLJIT_CALL *func2)(sljit_sw a, sljit_sw b);
	sljit_sw (SLJIT_CALL *func3)(sljit_sw a, sljit_sw b, sljit_sw c);
};
typedef union executable_code executable_code;

static sljit_si successful_tests = 0;
static sljit_si verbose = 0;
static sljit_si silent = 0;

#define FAILED(cond, text) \
	if (SLJIT_UNLIKELY(cond)) { \
		printf(text); \
		return; \
	}

#define CHECK(compiler) \
	if (sljit_get_compiler_error(compiler) != SLJIT_ERR_COMPILED) { \
		printf("Compiler error: %d\n", sljit_get_compiler_error(compiler)); \
		sljit_free_compiler(compiler); \
		return; \
	}

static void cond_set(struct sljit_compiler *compiler, sljit_si dst, sljit_sw dstw, sljit_si type)
{
	/* Testing both sljit_emit_op_flags and sljit_emit_jump. */
	struct sljit_jump* jump;
	struct sljit_label* label;

	sljit_emit_op_flags(compiler, SLJIT_MOV, dst, dstw, SLJIT_UNUSED, 0, type);
	jump = sljit_emit_jump(compiler, type);
	sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_KEEP_FLAGS, dst, dstw, dst, dstw, SLJIT_IMM, 2);
	label = sljit_emit_label(compiler);
	sljit_set_label(jump, label);
}

#if !(defined SLJIT_CONFIG_UNSUPPORTED && SLJIT_CONFIG_UNSUPPORTED)

#define MALLOC_EXEC(result, size) \
	result = SLJIT_MALLOC_EXEC(size); \
	if (!result) { \
		printf("Cannot allocate executable memory\n"); \
		return; \
	} \
	memset(result, 255, size);

static void test_exec_allocator(void)
{
	/* This is not an sljit test. */
	void *ptr1;
	void *ptr2;
	void *ptr3;

	if (verbose)
		printf("Run executable allocator test\n");

	MALLOC_EXEC(ptr1, 32);
	MALLOC_EXEC(ptr2, 512);
	MALLOC_EXEC(ptr3, 512);
	SLJIT_FREE_EXEC(ptr2);
	SLJIT_FREE_EXEC(ptr3);
	SLJIT_FREE_EXEC(ptr1);
	MALLOC_EXEC(ptr1, 262104);
	MALLOC_EXEC(ptr2, 32000);
	SLJIT_FREE_EXEC(ptr1);
	MALLOC_EXEC(ptr1, 262104);
	SLJIT_FREE_EXEC(ptr1);
	SLJIT_FREE_EXEC(ptr2);
	MALLOC_EXEC(ptr1, 512);
	MALLOC_EXEC(ptr2, 512);
	MALLOC_EXEC(ptr3, 512);
	SLJIT_FREE_EXEC(ptr2);
	MALLOC_EXEC(ptr2, 512);
#if (defined SLJIT_EXECUTABLE_ALLOCATOR && SLJIT_EXECUTABLE_ALLOCATOR)
	sljit_free_unused_memory_exec();
#endif
	SLJIT_FREE_EXEC(ptr3);
	SLJIT_FREE_EXEC(ptr1);
	SLJIT_FREE_EXEC(ptr2);
#if (defined SLJIT_UTIL_GLOBAL_LOCK && SLJIT_UTIL_GLOBAL_LOCK)
	/* Just call the global locks. */
	sljit_grab_lock();
	sljit_release_lock();
#endif

#if (defined SLJIT_EXECUTABLE_ALLOCATOR && SLJIT_EXECUTABLE_ALLOCATOR)
	sljit_free_unused_memory_exec();
#endif
}

#undef MALLOC_EXEC

#endif /* !(defined SLJIT_CONFIG_UNSUPPORTED && SLJIT_CONFIG_UNSUPPORTED) */

static void test1(void)
{
	/* Enter and return from an sljit function. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();

	if (verbose)
		printf("Run test1\n");

	FAILED(!compiler, "cannot create compiler\n");

	/* 3 arguments passed, 3 arguments used. */
	sljit_emit_enter(compiler, 0, 3, 3, 3, 0, 0, 0);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_S1, 0);

	SLJIT_ASSERT(sljit_get_generated_code_size(compiler) == 0);
	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	SLJIT_ASSERT(compiler->error == SLJIT_ERR_COMPILED);
	SLJIT_ASSERT(sljit_get_generated_code_size(compiler) > 0);
	sljit_free_compiler(compiler);

	FAILED(code.func3(3, -21, 86) != -21, "test1 case 1 failed\n");
	FAILED(code.func3(4789, 47890, 997) != 47890, "test1 case 2 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test2(void)
{
	/* Test mov. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_sw buf[8];
	static sljit_sw data[2] = { 0, -9876 };

	if (verbose)
		printf("Run test2\n");

	FAILED(!compiler, "cannot create compiler\n");

	buf[0] = 5678;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 0;
	buf[5] = 0;
	buf[6] = 0;
	buf[7] = 0;
	sljit_emit_enter(compiler, 0, 1, 3, 2, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_UNUSED, 0, SLJIT_MEM0(), (sljit_sw)&buf);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 9999);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_S0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_MEM2(SLJIT_S1, SLJIT_R1), 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, 2);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM2(SLJIT_S1, SLJIT_S0), SLJIT_WORD_SHIFT, SLJIT_MEM2(SLJIT_S1, SLJIT_R1), 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, 3);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM2(SLJIT_S1, SLJIT_S0), SLJIT_WORD_SHIFT, SLJIT_MEM0(), (sljit_sw)&buf);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R0), (sljit_sw)&data);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&buf - 0x12345678);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R0), 0x12345678);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 5 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 3456);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)&buf - 0xff890 + 6 * sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), 0xff890, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)&buf + 0xff890 + 7 * sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), -0xff890, SLJIT_R0, 0);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_R2, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_sw)&buf) != 9999, "test2 case 1 failed\n");
	FAILED(buf[1] != 9999, "test2 case 2 failed\n");
	FAILED(buf[2] != 9999, "test2 case 3 failed\n");
	FAILED(buf[3] != 5678, "test2 case 4 failed\n");
	FAILED(buf[4] != -9876, "test2 case 5 failed\n");
	FAILED(buf[5] != 5678, "test2 case 6 failed\n");
	FAILED(buf[6] != 3456, "test2 case 6 failed\n");
	FAILED(buf[7] != 3456, "test2 case 6 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test3(void)
{
	/* Test not. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_sw buf[5];

	if (verbose)
		printf("Run test3\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 1234;
	buf[1] = 0;
	buf[2] = 9876;
	buf[3] = 0;
	buf[4] = 0x12345678;

	sljit_emit_enter(compiler, 0, 1, 3, 1, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_NOT, SLJIT_UNUSED, 0, SLJIT_MEM0(), (sljit_sw)&buf);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op1(compiler, SLJIT_NOT, SLJIT_MEM0(), (sljit_sw)&buf[1], SLJIT_MEM0(), (sljit_sw)&buf[1]);
	sljit_emit_op1(compiler, SLJIT_NOT, SLJIT_RETURN_REG, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op1(compiler, SLJIT_NOT, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 2);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)&buf[4] - 0xff0000 - 0x20);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, (sljit_sw)&buf[4] - 0xff0000);
	sljit_emit_op1(compiler, SLJIT_NOT, SLJIT_MEM1(SLJIT_R1), 0xff0000 + 0x20, SLJIT_MEM1(SLJIT_R2), 0xff0000);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_sw)&buf) != ~1234, "test3 case 1 failed\n");
	FAILED(buf[1] != ~1234, "test3 case 2 failed\n");
	FAILED(buf[3] != ~9876, "test3 case 3 failed\n");
	FAILED(buf[4] != ~0x12345678, "test3 case 4 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test4(void)
{
	/* Test neg. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_sw buf[4];

	if (verbose)
		printf("Run test4\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	buf[1] = 1234;
	buf[2] = 0;
	buf[3] = 0;

	sljit_emit_enter(compiler, 0, 2, 3, 2, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_NEG, SLJIT_UNUSED, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op1(compiler, SLJIT_NEG, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 2, SLJIT_S1, 0);
	sljit_emit_op1(compiler, SLJIT_NEG, SLJIT_MEM0(), (sljit_sw)&buf[0], SLJIT_MEM0(), (sljit_sw)&buf[1]);
	sljit_emit_op1(compiler, SLJIT_NEG, SLJIT_RETURN_REG, 0, SLJIT_S1, 0);
	sljit_emit_op1(compiler, SLJIT_NEG, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3, SLJIT_IMM, 299);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func2((sljit_sw)&buf, 4567) != -4567, "test4 case 1 failed\n");
	FAILED(buf[0] != -1234, "test4 case 2 failed\n");
	FAILED(buf[2] != -4567, "test4 case 3 failed\n");
	FAILED(buf[3] != -299, "test4 case 4 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test5(void)
{
	/* Test add. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_sw buf[9];

	if (verbose)
		printf("Run test5\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 100;
	buf[1] = 200;
	buf[2] = 300;
	buf[3] = 0;
	buf[4] = 0;
	buf[5] = 0;
	buf[6] = 0;
	buf[7] = 0;
	buf[8] = 313;

	sljit_emit_enter(compiler, 0, 1, 3, 2, 0, 0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_UNUSED, 0, SLJIT_IMM, 16, SLJIT_IMM, 16);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_UNUSED, 0, SLJIT_IMM, 255, SLJIT_IMM, 255);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_UNUSED, 0, SLJIT_S0, 0, SLJIT_S0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 50);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), 1, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), 1, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, sizeof(sljit_sw) + 2);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 50);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_IMM, 4, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_IMM, 50, SLJIT_R1, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_IMM, 50, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw));
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_R1, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_R1, 0, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw));
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw));
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0x1e7d39f2);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_R1, 0, SLJIT_IMM, 0x23de7c06);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_IMM, 0x3d72e452, SLJIT_R1, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw), SLJIT_IMM, -43, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw));
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_IMM, 1000, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 1430);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_IMM, -99, SLJIT_R0, 0);

	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_R0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_sw)&buf) != 2437 + 2 * sizeof(sljit_sw), "test5 case 1 failed\n");
	FAILED(buf[0] != 202 + 2 * sizeof(sljit_sw), "test5 case 2 failed\n");
	FAILED(buf[2] != 500, "test5 case 3 failed\n");
	FAILED(buf[3] != 400, "test5 case 4 failed\n");
	FAILED(buf[4] != 200, "test5 case 5 failed\n");
	FAILED(buf[5] != 250, "test5 case 6 failed\n");
	FAILED(buf[6] != 0x425bb5f8, "test5 case 7 failed\n");
	FAILED(buf[7] != 0x5bf01e44, "test5 case 8 failed\n");
	FAILED(buf[8] != 270, "test5 case 9 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test6(void)
{
	/* Test addc, sub, subc. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_sw buf[10];

	if (verbose)
		printf("Run test6\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 0;
	buf[5] = 0;
	buf[6] = 0;
	buf[7] = 0;
	buf[8] = 0;
	buf[9] = 0;

	sljit_emit_enter(compiler, 0, 1, 3, 1, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -1);
	sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_SET_C, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, -1);
	sljit_emit_op2(compiler, SLJIT_ADDC, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_IMM, 0, SLJIT_IMM, 0);
	sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_SET_C, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_ADDC, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_IMM, 4);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 100);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 2, SLJIT_R0, 0, SLJIT_IMM, 50);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_C, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 6000);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3, SLJIT_IMM, 10);
	sljit_emit_op2(compiler, SLJIT_SUBC, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3, SLJIT_IMM, 5);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 100);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 2);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 4, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 5000);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 4, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 5, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 5000);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R0, 0, SLJIT_IMM, 6000, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 6, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 100);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, 32768);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 7, SLJIT_R1, 0);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, -32767);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 8, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x52cd3bf4);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 9, SLJIT_R0, 0, SLJIT_IMM, 0x3da297c6);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0, SLJIT_IMM, 10);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_C, SLJIT_RETURN_REG, 0, SLJIT_RETURN_REG, 0, SLJIT_IMM, 5);
	sljit_emit_op2(compiler, SLJIT_SUBC, SLJIT_RETURN_REG, 0, SLJIT_RETURN_REG, 0, SLJIT_IMM, 2);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_RETURN_REG, 0, SLJIT_RETURN_REG, 0, SLJIT_IMM, -2220);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_sw)&buf) != 2223, "test6 case 1 failed\n");
	FAILED(buf[0] != 1, "test6 case 2 failed\n");
	FAILED(buf[1] != 5, "test6 case 3 failed\n");
	FAILED(buf[2] != 50, "test6 case 4 failed\n");
	FAILED(buf[3] != 4, "test6 case 5 failed\n");
	FAILED(buf[4] != 50, "test6 case 6 failed\n");
	FAILED(buf[5] != 50, "test6 case 7 failed\n");
	FAILED(buf[6] != 1000, "test6 case 8 failed\n");
	FAILED(buf[7] != 100 - 32768, "test6 case 9 failed\n");
	FAILED(buf[8] != 100 + 32767, "test6 case 10 failed\n");
	FAILED(buf[9] != 0x152aa42e, "test6 case 11 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test7(void)
{
	/* Test logical operators. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_sw buf[8];

	if (verbose)
		printf("Run test7\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0xff80;
	buf[1] = 0x0f808080;
	buf[2] = 0;
	buf[3] = 0xaaaaaa;
	buf[4] = 0;
	buf[5] = 0x4040;
	buf[6] = 0;
	buf[7] = 0xc43a7f95;

	sljit_emit_enter(compiler, 0, 1, 3, 1, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0xf0C000);
	sljit_emit_op2(compiler, SLJIT_OR, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 2, SLJIT_R0, 0, SLJIT_IMM, 0x308f);
	sljit_emit_op2(compiler, SLJIT_XOR, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw));
	sljit_emit_op2(compiler, SLJIT_AND, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3, SLJIT_IMM, 0xf0f0f0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0xC0F0);
	sljit_emit_op2(compiler, SLJIT_XOR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 5);
	sljit_emit_op2(compiler, SLJIT_OR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 0xff0000);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 4, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 0xC0F0);
	sljit_emit_op2(compiler, SLJIT_AND, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 5);
	sljit_emit_op2(compiler, SLJIT_OR, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 5, SLJIT_R2, 0, SLJIT_IMM, 0xff0000);
	sljit_emit_op2(compiler, SLJIT_XOR, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_IMM, 0xFFFFFF, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw));
	sljit_emit_op2(compiler, SLJIT_OR, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 6, SLJIT_IMM, 0xa56c82c0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 6);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 7);
	sljit_emit_op2(compiler, SLJIT_XOR, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 7, SLJIT_IMM, 0xff00ff00, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0xff00ff00);
	sljit_emit_op2(compiler, SLJIT_OR, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, 0x0f);
	sljit_emit_op2(compiler, SLJIT_AND, SLJIT_RETURN_REG, 0, SLJIT_IMM, 0x888888, SLJIT_R1, 0);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_sw)&buf) != 0x8808, "test7 case 1 failed\n");
	FAILED(buf[0] != 0x0F807F00, "test7 case 2 failed\n");
	FAILED(buf[1] != 0x0F7F7F7F, "test7 case 3 failed\n");
	FAILED(buf[2] != 0x00F0F08F, "test7 case 4 failed\n");
	FAILED(buf[3] != 0x00A0A0A0, "test7 case 5 failed\n");
	FAILED(buf[4] != 0x00FF80B0, "test7 case 6 failed\n");
	FAILED(buf[5] != 0x00FF4040, "test7 case 7 failed\n");
	FAILED(buf[6] != 0xa56c82c0, "test7 case 8 failed\n");
	FAILED(buf[7] != 0x3b3a8095, "test7 case 9 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test8(void)
{
	/* Test flags (neg, cmp, test). */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_sw buf[13];

	if (verbose)
		printf("Run test8\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 100;
	buf[1] = 3;
	buf[2] = 3;
	buf[3] = 3;
	buf[4] = 3;
	buf[5] = 3;
	buf[6] = 3;
	buf[7] = 3;
	buf[8] = 3;
	buf[9] = 3;
	buf[10] = 3;
	buf[11] = 3;
	buf[12] = 3;

	sljit_emit_enter(compiler, 0, 1, 3, 2, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 20);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 10);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_IMM, 6, SLJIT_IMM, 5);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_UNUSED, 0, SLJIT_NOT_EQUAL);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 2, SLJIT_UNUSED, 0, SLJIT_EQUAL);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_U, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_IMM, 3000);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_UNUSED, 0, SLJIT_GREATER);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3, SLJIT_S1, 0);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_UNUSED, 0, SLJIT_LESS);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 4, SLJIT_R2, 0);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_S, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_IMM, -15);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_UNUSED, 0, SLJIT_SIG_GREATER);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 5, SLJIT_R2, 0);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_E | SLJIT_SET_O, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_E | SLJIT_SET_O, SLJIT_UNUSED, 0, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_NEG | SLJIT_SET_E | SLJIT_SET_O, SLJIT_UNUSED, 0, SLJIT_IMM, (sljit_sw)1 << ((sizeof(sljit_sw) << 3) - 1));
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 6, SLJIT_UNUSED, 0, SLJIT_OVERFLOW);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_NOT | SLJIT_SET_E, SLJIT_R1, 0, SLJIT_R0, 0);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 7, SLJIT_UNUSED, 0, SLJIT_ZERO);
	sljit_emit_op1(compiler, SLJIT_NOT | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_R1, 0);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 8, SLJIT_UNUSED, 0, SLJIT_ZERO);
	sljit_emit_op2(compiler, SLJIT_AND | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_IMM, 0xffff, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_AND | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_IMM, 0xffff);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 9, SLJIT_UNUSED, 0, SLJIT_NOT_ZERO);
	sljit_emit_op2(compiler, SLJIT_AND | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_IMM, 0xffff, SLJIT_R1, 0);
	sljit_emit_op2(compiler, SLJIT_AND | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_R1, 0, SLJIT_IMM, 0xffff);
	sljit_emit_op2(compiler, SLJIT_AND | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R1, 0);
	sljit_emit_op2(compiler, SLJIT_AND | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op2(compiler, SLJIT_AND | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op2(compiler, SLJIT_AND | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_IMM, 0x1);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 10, SLJIT_UNUSED, 0, SLJIT_NOT_ZERO);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)1 << ((sizeof(sljit_sw) << 3) - 1));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_O, SLJIT_UNUSED, 0, SLJIT_R1, 0, SLJIT_R0, 0);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 11, SLJIT_UNUSED, 0, SLJIT_OVERFLOW);
	sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_SET_O, SLJIT_UNUSED, 0, SLJIT_R1, 0, SLJIT_R0, 0);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 12, SLJIT_UNUSED, 0, SLJIT_OVERFLOW);
	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);
	FAILED(buf[1] != 1, "test8 case 1 failed\n");
	FAILED(buf[2] != 0, "test8 case 2 failed\n");
	FAILED(buf[3] != 0, "test8 case 3 failed\n");
	FAILED(buf[4] != 1, "test8 case 4 failed\n");
	FAILED(buf[5] != 1, "test8 case 5 failed\n");
	FAILED(buf[6] != 1, "test8 case 6 failed\n");
	FAILED(buf[7] != 1, "test8 case 7 failed\n");
	FAILED(buf[8] != 0, "test8 case 8 failed\n");
	FAILED(buf[9] != 1, "test8 case 9 failed\n");
	FAILED(buf[10] != 0, "test8 case 10 failed\n");
	FAILED(buf[11] != 1, "test8 case 11 failed\n");
	FAILED(buf[12] != 0, "test8 case 12 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test9(void)
{
	/* Test shift. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_sw buf[13];

	if (verbose)
		printf("Run test9\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 1 << 10;
	buf[5] = 0;
	buf[6] = 0;
	buf[7] = 0;
	buf[8] = 0;
	buf[9] = 3;
	buf[10] = 0;
	buf[11] = 0;
	buf[12] = 0;

	sljit_emit_enter(compiler, 0, 1, 3, 2, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0xf);
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 3);
	sljit_emit_op2(compiler, SLJIT_LSHR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_ASHR, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_IMM, 2);
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R1, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -64);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_PREF_SHIFT_REG, 0, SLJIT_IMM, 2);
	sljit_emit_op2(compiler, SLJIT_ASHR, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 2, SLJIT_R0, 0, SLJIT_PREF_SHIFT_REG, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_PREF_SHIFT_REG, 0, SLJIT_IMM, 0xff);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 4);
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_PREF_SHIFT_REG, 0, SLJIT_PREF_SHIFT_REG, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3, SLJIT_PREF_SHIFT_REG, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_PREF_SHIFT_REG, 0, SLJIT_IMM, 0xff);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 8);
	sljit_emit_op2(compiler, SLJIT_LSHR, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 4, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 4, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 5, SLJIT_PREF_SHIFT_REG, 0, SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, 0xf);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 2);
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_S1, 0, SLJIT_S1, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 6, SLJIT_S1, 0);
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_R0, 0, SLJIT_S1, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 7, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 0xf00);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 4);
	sljit_emit_op2(compiler, SLJIT_LSHR, SLJIT_R1, 0, SLJIT_R2, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 8, SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)buf);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 9);
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_MEM2(SLJIT_R0, SLJIT_R1), SLJIT_WORD_SHIFT, SLJIT_MEM2(SLJIT_R0, SLJIT_R1), SLJIT_WORD_SHIFT, SLJIT_MEM2(SLJIT_R0, SLJIT_R1), SLJIT_WORD_SHIFT);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_PREF_SHIFT_REG, 0, SLJIT_IMM, 4);
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_PREF_SHIFT_REG, 0, SLJIT_IMM, 2, SLJIT_PREF_SHIFT_REG, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 10, SLJIT_PREF_SHIFT_REG, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0xa9);
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R0, 0, SLJIT_IMM, 0x7d00);
	sljit_emit_op2(compiler, SLJIT_ILSHR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 32);
#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	sljit_emit_op1(compiler, SLJIT_MOV_UI, SLJIT_R0, 0, SLJIT_R0, 0);
#endif
	sljit_emit_op2(compiler, SLJIT_OR, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0xe30000);
#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	sljit_emit_op2(compiler, SLJIT_ASHR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 0xffc0);
#else
	sljit_emit_op2(compiler, SLJIT_ASHR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 0xffe0);
#endif
	sljit_emit_op2(compiler, SLJIT_OR, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_SI | SLJIT_INT_OP, SLJIT_R0, 0, SLJIT_IMM, 0x25000000);
	sljit_emit_op2(compiler, SLJIT_ISHL, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 0xfffe1);
#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	sljit_emit_op1(compiler, SLJIT_MOV_UI, SLJIT_R0, 0, SLJIT_R0, 0);
#endif
	sljit_emit_op2(compiler, SLJIT_OR, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 11, SLJIT_R1, 0, SLJIT_R0, 0);

	SLJIT_ASSERT(SLJIT_R2 == SLJIT_PREF_SHIFT_REG);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_PREF_SHIFT_REG, 0, SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x5c);
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_PREF_SHIFT_REG, 0);
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R0, 0, SLJIT_IMM, 0xf600);
	sljit_emit_op2(compiler, SLJIT_ILSHR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_PREF_SHIFT_REG, 0);
#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	/* Alternative form of uint32 type cast. */
	sljit_emit_op2(compiler, SLJIT_AND, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 0xffffffff);
#endif
	sljit_emit_op2(compiler, SLJIT_OR, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x630000);
	sljit_emit_op2(compiler, SLJIT_ASHR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_PREF_SHIFT_REG, 0);
	sljit_emit_op2(compiler, SLJIT_OR, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 12, SLJIT_R1, 0, SLJIT_R0, 0);

	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);
	FAILED(buf[0] != 0x3c, "test9 case 1 failed\n");
	FAILED(buf[1] != 0xf0, "test9 case 2 failed\n");
	FAILED(buf[2] != -16, "test9 case 3 failed\n");
	FAILED(buf[3] != 0xff0, "test9 case 4 failed\n");
	FAILED(buf[4] != 4, "test9 case 5 failed\n");
	FAILED(buf[5] != 0xff00, "test9 case 6 failed\n");
	FAILED(buf[6] != 0x3c, "test9 case 7 failed\n");
	FAILED(buf[7] != 0xf0, "test9 case 8 failed\n");
	FAILED(buf[8] != 0xf0, "test9 case 9 failed\n");
	FAILED(buf[9] != 0x18, "test9 case 10 failed\n");
	FAILED(buf[10] != 32, "test9 case 11 failed\n");
	FAILED(buf[11] != 0x4ae37da9, "test9 case 12 failed\n");
	FAILED(buf[12] != 0x63f65c, "test9 case 13 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test10(void)
{
	/* Test multiplications. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_sw buf[7];

	if (verbose)
		printf("Run test10\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 3;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 6;
	buf[4] = -10;
	buf[5] = 0;
	buf[6] = 0;

	sljit_emit_enter(compiler, 0, 1, 3, 1, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 5);
	sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 7);
	sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_R0, 0, SLJIT_R2, 0, SLJIT_IMM, 8);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_R0, 0, SLJIT_IMM, -3, SLJIT_IMM, -4);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 2, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -2);
	sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, sizeof(sljit_sw) / 2);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)&buf[3]);
	sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_MEM2(SLJIT_R1, SLJIT_R0), 1, SLJIT_MEM2(SLJIT_R1, SLJIT_R0), 1, SLJIT_MEM2(SLJIT_R1, SLJIT_R0), 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 9);
	sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 5, SLJIT_R0, 0);
#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 3);
	sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_R0, 0, SLJIT_R1, 0, SLJIT_IMM, SLJIT_W(0x123456789));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 6, SLJIT_R0, 0);
#endif
	sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_RETURN_REG, 0, SLJIT_IMM, 11, SLJIT_IMM, 10);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_sw)&buf) != 110, "test10 case 1 failed\n");
	FAILED(buf[0] != 15, "test10 case 2 failed\n");
	FAILED(buf[1] != 56, "test10 case 3 failed\n");
	FAILED(buf[2] != 12, "test10 case 4 failed\n");
	FAILED(buf[3] != -12, "test10 case 5 failed\n");
	FAILED(buf[4] != 100, "test10 case 6 failed\n");
	FAILED(buf[5] != 81, "test10 case 7 failed\n");
#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	FAILED(buf[6] != SLJIT_W(0x123456789) * 3, "test10 case 8 failed\n");
#endif

	sljit_free_code(code.code);
	successful_tests++;
}

static void test11(void)
{
	/* Test rewritable constants. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	struct sljit_const* const1;
	struct sljit_const* const2;
	struct sljit_const* const3;
	struct sljit_const* const4;
	void* value;
	sljit_uw const1_addr;
	sljit_uw const2_addr;
	sljit_uw const3_addr;
	sljit_uw const4_addr;
#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	sljit_sw word_value1 = SLJIT_W(0xaaaaaaaaaaaaaaaa);
	sljit_sw word_value2 = SLJIT_W(0xfee1deadfbadf00d);
#else
	sljit_sw word_value1 = 0xaaaaaaaal;
	sljit_sw word_value2 = 0xfbadf00dl;
#endif
	sljit_sw buf[3];

	if (verbose)
		printf("Run test11\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;

	sljit_emit_enter(compiler, 0, 1, 3, 1, 0, 0, 0);

	const1 = sljit_emit_const(compiler, SLJIT_MEM0(), (sljit_sw)&buf[0], -0x81b9);
	SLJIT_ASSERT(!sljit_alloc_memory(compiler, 0));
	SLJIT_ASSERT(!sljit_alloc_memory(compiler, 16 * sizeof(sljit_sw) + 1));
	value = sljit_alloc_memory(compiler, 16 * sizeof(sljit_sw));
	SLJIT_ASSERT(!((sljit_sw)value & (sizeof(sljit_sw) - 1)));
	memset(value, 255, 16 * sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 2);
	const2 = sljit_emit_const(compiler, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), SLJIT_WORD_SHIFT - 1, -65535);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&buf[0] + 2 * sizeof(sljit_sw) - 2);
	const3 = sljit_emit_const(compiler, SLJIT_MEM1(SLJIT_R0), 0, word_value1);
	value = sljit_alloc_memory(compiler, 17);
	SLJIT_ASSERT(!((sljit_sw)value & (sizeof(sljit_sw) - 1)));
	memset(value, 255, 16);
	const4 = sljit_emit_const(compiler, SLJIT_RETURN_REG, 0, 0xf7afcdb7);

	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	const1_addr = sljit_get_const_addr(const1);
	const2_addr = sljit_get_const_addr(const2);
	const3_addr = sljit_get_const_addr(const3);
	const4_addr = sljit_get_const_addr(const4);
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_sw)&buf) != 0xf7afcdb7, "test11 case 1 failed\n");
	FAILED(buf[0] != -0x81b9, "test11 case 2 failed\n");
	FAILED(buf[1] != -65535, "test11 case 3 failed\n");
	FAILED(buf[2] != word_value1, "test11 case 4 failed\n");

	sljit_set_const(const1_addr, -1);
	sljit_set_const(const2_addr, word_value2);
	sljit_set_const(const3_addr, 0xbab0fea1);
	sljit_set_const(const4_addr, -60089);

	FAILED(code.func1((sljit_sw)&buf) != -60089, "test11 case 5 failed\n");
	FAILED(buf[0] != -1, "test11 case 6 failed\n");
	FAILED(buf[1] != word_value2, "test11 case 7 failed\n");
	FAILED(buf[2] != 0xbab0fea1, "test11 case 8 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test12(void)
{
	/* Test rewriteable jumps. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	struct sljit_label *label1;
	struct sljit_label *label2;
	struct sljit_label *label3;
	struct sljit_jump *jump1;
	struct sljit_jump *jump2;
	struct sljit_jump *jump3;
	void* value;
	sljit_uw jump1_addr;
	sljit_uw label1_addr;
	sljit_uw label2_addr;
	sljit_sw buf[1];

	if (verbose)
		printf("Run test12\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;

	sljit_emit_enter(compiler, 0, 2, 3, 2, 0, 0, 0);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_S, SLJIT_UNUSED, 0, SLJIT_S1, 0, SLJIT_IMM, 10);
	jump1 = sljit_emit_jump(compiler, SLJIT_REWRITABLE_JUMP | SLJIT_SIG_GREATER);
	/* Default handler. */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_IMM, 5);
	jump2 = sljit_emit_jump(compiler, SLJIT_JUMP);
	value = sljit_alloc_memory(compiler, 15);
	SLJIT_ASSERT(!((sljit_sw)value & (sizeof(sljit_sw) - 1)));
	memset(value, 255, 15);
	/* Handler 1. */
	label1 = sljit_emit_label(compiler);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_IMM, 6);
	jump3 = sljit_emit_jump(compiler, SLJIT_JUMP);
	/* Handler 2. */
	label2 = sljit_emit_label(compiler);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_IMM, 7);
	/* Exit. */
	label3 = sljit_emit_label(compiler);
	sljit_set_label(jump2, label3);
	sljit_set_label(jump3, label3);
	/* By default, set to handler 1. */
	sljit_set_label(jump1, label1);
	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	value = sljit_alloc_memory(compiler, 8);
	SLJIT_ASSERT(!((sljit_sw)value & (sizeof(sljit_sw) - 1)));
	memset(value, 255, 8);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	jump1_addr = sljit_get_jump_addr(jump1);
	label1_addr = sljit_get_label_addr(label1);
	label2_addr = sljit_get_label_addr(label2);
	sljit_free_compiler(compiler);

	code.func2((sljit_sw)&buf, 4);
	FAILED(buf[0] != 5, "test12 case 1 failed\n");

	code.func2((sljit_sw)&buf, 11);
	FAILED(buf[0] != 6, "test12 case 2 failed\n");

	sljit_set_jump_addr(jump1_addr, label2_addr);
	code.func2((sljit_sw)&buf, 12);
	FAILED(buf[0] != 7, "test12 case 3 failed\n");

	sljit_set_jump_addr(jump1_addr, label1_addr);
	code.func2((sljit_sw)&buf, 13);
	FAILED(buf[0] != 6, "test12 case 4 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test13(void)
{
	/* Test fpu monadic functions. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_d buf[7];
	sljit_sw buf2[6];

	if (verbose)
		printf("Run test13\n");

	if (!sljit_is_fpu_available()) {
		if (verbose)
			printf("no fpu available, test13 skipped\n");
		successful_tests++;
		if (compiler)
			sljit_free_compiler(compiler);
		return;
	}

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 7.75;
	buf[1] = -4.5;
	buf[2] = 0.0;
	buf[3] = 0.0;
	buf[4] = 0.0;
	buf[5] = 0.0;
	buf[6] = 0.0;

	buf2[0] = 10;
	buf2[1] = 10;
	buf2[2] = 10;
	buf2[3] = 10;
	buf2[4] = 10;
	buf2[5] = 10;

	sljit_emit_enter(compiler, 0, 2, 3, 2, 6, 0, 0);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_MEM0(), (sljit_sw)&buf[2], SLJIT_MEM0(), (sljit_sw)&buf[1]);
	sljit_emit_fop1(compiler, SLJIT_DABS, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_d), SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d));
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR0, 0, SLJIT_MEM0(), (sljit_sw)&buf[0]);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 2 * sizeof(sljit_d));
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR1, 0, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), 0);
	sljit_emit_fop1(compiler, SLJIT_DNEG, SLJIT_FR2, 0, SLJIT_FR0, 0);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR3, 0, SLJIT_FR2, 0);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_MEM0(), (sljit_sw)&buf[4], SLJIT_FR3, 0);
	sljit_emit_fop1(compiler, SLJIT_DABS, SLJIT_FR4, 0, SLJIT_FR1, 0);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_d), SLJIT_FR4, 0);
	sljit_emit_fop1(compiler, SLJIT_DNEG, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_d), SLJIT_FR4, 0);

	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR5, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_DCMP | SLJIT_SET_S, SLJIT_FR5, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d));
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_UNUSED, 0, SLJIT_D_GREATER);
	sljit_emit_fop1(compiler, SLJIT_DCMP | SLJIT_SET_S, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d), SLJIT_FR5, 0);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_sw), SLJIT_UNUSED, 0, SLJIT_D_GREATER);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR1, 0, SLJIT_FR5, 0);
	sljit_emit_fop1(compiler, SLJIT_DCMP | SLJIT_SET_E | SLJIT_SET_S, SLJIT_FR1, 0, SLJIT_FR1, 0);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_sw), SLJIT_UNUSED, 0, SLJIT_D_EQUAL);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_sw), SLJIT_UNUSED, 0, SLJIT_D_LESS);
	sljit_emit_fop1(compiler, SLJIT_DCMP | SLJIT_SET_E, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d));
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_sw), SLJIT_UNUSED, 0, SLJIT_D_EQUAL);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 5 * sizeof(sljit_sw), SLJIT_UNUSED, 0, SLJIT_D_NOT_EQUAL);

	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func2((sljit_sw)&buf, (sljit_sw)&buf2);
	FAILED(buf[2] != -4.5, "test13 case 1 failed\n");
	FAILED(buf[3] != 4.5, "test13 case 2 failed\n");
	FAILED(buf[4] != -7.75, "test13 case 3 failed\n");
	FAILED(buf[5] != 4.5, "test13 case 4 failed\n");
	FAILED(buf[6] != -4.5, "test13 case 5 failed\n");

	FAILED(buf2[0] != 1, "test13 case 6 failed\n");
	FAILED(buf2[1] != 0, "test13 case 7 failed\n");
	FAILED(buf2[2] != 1, "test13 case 8 failed\n");
	FAILED(buf2[3] != 0, "test13 case 9 failed\n");
	FAILED(buf2[4] != 0, "test13 case 10 failed\n");
	FAILED(buf2[5] != 1, "test13 case 11 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test14(void)
{
	/* Test fpu diadic functions. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_d buf[15];

	if (verbose)
		printf("Run test14\n");

	if (!sljit_is_fpu_available()) {
		if (verbose)
			printf("no fpu available, test14 skipped\n");
		successful_tests++;
		if (compiler)
			sljit_free_compiler(compiler);
		return;
	}
	buf[0] = 7.25;
	buf[1] = 3.5;
	buf[2] = 1.75;
	buf[3] = 0.0;
	buf[4] = 0.0;
	buf[5] = 0.0;
	buf[6] = 0.0;
	buf[7] = 0.0;
	buf[8] = 0.0;
	buf[9] = 0.0;
	buf[10] = 0.0;
	buf[11] = 0.0;
	buf[12] = 8.0;
	buf[13] = 4.0;
	buf[14] = 0.0;

	FAILED(!compiler, "cannot create compiler\n");
	sljit_emit_enter(compiler, 0, 1, 3, 1, 6, 0, 0);

	/* ADD */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, sizeof(sljit_d));
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d));
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 2);
	sljit_emit_fop2(compiler, SLJIT_DADD, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 3, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop2(compiler, SLJIT_DADD, SLJIT_FR0, 0, SLJIT_FR0, 0, SLJIT_FR1, 0);
	sljit_emit_fop2(compiler, SLJIT_DADD, SLJIT_FR1, 0, SLJIT_FR0, 0, SLJIT_FR1, 0);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 4, SLJIT_FR0, 0);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 5, SLJIT_FR1, 0);

	/* SUB */
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR3, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 2);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 2);
	sljit_emit_fop2(compiler, SLJIT_DSUB, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 6, SLJIT_FR3, 0, SLJIT_MEM2(SLJIT_S0, SLJIT_R1), SLJIT_DOUBLE_SHIFT);
	sljit_emit_fop2(compiler, SLJIT_DSUB, SLJIT_FR2, 0, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 2);
	sljit_emit_fop2(compiler, SLJIT_DSUB, SLJIT_FR3, 0, SLJIT_FR2, 0, SLJIT_FR3, 0);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 7, SLJIT_FR2, 0);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 8, SLJIT_FR3, 0);

	/* MUL */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 1);
	sljit_emit_fop2(compiler, SLJIT_DMUL, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 9, SLJIT_MEM2(SLJIT_S0, SLJIT_R1), SLJIT_DOUBLE_SHIFT, SLJIT_FR1, 0);
	sljit_emit_fop2(compiler, SLJIT_DMUL, SLJIT_FR1, 0, SLJIT_FR1, 0, SLJIT_FR2, 0);
	sljit_emit_fop2(compiler, SLJIT_DMUL, SLJIT_FR5, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 2, SLJIT_FR2, 0);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 10, SLJIT_FR1, 0);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 11, SLJIT_FR5, 0);

	/* DIV */
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR5, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 12);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 13);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR4, 0, SLJIT_FR5, 0);
	sljit_emit_fop2(compiler, SLJIT_DDIV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 12, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 12, SLJIT_FR1, 0);
	sljit_emit_fop2(compiler, SLJIT_DDIV, SLJIT_FR5, 0, SLJIT_FR5, 0, SLJIT_FR1, 0);
	sljit_emit_fop2(compiler, SLJIT_DDIV, SLJIT_FR4, 0, SLJIT_FR1, 0, SLJIT_FR4, 0);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 13, SLJIT_FR5, 0);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 14, SLJIT_FR4, 0);

	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);
	FAILED(buf[3] != 10.75, "test14 case 1 failed\n");
	FAILED(buf[4] != 5.25, "test14 case 2 failed\n");
	FAILED(buf[5] != 7.0, "test14 case 3 failed\n");
	FAILED(buf[6] != 0.0, "test14 case 4 failed\n");
	FAILED(buf[7] != 5.5, "test14 case 5 failed\n");
	FAILED(buf[8] != 3.75, "test14 case 6 failed\n");
	FAILED(buf[9] != 24.5, "test14 case 7 failed\n");
	FAILED(buf[10] != 38.5, "test14 case 8 failed\n");
	FAILED(buf[11] != 9.625, "test14 case 9 failed\n");
	FAILED(buf[12] != 2.0, "test14 case 10 failed\n");
	FAILED(buf[13] != 2.0, "test14 case 11 failed\n");
	FAILED(buf[14] != 0.5, "test14 case 12 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static sljit_sw SLJIT_CALL func(sljit_sw a, sljit_sw b, sljit_sw c)
{
	return a + b + c + 5;
}

static void test15(void)
{
	/* Test function call. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	struct sljit_jump* jump;
	sljit_sw buf[7];

	if (verbose)
		printf("Run test15\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 0;
	buf[5] = 0;
	buf[6] = SLJIT_FUNC_OFFSET(func);

	sljit_emit_enter(compiler, 0, 1, 4, 1, 0, 0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 5);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 7);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, -3);
	sljit_emit_ijump(compiler, SLJIT_CALL3, SLJIT_IMM, SLJIT_FUNC_OFFSET(func));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_RETURN_REG, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -5);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, -10);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 2);
	jump = sljit_emit_jump(compiler, SLJIT_CALL3 | SLJIT_REWRITABLE_JUMP);
	sljit_set_target(jump, (sljit_sw)-1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_RETURN_REG, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_FUNC_OFFSET(func));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 40);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, -3);
	sljit_emit_ijump(compiler, SLJIT_CALL3, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_RETURN_REG, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -60);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, SLJIT_FUNC_OFFSET(func));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, -30);
	sljit_emit_ijump(compiler, SLJIT_CALL3, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_RETURN_REG, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 10);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 16);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, SLJIT_FUNC_OFFSET(func));
	sljit_emit_ijump(compiler, SLJIT_CALL3, SLJIT_R2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_RETURN_REG, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 100);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 110);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 120);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, SLJIT_FUNC_OFFSET(func));
	sljit_emit_ijump(compiler, SLJIT_CALL3, SLJIT_R3, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_RETURN_REG, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -10);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, -16);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 6);
	sljit_emit_ijump(compiler, SLJIT_CALL3, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_RETURN_REG, 0);

	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_set_jump_addr(sljit_get_jump_addr(jump), SLJIT_FUNC_OFFSET(func));
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_sw)&buf) != -15, "test15 case 1 failed\n");
	FAILED(buf[0] != 14, "test15 case 2 failed\n");
	FAILED(buf[1] != -8, "test15 case 3 failed\n");
	FAILED(buf[2] != SLJIT_FUNC_OFFSET(func) + 42, "test15 case 4 failed\n");
	FAILED(buf[3] != SLJIT_FUNC_OFFSET(func) - 85, "test15 case 5 failed\n");
	FAILED(buf[4] != SLJIT_FUNC_OFFSET(func) + 31, "test15 case 6 failed\n");
	FAILED(buf[5] != 335, "test15 case 7 failed\n");
	FAILED(buf[6] != -15, "test15 case 8 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test16(void)
{
	/* Ackermann benchmark. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	struct sljit_label *entry;
	struct sljit_label *label;
	struct sljit_jump *jump;
	struct sljit_jump *jump1;
	struct sljit_jump *jump2;

	if (verbose)
		printf("Run test16\n");

	FAILED(!compiler, "cannot create compiler\n");

	entry = sljit_emit_label(compiler);
	sljit_emit_enter(compiler, 0, 2, 3, 2, 0, 0, 0);
	/* If x == 0. */
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_S0, 0, SLJIT_IMM, 0);
	jump1 = sljit_emit_jump(compiler, SLJIT_EQUAL);
	/* If y == 0. */
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_S1, 0, SLJIT_IMM, 0);
	jump2 = sljit_emit_jump(compiler, SLJIT_EQUAL);

	/* Ack(x,y-1). */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R1, 0, SLJIT_S1, 0, SLJIT_IMM, 1);
	jump = sljit_emit_jump(compiler, SLJIT_CALL2);
	sljit_set_label(jump, entry);

	/* Returns with Ack(x-1, Ack(x,y-1)). */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_RETURN_REG, 0);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_IMM, 1);
	jump = sljit_emit_jump(compiler, SLJIT_CALL2);
	sljit_set_label(jump, entry);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);

	/* Returns with y+1. */
	label = sljit_emit_label(compiler);
	sljit_set_label(jump1, label);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_RETURN_REG, 0, SLJIT_IMM, 1, SLJIT_S1, 0);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);

	/* Returns with Ack(x-1,1) */
	label = sljit_emit_label(compiler);
	sljit_set_label(jump2, label);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 1);
	jump = sljit_emit_jump(compiler, SLJIT_CALL2);
	sljit_set_label(jump, entry);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func2(3, 3) != 61, "test16 case 1 failed\n");
	/* For benchmarking. */
	/* FAILED(code.func2(3, 11) != 16381, "test16 case 1 failed\n"); */

	sljit_free_code(code.code);
	successful_tests++;
}

static void test17(void)
{
	/* Test arm constant pool. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_si i;
	sljit_sw buf[5];

	if (verbose)
		printf("Run test17\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 0;

	sljit_emit_enter(compiler, 0, 1, 3, 1, 0, 0, 0);
	for (i = 0; i <= 0xfff; i++) {
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x81818000 | i);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x81818000 | i);
		if ((i & 0x3ff) == 0)
			sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), (i >> 10) * sizeof(sljit_sw), SLJIT_R0, 0);
	}
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);
	FAILED((sljit_uw)buf[0] != 0x81818000, "test17 case 1 failed\n");
	FAILED((sljit_uw)buf[1] != 0x81818400, "test17 case 2 failed\n");
	FAILED((sljit_uw)buf[2] != 0x81818800, "test17 case 3 failed\n");
	FAILED((sljit_uw)buf[3] != 0x81818c00, "test17 case 4 failed\n");
	FAILED((sljit_uw)buf[4] != 0x81818fff, "test17 case 5 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test18(void)
{
	/* Test 64 bit. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_sw buf[11];

	if (verbose)
		printf("Run test18\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 0;
	buf[5] = 100;
	buf[6] = 100;
	buf[7] = 100;
	buf[8] = 100;
	buf[9] = 0;
#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE) && (defined SLJIT_BIG_ENDIAN && SLJIT_BIG_ENDIAN)
	buf[10] = SLJIT_W(1) << 32;
#else
	buf[10] = 1;
#endif

	sljit_emit_enter(compiler, 0, 1, 3, 2, 0, 0, 0);

#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_IMM, SLJIT_W(0x1122334455667788));
	sljit_emit_op1(compiler, SLJIT_MOV_UI, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_IMM, SLJIT_W(0x1122334455667788));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(1000000000000));
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 2, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(1000000000000));
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3, SLJIT_IMM, SLJIT_W(5000000000000), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R1, 0, SLJIT_IMM, SLJIT_W(0x1108080808));
	sljit_emit_op2(compiler, SLJIT_IADD, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 4, SLJIT_R1, 0, SLJIT_IMM, SLJIT_W(0x1120202020));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(0x1108080808));
	sljit_emit_op2(compiler, SLJIT_AND | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(0x1120202020));
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_UNUSED, 0, SLJIT_ZERO);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 5, SLJIT_S1, 0);
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_IAND | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(0x1120202020));
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 6, SLJIT_UNUSED, 0, SLJIT_I_ZERO);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(0x1108080808));
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_U, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(0x2208080808));
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 7, SLJIT_UNUSED, 0, SLJIT_LESS);
	sljit_emit_op1(compiler, SLJIT_MOV_SI | SLJIT_INT_OP, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_IAND | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(0x1104040404));
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 8, SLJIT_UNUSED, 0, SLJIT_I_NOT_ZERO);

	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R0, 0, SLJIT_IMM, 4);
	sljit_emit_op2(compiler, SLJIT_ISHL, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 9, SLJIT_IMM, SLJIT_W(0xffff0000), SLJIT_R0, 0);

	sljit_emit_op2(compiler, SLJIT_IMUL, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 10, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 10, SLJIT_IMM, -1);
#else
	/* 32 bit operations. */

	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_IMM, 0x11223344);
	sljit_emit_op2(compiler, SLJIT_IADD, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_IMM, 0x44332211);

#endif

	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);
#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	FAILED(buf[0] != SLJIT_W(0x1122334455667788), "test18 case 1 failed\n");
#if (defined SLJIT_LITTLE_ENDIAN && SLJIT_LITTLE_ENDIAN)
	FAILED(buf[1] != 0x55667788, "test18 case 2 failed\n");
#else
	FAILED(buf[1] != SLJIT_W(0x5566778800000000), "test18 case 2 failed\n");
#endif
	FAILED(buf[2] != SLJIT_W(2000000000000), "test18 case 3 failed\n");
	FAILED(buf[3] != SLJIT_W(4000000000000), "test18 case 4 failed\n");
#if (defined SLJIT_LITTLE_ENDIAN && SLJIT_LITTLE_ENDIAN)
	FAILED(buf[4] != 0x28282828, "test18 case 5 failed\n");
#else
	FAILED(buf[4] != SLJIT_W(0x2828282800000000), "test18 case 5 failed\n");
#endif
	FAILED(buf[5] != 0, "test18 case 6 failed\n");
	FAILED(buf[6] != 1, "test18 case 7 failed\n");
	FAILED(buf[7] != 1, "test18 case 8 failed\n");
	FAILED(buf[8] != 0, "test18 case 9 failed\n");
#if (defined SLJIT_LITTLE_ENDIAN && SLJIT_LITTLE_ENDIAN)
	FAILED(buf[9] != 0xfff00000, "test18 case 10 failed\n");
	FAILED(buf[10] != 0xffffffff, "test18 case 11 failed\n");
#else
	FAILED(buf[9] != SLJIT_W(0xfff0000000000000), "test18 case 10 failed\n");
	FAILED(buf[10] != SLJIT_W(0xffffffff00000000), "test18 case 11 failed\n");
#endif
#else
	FAILED(buf[0] != 0x11223344, "test18 case 1 failed\n");
	FAILED(buf[1] != 0x44332211, "test18 case 2 failed\n");
#endif

	sljit_free_code(code.code);
	successful_tests++;
}

static void test19(void)
{
	/* Test arm partial instruction caching. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_sw buf[10];

	if (verbose)
		printf("Run test19\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 6;
	buf[1] = 4;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 0;
	buf[5] = 0;
	buf[6] = 2;
	buf[7] = 0;

	sljit_emit_enter(compiler, 0, 1, 3, 1, 0, 0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw));
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM0(), (sljit_sw)&buf[2], SLJIT_MEM0(), (sljit_sw)&buf[1], SLJIT_MEM0(), (sljit_sw)&buf[0]);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, sizeof(sljit_sw));
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3, SLJIT_MEM1(SLJIT_R0), (sljit_sw)&buf[0], SLJIT_MEM1(SLJIT_R1), (sljit_sw)&buf[0]);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 4, SLJIT_MEM0(), (sljit_sw)&buf[0], SLJIT_IMM, 2);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 5, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 2, SLJIT_MEM1(SLJIT_R0), (sljit_sw)&buf[0] + 4 * sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 7, SLJIT_IMM, 10);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 7);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_R1), (sljit_sw)&buf[5], SLJIT_MEM2(SLJIT_S0, SLJIT_R0), SLJIT_WORD_SHIFT, SLJIT_MEM1(SLJIT_R1), (sljit_sw)&buf[5]);

	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);
	FAILED(buf[0] != 10, "test19 case 1 failed\n");
	FAILED(buf[1] != 4, "test19 case 2 failed\n");
	FAILED(buf[2] != 14, "test19 case 3 failed\n");
	FAILED(buf[3] != 14, "test19 case 4 failed\n");
	FAILED(buf[4] != 8, "test19 case 5 failed\n");
	FAILED(buf[5] != 6, "test19 case 6 failed\n");
	FAILED(buf[6] != 12, "test19 case 7 failed\n");
	FAILED(buf[7] != 10, "test19 case 8 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test20(void)
{
	/* Test stack. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	struct sljit_jump* jump;
	struct sljit_label* label;
	sljit_sw buf[6];
#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	sljit_sw offset_value = SLJIT_W(0x1234567812345678);
#else
	sljit_sw offset_value = SLJIT_W(0x12345678);
#endif

	if (verbose)
		printf("Run test20\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 5;
	buf[1] = 12;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 111;
	buf[5] = -12345;

	sljit_emit_enter(compiler, 0, 1, 5, 5, 0, 0, 4 * sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_uw), SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R4, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S3, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S4, 0, SLJIT_IMM, -1);
	sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_uw), SLJIT_MEM1(SLJIT_SP), 0, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_uw));
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_uw), SLJIT_MEM1(SLJIT_SP), sizeof(sljit_uw), SLJIT_MEM1(SLJIT_SP), 0);
	sljit_get_local_base(compiler, SLJIT_R0, 0, -offset_value);
	sljit_get_local_base(compiler, SLJIT_MEM1(SLJIT_S0), 0, -0x1234);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_uw), SLJIT_MEM1(SLJIT_R0), offset_value, SLJIT_MEM1(SLJIT_R1), 0x1234 + sizeof(sljit_sw));
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_uw));
	/* Dummy last instructions. */
	sljit_emit_const(compiler, SLJIT_R0, 0, -9);
	sljit_emit_label(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_sw)&buf) != -12345, "test20 case 1 failed\n")

	FAILED(buf[2] != 60, "test20 case 2 failed\n");
	FAILED(buf[3] != 17, "test20 case 3 failed\n");
	FAILED(buf[4] != 7, "test20 case 4 failed\n");

	sljit_free_code(code.code);

	compiler = sljit_create_compiler();
	sljit_emit_enter(compiler, 0, 0, 3, 0, 0, 0, SLJIT_MAX_LOCAL_SIZE);

	sljit_get_local_base(compiler, SLJIT_R0, 0, SLJIT_MAX_LOCAL_SIZE - sizeof(sljit_sw));
	sljit_get_local_base(compiler, SLJIT_R1, 0, -(sljit_sw)sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, -1);
	label = sljit_emit_label(compiler);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_R1), sizeof(sljit_sw), SLJIT_R2, 0);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_R1, 0, SLJIT_R0, 0);
	jump = sljit_emit_jump(compiler, SLJIT_NOT_EQUAL);
	sljit_set_label(jump, label);

	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	/* Just survive this code. */
	code.func0();

	sljit_free_code(code.code);
	successful_tests++;
}

static void test21(void)
{
	/* Test fake enter. The parts of the jit code can be separated in the memory. */
	executable_code code1;
	executable_code code2;
	struct sljit_compiler* compiler = sljit_create_compiler();
	struct sljit_jump* jump;
	sljit_uw addr;
	sljit_sw buf[4];

	if (verbose)
		printf("Run test21\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 9;
	buf[1] = -6;
	buf[2] = 0;
	buf[3] = 0;

	sljit_emit_enter(compiler, 0, 1, 3, 2, 0, 0, 2 * sizeof(sljit_sw));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_IMM, 10);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S0), 0, SLJIT_MEM1(SLJIT_SP), 0);
	jump = sljit_emit_jump(compiler, SLJIT_JUMP | SLJIT_REWRITABLE_JUMP);
	sljit_set_target(jump, 0);

	code1.code = sljit_generate_code(compiler);
	CHECK(compiler);
	addr = sljit_get_jump_addr(jump);
	sljit_free_compiler(compiler);

	compiler = sljit_create_compiler();
	FAILED(!compiler, "cannot create compiler\n");

	/* Other part of the jit code. */
	sljit_set_context(compiler, 0, 1, 3, 2, 0, 0, 2 * sizeof(sljit_sw));

	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 2, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_MEM1(SLJIT_SP), 0);
	sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_MEM1(SLJIT_SP), 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_sw));

	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);

	code2.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	sljit_set_jump_addr(addr, SLJIT_FUNC_OFFSET(code2.code));

	FAILED(code1.func1((sljit_sw)&buf) != 19, "test21 case 1 failed\n");
	FAILED(buf[2] != -16, "test21 case 2 failed\n");
	FAILED(buf[3] != 100, "test21 case 3 failed\n");

	sljit_free_code(code1.code);
	sljit_free_code(code2.code);
	successful_tests++;
}

static void test22(void)
{
	/* Test simple byte and half-int data transfers. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_sw buf[9];
	sljit_sh sbuf[7];
	sljit_sb bbuf[5];

	if (verbose)
		printf("Run test22\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 5;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 0;
	buf[5] = 0;
	buf[6] = 0;
	buf[7] = 0;
	buf[8] = 0;

	sbuf[0] = 0;
	sbuf[1] = 0;
	sbuf[2] = -9;
	sbuf[3] = 0;
	sbuf[4] = 0;
	sbuf[5] = 0;
	sbuf[6] = 0;

	bbuf[0] = 0;
	bbuf[1] = 0;
	bbuf[2] = -56;
	bbuf[3] = 0;
	bbuf[4] = 0;

	sljit_emit_enter(compiler, 0, 3, 3, 3, 0, 0, 0);

	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_IMM, sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_S0, 0);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_R1), sizeof(sljit_sw), SLJIT_IMM, -13);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_R1), sizeof(sljit_sw), SLJIT_R2, 0);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_R1), sizeof(sljit_sw), SLJIT_MEM1(SLJIT_R0), sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM2(SLJIT_R1, SLJIT_R0), SLJIT_WORD_SHIFT, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM2(SLJIT_R1, SLJIT_R0), 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 2);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM2(SLJIT_R1, SLJIT_R0), SLJIT_WORD_SHIFT, SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV_SH, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_IMM, -13);
	sljit_emit_op1(compiler, SLJIT_MOVU_UH, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_sh), SLJIT_IMM, 0x1234);
	sljit_emit_op1(compiler, SLJIT_MOVU_SH, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_sh));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_UH, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_sh), SLJIT_MEM1(SLJIT_S1), -(sljit_sw)sizeof(sljit_sh));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0xff0000 + 8000);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 2);
	sljit_emit_op1(compiler, SLJIT_MOV_SH, SLJIT_MEM2(SLJIT_S1, SLJIT_R1), 1, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 3);
	sljit_emit_op1(compiler, SLJIT_MOVU_SH, SLJIT_MEM2(SLJIT_S1, SLJIT_R1), 1, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_SH, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_IMM, -9317);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S1, 0, SLJIT_IMM, 5 * sizeof(sljit_sh));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -5);
	sljit_emit_op1(compiler, SLJIT_MOV_UH, SLJIT_R1, 0, SLJIT_MEM2(SLJIT_R1, SLJIT_R0), 1);
	sljit_emit_op1(compiler, SLJIT_MOV_UH, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_sh), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV_SB, SLJIT_MEM1(SLJIT_S2), 0, SLJIT_IMM, -45);
	sljit_emit_op1(compiler, SLJIT_MOVU_UB, SLJIT_MEM1(SLJIT_S2), sizeof(sljit_sb), SLJIT_IMM, 0x12);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 2 * sizeof(sljit_sb));
	sljit_emit_op1(compiler, SLJIT_MOVU_SB, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_S2), sizeof(sljit_sb));
	sljit_emit_op1(compiler, SLJIT_MOV_SB, SLJIT_S1, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_UB, SLJIT_S1, 0, SLJIT_S1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_SB, SLJIT_R2, 0, SLJIT_S1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw), SLJIT_R2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_UB, SLJIT_MEM1(SLJIT_S2), sizeof(sljit_sb), SLJIT_S1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_UB, SLJIT_MEM2(SLJIT_S2, SLJIT_R0), 0, SLJIT_R0, 0);

	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func3((sljit_sw)&buf, (sljit_sw)&sbuf, (sljit_sw)&bbuf);
	FAILED(buf[1] != -13, "test22 case 1 failed\n");
	FAILED(buf[2] != 5, "test22 case 2 failed\n");
	FAILED(buf[3] != -13, "test22 case 3 failed\n");
	FAILED(buf[4] != (sljit_sw)&buf[3], "test22 case 4 failed\n");
	FAILED(buf[5] != (sljit_sw)&buf[4], "test22 case 5 failed\n");
	FAILED(buf[6] != (sljit_sw)&buf[4], "test22 case 6 failed\n");
	FAILED(buf[7] != -9, "test22 case 7 failed\n");
	FAILED(buf[8] != -56, "test22 case 8 failed\n");

	FAILED(sbuf[0] != -13, "test22 case 9 failed\n");
	FAILED(sbuf[1] != 0x1234, "test22 case 10 failed\n");
	FAILED(sbuf[3] != 0x1234, "test22 case 11 failed\n");
	FAILED(sbuf[4] != 8000, "test22 case 12 failed\n");
	FAILED(sbuf[5] != -9317, "test22 case 13 failed\n");
	FAILED(sbuf[6] != -9317, "test22 case 14 failed\n");

	FAILED(bbuf[0] != -45, "test22 case 15 failed\n");
	FAILED(bbuf[1] != 0x12, "test22 case 16 failed\n");
	FAILED(bbuf[3] != -56, "test22 case 17 failed\n");
	FAILED(bbuf[4] != 2, "test22 case 18 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test23(void)
{
	/* Test 32 bit / 64 bit signed / unsigned int transfer and conversion.
	   This test has do real things on 64 bit systems, but works on 32 bit systems as well. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_sw buf[9];
	sljit_si ibuf[5];
	union {
		sljit_si asint;
		sljit_ub asbytes[4];
	} u;
#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	sljit_sw garbage = SLJIT_W(0x1234567812345678);
#else
	sljit_sw garbage = 0x12345678;
#endif

	if (verbose)
		printf("Run test23\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 0;
	buf[5] = 0;
	buf[6] = 0;
	buf[7] = 0;
	buf[8] = 0;

	ibuf[0] = 0;
	ibuf[1] = 0;
	ibuf[2] = -5791;
	ibuf[3] = 43579;
	ibuf[4] = 658923;

	sljit_emit_enter(compiler, 0, 2, 3, 3, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_UI, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_IMM, 34567);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOVU_SI, SLJIT_MEM2(SLJIT_S1, SLJIT_R0), 2, SLJIT_IMM, -7654);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, garbage);
	sljit_emit_op1(compiler, SLJIT_MOVU_SI, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_si));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, garbage);
	sljit_emit_op1(compiler, SLJIT_MOVU_UI, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_si));
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, garbage);
	sljit_emit_op1(compiler, SLJIT_MOV_SI, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_si));
	sljit_emit_op1(compiler, SLJIT_MOV_UI, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x0f00f00);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_S0, 0, SLJIT_S0, 0, SLJIT_IMM, 0x7777);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), 0x7777 + 2 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_S0, 0, SLJIT_S0, 0, SLJIT_IMM, 0x7777);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), -0x7777 + (sljit_sw)sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R1, 0, SLJIT_S0, 0, SLJIT_IMM, 16 - sizeof(sljit_sw));
	sljit_emit_op2(compiler, SLJIT_LSHR, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 16);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM2(SLJIT_R0, SLJIT_R1), 1, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_OR, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_IMM, 64, SLJIT_MEM1(SLJIT_R0), 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_R0), (sljit_sw)&buf[6], SLJIT_MEM0(), (sljit_sw)&buf[6]);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_IMM, 0x123456);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_S0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_IMM, sizeof(sljit_sw));
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_S0, 0, SLJIT_S0, 0, SLJIT_IMM, 100000 * sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), 100001 * sizeof(sljit_sw), SLJIT_S0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_IMM, sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOVU_SI, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_si), SLJIT_IMM, 0x12345678);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0x2bd700 | 243);
	sljit_emit_return(compiler, SLJIT_MOV_SB, SLJIT_R1, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func2((sljit_sw)&buf, (sljit_sw)&ibuf) != -13, "test23 case 1 failed\n");
	FAILED(buf[0] != -5791, "test23 case 2 failed\n");
	FAILED(buf[1] != 43579, "test23 case 3 failed\n");
	FAILED(buf[2] != 658923, "test23 case 4 failed\n");
	FAILED(buf[3] != 0x0f00f00, "test23 case 5 failed\n");
	FAILED(buf[4] != 0x0f00f00, "test23 case 6 failed\n");
	FAILED(buf[5] != 80, "test23 case 7 failed\n");
	FAILED(buf[6] != 0x123456, "test23 case 8 failed\n");
	FAILED(buf[7] != (sljit_sw)&buf[5], "test23 case 9 failed\n");
	FAILED(buf[8] != (sljit_sw)&buf[8] - 100000 * sizeof(sljit_sw), "test23 case 10 failed\n");

	FAILED(ibuf[0] != 34567, "test23 case 11 failed\n");
	FAILED(ibuf[1] != -7654, "test23 case 12 failed\n");
	u.asint = ibuf[4];
#if (defined SLJIT_LITTLE_ENDIAN && SLJIT_LITTLE_ENDIAN)
	FAILED(u.asbytes[0] != 0x78, "test23 case 13 failed\n");
	FAILED(u.asbytes[1] != 0x56, "test23 case 14 failed\n");
	FAILED(u.asbytes[2] != 0x34, "test23 case 15 failed\n");
	FAILED(u.asbytes[3] != 0x12, "test23 case 16 failed\n");
#else
	FAILED(u.asbytes[0] != 0x12, "test23 case 13 failed\n");
	FAILED(u.asbytes[1] != 0x34, "test23 case 14 failed\n");
	FAILED(u.asbytes[2] != 0x56, "test23 case 15 failed\n");
	FAILED(u.asbytes[3] != 0x78, "test23 case 16 failed\n");
#endif

	sljit_free_code(code.code);
	successful_tests++;
}

static void test24(void)
{
	/* Some complicated addressing modes. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_sw buf[9];
	sljit_sh sbuf[5];
	sljit_sb bbuf[7];

	if (verbose)
		printf("Run test24\n");

	FAILED(!compiler, "cannot create compiler\n");

	buf[0] = 100567;
	buf[1] = 75799;
	buf[2] = 0;
	buf[3] = -8;
	buf[4] = -50;
	buf[5] = 0;
	buf[6] = 0;
	buf[7] = 0;
	buf[8] = 0;

	sbuf[0] = 30000;
	sbuf[1] = 0;
	sbuf[2] = 0;
	sbuf[3] = -12345;
	sbuf[4] = 0;

	bbuf[0] = -128;
	bbuf[1] = 0;
	bbuf[2] = 0;
	bbuf[3] = 99;
	bbuf[4] = 0;
	bbuf[5] = 0;
	bbuf[6] = 0;

	sljit_emit_enter(compiler, 0, 3, 3, 3, 0, 0, 0);

	/* Nothing should be updated. */
	sljit_emit_op1(compiler, SLJIT_MOVU_SH, SLJIT_MEM0(), (sljit_sw)&sbuf[1], SLJIT_MEM0(), (sljit_sw)&sbuf[0]);
	sljit_emit_op1(compiler, SLJIT_MOVU_SB, SLJIT_MEM0(), (sljit_sw)&bbuf[1], SLJIT_MEM0(), (sljit_sw)&bbuf[0]);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 2);
	sljit_emit_op1(compiler, SLJIT_MOV_UH, SLJIT_MEM2(SLJIT_S1, SLJIT_R0), 1, SLJIT_MEM0(), (sljit_sw)&sbuf[3]);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&buf[0]);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 2);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM2(SLJIT_R0, SLJIT_R2), SLJIT_WORD_SHIFT, SLJIT_MEM0(), (sljit_sw)&buf[0], SLJIT_MEM2(SLJIT_R1, SLJIT_R0), 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, sizeof(sljit_sb));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, sizeof(sljit_sb));
	sljit_emit_op1(compiler, SLJIT_MOVU_UB, SLJIT_MEM1(SLJIT_R0), (sljit_sw)&bbuf[1], SLJIT_MEM1(SLJIT_R1), (sljit_sw)&bbuf[0]);
	sljit_emit_op1(compiler, SLJIT_MOVU_UB, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_MEM1(SLJIT_R1), 2 * sizeof(sljit_sb));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, sizeof(sljit_sh));
	sljit_emit_op1(compiler, SLJIT_MOV_SH, SLJIT_MEM1(SLJIT_R1), (sljit_sw)&sbuf[3], SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 3);
	sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), SLJIT_WORD_SHIFT, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), SLJIT_WORD_SHIFT, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), SLJIT_WORD_SHIFT);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 4);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_S0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), SLJIT_WORD_SHIFT, SLJIT_MEM2(SLJIT_R1, SLJIT_R0), SLJIT_WORD_SHIFT, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), SLJIT_WORD_SHIFT);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S0, 0, SLJIT_IMM, sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 4);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM2(SLJIT_R1, SLJIT_R2), SLJIT_WORD_SHIFT, SLJIT_MEM2(SLJIT_R0, SLJIT_R2), SLJIT_WORD_SHIFT);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&buf - 0x7fff8000 + 6 * sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 952467);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), 0x7fff8000, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), 0x7fff8000 + sizeof(sljit_sw), SLJIT_MEM1(SLJIT_R0), 0x7fff8000);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&buf + 0x7fff7fff + 6 * sizeof(sljit_sw));
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_R0), -0x7fff7fff + 2 * sizeof(sljit_sw), SLJIT_MEM1(SLJIT_R0), -0x7fff7fff + sizeof(sljit_sw), SLJIT_MEM1(SLJIT_R0), -0x7fff7fff);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&bbuf - 0x7fff7ffe + 3 * sizeof(sljit_sb));
	sljit_emit_op1(compiler, SLJIT_MOV_SB, SLJIT_MEM1(SLJIT_R0), 0x7fff7fff, SLJIT_MEM1(SLJIT_R0), 0x7fff7ffe);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&bbuf + 0x7fff7fff + 5 * sizeof(sljit_sb));
	sljit_emit_op1(compiler, SLJIT_MOV_SB, SLJIT_MEM1(SLJIT_R0), -0x7fff7fff, SLJIT_MEM1(SLJIT_R0), -0x7fff8000);
#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&bbuf - SLJIT_W(0x123456123456));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)&bbuf - SLJIT_W(0x123456123456));
	sljit_emit_op1(compiler, SLJIT_MOV_SB, SLJIT_MEM1(SLJIT_R0), SLJIT_W(0x123456123456) + 6 * sizeof(sljit_sb), SLJIT_MEM1(SLJIT_R1), SLJIT_W(0x123456123456));
#endif

	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func3((sljit_sw)&buf, (sljit_sw)&sbuf, (sljit_sw)&bbuf);
	FAILED(buf[2] != 176366, "test24 case 1 failed\n");
	FAILED(buf[3] != 64, "test24 case 2 failed\n");
	FAILED(buf[4] != -100, "test24 case 3 failed\n");
	FAILED(buf[5] != -100 + (sljit_sw)&buf[5] + (sljit_sw)&buf[4], "test24 case 4 failed\n");
	FAILED(buf[6] != 952467, "test24 case 5 failed\n");
	FAILED(buf[7] != 952467, "test24 case 6 failed\n");
	FAILED(buf[8] != 952467 * 2, "test24 case 7 failed\n");

	FAILED(sbuf[1] != 30000, "test24 case 8 failed\n");
	FAILED(sbuf[2] != -12345, "test24 case 9 failed\n");
	FAILED(sbuf[4] != sizeof(sljit_sh), "test24 case 10 failed\n");

	FAILED(bbuf[1] != -128, "test24 case 11 failed\n");
	FAILED(bbuf[2] != 99, "test24 case 12 failed\n");
	FAILED(bbuf[4] != 99, "test24 case 13 failed\n");
	FAILED(bbuf[5] != 99, "test24 case 14 failed\n");
#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	FAILED(bbuf[6] != -128, "test24 case 15 failed\n");
#endif

	sljit_free_code(code.code);
	successful_tests++;
}

static void test25(void)
{
#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	/* 64 bit loads. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_sw buf[14];

	if (verbose)
		printf("Run test25\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 7;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 0;
	buf[5] = 0;
	buf[6] = 0;
	buf[7] = 0;
	buf[8] = 0;
	buf[9] = 0;
	buf[10] = 0;
	buf[11] = 0;
	buf[12] = 0;
	buf[13] = 0;

	sljit_emit_enter(compiler, 0, 1, 3, 1, 0, 0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 1 * sizeof(sljit_sw), SLJIT_IMM, 0x7fff);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_IMM, -0x8000);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_IMM, 0x7fffffff);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_IMM, SLJIT_W(-0x80000000));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_IMM, SLJIT_W(0x1234567887654321));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_IMM, SLJIT_W(0xff80000000));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_IMM, SLJIT_W(0x3ff0000000));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw), SLJIT_IMM, SLJIT_W(0xfffffff800100000));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_sw), SLJIT_IMM, SLJIT_W(0xfffffff80010f000));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 10 * sizeof(sljit_sw), SLJIT_IMM, SLJIT_W(0x07fff00000008001));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 11 * sizeof(sljit_sw), SLJIT_IMM, SLJIT_W(0x07fff00080010000));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 12 * sizeof(sljit_sw), SLJIT_IMM, SLJIT_W(0x07fff00080018001));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 13 * sizeof(sljit_sw), SLJIT_IMM, SLJIT_W(0x07fff00ffff00000));

	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);
	FAILED(buf[0] != 0, "test25 case 1 failed\n");
	FAILED(buf[1] != 0x7fff, "test25 case 2 failed\n");
	FAILED(buf[2] != -0x8000, "test25 case 3 failed\n");
	FAILED(buf[3] != 0x7fffffff, "test25 case 4 failed\n");
	FAILED(buf[4] != SLJIT_W(-0x80000000), "test25 case 5 failed\n");
	FAILED(buf[5] != SLJIT_W(0x1234567887654321), "test25 case 6 failed\n");
	FAILED(buf[6] != SLJIT_W(0xff80000000), "test25 case 7 failed\n");
	FAILED(buf[7] != SLJIT_W(0x3ff0000000), "test25 case 8 failed\n");
	FAILED((sljit_uw)buf[8] != SLJIT_W(0xfffffff800100000), "test25 case 9 failed\n");
	FAILED((sljit_uw)buf[9] != SLJIT_W(0xfffffff80010f000), "test25 case 10 failed\n");
	FAILED(buf[10] != SLJIT_W(0x07fff00000008001), "test25 case 11 failed\n");
	FAILED(buf[11] != SLJIT_W(0x07fff00080010000), "test25 case 12 failed\n");
	FAILED(buf[12] != SLJIT_W(0x07fff00080018001), "test25 case 13 failed\n");
	FAILED(buf[13] != SLJIT_W(0x07fff00ffff00000), "test25 case 14 failed\n");

	sljit_free_code(code.code);
#endif
	successful_tests++;
}

static void test26(void)
{
	/* Aligned access without aligned offsets. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_sw buf[4];
	sljit_si ibuf[4];
	sljit_d dbuf[4];

	if (verbose)
		printf("Run test26\n");

	FAILED(!compiler, "cannot create compiler\n");

	buf[0] = -2789;
	buf[1] = 0;
	buf[2] = 4;
	buf[3] = -4;

	ibuf[0] = -689;
	ibuf[1] = 0;
	ibuf[2] = -6;
	ibuf[3] = 3;

	dbuf[0] = 5.75;
	dbuf[1] = 0.0;
	dbuf[2] = 0.0;
	dbuf[3] = -4.0;

	sljit_emit_enter(compiler, 0, 3, 3, 3, 0, 0, 0);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_S0, 0, SLJIT_S0, 0, SLJIT_IMM, 3);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_S1, 0, SLJIT_S1, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), -3);
	sljit_emit_op1(compiler, SLJIT_MOV_SI, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_si) - 1, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_SI, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S1), -1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) - 3, SLJIT_R0, 0);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_IMM, 100);
	sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_sw) * 2 - 103, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 2 - 3, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3 - 3);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S1, 0, SLJIT_IMM, 100);
	sljit_emit_op2(compiler, SLJIT_IMUL, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_si) * 2 - 101, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_si) * 2 - 1, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_si) * 3 - 1);

	if (sljit_is_fpu_available()) {
		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_S2, 0, SLJIT_S2, 0, SLJIT_IMM, 3);
		sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_MEM1(SLJIT_S2), sizeof(sljit_d) - 3, SLJIT_MEM1(SLJIT_S2), -3);
		sljit_emit_fop2(compiler, SLJIT_DADD, SLJIT_MEM1(SLJIT_S2), sizeof(sljit_d) * 2 - 3, SLJIT_MEM1(SLJIT_S2), -3, SLJIT_MEM1(SLJIT_S2), sizeof(sljit_d) - 3);
		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S2, 0, SLJIT_IMM, 2);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sizeof(sljit_d) * 3 - 4) >> 1);
		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R2, 0, SLJIT_S2, 0, SLJIT_IMM, 1);
		sljit_emit_fop2(compiler, SLJIT_DDIV, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_d) * 3 - 5, SLJIT_MEM1(SLJIT_S2), sizeof(sljit_d) * 2 - 3, SLJIT_MEM2(SLJIT_R2, SLJIT_R1), 1);
	}

	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func3((sljit_sw)&buf, (sljit_sw)&ibuf, (sljit_sw)&dbuf);

	FAILED(buf[1] != -689, "test26 case 1 failed\n");
	FAILED(buf[2] != -16, "test26 case 2 failed\n");
	FAILED(ibuf[1] != -2789, "test26 case 3 failed\n");
	FAILED(ibuf[2] != -18, "test26 case 4 failed\n");

	if (sljit_is_fpu_available()) {
		FAILED(dbuf[1] != 5.75, "test26 case 5 failed\n");
		FAILED(dbuf[2] != 11.5, "test26 case 6 failed\n");
		FAILED(dbuf[3] != -2.875, "test26 case 7 failed\n");
	}

	sljit_free_code(code.code);
	successful_tests++;
}

static void test27(void)
{
#define SET_NEXT_BYTE(type) \
		cond_set(compiler, SLJIT_R2, 0, type); \
		sljit_emit_op1(compiler, SLJIT_MOVU_UB, SLJIT_MEM1(SLJIT_S0), 1, SLJIT_R2, 0);
#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
#define RESULT(i) i
#else
#define RESULT(i) (3 - i)
#endif

	/* Playing with conditional flags. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_sb buf[37];
	sljit_si i;

	if (verbose)
		printf("Run test27\n");

	for (i = 0; i < 37; ++i)
		buf[i] = 10;

	FAILED(!compiler, "cannot create compiler\n");

	/* 3 arguments passed, 3 arguments used. */
	sljit_emit_enter(compiler, 0, 1, 3, 3, 0, 0, 0);

	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_S0, 0, SLJIT_S0, 0, SLJIT_IMM, 1);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x1001);
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 20);
	/* 0x100100000 on 64 bit machines, 0x100000 on 32 bit machines. */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0x800000);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_U, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op0(compiler, SLJIT_NOP); /* Nop should keep the flags. */
	SET_NEXT_BYTE(SLJIT_GREATER);
	SET_NEXT_BYTE(SLJIT_LESS);
	sljit_emit_op1(compiler, SLJIT_MOV_SI | SLJIT_INT_OP, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_SI | SLJIT_INT_OP, SLJIT_R1, 0, SLJIT_R1, 0);
	sljit_emit_op2(compiler, SLJIT_ISUB | SLJIT_SET_U, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op0(compiler, SLJIT_NOP); /* Nop should keep the flags. */
	SET_NEXT_BYTE(SLJIT_I_GREATER);
	SET_NEXT_BYTE(SLJIT_I_LESS);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x1000);
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 20);
	sljit_emit_op2(compiler, SLJIT_OR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 0x10);
	/* 0x100000010 on 64 bit machines, 0x10 on 32 bit machines. */
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_U, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_IMM, 0x80);
	SET_NEXT_BYTE(SLJIT_GREATER);
	SET_NEXT_BYTE(SLJIT_LESS);
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_ISUB | SLJIT_SET_U, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_IMM, 0x80);
	SET_NEXT_BYTE(SLJIT_I_GREATER);
	SET_NEXT_BYTE(SLJIT_I_LESS);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
	/* 0xff..ff on all machines. */
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_U, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
	SET_NEXT_BYTE(SLJIT_LESS_EQUAL);
	SET_NEXT_BYTE(SLJIT_GREATER_EQUAL);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_S, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, -1);
	SET_NEXT_BYTE(SLJIT_SIG_GREATER);
	SET_NEXT_BYTE(SLJIT_SIG_LESS);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_E, SLJIT_R0, 0, SLJIT_R1, 0, SLJIT_R0, 0);
	SET_NEXT_BYTE(SLJIT_EQUAL);
	SET_NEXT_BYTE(SLJIT_NOT_EQUAL);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_O | SLJIT_SET_U, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, -2);
	SET_NEXT_BYTE(SLJIT_OVERFLOW);
	SET_NEXT_BYTE(SLJIT_NOT_OVERFLOW);
	SET_NEXT_BYTE(SLJIT_GREATER_EQUAL);
	SET_NEXT_BYTE(SLJIT_LESS_EQUAL);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x80000000);
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 16);
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 16);
	/* 0x80..0 on 64 bit machines, 0 on 32 bit machines. */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0xffffffff);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_O, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_R1, 0);
	SET_NEXT_BYTE(SLJIT_OVERFLOW);
	SET_NEXT_BYTE(SLJIT_NOT_OVERFLOW);
	sljit_emit_op1(compiler, SLJIT_MOV_SI, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_UI, SLJIT_R1, 0, SLJIT_R1, 0);
	sljit_emit_op2(compiler, SLJIT_ISUB | SLJIT_SET_O, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_R1, 0);
	SET_NEXT_BYTE(SLJIT_I_OVERFLOW);
	SET_NEXT_BYTE(SLJIT_I_NOT_OVERFLOW);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_C, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op2(compiler, SLJIT_SUBC | SLJIT_SET_C, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op2(compiler, SLJIT_SUBC, SLJIT_R0, 0, SLJIT_IMM, 6, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOVU_UB, SLJIT_MEM1(SLJIT_S0), 1, SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -1);
	sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_SET_C, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op2(compiler, SLJIT_ADDC | SLJIT_SET_C, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op2(compiler, SLJIT_ADDC, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 9);
	sljit_emit_op1(compiler, SLJIT_MOVU_UB, SLJIT_MEM1(SLJIT_S0), 1, SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, (8 * sizeof(sljit_sw)) - 1);
	sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_IMM, 0);
	SET_NEXT_BYTE(SLJIT_EQUAL);
	sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_R0, 0);
	SET_NEXT_BYTE(SLJIT_EQUAL);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op2(compiler, SLJIT_ASHR | SLJIT_SET_E, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 0);
	SET_NEXT_BYTE(SLJIT_EQUAL);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op2(compiler, SLJIT_LSHR | SLJIT_SET_E, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 0xffffc0);
	SET_NEXT_BYTE(SLJIT_NOT_EQUAL);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_PREF_SHIFT_REG, 0, SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op2(compiler, SLJIT_ASHR | SLJIT_SET_E, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_PREF_SHIFT_REG, 0);
	SET_NEXT_BYTE(SLJIT_EQUAL);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_PREF_SHIFT_REG, 0, SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op2(compiler, SLJIT_LSHR | SLJIT_SET_E, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_PREF_SHIFT_REG, 0);
	SET_NEXT_BYTE(SLJIT_NOT_EQUAL);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 1);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_C, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_SUBC, SLJIT_R2, 0, SLJIT_IMM, 1, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOVU_UB, SLJIT_MEM1(SLJIT_S0), 1, SLJIT_R2, 0);
	sljit_emit_op2(compiler, SLJIT_SUBC | SLJIT_SET_C, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op2(compiler, SLJIT_SUBC, SLJIT_R2, 0, SLJIT_IMM, 1, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOVU_UB, SLJIT_MEM1(SLJIT_S0), 1, SLJIT_R2, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -34);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_S | SLJIT_SET_U, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_IMM, 0x1234);
	SET_NEXT_BYTE(SLJIT_LESS);
	SET_NEXT_BYTE(SLJIT_SIG_LESS);
#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(0x12300000000) - 43);
#else
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R0, 0, SLJIT_IMM, -43);
#endif
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R1, 0, SLJIT_IMM, -96);
	sljit_emit_op2(compiler, SLJIT_ISUB | SLJIT_SET_S | SLJIT_SET_U, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_R1, 0);
	SET_NEXT_BYTE(SLJIT_I_LESS);
	SET_NEXT_BYTE(SLJIT_I_SIG_GREATER);

	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);

	FAILED(buf[0] != RESULT(1), "test27 case 1 failed\n");
	FAILED(buf[1] != RESULT(2), "test27 case 2 failed\n");
	FAILED(buf[2] != 2, "test27 case 3 failed\n");
	FAILED(buf[3] != 1, "test27 case 4 failed\n");
	FAILED(buf[4] != RESULT(1), "test27 case 5 failed\n");
	FAILED(buf[5] != RESULT(2), "test27 case 6 failed\n");
	FAILED(buf[6] != 2, "test27 case 7 failed\n");
	FAILED(buf[7] != 1, "test27 case 8 failed\n");

	FAILED(buf[8] != 2, "test27 case 9 failed\n");
	FAILED(buf[9] != 1, "test27 case 10 failed\n");
	FAILED(buf[10] != 2, "test27 case 11 failed\n");
	FAILED(buf[11] != 1, "test27 case 12 failed\n");
	FAILED(buf[12] != 1, "test27 case 13 failed\n");
	FAILED(buf[13] != 2, "test27 case 14 failed\n");
	FAILED(buf[14] != 2, "test27 case 15 failed\n");
	FAILED(buf[15] != 1, "test27 case 16 failed\n");
	FAILED(buf[16] != 1, "test27 case 17 failed\n");
	FAILED(buf[17] != 2, "test27 case 18 failed\n");

	FAILED(buf[18] != RESULT(1), "test27 case 19 failed\n");
	FAILED(buf[19] != RESULT(2), "test27 case 20 failed\n");
	FAILED(buf[20] != 2, "test27 case 21 failed\n");
	FAILED(buf[21] != 1, "test27 case 22 failed\n");

	FAILED(buf[22] != 5, "test27 case 23 failed\n");
	FAILED(buf[23] != 9, "test27 case 24 failed\n");

	FAILED(buf[24] != 2, "test27 case 25 failed\n");
	FAILED(buf[25] != 1, "test27 case 26 failed\n");

	FAILED(buf[26] != 1, "test27 case 27 failed\n");
	FAILED(buf[27] != 1, "test27 case 28 failed\n");
	FAILED(buf[28] != 1, "test27 case 29 failed\n");
	FAILED(buf[29] != 1, "test27 case 30 failed\n");

	FAILED(buf[30] != 1, "test27 case 31 failed\n");
	FAILED(buf[31] != 0, "test27 case 32 failed\n");

	FAILED(buf[32] != 2, "test27 case 33 failed\n");
	FAILED(buf[33] != 1, "test27 case 34 failed\n");
	FAILED(buf[34] != 2, "test27 case 35 failed\n");
	FAILED(buf[35] != 1, "test27 case 36 failed\n");
	FAILED(buf[36] != 10, "test27 case 37 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
#undef SET_NEXT_BYTE
#undef RESULT
}

static void test28(void)
{
	/* Test mov. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	struct sljit_const* const1;
	struct sljit_label* label;
	sljit_uw label_addr;
	sljit_sw buf[5];

	if (verbose)
		printf("Run test28\n");

	FAILED(!compiler, "cannot create compiler\n");

	buf[0] = -36;
	buf[1] = 8;
	buf[2] = 0;
	buf[3] = 10;
	buf[4] = 0;

	FAILED(!compiler, "cannot create compiler\n");
	sljit_emit_enter(compiler, 0, 1, 5, 5, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, -234);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R4, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw));
	sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_S3, 0, SLJIT_R3, 0, SLJIT_R4, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_S3, 0);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_S3, 0, SLJIT_IMM, 0);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_S3, 0, SLJIT_UNUSED, 0, SLJIT_NOT_ZERO);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_S3, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S4, 0, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw));
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_S4, 0, SLJIT_S4, 0, SLJIT_R4, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_S4, 0);
	const1 = sljit_emit_const(compiler, SLJIT_S3, 0, 0);
	sljit_emit_ijump(compiler, SLJIT_JUMP, SLJIT_S3, 0);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_S3, 0, SLJIT_S3, 0, SLJIT_IMM, 100);
	label = sljit_emit_label(compiler);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_S3, 0);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_R4, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	label_addr = sljit_get_label_addr(label);
	sljit_set_const(sljit_get_const_addr(const1), label_addr);
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_sw)&buf) != 8, "test28 case 1 failed\n");
	FAILED(buf[1] != -1872, "test28 case 2 failed\n");
	FAILED(buf[2] != 1, "test28 case 3 failed\n");
	FAILED(buf[3] != 2, "test28 case 4 failed\n");
	FAILED(buf[4] != label_addr, "test28 case 5 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test29(void)
{
	/* Test signed/unsigned bytes and halfs. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_sw buf[25];

	if (verbose)
		printf("Run test29\n");

	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 0;
	buf[5] = 0;
	buf[6] = 0;
	buf[7] = 0;
	buf[8] = 0;
	buf[9] = 0;
	buf[10] = 0;
	buf[11] = 0;
	buf[12] = 0;
	buf[13] = 0;
	buf[14] = 0;
	buf[15] = 0;
	buf[16] = 0;
	buf[17] = 0;
	buf[18] = 0;
	buf[19] = 0;
	buf[20] = 0;
	buf[21] = 0;
	buf[22] = 0;
	buf[23] = 0;
	buf[24] = 0;

	FAILED(!compiler, "cannot create compiler\n");
	sljit_emit_enter(compiler, 0, 1, 5, 5, 0, 0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV_SB, SLJIT_R0, 0, SLJIT_IMM, -187);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOVU_SB, SLJIT_R0, 0, SLJIT_IMM, -605);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_UB, SLJIT_R0, 0, SLJIT_IMM, -56);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOVU_UB, SLJIT_R4, 0, SLJIT_IMM, 0xcde5);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw), SLJIT_R4, 0);

	sljit_emit_op1(compiler, SLJIT_MOV_SH, SLJIT_R0, 0, SLJIT_IMM, -45896);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOVU_SH, SLJIT_R0, 0, SLJIT_IMM, -1472797);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_UH, SLJIT_R0, 0, SLJIT_IMM, -12890);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOVU_UH, SLJIT_R4, 0, SLJIT_IMM, 0x9cb0a6);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw), SLJIT_R4, 0);

#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	sljit_emit_op1(compiler, SLJIT_MOV_SI, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(-3580429715));
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOVU_SI, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(-100722768662));
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_UI, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(-1457052677972));
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOVU_UI, SLJIT_R4, 0, SLJIT_IMM, SLJIT_W(0xcef97a70b5));
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw), SLJIT_R4, 0);
#else
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_S0, 0, SLJIT_S0, 0, SLJIT_IMM, 4 * sizeof(sljit_uw));
#endif

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, -187);
	sljit_emit_op1(compiler, SLJIT_MOV_SB, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, -605);
	sljit_emit_op1(compiler, SLJIT_MOVU_SB, SLJIT_R0, 0, SLJIT_S2, 0);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, -56);
	sljit_emit_op1(compiler, SLJIT_MOV_UB, SLJIT_R0, 0, SLJIT_R2, 0);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, 0xcde5);
	sljit_emit_op1(compiler, SLJIT_MOVU_UB, SLJIT_R4, 0, SLJIT_R3, 0);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw), SLJIT_R4, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, -45896);
	sljit_emit_op1(compiler, SLJIT_MOV_SH, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, -1472797);
	sljit_emit_op1(compiler, SLJIT_MOVU_SH, SLJIT_R0, 0, SLJIT_S2, 0);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, -12890);
	sljit_emit_op1(compiler, SLJIT_MOV_UH, SLJIT_R0, 0, SLJIT_R2, 0);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, 0x9cb0a6);
	sljit_emit_op1(compiler, SLJIT_MOVU_UH, SLJIT_R4, 0, SLJIT_R3, 0);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw), SLJIT_R4, 0);

#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, SLJIT_W(-3580429715));
	sljit_emit_op1(compiler, SLJIT_MOV_SI, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, SLJIT_W(-100722768662));
	sljit_emit_op1(compiler, SLJIT_MOVU_SI, SLJIT_R0, 0, SLJIT_S2, 0);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, SLJIT_W(-1457052677972));
	sljit_emit_op1(compiler, SLJIT_MOV_UI, SLJIT_R0, 0, SLJIT_R2, 0);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, SLJIT_W(0xcef97a70b5));
	sljit_emit_op1(compiler, SLJIT_MOVU_UI, SLJIT_R4, 0, SLJIT_R3, 0);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw), SLJIT_R4, 0);
#else
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_S0, 0, SLJIT_S0, 0, SLJIT_IMM, 4 * sizeof(sljit_uw));
#endif

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, 0x9faa5);
	sljit_emit_op1(compiler, SLJIT_MOV_SB, SLJIT_S2, 0, SLJIT_S2, 0);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw), SLJIT_S2, 0);

	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);
	FAILED(buf[0] != 69, "test29 case 1 failed\n");
	FAILED(buf[1] != -93, "test29 case 2 failed\n");
	FAILED(buf[2] != 200, "test29 case 3 failed\n");
	FAILED(buf[3] != 0xe5, "test29 case 4 failed\n");
	FAILED(buf[4] != 19640, "test29 case 5 failed\n");
	FAILED(buf[5] != -31005, "test29 case 6 failed\n");
	FAILED(buf[6] != 52646, "test29 case 7 failed\n");
	FAILED(buf[7] != 0xb0a6, "test29 case 8 failed\n");

#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	FAILED(buf[8] != SLJIT_W(714537581), "test29 case 9 failed\n");
	FAILED(buf[9] != SLJIT_W(-1938520854), "test29 case 10 failed\n");
	FAILED(buf[10] != SLJIT_W(3236202668), "test29 case 11 failed\n");
	FAILED(buf[11] != SLJIT_W(0xf97a70b5), "test29 case 12 failed\n");
#endif

	FAILED(buf[12] != 69, "test29 case 13 failed\n");
	FAILED(buf[13] != -93, "test29 case 14 failed\n");
	FAILED(buf[14] != 200, "test29 case 15 failed\n");
	FAILED(buf[15] != 0xe5, "test29 case 16 failed\n");
	FAILED(buf[16] != 19640, "test29 case 17 failed\n");
	FAILED(buf[17] != -31005, "test29 case 18 failed\n");
	FAILED(buf[18] != 52646, "test29 case 19 failed\n");
	FAILED(buf[19] != 0xb0a6, "test29 case 20 failed\n");

#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	FAILED(buf[20] != SLJIT_W(714537581), "test29 case 21 failed\n");
	FAILED(buf[21] != SLJIT_W(-1938520854), "test29 case 22 failed\n");
	FAILED(buf[22] != SLJIT_W(3236202668), "test29 case 23 failed\n");
	FAILED(buf[23] != SLJIT_W(0xf97a70b5), "test29 case 24 failed\n");
#endif

	FAILED(buf[24] != -91, "test29 case 25 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test30(void)
{
	/* Test unused results. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_sw buf[1];

	if (verbose)
		printf("Run test30\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	sljit_emit_enter(compiler, 0, 1, 5, 5, 0, 0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R4, 0, SLJIT_IMM, 1);
#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_S1, 0, SLJIT_IMM, SLJIT_W(-0x123ffffffff));
#else
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_S1, 0, SLJIT_IMM, 1);
#endif
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S3, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S4, 0, SLJIT_IMM, 1);

	/* Some calculations with unused results. */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_UNUSED, 0, SLJIT_R2, 0);
	sljit_emit_op1(compiler, SLJIT_NOT, SLJIT_UNUSED, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_NEG, SLJIT_UNUSED, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op2(compiler, SLJIT_ISUB | SLJIT_SET_U, SLJIT_UNUSED, 0, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_UNUSED, 0, SLJIT_S0, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op2(compiler, SLJIT_SHL | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_S3, 0, SLJIT_R2, 0);
	sljit_emit_op2(compiler, SLJIT_LSHR | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_IMM, 5);
	sljit_emit_op2(compiler, SLJIT_AND, SLJIT_UNUSED, 0, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_IMM, 0xff);
	sljit_emit_op1(compiler, SLJIT_INOT | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_S1, 0);

	/* Testing that any change happens. */
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R2, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R3, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R4, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_UI, SLJIT_S1, 0, SLJIT_S1, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_S1, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_S2, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_S3, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0, SLJIT_S4, 0);

	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);
	FAILED(buf[0] != 9, "test30 case 1 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test31(void)
{
	/* Integer mul and set flags. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_sw buf[12];
#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	sljit_sw big_word = SLJIT_W(0x7fffffff00000000);
	sljit_sw big_word2 = SLJIT_W(0x7fffffff00000012);
#else
	sljit_sw big_word = 0x7fffffff;
	sljit_sw big_word2 = 0x00000012;
#endif

	if (verbose)
		printf("Run test31\n");

	buf[0] = 3;
	buf[1] = 3;
	buf[2] = 3;
	buf[3] = 3;
	buf[4] = 3;
	buf[5] = 3;
	buf[6] = 3;
	buf[7] = 3;
	buf[8] = 3;
	buf[9] = 3;
	buf[10] = 3;
	buf[11] = 3;

	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, 1, 3, 5, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0);
	sljit_emit_op2(compiler, SLJIT_MUL | SLJIT_SET_O, SLJIT_UNUSED, 0, SLJIT_R1, 0, SLJIT_IMM, -45);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_MUL_NOT_OVERFLOW);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_MUL_OVERFLOW);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, big_word);
	sljit_emit_op2(compiler, SLJIT_MUL | SLJIT_SET_O, SLJIT_R2, 0, SLJIT_S2, 0, SLJIT_IMM, -2);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 33); /* Should not change flags. */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0); /* Should not change flags. */
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_MUL_OVERFLOW);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_MUL_NOT_OVERFLOW);

	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_S3, 0, SLJIT_IMM, 0x3f6b0);
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_S4, 0, SLJIT_IMM, 0x2a783);
	sljit_emit_op2(compiler, SLJIT_IMUL | SLJIT_SET_O, SLJIT_R1, 0, SLJIT_S3, 0, SLJIT_S4, 0);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_I_MUL_OVERFLOW);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R1, 0, SLJIT_IMM, big_word2);
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R2, 0, SLJIT_R1, 0);
	sljit_emit_op2(compiler, SLJIT_IMUL | SLJIT_SET_O, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 23);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_I_MUL_OVERFLOW);

	sljit_emit_op2(compiler, SLJIT_IMUL | SLJIT_SET_O, SLJIT_UNUSED, 0, SLJIT_R2, 0, SLJIT_IMM, -23);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_I_MUL_NOT_OVERFLOW);
	sljit_emit_op2(compiler, SLJIT_MUL | SLJIT_SET_O, SLJIT_UNUSED, 0, SLJIT_R2, 0, SLJIT_IMM, -23);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw), SLJIT_MUL_NOT_OVERFLOW);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 67);
	sljit_emit_op2(compiler, SLJIT_MUL | SLJIT_SET_O, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, -23);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);

	FAILED(buf[0] != 1, "test31 case 1 failed\n");
	FAILED(buf[1] != 2, "test31 case 2 failed\n");
/* Qemu issues for 64 bit muls. */
#if !(defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	FAILED(buf[2] != 1, "test31 case 3 failed\n");
	FAILED(buf[3] != 2, "test31 case 4 failed\n");
#endif
	FAILED(buf[4] != 1, "test31 case 5 failed\n");
	FAILED((buf[5] & 0xffffffff) != 0x85540c10, "test31 case 6 failed\n");
	FAILED(buf[6] != 2, "test31 case 7 failed\n");
	FAILED(buf[7] != 1, "test31 case 8 failed\n");
#if !(defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	FAILED(buf[8] != 1, "test31 case 9 failed\n");
#endif
	FAILED(buf[9] != -1541, "test31 case 10 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test32(void)
{
	/* Floating point set flags. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();

	sljit_sw buf[16];
	union {
		sljit_d value;
		struct {
			sljit_si value1;
			sljit_si value2;
		} u;
	} dbuf[4];

	if (verbose)
		printf("Run test32\n");

	buf[0] = 5;
	buf[1] = 5;
	buf[2] = 5;
	buf[3] = 5;
	buf[4] = 5;
	buf[5] = 5;
	buf[6] = 5;
	buf[7] = 5;
	buf[8] = 5;
	buf[9] = 5;
	buf[10] = 5;
	buf[11] = 5;
	buf[12] = 5;
	buf[13] = 5;
	buf[14] = 5;
	buf[15] = 5;

	/* Two NaNs */
	dbuf[0].u.value1 = 0x7fffffff;
	dbuf[0].u.value2 = 0x7fffffff;
	dbuf[1].u.value1 = 0x7fffffff;
	dbuf[1].u.value2 = 0x7fffffff;
	dbuf[2].value = -13.0;
	dbuf[3].value = 27.0;

	if (!sljit_is_fpu_available()) {
		if (verbose)
			printf("no fpu available, test32 skipped\n");
		successful_tests++;
		if (compiler)
			sljit_free_compiler(compiler);
		return;
	}

	FAILED(!compiler, "cannot create compiler\n");
	SLJIT_ASSERT(sizeof(sljit_d) == 8 && sizeof(sljit_si) == 4 && sizeof(dbuf[0]) == 8);

	sljit_emit_enter(compiler, 0, 2, 1, 2, 4, 0, 0);

	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S1), 0);
	sljit_emit_fop1(compiler, SLJIT_DCMP | SLJIT_SET_E, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_d), SLJIT_FR0, 0);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_d));
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_D_UNORDERED);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_D_ORDERED);

	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_d));
	sljit_emit_fop1(compiler, SLJIT_DCMP | SLJIT_SET_E | SLJIT_SET_S, SLJIT_FR1, 0, SLJIT_FR2, 0);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_D_UNORDERED);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_D_ORDERED);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_D_LESS);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_D_GREATER_EQUAL);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_D_GREATER);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_D_LESS_EQUAL);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw), SLJIT_D_EQUAL);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_sw), SLJIT_D_NOT_EQUAL);

	sljit_emit_fop1(compiler, SLJIT_DCMP | SLJIT_SET_E, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_d));
	sljit_emit_fop2(compiler, SLJIT_DADD, SLJIT_FR3, 0, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_d));
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 10 * sizeof(sljit_sw), SLJIT_D_UNORDERED);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 11 * sizeof(sljit_sw), SLJIT_D_EQUAL);

	sljit_emit_fop1(compiler, SLJIT_DCMP | SLJIT_SET_S, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_d), SLJIT_FR0, 0);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 12 * sizeof(sljit_sw), SLJIT_D_ORDERED);

	sljit_emit_fop1(compiler, SLJIT_DCMP | SLJIT_SET_S, SLJIT_FR3, 0, SLJIT_FR2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_UB, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S1), 0);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 13 * sizeof(sljit_sw), SLJIT_D_UNORDERED);

	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func2((sljit_sw)&buf, (sljit_sw)&dbuf);

	FAILED(buf[0] != 1, "test32 case 1 failed\n");
	FAILED(buf[1] != 2, "test32 case 2 failed\n");
	FAILED(buf[2] != 2, "test32 case 3 failed\n");
	FAILED(buf[3] != 1, "test32 case 4 failed\n");
	FAILED(buf[4] != 1, "test32 case 5 failed\n");
	FAILED(buf[5] != 2, "test32 case 6 failed\n");
	FAILED(buf[6] != 2, "test32 case 7 failed\n");
	FAILED(buf[7] != 1, "test32 case 8 failed\n");
	FAILED(buf[8] != 2, "test32 case 9 failed\n");
	FAILED(buf[9] != 1, "test32 case 10 failed\n");
	FAILED(buf[10] != 2, "test32 case 11 failed\n");
	FAILED(buf[11] != 1, "test32 case 12 failed\n");
	FAILED(buf[12] != 2, "test32 case 13 failed\n");
	FAILED(buf[13] != 1, "test32 case 14 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test33(void)
{
	/* Test keep flags. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_sw buf[7];
#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	sljit_sw big_word = SLJIT_W(0x8000000000000003);
#else
	sljit_sw big_word = 0x80000003;
#endif

	if (verbose)
		printf("Run test33\n");

	buf[0] = 3;
	buf[1] = 3;
	buf[2] = 3;
	buf[3] = 3;
	buf[4] = 3;
	buf[5] = 3;
	buf[6] = 3;

	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, 1, 3, 3, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, big_word);
	sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_SET_E | SLJIT_SET_C, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, big_word);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 8);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_KEEP_FLAGS, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 8);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_UNUSED, 0, SLJIT_NOT_ZERO);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op2(compiler, SLJIT_ADDC | SLJIT_KEEP_FLAGS, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op2(compiler, SLJIT_ADDC, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_R0, 0, SLJIT_IMM, 7);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 8);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_IMM, 8);
	sljit_emit_op2(compiler, SLJIT_SHL | SLJIT_KEEP_FLAGS, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw));
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_UNUSED, 0, SLJIT_EQUAL);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_IMM, 0x124);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 2);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_KEEP_FLAGS, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op_flags(compiler, SLJIT_OR, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_NOT_EQUAL);

	/* PowerPC specific test. */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -1);
	sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_SET_C, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 2);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x80);
	sljit_emit_op2(compiler, SLJIT_ASHR | SLJIT_KEEP_FLAGS, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0);
	sljit_emit_op2(compiler, SLJIT_ADDC, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_R1, 0, SLJIT_IMM, 0);

	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);

	FAILED(buf[0] != 6, "test33 case 1 failed\n");
	FAILED(buf[1] != 1, "test33 case 2 failed\n");
	FAILED(buf[2] != 8, "test33 case 3 failed\n");
	FAILED(buf[3] != 16, "test33 case 4 failed\n");
	FAILED(buf[4] != 1, "test33 case 5 failed\n");
	FAILED(buf[5] != 0x125, "test33 case 6 failed\n");
	FAILED(buf[6] != 1, "test33 case 7 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test34(void)
{
	/* Test fast calls. */
	executable_code codeA;
	executable_code codeB;
	executable_code codeC;
	executable_code codeD;
	executable_code codeE;
	executable_code codeF;
	struct sljit_compiler* compiler;
	struct sljit_jump *jump;
	struct sljit_label* label;
	sljit_uw addr;
	sljit_p buf[2];

	if (verbose)
		printf("Run test34\n");

	buf[0] = 0;
	buf[1] = 0;

	/* A */
	compiler = sljit_create_compiler();
	FAILED(!compiler, "cannot create compiler\n");
	sljit_set_context(compiler, 0, 1, 5, 5, 0, 0, 2 * sizeof(sljit_p));

	sljit_emit_fast_enter(compiler, SLJIT_R1, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 4);
	sljit_emit_fast_return(compiler, SLJIT_R1, 0);

	codeA.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	/* B */
	compiler = sljit_create_compiler();
	FAILED(!compiler, "cannot create compiler\n");
	sljit_set_context(compiler, 0, 1, 5, 5, 0, 0, 2 * sizeof(sljit_p));

	sljit_emit_fast_enter(compiler, SLJIT_R4, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 6);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, SLJIT_FUNC_OFFSET(codeA.code));
	sljit_emit_ijump(compiler, SLJIT_FAST_CALL, SLJIT_R1, 0);
	sljit_emit_fast_return(compiler, SLJIT_R4, 0);

	codeB.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	/* C */
	compiler = sljit_create_compiler();
	FAILED(!compiler, "cannot create compiler\n");
	sljit_set_context(compiler, 0, 1, 5, 5, 0, 0, 2 * sizeof(sljit_p));

	sljit_emit_fast_enter(compiler, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_p));
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 8);
	jump = sljit_emit_jump(compiler, SLJIT_REWRITABLE_JUMP | SLJIT_FAST_CALL);
	sljit_set_target(jump, SLJIT_FUNC_OFFSET(codeB.code));
	sljit_emit_fast_return(compiler, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_p));

	codeC.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	/* D */
	compiler = sljit_create_compiler();
	FAILED(!compiler, "cannot create compiler\n");
	sljit_set_context(compiler, 0, 1, 5, 5, 0, 0, 2 * sizeof(sljit_p));

	sljit_emit_fast_enter(compiler, SLJIT_MEM1(SLJIT_SP), 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 10);
	sljit_emit_ijump(compiler, SLJIT_FAST_CALL, SLJIT_IMM, SLJIT_FUNC_OFFSET(codeC.code));
	sljit_emit_fast_return(compiler, SLJIT_MEM1(SLJIT_SP), 0);

	codeD.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	/* E */
	compiler = sljit_create_compiler();
	FAILED(!compiler, "cannot create compiler\n");
	sljit_set_context(compiler, 0, 1, 5, 5, 0, 0, 2 * sizeof(sljit_p));

	sljit_emit_fast_enter(compiler, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 12);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_p), SLJIT_IMM, SLJIT_FUNC_OFFSET(codeD.code));
	sljit_emit_ijump(compiler, SLJIT_FAST_CALL, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_p));
	sljit_emit_fast_return(compiler, SLJIT_MEM1(SLJIT_S0), 0);

	codeE.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	/* F */
	compiler = sljit_create_compiler();
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, 1, 5, 5, 0, 0, 2 * sizeof(sljit_p));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_ijump(compiler, SLJIT_FAST_CALL, SLJIT_IMM, SLJIT_FUNC_OFFSET(codeE.code));
	label = sljit_emit_label(compiler);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_R0, 0);

	codeF.code = sljit_generate_code(compiler);
	CHECK(compiler);
	addr = sljit_get_label_addr(label);
	sljit_free_compiler(compiler);

	FAILED(codeF.func1((sljit_sw)&buf) != 40, "test34 case 1 failed\n");
	FAILED(buf[0] != addr - SLJIT_RETURN_ADDRESS_OFFSET, "test34 case 2 failed\n");

	sljit_free_code(codeA.code);
	sljit_free_code(codeB.code);
	sljit_free_code(codeC.code);
	sljit_free_code(codeD.code);
	sljit_free_code(codeE.code);
	sljit_free_code(codeF.code);
	successful_tests++;
}

static void test35(void)
{
	/* More complicated tests for fast calls. */
	executable_code codeA;
	executable_code codeB;
	executable_code codeC;
	struct sljit_compiler* compiler;
	struct sljit_jump *jump;
	struct sljit_label* label;
	sljit_uw return_addr, jump_addr;
	sljit_p buf[1];

	if (verbose)
		printf("Run test35\n");

	buf[0] = 0;

	/* A */
	compiler = sljit_create_compiler();
	FAILED(!compiler, "cannot create compiler\n");
	sljit_set_context(compiler, 0, 0, 2, 2, 0, 0, 0);

	sljit_emit_fast_enter(compiler, SLJIT_MEM0(), (sljit_sw)&buf[0]);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 5);
	jump = sljit_emit_jump(compiler, SLJIT_REWRITABLE_JUMP | SLJIT_FAST_CALL);
	sljit_set_target(jump, 0);
	label = sljit_emit_label(compiler);
	sljit_emit_fast_return(compiler, SLJIT_MEM0(), (sljit_sw)&buf[0]);

	codeA.code = sljit_generate_code(compiler);
	CHECK(compiler);
	return_addr = sljit_get_label_addr(label) - SLJIT_RETURN_ADDRESS_OFFSET;
	jump_addr = sljit_get_jump_addr(jump);
	sljit_free_compiler(compiler);

	/* B */
	compiler = sljit_create_compiler();
	FAILED(!compiler, "cannot create compiler\n");
	sljit_set_context(compiler, 0, 0, 2, 2, 0, 0, 0);

	sljit_emit_fast_enter(compiler, SLJIT_UNUSED, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 7);
	sljit_emit_fast_return(compiler, SLJIT_IMM, return_addr);

	codeB.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);
	sljit_set_jump_addr(jump_addr, SLJIT_FUNC_OFFSET(codeB.code));

	/* C */
	compiler = sljit_create_compiler();
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, 0, 2, 2, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_ijump(compiler, SLJIT_FAST_CALL, SLJIT_IMM, SLJIT_FUNC_OFFSET(codeA.code));
	label = sljit_emit_label(compiler);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_R0, 0);

	codeC.code = sljit_generate_code(compiler);
	CHECK(compiler);
	return_addr = sljit_get_label_addr(label);
	sljit_free_compiler(compiler);

	FAILED(codeC.func0() != 12, "test35 case 1 failed\n");
	FAILED(buf[0] != return_addr - SLJIT_RETURN_ADDRESS_OFFSET, "test35 case 2 failed\n");

	sljit_free_code(codeA.code);
	sljit_free_code(codeB.code);
	sljit_free_code(codeC.code);
	successful_tests++;
}

static sljit_si cmp_test(struct sljit_compiler *compiler, sljit_si type, sljit_si src1, sljit_sw src1w, sljit_si src2, sljit_sw src2w)
{
	/* 2 = true, 1 = false */
	struct sljit_jump* jump;
	struct sljit_label* label;

	if (sljit_emit_op1(compiler, SLJIT_MOVU_UB, SLJIT_MEM1(SLJIT_S0), 1, SLJIT_IMM, 2))
		return compiler->error;
	jump = sljit_emit_cmp(compiler, type, src1, src1w, src2, src2w);
	if (!jump)
		return compiler->error;
	if (sljit_emit_op1(compiler, SLJIT_MOV_UB, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_IMM, 1))
		return compiler->error;
	label = sljit_emit_label(compiler);
	if (!label)
		return compiler->error;
	sljit_set_label(jump, label);
	return SLJIT_SUCCESS;
}

#define TEST_CASES	(7 + 10 + 12 + 11 + 4)
static void test36(void)
{
	/* Compare instruction. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();

	sljit_sb buf[TEST_CASES];
	sljit_sb compare_buf[TEST_CASES] = {
		1, 1, 2, 2, 1, 2, 2,
		1, 1, 2, 2, 2, 1, 2, 2, 1, 1,
		2, 2, 2, 1, 2, 2, 2, 2, 1, 1, 2, 2,
		2, 1, 2, 1, 1, 1, 2, 1, 2, 1, 2,
		2, 1, 1, 2
	};
	sljit_sw data[4];
	sljit_si i;

	if (verbose)
		printf("Run test36\n");

	FAILED(!compiler, "cannot create compiler\n");
	for (i = 0; i < TEST_CASES; ++i)
		buf[i] = 100;
	data[0] = 32;
	data[1] = -9;
	data[2] = 43;
	data[3] = -13;

	sljit_emit_enter(compiler, 0, 2, 3, 2, 0, 0, 0);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_S0, 0, SLJIT_S0, 0, SLJIT_IMM, 1);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 13);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 15);
	cmp_test(compiler, SLJIT_EQUAL, SLJIT_IMM, 9, SLJIT_R0, 0);
	cmp_test(compiler, SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 3);
	cmp_test(compiler, SLJIT_EQUAL, SLJIT_MEM2(SLJIT_S1, SLJIT_R0), SLJIT_WORD_SHIFT, SLJIT_IMM, -13);
	cmp_test(compiler, SLJIT_NOT_EQUAL, SLJIT_IMM, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	cmp_test(compiler, SLJIT_NOT_EQUAL | SLJIT_REWRITABLE_JUMP, SLJIT_IMM, 0, SLJIT_R0, 0);
	cmp_test(compiler, SLJIT_EQUAL, SLJIT_MEM2(SLJIT_S1, SLJIT_R0), SLJIT_WORD_SHIFT, SLJIT_MEM2(SLJIT_S1, SLJIT_R0), SLJIT_WORD_SHIFT);
	cmp_test(compiler, SLJIT_EQUAL | SLJIT_REWRITABLE_JUMP, SLJIT_R0, 0, SLJIT_IMM, 0);

	cmp_test(compiler, SLJIT_SIG_LESS, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -8);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0);
	cmp_test(compiler, SLJIT_SIG_GREATER, SLJIT_R0, 0, SLJIT_IMM, 0);
	cmp_test(compiler, SLJIT_SIG_LESS_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0);
	cmp_test(compiler, SLJIT_SIG_LESS | SLJIT_REWRITABLE_JUMP, SLJIT_R0, 0, SLJIT_IMM, 0);
	cmp_test(compiler, SLJIT_SIG_GREATER_EQUAL, SLJIT_R1, 0, SLJIT_IMM, 0);
	cmp_test(compiler, SLJIT_SIG_GREATER, SLJIT_IMM, 0, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_sw));
	cmp_test(compiler, SLJIT_SIG_LESS_EQUAL, SLJIT_IMM, 0, SLJIT_R1, 0);
	cmp_test(compiler, SLJIT_SIG_LESS, SLJIT_IMM, 0, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_sw));
	cmp_test(compiler, SLJIT_SIG_LESS, SLJIT_IMM, 0, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_sw));
	cmp_test(compiler, SLJIT_SIG_LESS | SLJIT_REWRITABLE_JUMP, SLJIT_IMM, 0, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_sw));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 8);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0);
	cmp_test(compiler, SLJIT_LESS, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_sw));
	cmp_test(compiler, SLJIT_GREATER_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 8);
	cmp_test(compiler, SLJIT_LESS, SLJIT_R0, 0, SLJIT_IMM, -10);
	cmp_test(compiler, SLJIT_LESS, SLJIT_R0, 0, SLJIT_IMM, 8);
	cmp_test(compiler, SLJIT_GREATER_EQUAL, SLJIT_IMM, 8, SLJIT_R1, 0);
	cmp_test(compiler, SLJIT_GREATER_EQUAL | SLJIT_REWRITABLE_JUMP, SLJIT_IMM, 8, SLJIT_R1, 0);
	cmp_test(compiler, SLJIT_GREATER, SLJIT_IMM, 8, SLJIT_R1, 0);
	cmp_test(compiler, SLJIT_LESS_EQUAL, SLJIT_IMM, 7, SLJIT_R0, 0);
	cmp_test(compiler, SLJIT_GREATER, SLJIT_IMM, 1, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_sw));
	cmp_test(compiler, SLJIT_LESS_EQUAL, SLJIT_R0, 0, SLJIT_R1, 0);
	cmp_test(compiler, SLJIT_GREATER, SLJIT_R0, 0, SLJIT_R1, 0);
	cmp_test(compiler, SLJIT_GREATER | SLJIT_REWRITABLE_JUMP, SLJIT_R0, 0, SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -3);
	cmp_test(compiler, SLJIT_SIG_LESS, SLJIT_R0, 0, SLJIT_R1, 0);
	cmp_test(compiler, SLJIT_SIG_GREATER_EQUAL, SLJIT_R0, 0, SLJIT_R1, 0);
	cmp_test(compiler, SLJIT_SIG_LESS, SLJIT_R0, 0, SLJIT_IMM, -1);
	cmp_test(compiler, SLJIT_SIG_GREATER_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 1);
	cmp_test(compiler, SLJIT_SIG_LESS, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_IMM, -1);
	cmp_test(compiler, SLJIT_SIG_LESS | SLJIT_REWRITABLE_JUMP, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_IMM, -1);
	cmp_test(compiler, SLJIT_SIG_LESS_EQUAL, SLJIT_R0, 0, SLJIT_R1, 0);
	cmp_test(compiler, SLJIT_SIG_GREATER, SLJIT_R0, 0, SLJIT_R1, 0);
	cmp_test(compiler, SLJIT_SIG_LESS_EQUAL, SLJIT_IMM, -4, SLJIT_R0, 0);
	cmp_test(compiler, SLJIT_SIG_GREATER, SLJIT_IMM, -1, SLJIT_R1, 0);
	cmp_test(compiler, SLJIT_SIG_GREATER | SLJIT_REWRITABLE_JUMP, SLJIT_R1, 0, SLJIT_IMM, -1);

#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(0xf00000004));
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R1, 0, SLJIT_R0, 0);
	cmp_test(compiler, SLJIT_I_LESS, SLJIT_R1, 0, SLJIT_IMM, 5);
	cmp_test(compiler, SLJIT_LESS, SLJIT_R0, 0, SLJIT_IMM, 5);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(0xff0000004));
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R1, 0, SLJIT_R0, 0);
	cmp_test(compiler, SLJIT_I_SIG_GREATER, SLJIT_R1, 0, SLJIT_IMM, 5);
	cmp_test(compiler, SLJIT_SIG_GREATER, SLJIT_R0, 0, SLJIT_IMM, 5);
#else
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R0, 0, SLJIT_IMM, 4);
	cmp_test(compiler, SLJIT_I_LESS, SLJIT_R0, 0, SLJIT_IMM, 5);
	cmp_test(compiler, SLJIT_I_GREATER, SLJIT_R0, 0, SLJIT_IMM, 5);
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R0, 0, SLJIT_IMM, 0xf0000004);
	cmp_test(compiler, SLJIT_I_SIG_GREATER, SLJIT_R0, 0, SLJIT_IMM, 5);
	cmp_test(compiler, SLJIT_I_SIG_LESS, SLJIT_R0, 0, SLJIT_IMM, 5);
#endif

	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func2((sljit_sw)&buf, (sljit_sw)&data);

	for (i = 0; i < TEST_CASES; ++i)
		if (SLJIT_UNLIKELY(buf[i] != compare_buf[i])) {
			printf("test36 case %d failed\n", i + 1);
			return;
		}

	sljit_free_code(code.code);
	successful_tests++;
}
#undef TEST_CASES

#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
#define BITN(n) (SLJIT_W(1) << (63 - (n)))
#define RESN(n) (n)
#else
#define BITN(n) (1 << (31 - ((n) & 0x1f)))
#define RESN(n) ((n) & 0x1f)
#endif

static void test37(void)
{
	/* Test count leading zeroes. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_sw buf[15];
	sljit_si ibuf[2];
	sljit_si i;

	if (verbose)
		printf("Run test37\n");

	FAILED(!compiler, "cannot create compiler\n");

	for (i = 0; i < 15; i++)
		buf[i] = -1;
	buf[3] = 0;
	buf[7] = BITN(13);
	ibuf[0] = -1;
	ibuf[1] = -1;
	sljit_emit_enter(compiler, 0, 2, 1, 2, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, BITN(27));
	sljit_emit_op1(compiler, SLJIT_CLZ, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_CLZ | SLJIT_SET_E, SLJIT_R0, 0, SLJIT_IMM, BITN(47));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_UNUSED, 0, SLJIT_ZERO);
	sljit_emit_op1(compiler, SLJIT_CLZ, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_CLZ | SLJIT_SET_E, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_UNUSED, 0, SLJIT_ZERO);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_ICLZ, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_CLZ | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_R0, 0);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_UNUSED, 0, SLJIT_ZERO);
	sljit_emit_op1(compiler, SLJIT_CLZ | SLJIT_KEEP_FLAGS, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw), SLJIT_UNUSED, 0, SLJIT_NOT_ZERO);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, BITN(58));
	sljit_emit_op1(compiler, SLJIT_CLZ, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_CLZ | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_MEM1(SLJIT_S0), 10 * sizeof(sljit_sw));
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 10 * sizeof(sljit_sw), SLJIT_UNUSED, 0, SLJIT_NOT_ZERO);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_CLZ, SLJIT_MEM1(SLJIT_S0), 11 * sizeof(sljit_sw), SLJIT_R0, 0);
#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(0xff08a00000));
#else
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R0, 0, SLJIT_IMM, 0x08a00000);
#endif
	sljit_emit_op1(compiler, SLJIT_ICLZ, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_si), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_ICLZ, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 12 * sizeof(sljit_sw), SLJIT_R0, 0);
#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(0xffc8a00000));
#else
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R0, 0, SLJIT_IMM, 0xc8a00000);
#endif
	sljit_emit_op1(compiler, SLJIT_ICLZ | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_R0, 0);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 13 * sizeof(sljit_sw), SLJIT_UNUSED, 0, SLJIT_I_ZERO);
	sljit_emit_op1(compiler, SLJIT_ICLZ | SLJIT_SET_E, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 14 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func2((sljit_sw)&buf, (sljit_sw)&ibuf);
	FAILED(buf[0] != RESN(27), "test37 case 1 failed\n");
	FAILED(buf[1] != RESN(47), "test37 case 2 failed\n");
	FAILED(buf[2] != 0, "test37 case 3 failed\n");
#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	FAILED(buf[3] != 64, "test37 case 4 failed\n");
#else
	FAILED(buf[3] != 32, "test37 case 4 failed\n");
#endif
	FAILED(buf[4] != 1, "test37 case 5 failed\n");
	FAILED(buf[5] != 0, "test37 case 6 failed\n");
	FAILED(ibuf[0] != 32, "test37 case 7 failed\n");
	FAILED(buf[6] != 1, "test37 case 8 failed\n");
	FAILED(buf[7] != RESN(13), "test37 case 9 failed\n");
#if !(defined SLJIT_CONFIG_SPARC_32 && SLJIT_CONFIG_SPARC_32)
	FAILED(buf[8] != 0, "test37 case 10 failed\n");
#endif
	FAILED(buf[9] != RESN(58), "test37 case 11 failed\n");
	FAILED(buf[10] != 0, "test37 case 12 failed\n");
#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	FAILED(buf[11] != 64, "test37 case 13 failed\n");
#else
	FAILED(buf[11] != 32, "test37 case 13 failed\n");
#endif
	FAILED(ibuf[1] != 4, "test37 case 14 failed\n");
	FAILED((buf[12] & 0xffffffff) != 4, "test37 case 15 failed\n");
	FAILED(buf[13] != 1, "test37 case 16 failed\n");
	FAILED((buf[14] & 0xffffffff) != 0, "test37 case 17 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}
#undef BITN
#undef RESN

static void test38(void)
{
#if (defined SLJIT_UTIL_STACK && SLJIT_UTIL_STACK)
	/* Test stack utility. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	struct sljit_jump* alloc_fail;
	struct sljit_jump* alloc2_fail;
	struct sljit_jump* alloc3_fail;
	struct sljit_jump* jump;
	struct sljit_label* label;

	if (verbose)
		printf("Run test38\n");

	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, 0, 2, 1, 0, 0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 8192);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 65536);
	sljit_emit_ijump(compiler, SLJIT_CALL2, SLJIT_IMM, SLJIT_FUNC_OFFSET(sljit_allocate_stack));
	alloc_fail = sljit_emit_cmp(compiler, SLJIT_EQUAL, SLJIT_RETURN_REG, 0, SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_RETURN_REG, 0);

	/* Write 8k data. */
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), SLJIT_OFFSETOF(struct sljit_stack, base), SLJIT_IMM, sizeof(sljit_sw));
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, 8192);
	label = sljit_emit_label(compiler);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_sw), SLJIT_IMM, -1);
	jump = sljit_emit_cmp(compiler, SLJIT_LESS, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_set_label(jump, label);

	/* Grow stack. */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_S0), SLJIT_OFFSETOF(struct sljit_stack, base), SLJIT_IMM, 65536);
	sljit_emit_ijump(compiler, SLJIT_CALL2, SLJIT_IMM, SLJIT_FUNC_OFFSET(sljit_stack_resize));
	alloc2_fail = sljit_emit_cmp(compiler, SLJIT_NOT_EQUAL, SLJIT_RETURN_REG, 0, SLJIT_IMM, 0);

	/* Write 64k data. */
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), SLJIT_OFFSETOF(struct sljit_stack, base), SLJIT_IMM, sizeof(sljit_sw));
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, 65536);
	label = sljit_emit_label(compiler);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_sw), SLJIT_IMM, -1);
	jump = sljit_emit_cmp(compiler, SLJIT_LESS, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_set_label(jump, label);

	/* Shrink stack. */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_S0), SLJIT_OFFSETOF(struct sljit_stack, base), SLJIT_IMM, 32768);
	sljit_emit_ijump(compiler, SLJIT_CALL2, SLJIT_IMM, SLJIT_FUNC_OFFSET(sljit_stack_resize));
	alloc3_fail = sljit_emit_cmp(compiler, SLJIT_NOT_EQUAL, SLJIT_RETURN_REG, 0, SLJIT_IMM, 0);

	/* Write 32k data. */
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), SLJIT_OFFSETOF(struct sljit_stack, base), SLJIT_IMM, sizeof(sljit_sw));
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_S0), SLJIT_OFFSETOF(struct sljit_stack, limit), SLJIT_IMM, sizeof(sljit_sw));
	label = sljit_emit_label(compiler);
	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_sw), SLJIT_IMM, -1);
	jump = sljit_emit_cmp(compiler, SLJIT_LESS, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_set_label(jump, label);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
	sljit_emit_ijump(compiler, SLJIT_CALL1, SLJIT_IMM, SLJIT_FUNC_OFFSET(sljit_free_stack));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0, SLJIT_IMM, 1);
	label = sljit_emit_label(compiler);
	sljit_set_label(alloc_fail, label);
	sljit_set_label(alloc2_fail, label);
	sljit_set_label(alloc3_fail, label);
	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	/* Just survive this. */
	FAILED(code.func0() != 1, "test38 case 1 failed\n");

	sljit_free_code(code.code);
#endif
	successful_tests++;
}

static void test39(void)
{
	/* Test error handling. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	struct sljit_jump* jump;

	if (verbose)
		printf("Run test39\n");

	FAILED(!compiler, "cannot create compiler\n");

	/* Such assignment should never happen in a regular program. */
	compiler->error = -3967;

	SLJIT_ASSERT(sljit_emit_enter(compiler, 0, 2, 5, 5, 6, 0, 32) == -3967);
	SLJIT_ASSERT(sljit_emit_return(compiler, SLJIT_MOV, SLJIT_R1, 0) == -3967);
	SLJIT_ASSERT(sljit_emit_op0(compiler, SLJIT_NOP) == -3967);
	SLJIT_ASSERT(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM2(SLJIT_R0, SLJIT_R1), 1) == -3967);
	SLJIT_ASSERT(sljit_emit_op2(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R0), 64, SLJIT_MEM1(SLJIT_S0), -64) == -3967);
	SLJIT_ASSERT(sljit_emit_fop1(compiler, SLJIT_DABS, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_R1), 0) == -3967);
	SLJIT_ASSERT(sljit_emit_fop2(compiler, SLJIT_DDIV, SLJIT_FR2, 0, SLJIT_MEM2(SLJIT_R0, SLJIT_S0), 0, SLJIT_FR2, 0) == -3967);
	SLJIT_ASSERT(!sljit_emit_label(compiler));
	jump = sljit_emit_jump(compiler, SLJIT_CALL3);
	SLJIT_ASSERT(!jump);
	sljit_set_label(jump, (struct sljit_label*)0x123450);
	sljit_set_target(jump, 0x123450);
	jump = sljit_emit_cmp(compiler, SLJIT_SIG_LESS_EQUAL, SLJIT_R0, 0, SLJIT_R1, 0);
	SLJIT_ASSERT(!jump);
	SLJIT_ASSERT(sljit_emit_ijump(compiler, SLJIT_JUMP, SLJIT_MEM1(SLJIT_R0), 8) == -3967);
	SLJIT_ASSERT(sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_UNUSED, 0, SLJIT_MUL_OVERFLOW) == -3967);
	SLJIT_ASSERT(!sljit_emit_const(compiler, SLJIT_R0, 0, 99));

	SLJIT_ASSERT(!compiler->labels && !compiler->jumps && !compiler->consts);
	SLJIT_ASSERT(!compiler->last_label && !compiler->last_jump && !compiler->last_const);
	SLJIT_ASSERT(!compiler->buf->next && !compiler->buf->used_size);
	SLJIT_ASSERT(!compiler->abuf->next && !compiler->abuf->used_size);

	code.code = sljit_generate_code(compiler);
	FAILED(sljit_get_compiler_error(compiler) != -3967, "test39 case 1 failed\n");
	FAILED(!!code.code, "test39 case 2 failed\n");
	sljit_free_compiler(compiler);
	successful_tests++;
}

static void test40(void)
{
	/* Test emit_op_flags. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_sw buf[10];

	if (verbose)
		printf("Run test40\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = -100;
	buf[1] = -100;
	buf[2] = -100;
	buf[3] = -8;
	buf[4] = -100;
	buf[5] = -100;
	buf[6] = 0;
	buf[7] = 0;
	buf[8] = -100;
	buf[9] = -100;

	sljit_emit_enter(compiler, 0, 1, 3, 4, 0, 0, sizeof(sljit_sw));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -5);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_S, SLJIT_UNUSED, 0, SLJIT_IMM, -6, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0x123456);
	sljit_emit_op_flags(compiler, SLJIT_OR, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_SIG_LESS);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -13);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_IMM, -13, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_IMM, 0);
	sljit_emit_op_flags(compiler, SLJIT_OR | SLJIT_KEEP_FLAGS, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_EQUAL);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_UNUSED, 0, SLJIT_EQUAL);
	sljit_emit_op2(compiler, SLJIT_OR | SLJIT_KEEP_FLAGS, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_MEM1(SLJIT_SP), 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0);
	sljit_emit_op_flags(compiler, SLJIT_OR | SLJIT_SET_E, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_EQUAL);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 2, SLJIT_UNUSED, 0, SLJIT_EQUAL);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -13);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 3);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_S, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op_flags(compiler, SLJIT_OR, SLJIT_MEM2(SLJIT_S0, SLJIT_R1), SLJIT_WORD_SHIFT, SLJIT_MEM2(SLJIT_S0, SLJIT_R1), SLJIT_WORD_SHIFT, SLJIT_SIG_LESS);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -8);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 33);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_U, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, 0);
	sljit_emit_op_flags(compiler, SLJIT_OR | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_UNUSED, 0, SLJIT_GREATER);
	sljit_emit_op_flags(compiler, SLJIT_OR | SLJIT_KEEP_FLAGS, SLJIT_S1, 0, SLJIT_S1, 0, SLJIT_EQUAL);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S3, 0, SLJIT_IMM, 0x88);
	sljit_emit_op_flags(compiler, SLJIT_OR | SLJIT_KEEP_FLAGS, SLJIT_S3, 0, SLJIT_S3, 0, SLJIT_NOT_EQUAL);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 4, SLJIT_S1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 5, SLJIT_S3, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x84);
	sljit_emit_op2(compiler, SLJIT_AND | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_IMM, 0x180, SLJIT_R0, 0);
	sljit_emit_op_flags(compiler, SLJIT_OR | SLJIT_SET_E, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 6, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 6, SLJIT_EQUAL);
	sljit_emit_op_flags(compiler, SLJIT_OR, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 7, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 7, SLJIT_EQUAL);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op_flags(compiler, SLJIT_OR | SLJIT_SET_E, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_NOT_EQUAL);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 8, SLJIT_UNUSED, 0, SLJIT_NOT_EQUAL);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x123456);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_U, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op_flags(compiler, SLJIT_OR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_GREATER);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 9, SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_IMM, 0xbaddead);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_sw)&buf) != 0xbaddead, "test40 case 1 failed\n");
	FAILED(buf[0] != 0x123457, "test40 case 2 failed\n");
	FAILED(buf[1] != 1, "test40 case 3 failed\n");
	FAILED(buf[2] != 0, "test40 case 4 failed\n");
	FAILED(buf[3] != -7, "test40 case 5 failed\n");
	FAILED(buf[4] != 0, "test40 case 6 failed\n");
	FAILED(buf[5] != 0x89, "test40 case 7 failed\n");
	FAILED(buf[6] != 0, "test40 case 8 failed\n");
	FAILED(buf[7] != 1, "test40 case 9 failed\n");
	FAILED(buf[8] != 1, "test40 case 10 failed\n");
	FAILED(buf[9] != 0x123457, "test40 case 11 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test41(void)
{
	/* Test inline assembly. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_si i;
	sljit_d buf[3];
#if (defined SLJIT_CONFIG_X86_32 && SLJIT_CONFIG_X86_32)
	sljit_ub inst[16];
#elif (defined SLJIT_CONFIG_X86_64 && SLJIT_CONFIG_X86_64)
	sljit_ub inst[16];
	sljit_si reg;
#else
	sljit_ui inst;
#endif

	if (verbose)
		printf("Run test41\n");

	for (i = 0; i < SLJIT_NUMBER_OF_REGISTERS; i++) {
#if (defined SLJIT_CONFIG_X86_32 && SLJIT_CONFIG_X86_32)
		if (SLJIT_R(i) >= SLJIT_R3 && SLJIT_R(i) <= SLJIT_R6) {
			SLJIT_ASSERT(sljit_get_register_index(SLJIT_R(i)) == -1);
			continue;
		}
#endif
		SLJIT_ASSERT(sljit_get_register_index(SLJIT_R(i)) >= 0 && sljit_get_register_index(SLJIT_R(i)) < 64);
	}

	FAILED(!compiler, "cannot create compiler\n");
	sljit_emit_enter(compiler, 0, 2, 3, 3, 0, 0, 0);

	/* Returns with the sum of SLJIT_S0 and SLJIT_S1. */
#if (defined SLJIT_CONFIG_X86_32 && SLJIT_CONFIG_X86_32)
	/* lea SLJIT_RETURN_REG, [SLJIT_S0, SLJIT_S1] */
	inst[0] = 0x48;
	inst[1] = 0x8d;
	inst[2] = 0x04 | ((sljit_get_register_index(SLJIT_RETURN_REG) & 0x7) << 3);
	inst[3] = (sljit_get_register_index(SLJIT_S0) & 0x7)
		| ((sljit_get_register_index(SLJIT_S1) & 0x7) << 3);
	sljit_emit_op_custom(compiler, inst, 4);
#elif (defined SLJIT_CONFIG_X86_64 && SLJIT_CONFIG_X86_64)
	/* lea SLJIT_RETURN_REG, [SLJIT_S0, SLJIT_S1] */
	inst[0] = 0x48; /* REX_W */
	inst[1] = 0x8d;
	inst[2] = 0x04;
	reg = sljit_get_register_index(SLJIT_RETURN_REG);
	inst[2] |= ((reg & 0x7) << 3);
	if (reg > 7)
		inst[0] |= 0x04; /* REX_R */
	reg = sljit_get_register_index(SLJIT_S0);
	inst[3] = reg & 0x7;
	if (reg > 7)
		inst[0] |= 0x01; /* REX_B */
	reg = sljit_get_register_index(SLJIT_S1);
	inst[3] |= (reg & 0x7) << 3;
	if (reg > 7)
		inst[0] |= 0x02; /* REX_X */
	sljit_emit_op_custom(compiler, inst, 4);
#elif (defined SLJIT_CONFIG_ARM_V5 && SLJIT_CONFIG_ARM_V5) || (defined SLJIT_CONFIG_ARM_V7 && SLJIT_CONFIG_ARM_V7)
	/* add rd, rn, rm */
	inst = 0xe0800000 | (sljit_get_register_index(SLJIT_RETURN_REG) << 12)
		| (sljit_get_register_index(SLJIT_S0) << 16)
		| sljit_get_register_index(SLJIT_S1);
	sljit_emit_op_custom(compiler, &inst, sizeof(sljit_ui));
#elif (defined SLJIT_CONFIG_ARM_THUMB2 && SLJIT_CONFIG_ARM_THUMB2)
	/* add rd, rn, rm */
	inst = 0xeb000000 | (sljit_get_register_index(SLJIT_RETURN_REG) << 8)
		| (sljit_get_register_index(SLJIT_S0) << 16)
		| sljit_get_register_index(SLJIT_S1);
	sljit_emit_op_custom(compiler, &inst, sizeof(sljit_ui));
#elif (defined SLJIT_CONFIG_ARM_64 && SLJIT_CONFIG_ARM_64)
	/* add rd, rn, rm */
	inst = 0x8b000000 | sljit_get_register_index(SLJIT_RETURN_REG)
		| (sljit_get_register_index(SLJIT_S0) << 5)
		| (sljit_get_register_index(SLJIT_S1) << 16);
	sljit_emit_op_custom(compiler, &inst, sizeof(sljit_ui));
#elif (defined SLJIT_CONFIG_PPC && SLJIT_CONFIG_PPC)
	/* add rD, rA, rB */
	inst = (31 << 26) | (266 << 1) | (sljit_get_register_index(SLJIT_RETURN_REG) << 21)
		| (sljit_get_register_index(SLJIT_S0) << 16)
		| (sljit_get_register_index(SLJIT_S1) << 11);
	sljit_emit_op_custom(compiler, &inst, sizeof(sljit_ui));
#elif (defined SLJIT_CONFIG_MIPS_32 && SLJIT_CONFIG_MIPS_32)
	/* addu rd, rs, rt */
	inst = 33 | (sljit_get_register_index(SLJIT_RETURN_REG) << 11)
		| (sljit_get_register_index(SLJIT_S0) << 21)
		| (sljit_get_register_index(SLJIT_S1) << 16);
	sljit_emit_op_custom(compiler, &inst, sizeof(sljit_ui));
#elif (defined SLJIT_CONFIG_MIPS_64 && SLJIT_CONFIG_MIPS_64)
	/* daddu rd, rs, rt */
	inst = 45 | (sljit_get_register_index(SLJIT_RETURN_REG) << 11)
		| (sljit_get_register_index(SLJIT_S0) << 21)
		| (sljit_get_register_index(SLJIT_S1) << 16);
	sljit_emit_op_custom(compiler, &inst, sizeof(sljit_ui));
#elif (defined SLJIT_CONFIG_SPARC_32 && SLJIT_CONFIG_SPARC_32)
	/* add rd, rs1, rs2 */
	inst = (0x2 << 30) | (sljit_get_register_index(SLJIT_RETURN_REG) << 25)
		| (sljit_get_register_index(SLJIT_S0) << 14)
		| sljit_get_register_index(SLJIT_S1);
	sljit_emit_op_custom(compiler, &inst, sizeof(sljit_ui));
#else
	inst = 0;
	sljit_emit_op_custom(compiler, &inst, 0);
#endif

	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func2(32, -11) != 21, "test41 case 1 failed\n");
	FAILED(code.func2(1000, 234) != 1234, "test41 case 2 failed\n");
#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	FAILED(code.func2(SLJIT_W(0x20f0a04090c06070), SLJIT_W(0x020f0a04090c0607)) != SLJIT_W(0x22ffaa4499cc6677), "test41 case 3 failed\n");
#endif

	sljit_free_code(code.code);

	if (sljit_is_fpu_available()) {
		buf[0] = 13.5;
		buf[1] = -2.25;
		buf[2] = 0.0;

		compiler = sljit_create_compiler();
		sljit_emit_enter(compiler, 0, 1, 0, 1, 2, 0, 0);
		sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S0), 0);
		sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d));
#if (defined SLJIT_CONFIG_X86_32 && SLJIT_CONFIG_X86_32)
		/* addsd x, xm */
		inst[0] = 0xf2;
		inst[1] = 0x0f;
		inst[2] = 0x58;
		inst[3] = 0xc0 | (sljit_get_float_register_index(SLJIT_FR0) << 3)
			| sljit_get_float_register_index(SLJIT_FR1);
		sljit_emit_op_custom(compiler, inst, 4);
#elif (defined SLJIT_CONFIG_X86_64 && SLJIT_CONFIG_X86_64)
		/* addsd x, xm */
		if (sljit_get_float_register_index(SLJIT_FR0) > 7 || sljit_get_float_register_index(SLJIT_FR1) > 7) {
			inst[0] = 0;
			if (sljit_get_float_register_index(SLJIT_FR0) > 7)
				inst[0] |= 0x04; /* REX_R */
			if (sljit_get_float_register_index(SLJIT_FR1) > 7)
				inst[0] |= 0x01; /* REX_B */
			inst[1] = 0xf2;
			inst[2] = 0x0f;
			inst[3] = 0x58;
			inst[4] = 0xc0 | ((sljit_get_float_register_index(SLJIT_FR0) & 0x7) << 3)
				| (sljit_get_float_register_index(SLJIT_FR1) & 0x7);
			sljit_emit_op_custom(compiler, inst, 5);
		}
		else {
			inst[0] = 0xf2;
			inst[1] = 0x0f;
			inst[2] = 0x58;
			inst[3] = 0xc0 | (sljit_get_float_register_index(SLJIT_FR0) << 3)
				| sljit_get_float_register_index(SLJIT_FR1);
			sljit_emit_op_custom(compiler, inst, 4);
		}
#elif (defined SLJIT_CONFIG_ARM_32 && SLJIT_CONFIG_ARM_32)
		/* vadd.f64 dd, dn, dm */
		inst = 0xee300b00 | ((sljit_get_float_register_index(SLJIT_FR0) >> 1) << 12)
			| ((sljit_get_float_register_index(SLJIT_FR0) >> 1) << 16)
			| (sljit_get_float_register_index(SLJIT_FR1) >> 1);
		sljit_emit_op_custom(compiler, &inst, sizeof(sljit_ui));
#elif (defined SLJIT_CONFIG_ARM_64 && SLJIT_CONFIG_ARM_64)
		/* fadd rd, rn, rm */
		inst = 0x1e602800 | sljit_get_float_register_index(SLJIT_FR0)
			| (sljit_get_float_register_index(SLJIT_FR0) << 5)
			| (sljit_get_float_register_index(SLJIT_FR1) << 16);
		sljit_emit_op_custom(compiler, &inst, sizeof(sljit_ui));
#elif (defined SLJIT_CONFIG_PPC && SLJIT_CONFIG_PPC)
		/* fadd frD, frA, frB */
		inst = (63 << 26) | (21 << 1) | (sljit_get_float_register_index(SLJIT_FR0) << 21)
			| (sljit_get_float_register_index(SLJIT_FR0) << 16)
			| (sljit_get_float_register_index(SLJIT_FR1) << 11);
		sljit_emit_op_custom(compiler, &inst, sizeof(sljit_ui));
#elif (defined SLJIT_CONFIG_MIPS && SLJIT_CONFIG_MIPS)
		/* add.d fd, fs, ft */
		inst = (17 << 26) | (17 << 21) | (sljit_get_float_register_index(SLJIT_FR0) << 6)
			| (sljit_get_float_register_index(SLJIT_FR0) << 11)
			| (sljit_get_float_register_index(SLJIT_FR1) << 16);
		sljit_emit_op_custom(compiler, &inst, sizeof(sljit_ui));
#elif (defined SLJIT_CONFIG_SPARC_32 && SLJIT_CONFIG_SPARC_32)
		/* faddd rd, rs1, rs2 */
		inst = (0x2 << 30) | (0x34 << 19) | (0x42 << 5)
			| (sljit_get_float_register_index(SLJIT_FR0) << 25)
			| (sljit_get_float_register_index(SLJIT_FR0) << 14)
			| sljit_get_float_register_index(SLJIT_FR1);
		sljit_emit_op_custom(compiler, &inst, sizeof(sljit_ui));
#endif
		sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_d), SLJIT_FR0, 0);
		sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

		code.code = sljit_generate_code(compiler);
		CHECK(compiler);
		sljit_free_compiler(compiler);

		code.func1((sljit_sw)&buf);
		FAILED(buf[2] != 11.25, "test41 case 3 failed\n");

		sljit_free_code(code.code);
	}

	successful_tests++;
}

static void test42(void)
{
	/* Test long multiply and division. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_si i;
	sljit_sw buf[7 + 8 + 4];

	if (verbose)
		printf("Run test42\n");

	FAILED(!compiler, "cannot create compiler\n");
	for (i = 0; i < 7 + 8; i++)
		buf[i] = -1;

	sljit_emit_enter(compiler, 0, 1, 5, 5, 0, 0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, -0x1fb308a);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, 0xf50c873);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R4, 0, SLJIT_IMM, 0x8a0475b);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, 0x9dc849b);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, -0x7c69a35);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S3, 0, SLJIT_IMM, 0x5a4d0c4);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S4, 0, SLJIT_IMM, 0x9a3b06d);

#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(-0x5dc4f897b8cd67f5));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, SLJIT_W(0x3f8b5c026cb088df));
	sljit_emit_op0(compiler, SLJIT_LUMUL);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(-0x5dc4f897b8cd67f5));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, SLJIT_W(0x3f8b5c026cb088df));
	sljit_emit_op0(compiler, SLJIT_LSMUL);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 10 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(-0x5dc4f897b8cd67f5));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, SLJIT_W(0x3f8b5c026cb088df));
	sljit_emit_op0(compiler, SLJIT_LUDIV);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 11 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 12 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(-0x5dc4f897b8cd67f5));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, SLJIT_W(0x3f8b5c026cb088df));
	sljit_emit_op0(compiler, SLJIT_LSDIV);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 13 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 14 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(0x5cf783d3cf0a74b0));
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R1, 0, SLJIT_IMM, SLJIT_W(0x3d5df42d03a28fc7));
	sljit_emit_op0(compiler, SLJIT_ILUDIV);
	sljit_emit_op1(compiler, SLJIT_MOV_UI, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_UI, SLJIT_R1, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 15 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 16 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(0x371df5197ba26a28));
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R1, 0, SLJIT_IMM, SLJIT_W(0x46c78a5cfd6a420c));
	sljit_emit_op0(compiler, SLJIT_ILSDIV);
	sljit_emit_op1(compiler, SLJIT_MOV_SI, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_SI, SLJIT_R1, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 17 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 18 * sizeof(sljit_sw), SLJIT_R1, 0);

#else
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -0x58cd67f5);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0x3cb088df);
	sljit_emit_op0(compiler, SLJIT_LUMUL);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -0x58cd67f5);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0x3cb088df);
	sljit_emit_op0(compiler, SLJIT_LSMUL);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 10 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -0x58cd67f5);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0x3cb088df);
	sljit_emit_op0(compiler, SLJIT_LUDIV);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 11 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 12 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -0x58cd67f5);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0x3cb088df);
	sljit_emit_op0(compiler, SLJIT_LSDIV);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 13 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 14 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R0, 0, SLJIT_IMM, 0xcf0a74b0);
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R1, 0, SLJIT_IMM, 0x03a28fc7);
	sljit_emit_op0(compiler, SLJIT_ILUDIV);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 15 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 16 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R0, 0, SLJIT_IMM, 0x7ba26a28);
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)0xfd6a420c);
	sljit_emit_op0(compiler, SLJIT_ILSDIV);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 17 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 18 * sizeof(sljit_sw), SLJIT_R1, 0);
#endif

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R3, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_R4, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_S1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_S2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_S3, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_S4, 0);

	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);

	FAILED(buf[0] != -0x1fb308a, "test42 case 1 failed\n");
	FAILED(buf[1] != 0xf50c873, "test42 case 2 failed\n");
	FAILED(buf[2] != 0x8a0475b, "test42 case 3 failed\n");
	FAILED(buf[3] != 0x9dc849b, "test42 case 4 failed\n");
	FAILED(buf[4] != -0x7c69a35, "test42 case 5 failed\n");
	FAILED(buf[5] != 0x5a4d0c4, "test42 case 6 failed\n");
	FAILED(buf[6] != 0x9a3b06d, "test42 case 7 failed\n");

#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	FAILED(buf[7] != SLJIT_W(-4388959407985636971), "test42 case 8 failed\n");
	FAILED(buf[8] != SLJIT_W(2901680654366567099), "test42 case 9 failed\n");
	FAILED(buf[9] != SLJIT_W(-4388959407985636971), "test42 case 10 failed\n");
	FAILED(buf[10] != SLJIT_W(-1677173957268872740), "test42 case 11 failed\n");
	FAILED(buf[11] != SLJIT_W(2), "test42 case 12 failed\n");
	FAILED(buf[12] != SLJIT_W(2532236178951865933), "test42 case 13 failed\n");
	FAILED(buf[13] != SLJIT_W(-1), "test42 case 14 failed\n");
	FAILED(buf[14] != SLJIT_W(-2177944059851366166), "test42 case 15 failed\n");
#else
	FAILED(buf[7] != -1587000939, "test42 case 8 failed\n");
	FAILED(buf[8] != 665003983, "test42 case 9 failed\n");
	FAILED(buf[9] != -1587000939, "test42 case 10 failed\n");
	FAILED(buf[10] != -353198352, "test42 case 11 failed\n");
	FAILED(buf[11] != 2, "test42 case 12 failed\n");
	FAILED(buf[12] != 768706125, "test42 case 13 failed\n");
	FAILED(buf[13] != -1, "test42 case 14 failed\n");
	FAILED(buf[14] != -471654166, "test42 case 15 failed\n");
#endif

	FAILED(buf[15] != SLJIT_W(56), "test42 case 16 failed\n");
	FAILED(buf[16] != SLJIT_W(58392872), "test42 case 17 failed\n");
	FAILED(buf[17] != SLJIT_W(-47), "test42 case 18 failed\n");
	FAILED(buf[18] != SLJIT_W(35949148), "test42 case 19 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test43(void)
{
	/* Test floating point compare. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	struct sljit_jump* jump;

	union {
		sljit_d value;
		struct {
			sljit_ui value1;
			sljit_ui value2;
		} u;
	} dbuf[4];

	if (verbose)
		printf("Run test43\n");

	if (!sljit_is_fpu_available()) {
		if (verbose)
			printf("no fpu available, test43 skipped\n");
		successful_tests++;
		if (compiler)
			sljit_free_compiler(compiler);
		return;
	}

	FAILED(!compiler, "cannot create compiler\n");

	dbuf[0].value = 12.125;
	/* a NaN */
	dbuf[1].u.value1 = 0x7fffffff;
	dbuf[1].u.value2 = 0x7fffffff;
	dbuf[2].value = -13.5;
	dbuf[3].value = 12.125;

	sljit_emit_enter(compiler, 0, 1, 1, 1, 3, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 2);
	/* dbuf[0] < dbuf[2] -> -2 */
	jump = sljit_emit_fcmp(compiler, SLJIT_D_GREATER_EQUAL, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), SLJIT_DOUBLE_SHIFT);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_IMM, -2);

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S0), 0);
	/* dbuf[0] and dbuf[1] is not NaN -> 5 */
	jump = sljit_emit_fcmp(compiler, SLJIT_D_UNORDERED, SLJIT_MEM0(), (sljit_sw)&dbuf[1], SLJIT_FR1, 0);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_IMM, 5);

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_d));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0, SLJIT_IMM, 11);
	/* dbuf[0] == dbuf[3] -> 11 */
	jump = sljit_emit_fcmp(compiler, SLJIT_D_EQUAL, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_FR2, 0);

	/* else -> -17 */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0, SLJIT_IMM, -17);
	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_sw)&dbuf) != 11, "test43 case 1 failed\n");
	dbuf[3].value = 12;
	FAILED(code.func1((sljit_sw)&dbuf) != -17, "test43 case 2 failed\n");
	dbuf[1].value = 0;
	FAILED(code.func1((sljit_sw)&dbuf) != 5, "test43 case 3 failed\n");
	dbuf[2].value = 20;
	FAILED(code.func1((sljit_sw)&dbuf) != -2, "test43 case 4 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test44(void)
{
	/* Test mov. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	void *buf[5];

	if (verbose)
		printf("Run test44\n");

	FAILED(!compiler, "cannot create compiler\n");

	buf[0] = buf + 2;
	buf[1] = NULL;
	buf[2] = NULL;
	buf[3] = NULL;
	buf[4] = NULL;
	sljit_emit_enter(compiler, 0, 1, 3, 2, 0, 0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV_P, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op1(compiler, SLJIT_MOV_P, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_p), SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, sizeof(sljit_p));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 2);
	sljit_emit_op1(compiler, SLJIT_MOV_P, SLJIT_MEM2(SLJIT_S0, SLJIT_R1), SLJIT_POINTER_SHIFT, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, sizeof(sljit_p));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 3);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_S0, 0);
	sljit_emit_op1(compiler, SLJIT_MOVU_P, SLJIT_MEM2(SLJIT_R2, SLJIT_R1), SLJIT_POINTER_SHIFT, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 2 * sizeof(sljit_p));
	sljit_emit_op1(compiler, SLJIT_MOV_P, SLJIT_MEM1(SLJIT_R2), sizeof(sljit_p), SLJIT_R0, 0);

	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_R0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_sw)&buf) != (sljit_sw)(buf + 2), "test44 case 1 failed\n");
	FAILED(buf[1] != buf + 2, "test44 case 2 failed\n");
	FAILED(buf[2] != buf + 3, "test44 case 3 failed\n");
	FAILED(buf[3] != buf + 4, "test44 case 4 failed\n");
	FAILED(buf[4] != buf + 2, "test44 case 5 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test45(void)
{
	/* Test single precision floating point. */

	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_s buf[12];
	sljit_sw buf2[6];
	struct sljit_jump* jump;

	if (verbose)
		printf("Run test45\n");

	if (!sljit_is_fpu_available()) {
		if (verbose)
			printf("no fpu available, test45 skipped\n");
		successful_tests++;
		if (compiler)
			sljit_free_compiler(compiler);
		return;
	}

	FAILED(!compiler, "cannot create compiler\n");

	buf[0] = 5.5;
	buf[1] = -7.25;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 0;
	buf[5] = 0;
	buf[6] = 0;
	buf[7] = 8.75;
	buf[8] = 0;
	buf[9] = 16.5;
	buf[10] = 0;
	buf[11] = 0;

	buf2[0] = -1;
	buf2[1] = -1;
	buf2[2] = -1;
	buf2[3] = -1;
	buf2[4] = -1;
	buf2[5] = -1;

	sljit_emit_enter(compiler, 0, 2, 3, 2, 6, 0, 0);

	sljit_emit_fop1(compiler, SLJIT_SMOV, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_SMOV, SLJIT_FR5, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_s));
	sljit_emit_fop1(compiler, SLJIT_SNEG, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_s), SLJIT_FR0, 0);
	sljit_emit_fop1(compiler, SLJIT_SABS, SLJIT_FR1, 0, SLJIT_FR5, 0);
	sljit_emit_fop1(compiler, SLJIT_SMOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_s), SLJIT_FR1, 0);
	sljit_emit_fop1(compiler, SLJIT_SABS, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_s), SLJIT_FR5, 0);
	sljit_emit_fop1(compiler, SLJIT_SNEG, SLJIT_FR4, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_SMOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_s), SLJIT_FR4, 0);

	sljit_emit_fop2(compiler, SLJIT_SADD, SLJIT_FR0, 0, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_s));
	sljit_emit_fop1(compiler, SLJIT_SMOV, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_s), SLJIT_FR0, 0);
	sljit_emit_fop2(compiler, SLJIT_SSUB, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_s), SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_s), SLJIT_FR5, 0);
	sljit_emit_fop1(compiler, SLJIT_SMOV, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop2(compiler, SLJIT_SMUL, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_s), SLJIT_FR0, 0, SLJIT_FR0, 0);
	sljit_emit_fop2(compiler, SLJIT_SDIV, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_s), SLJIT_FR0, 0);
	sljit_emit_fop1(compiler, SLJIT_SABS, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_s), SLJIT_FR2, 0);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_IMM, 0x3d0ac);
	sljit_emit_fop1(compiler, SLJIT_SNEG, SLJIT_MEM1(SLJIT_S0), 10 * sizeof(sljit_s), SLJIT_MEM1(SLJIT_R0), 0x3d0ac);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_IMM, 0x3d0ac + sizeof(sljit_s));
	sljit_emit_fop1(compiler, SLJIT_SABS, SLJIT_MEM1(SLJIT_S0), 11 * sizeof(sljit_s), SLJIT_MEM1(SLJIT_R0), -0x3d0ac);

	sljit_emit_fop1(compiler, SLJIT_SMOV, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_SMOV, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_s));
	sljit_emit_fop1(compiler, SLJIT_SCMP | SLJIT_SET_E | SLJIT_SET_S, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S0), 0);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_S_EQUAL);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_sw), SLJIT_S_LESS);
	sljit_emit_fop1(compiler, SLJIT_SCMP | SLJIT_SET_E, SLJIT_FR1, 0, SLJIT_FR2, 0);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_sw), SLJIT_S_EQUAL);
	sljit_emit_fop1(compiler, SLJIT_SCMP | SLJIT_SET_S, SLJIT_FR1, 0, SLJIT_FR2, 0);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_sw), SLJIT_S_GREATER_EQUAL);

	jump = sljit_emit_fcmp(compiler, SLJIT_S_LESS_EQUAL, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_s));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_sw), SLJIT_IMM, 7);
	sljit_set_label(jump, sljit_emit_label(compiler));

	jump = sljit_emit_fcmp(compiler, SLJIT_S_GREATER, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_FR2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 5 * sizeof(sljit_sw), SLJIT_IMM, 6);
	sljit_set_label(jump, sljit_emit_label(compiler));

	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_R0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func2((sljit_sw)&buf, (sljit_sw)&buf2);
	FAILED(buf[2] != -5.5, "test45 case 1 failed\n");
	FAILED(buf[3] != 7.25, "test45 case 2 failed\n");
	FAILED(buf[4] != 7.25, "test45 case 3 failed\n");
	FAILED(buf[5] != -5.5, "test45 case 4 failed\n");
	FAILED(buf[6] != -1.75, "test45 case 5 failed\n");
	FAILED(buf[7] != 16.0, "test45 case 6 failed\n");
	FAILED(buf[8] != 30.25, "test45 case 7 failed\n");
	FAILED(buf[9] != 3, "test45 case 8 failed\n");
	FAILED(buf[10] != -5.5, "test45 case 9 failed\n");
	FAILED(buf[11] != 7.25, "test45 case 10 failed\n");
	FAILED(buf2[0] != 1, "test45 case 11 failed\n");
	FAILED(buf2[1] != 2, "test45 case 12 failed\n");
	FAILED(buf2[2] != 2, "test45 case 13 failed\n");
	FAILED(buf2[3] != 1, "test45 case 14 failed\n");
	FAILED(buf2[4] != 7, "test45 case 15 failed\n");
	FAILED(buf2[5] != -1, "test45 case 16 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test46(void)
{
	/* Test sljit_emit_op_flags with 32 bit operations. */

	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_si buf[24];
	sljit_sw buf2[6];
	sljit_si i;

	if (verbose)
		printf("Run test46\n");

	for (i = 0; i < 24; ++i)
		buf[i] = -17;
	buf[16] = 0;
	for (i = 0; i < 6; ++i)
		buf2[i] = -13;
	buf2[4] = -124;

	sljit_emit_enter(compiler, 0, 2, 3, 3, 0, 0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, -7);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_E | SLJIT_SET_U, SLJIT_UNUSED, 0, SLJIT_R2, 0, SLJIT_IMM, 13);
	sljit_emit_op_flags(compiler, SLJIT_MOV_SI, SLJIT_MEM0(), (sljit_sw)&buf, SLJIT_UNUSED, 0, SLJIT_LESS);
	sljit_emit_op_flags(compiler, SLJIT_IAND, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_si), SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_si), SLJIT_GREATER);

	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_E | SLJIT_SET_U, SLJIT_UNUSED, 0, SLJIT_R2, 0, SLJIT_IMM, -7);
	sljit_emit_op_flags(compiler, SLJIT_IAND | SLJIT_KEEP_FLAGS, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_si), SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_si), SLJIT_EQUAL);
	sljit_emit_op_flags(compiler, SLJIT_IAND | SLJIT_KEEP_FLAGS, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_si), SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_si), SLJIT_EQUAL);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x1235);
	sljit_emit_op_flags(compiler, SLJIT_IAND | SLJIT_SET_E, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_EQUAL);
	sljit_emit_op_flags(compiler, SLJIT_IAND, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_si), SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_si), SLJIT_I_EQUAL);
	sljit_emit_op1(compiler, SLJIT_MOV_UI, SLJIT_MEM1(SLJIT_S0), 10 * sizeof(sljit_si), SLJIT_R0, 0);

	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_R2, 0, SLJIT_IMM, -7);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 12);
	sljit_emit_op_flags(compiler, SLJIT_MOV_UI, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), 2, SLJIT_UNUSED, 0, SLJIT_EQUAL);
	sljit_emit_op_flags(compiler, SLJIT_IMOV, SLJIT_R0, 0, SLJIT_UNUSED, 0, SLJIT_EQUAL);
	sljit_emit_op1(compiler, SLJIT_MOV_SI, SLJIT_MEM1(SLJIT_S0), 14 * sizeof(sljit_si), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 16);
	sljit_emit_op_flags(compiler, SLJIT_IAND | SLJIT_SET_E, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), 2, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), 2, SLJIT_EQUAL);
	sljit_emit_op_flags(compiler, SLJIT_MOV_UI, SLJIT_MEM1(SLJIT_S0), 18 * sizeof(sljit_si), SLJIT_UNUSED, 0, SLJIT_I_NOT_EQUAL);

	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_R2, 0, SLJIT_IMM, -7);
	sljit_emit_op_flags(compiler, SLJIT_IXOR | SLJIT_KEEP_FLAGS, SLJIT_MEM1(SLJIT_S0), 20 * sizeof(sljit_si), SLJIT_MEM1(SLJIT_S0), 20 * sizeof(sljit_si), SLJIT_EQUAL);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 39);
	sljit_emit_op_flags(compiler, SLJIT_IXOR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_EQUAL);
	sljit_emit_op1(compiler, SLJIT_MOV_SI | SLJIT_INT_OP, SLJIT_MEM1(SLJIT_S0), 22 * sizeof(sljit_si), SLJIT_R0, 0);

	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_E | SLJIT_SET_U, SLJIT_UNUSED, 0, SLJIT_R2, 0, SLJIT_IMM, -7);
	sljit_emit_op_flags(compiler, SLJIT_AND, SLJIT_MEM0(), (sljit_sw)&buf2, SLJIT_MEM0(), (sljit_sw)&buf2, SLJIT_GREATER);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_E | SLJIT_SET_S | SLJIT_SET_U, SLJIT_UNUSED, 0, SLJIT_R2, 0, SLJIT_IMM, 5);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op_flags(compiler, SLJIT_AND | SLJIT_KEEP_FLAGS, SLJIT_MEM2(SLJIT_S1, SLJIT_R0), SLJIT_WORD_SHIFT, SLJIT_MEM2(SLJIT_S1, SLJIT_R0), SLJIT_WORD_SHIFT, SLJIT_SIG_LESS);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 2);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM2(SLJIT_S1, SLJIT_R0), SLJIT_WORD_SHIFT, SLJIT_UNUSED, 0, SLJIT_LESS);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_UNUSED, 0, SLJIT_NOT_EQUAL);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_sw), SLJIT_S2, 0);
	sljit_emit_op_flags(compiler, SLJIT_OR, SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_sw), SLJIT_GREATER);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_U, SLJIT_UNUSED, 0, SLJIT_R2, 0, SLJIT_IMM, 0);
	sljit_emit_op_flags(compiler, SLJIT_XOR, SLJIT_MEM1(SLJIT_S1), 5 * sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S1), 5 * sizeof(sljit_sw), SLJIT_GREATER);

	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func2((sljit_sw)&buf, (sljit_sw)&buf2);
	FAILED(buf[0] != 0, "test46 case 1 failed\n");
	FAILED(buf[1] != -17, "test46 case 2 failed\n");
	FAILED(buf[2] != 1, "test46 case 3 failed\n");
	FAILED(buf[3] != -17, "test46 case 4 failed\n");
	FAILED(buf[4] != 1, "test46 case 5 failed\n");
	FAILED(buf[5] != -17, "test46 case 6 failed\n");
	FAILED(buf[6] != 1, "test46 case 7 failed\n");
	FAILED(buf[7] != -17, "test46 case 8 failed\n");
	FAILED(buf[8] != 0, "test46 case 9 failed\n");
	FAILED(buf[9] != -17, "test46 case 10 failed\n");
	FAILED(buf[10] != 1, "test46 case 11 failed\n");
	FAILED(buf[11] != -17, "test46 case 12 failed\n");
	FAILED(buf[12] != 1, "test46 case 13 failed\n");
	FAILED(buf[13] != -17, "test46 case 14 failed\n");
	FAILED(buf[14] != 1, "test46 case 15 failed\n");
	FAILED(buf[15] != -17, "test46 case 16 failed\n");
	FAILED(buf[16] != 0, "test46 case 17 failed\n");
	FAILED(buf[17] != -17, "test46 case 18 failed\n");
	FAILED(buf[18] != 0, "test46 case 19 failed\n");
	FAILED(buf[19] != -17, "test46 case 20 failed\n");
	FAILED(buf[20] != -18, "test46 case 21 failed\n");
	FAILED(buf[21] != -17, "test46 case 22 failed\n");
	FAILED(buf[22] != 38, "test46 case 23 failed\n");
	FAILED(buf[23] != -17, "test46 case 24 failed\n");

	FAILED(buf2[0] != 0, "test46 case 25 failed\n");
	FAILED(buf2[1] != 1, "test46 case 26 failed\n");
	FAILED(buf2[2] != 0, "test46 case 27 failed\n");
	FAILED(buf2[3] != 1, "test46 case 28 failed\n");
	FAILED(buf2[4] != -123, "test46 case 29 failed\n");
	FAILED(buf2[5] != -14, "test46 case 30 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test47(void)
{
	/* Test jump optimizations. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_sw buf[3];

	if (verbose)
		printf("Run test47\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;

	sljit_emit_enter(compiler, 0, 1, 3, 1, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x3a5c6f);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_U, SLJIT_UNUSED, 0, SLJIT_R0, 0, SLJIT_IMM, 3);
	sljit_set_target(sljit_emit_jump(compiler, SLJIT_LESS), 0x11223344);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0xd37c10);
#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	sljit_set_target(sljit_emit_jump(compiler, SLJIT_LESS), SLJIT_W(0x112233445566));
#endif
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x59b48e);
#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	sljit_set_target(sljit_emit_jump(compiler, SLJIT_LESS), SLJIT_W(0x1122334455667788));
#endif
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);
	FAILED(buf[0] != 0x3a5c6f, "test47 case 1 failed\n");
	FAILED(buf[1] != 0xd37c10, "test47 case 2 failed\n");
	FAILED(buf[2] != 0x59b48e, "test47 case 3 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test48(void)
{
	/* Test floating point conversions. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	int i;
	sljit_d dbuf[9];
	sljit_s sbuf[9];
	sljit_sw wbuf[9];
	sljit_si ibuf[9];

	if (verbose)
		printf("Run test48\n");

	if (!sljit_is_fpu_available()) {
		if (verbose)
			printf("no fpu available, test48 skipped\n");
		successful_tests++;
		if (compiler)
			sljit_free_compiler(compiler);
		return;
	}

	FAILED(!compiler, "cannot create compiler\n");
	for (i = 0; i < 9; i++) {
		dbuf[i] = 0.0;
		sbuf[i] = 0.0;
		wbuf[i] = 0;
		ibuf[i] = 0;
	}

	dbuf[0] = 123.5;
	dbuf[1] = -367;
	dbuf[2] = 917.75;

	sbuf[0] = 476.25;
	sbuf[1] = -1689.75;

	wbuf[0] = 2345;

	ibuf[0] = 312;
	ibuf[1] = -9324;

	sljit_emit_enter(compiler, 0, 0, 3, 3, 6, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, (sljit_sw)&dbuf);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, (sljit_sw)&sbuf);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, (sljit_sw)&wbuf);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, (sljit_sw)&ibuf);

	/* sbuf[2] */
	sljit_emit_fop1(compiler, SLJIT_CONVS_FROMD, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_s), SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR5, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 3);
	/* sbuf[3] */
	sljit_emit_fop1(compiler, SLJIT_CONVS_FROMD, SLJIT_MEM2(SLJIT_S1, SLJIT_R0), SLJIT_SINGLE_SHIFT, SLJIT_FR5, 0);
	sljit_emit_fop1(compiler, SLJIT_CONVD_FROMS, SLJIT_FR4, 0, SLJIT_MEM1(SLJIT_S1), 0);
	/* dbuf[3] */
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_d), SLJIT_FR4, 0);
	sljit_emit_fop1(compiler, SLJIT_SMOV, SLJIT_FR3, 0, SLJIT_MEM1(SLJIT_S1), 0);
	sljit_emit_fop1(compiler, SLJIT_CONVD_FROMS, SLJIT_FR2, 0, SLJIT_FR3, 0);
	/* dbuf[4] */
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_d), SLJIT_FR2, 0);
	/* sbuf[4] */
	sljit_emit_fop1(compiler, SLJIT_SMOV, SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_s), SLJIT_FR3, 0);

	/* wbuf[1] */
	sljit_emit_fop1(compiler, SLJIT_CONVW_FROMD, SLJIT_MEM1(SLJIT_S2), sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 2);
	sljit_emit_fop1(compiler, SLJIT_CONVW_FROMD, SLJIT_UNUSED, 0, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), SLJIT_DOUBLE_SHIFT);
	sljit_emit_fop1(compiler, SLJIT_CONVW_FROMD, SLJIT_R0, 0, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), SLJIT_DOUBLE_SHIFT);
	/* wbuf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S2), 2 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_fop1(compiler, SLJIT_SMOV, SLJIT_FR5, 0, SLJIT_MEM1(SLJIT_S1), 0);
	/* wbuf[3] */
	sljit_emit_fop1(compiler, SLJIT_CONVW_FROMS, SLJIT_MEM1(SLJIT_S2), 3 * sizeof(sljit_sw), SLJIT_FR5, 0);
	sljit_emit_fop1(compiler, SLJIT_SNEG, SLJIT_FR0, 0, SLJIT_FR5, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 4);
	/* wbuf[4] */
	sljit_emit_fop1(compiler, SLJIT_CONVW_FROMS, SLJIT_MEM2(SLJIT_S2, SLJIT_R1), SLJIT_WORD_SHIFT, SLJIT_FR0, 0);
	sljit_emit_fop1(compiler, SLJIT_DNEG, SLJIT_FR4, 0, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_d));
	/* ibuf[2] */
	sljit_emit_fop1(compiler, SLJIT_CONVI_FROMD, SLJIT_MEM1(SLJIT_R2), 2 * sizeof(sljit_si), SLJIT_FR4, 0);
	sljit_emit_fop1(compiler, SLJIT_SMOV, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_s));
	sljit_emit_fop1(compiler, SLJIT_CONVI_FROMS, SLJIT_R0, 0, SLJIT_FR1, 0);
	/* ibuf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV_SI, SLJIT_MEM1(SLJIT_R2), 3 * sizeof(sljit_si), SLJIT_R0, 0);

	/* dbuf[5] */
	sljit_emit_fop1(compiler, SLJIT_CONVD_FROMW, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_d), SLJIT_MEM1(SLJIT_S2), 0);
	sljit_emit_fop1(compiler, SLJIT_CONVD_FROMW, SLJIT_FR2, 0, SLJIT_IMM, -6213);
	/* dbuf[6] */
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_d), SLJIT_FR2, 0);
	/* dbuf[7] */
	sljit_emit_fop1(compiler, SLJIT_CONVD_FROMI, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_d), SLJIT_MEM0(), (sljit_sw)&ibuf[0]);
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R2), sizeof(sljit_si));
	sljit_emit_fop1(compiler, SLJIT_CONVD_FROMI, SLJIT_FR1, 0, SLJIT_R0, 0);
	/* dbuf[8] */
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_d), SLJIT_FR1, 0);
	/* sbuf[5] */
	sljit_emit_fop1(compiler, SLJIT_CONVS_FROMW, SLJIT_MEM1(SLJIT_S1), 5 * sizeof(sljit_s), SLJIT_IMM, -123);
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R0, 0, SLJIT_IMM, 7190);
	sljit_emit_fop1(compiler, SLJIT_CONVS_FROMW, SLJIT_FR3, 0, SLJIT_R0, 0);
	/* sbuf[6] */
	sljit_emit_fop1(compiler, SLJIT_SMOV, SLJIT_MEM1(SLJIT_S1), 6 * sizeof(sljit_s), SLJIT_FR3, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 123);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R1, 0, SLJIT_R2, 0, SLJIT_IMM, 123 * sizeof(sljit_si));
	sljit_emit_fop1(compiler, SLJIT_CONVS_FROMI, SLJIT_FR1, 0, SLJIT_MEM2(SLJIT_R1, SLJIT_R0), 2);
	/* sbuf[7] */
	sljit_emit_fop1(compiler, SLJIT_SMOV, SLJIT_MEM1(SLJIT_S1), 7 * sizeof(sljit_s), SLJIT_FR1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 8);
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R1, 0, SLJIT_IMM, 3812);
	/* sbuf[8] */
	sljit_emit_fop1(compiler, SLJIT_CONVS_FROMI, SLJIT_MEM2(SLJIT_S1, SLJIT_R0), SLJIT_SINGLE_SHIFT, SLJIT_R1, 0);

	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func0();
	FAILED(dbuf[3] != 476.25, "test48 case 1 failed\n");
	FAILED(dbuf[4] != 476.25, "test48 case 2 failed\n");
	FAILED(dbuf[5] != 2345.0, "test48 case 3 failed\n");
	FAILED(dbuf[6] != -6213.0, "test48 case 4 failed\n");
	FAILED(dbuf[7] != 312.0, "test48 case 5 failed\n");
	FAILED(dbuf[8] != -9324.0, "test48 case 6 failed\n");

	FAILED(sbuf[2] != 123.5, "test48 case 7 failed\n");
	FAILED(sbuf[3] != 123.5, "test48 case 8 failed\n");
	FAILED(sbuf[4] != 476.25, "test48 case 9 failed\n");
	FAILED(sbuf[5] != -123, "test48 case 10 failed\n");
	FAILED(sbuf[6] != 7190, "test48 case 11 failed\n");
	FAILED(sbuf[7] != 312, "test48 case 12 failed\n");
	FAILED(sbuf[8] != 3812, "test48 case 13 failed\n");

	FAILED(wbuf[1] != -367, "test48 case 14 failed\n");
	FAILED(wbuf[2] != 917, "test48 case 15 failed\n");
	FAILED(wbuf[3] != 476, "test48 case 16 failed\n");
	FAILED(wbuf[4] != -476, "test48 case 17 failed\n");

	FAILED(ibuf[2] != -917, "test48 case 18 failed\n");
	FAILED(ibuf[3] != -1689, "test48 case 19 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test49(void)
{
	/* Test floating point conversions. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	int i;
	sljit_d dbuf[10];
	sljit_s sbuf[9];
	sljit_sw wbuf[9];
	sljit_si ibuf[9];
	sljit_si* dbuf_ptr = (sljit_si*)dbuf;
	sljit_si* sbuf_ptr = (sljit_si*)sbuf;

	if (verbose)
		printf("Run test49\n");

	if (!sljit_is_fpu_available()) {
		if (verbose)
			printf("no fpu available, test49 skipped\n");
		successful_tests++;
		if (compiler)
			sljit_free_compiler(compiler);
		return;
	}

	FAILED(!compiler, "cannot create compiler\n");

	for (i = 0; i < 9; i++) {
		dbuf_ptr[i << 1] = -1;
		dbuf_ptr[(i << 1) + 1] = -1;
		sbuf_ptr[i] = -1;
		wbuf[i] = -1;
		ibuf[i] = -1;
	}

#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	dbuf[9] = (sljit_d)SLJIT_W(0x1122334455);
#endif
	dbuf[0] = 673.75;
	sbuf[0] = -879.75;
	wbuf[0] = 345;
	ibuf[0] = -249;

	sljit_emit_enter(compiler, 0, 0, 3, 3, 3, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, (sljit_sw)&dbuf);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, (sljit_sw)&sbuf);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, (sljit_sw)&wbuf);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, (sljit_sw)&ibuf);

	/* dbuf[2] */
	sljit_emit_fop1(compiler, SLJIT_CONVD_FROMS, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_d), SLJIT_MEM1(SLJIT_S1), 0);
	/* sbuf[2] */
	sljit_emit_fop1(compiler, SLJIT_CONVS_FROMD, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_s), SLJIT_MEM1(SLJIT_S0), 0);
	/* wbuf[2] */
	sljit_emit_fop1(compiler, SLJIT_CONVW_FROMD, SLJIT_MEM1(SLJIT_S2), 2 * sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S0), 0);
	/* wbuf[4] */
	sljit_emit_fop1(compiler, SLJIT_CONVW_FROMS, SLJIT_MEM1(SLJIT_S2), 4 * sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S1), 0);
	/* ibuf[2] */
	sljit_emit_fop1(compiler, SLJIT_CONVI_FROMD, SLJIT_MEM1(SLJIT_R2), 2 * sizeof(sljit_si), SLJIT_MEM1(SLJIT_S0), 0);
	/* ibuf[4] */
	sljit_emit_fop1(compiler, SLJIT_CONVI_FROMS, SLJIT_MEM1(SLJIT_R2), 4 * sizeof(sljit_si), SLJIT_MEM1(SLJIT_S1), 0);
	/* dbuf[4] */
	sljit_emit_fop1(compiler, SLJIT_CONVD_FROMW, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_d), SLJIT_MEM1(SLJIT_S2), 0);
	/* sbuf[4] */
	sljit_emit_fop1(compiler, SLJIT_CONVS_FROMW, SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_s), SLJIT_MEM1(SLJIT_S2), 0);
	/* dbuf[6] */
	sljit_emit_fop1(compiler, SLJIT_CONVD_FROMI, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_d), SLJIT_MEM1(SLJIT_R2), 0);
	/* sbuf[6] */
	sljit_emit_fop1(compiler, SLJIT_CONVS_FROMI, SLJIT_MEM1(SLJIT_S1), 6 * sizeof(sljit_s), SLJIT_MEM1(SLJIT_R2), 0);

#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	sljit_emit_fop1(compiler, SLJIT_CONVW_FROMD, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_d));
	/* wbuf[8] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S2), 8 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_d));
	sljit_emit_fop1(compiler, SLJIT_CONVI_FROMD, SLJIT_R0, 0, SLJIT_FR2, 0);
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_IAND, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 0xffff);
	/* ibuf[8] */
	sljit_emit_op1(compiler, SLJIT_IMOV, SLJIT_MEM1(SLJIT_R2), 8 * sizeof(sljit_si), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(0x4455667788));
	/* dbuf[8] */
	sljit_emit_fop1(compiler, SLJIT_CONVD_FROMW, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_d), SLJIT_R0, 0);
	/* dbuf[9] */
	sljit_emit_fop1(compiler, SLJIT_CONVD_FROMI, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_d), SLJIT_IMM, SLJIT_W(0x7766554433));
#endif

	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func0();

	FAILED(dbuf_ptr[(1 * 2) + 0] != -1, "test49 case 1 failed\n");
	FAILED(dbuf_ptr[(1 * 2) + 1] != -1, "test49 case 2 failed\n");
	FAILED(dbuf[2] != -879.75, "test49 case 3 failed\n");
	FAILED(dbuf_ptr[(3 * 2) + 0] != -1, "test49 case 4 failed\n");
	FAILED(dbuf_ptr[(3 * 2) + 1] != -1, "test49 case 5 failed\n");
	FAILED(dbuf[4] != 345, "test49 case 6 failed\n");
	FAILED(dbuf_ptr[(5 * 2) + 0] != -1, "test49 case 7 failed\n");
	FAILED(dbuf_ptr[(5 * 2) + 1] != -1, "test49 case 8 failed\n");
	FAILED(dbuf[6] != -249, "test49 case 9 failed\n");
	FAILED(dbuf_ptr[(7 * 2) + 0] != -1, "test49 case 10 failed\n");
	FAILED(dbuf_ptr[(7 * 2) + 1] != -1, "test49 case 11 failed\n");

	FAILED(sbuf_ptr[1] != -1, "test49 case 12 failed\n");
	FAILED(sbuf[2] != 673.75, "test49 case 13 failed\n");
	FAILED(sbuf_ptr[3] != -1, "test49 case 14 failed\n");
	FAILED(sbuf[4] != 345, "test49 case 15 failed\n");
	FAILED(sbuf_ptr[5] != -1, "test49 case 16 failed\n");
	FAILED(sbuf[6] != -249, "test49 case 17 failed\n");
	FAILED(sbuf_ptr[7] != -1, "test49 case 18 failed\n");

	FAILED(wbuf[1] != -1, "test49 case 19 failed\n");
	FAILED(wbuf[2] != 673, "test49 case 20 failed\n");
	FAILED(wbuf[3] != -1, "test49 case 21 failed\n");
	FAILED(wbuf[4] != -879, "test49 case 22 failed\n");
	FAILED(wbuf[5] != -1, "test49 case 23 failed\n");

	FAILED(ibuf[1] != -1, "test49 case 24 failed\n");
	FAILED(ibuf[2] != 673, "test49 case 25 failed\n");
	FAILED(ibuf[3] != -1, "test49 case 26 failed\n");
	FAILED(ibuf[4] != -879, "test49 case 27 failed\n");
	FAILED(ibuf[5] != -1, "test49 case 28 failed\n");

#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
	FAILED(dbuf[8] != (sljit_d)SLJIT_W(0x4455667788), "test49 case 29 failed\n");
	FAILED(dbuf[9] != (sljit_d)SLJIT_W(0x66554433), "test49 case 30 failed\n");
	FAILED(wbuf[8] != SLJIT_W(0x1122334455), "test48 case 31 failed\n");
	FAILED(ibuf[8] == 0x4455, "test48 case 32 failed\n");
#endif

	sljit_free_code(code.code);
	successful_tests++;
}

static void test50(void)
{
	/* Test stack and floating point operations. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
#if !(defined SLJIT_CONFIG_X86 && SLJIT_CONFIG_X86)
	sljit_uw size1, size2, size3;
	int result;
#endif
	sljit_s sbuf[7];

	if (verbose)
		printf("Run test50\n");

	if (!sljit_is_fpu_available()) {
		if (verbose)
			printf("no fpu available, test50 skipped\n");
		successful_tests++;
		if (compiler)
			sljit_free_compiler(compiler);
		return;
	}

	FAILED(!compiler, "cannot create compiler\n");

	sbuf[0] = 245.5;
	sbuf[1] = -100.25;
	sbuf[2] = 713.75;

	sljit_emit_enter(compiler, 0, 1, 3, 3, 6, 0, 8 * sizeof(sljit_s));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_SMOV, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_s), SLJIT_MEM1(SLJIT_SP), 0);
	/* sbuf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_s), SLJIT_MEM1(SLJIT_SP), sizeof(sljit_s));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_s), SLJIT_MEM1(SLJIT_S0), sizeof(sljit_s));
	sljit_emit_fop2(compiler, SLJIT_SADD, SLJIT_MEM1(SLJIT_SP), 2 * sizeof(sljit_s), SLJIT_MEM1(SLJIT_SP), 0, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_s));
	/* sbuf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_s), SLJIT_MEM1(SLJIT_SP), 2 * sizeof(sljit_s));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), 2 * sizeof(sljit_s), SLJIT_IMM, 5934);
	sljit_emit_fop1(compiler, SLJIT_CONVS_FROMW, SLJIT_MEM1(SLJIT_SP), 3 * sizeof(sljit_s), SLJIT_MEM1(SLJIT_SP), 2 * sizeof(sljit_s));
	/* sbuf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_s), SLJIT_MEM1(SLJIT_SP), 3 * sizeof(sljit_s));

#if !(defined SLJIT_CONFIG_X86 && SLJIT_CONFIG_X86)
	size1 = compiler->size;
#endif
	sljit_emit_fop1(compiler, SLJIT_SMOV, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_s));
#if !(defined SLJIT_CONFIG_X86 && SLJIT_CONFIG_X86)
	size2 = compiler->size;
#endif
	sljit_emit_fop1(compiler, SLJIT_SMOV, SLJIT_FR5, 0, SLJIT_FR2, 0);
#if !(defined SLJIT_CONFIG_X86 && SLJIT_CONFIG_X86)
	size3 = compiler->size;
#endif
	/* sbuf[6] */
	sljit_emit_fop1(compiler, SLJIT_SMOV, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_s), SLJIT_FR5, 0);
#if !(defined SLJIT_CONFIG_X86 && SLJIT_CONFIG_X86)
	result = (compiler->size - size3) == (size3 - size2) && (size3 - size2) == (size2 - size1);
#endif

	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&sbuf);

	FAILED(sbuf[3] != 245.5, "test50 case 1 failed\n");
	FAILED(sbuf[4] != 145.25, "test50 case 2 failed\n");
	FAILED(sbuf[5] != 5934, "test50 case 3 failed\n");
	FAILED(sbuf[6] != 713.75, "test50 case 4 failed\n");
#if !(defined SLJIT_CONFIG_X86 && SLJIT_CONFIG_X86)
	FAILED(!result, "test50 case 5 failed\n");
#endif

	sljit_free_code(code.code);
	successful_tests++;
}

static void test51(void)
{
	/* Test all registers provided by the CPU. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	struct sljit_jump* jump;
	sljit_sw buf[2];
	sljit_si i;

	FAILED(!compiler, "cannot create compiler\n");

	buf[0] = 39;

	sljit_emit_enter(compiler, 0, 0, SLJIT_NUMBER_OF_REGISTERS, 0, 0, 0, 0);

	for (i = 0; i < SLJIT_NUMBER_OF_REGISTERS; i++)
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R(i), 0, SLJIT_IMM, 32);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)buf);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0);

	for (i = 2; i < SLJIT_NUMBER_OF_REGISTERS; i++) {
		if (sljit_get_register_index(SLJIT_R(i)) >= 0) {
			sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R(i), 0, SLJIT_R0, 0);
			sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R(i)), 0);
		} else
			sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, buf[0]);
	}

	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 32);
	for (i = 2; i < SLJIT_NUMBER_OF_REGISTERS; i++) {
		if (sljit_get_register_index(SLJIT_R(i)) >= 0) {
			sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R(i), 0, SLJIT_R0, 0);
			sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R(i)), 32);
		} else
			sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, buf[0]);
	}

	for (i = 2; i < SLJIT_NUMBER_OF_REGISTERS; i++) {
		if (sljit_get_register_index(SLJIT_R(i)) >= 0) {
			sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R(i), 0, SLJIT_IMM, 32);
			sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_MEM2(SLJIT_R(i), SLJIT_R0), 0);
			sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_MEM2(SLJIT_R0, SLJIT_R(i)), 0);
			sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R(i), 0, SLJIT_IMM, 8);
			sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_MEM2(SLJIT_R0, SLJIT_R(i)), 2);
		} else
			sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 3 * buf[0]);
	}

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), 32 + sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func0();

	FAILED(buf[1] != (39 * 5 * (SLJIT_NUMBER_OF_REGISTERS - 2)), "test51 case 1 failed\n");

	sljit_free_code(code.code);

	/* Next test. */

	compiler = sljit_create_compiler();

	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, 0, SLJIT_NUMBER_OF_SCRATCH_REGISTERS, SLJIT_NUMBER_OF_SAVED_REGISTERS, 0, 0, 0);

	for (i = 0; i < SLJIT_NUMBER_OF_REGISTERS; i++)
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R(i), 0, SLJIT_IMM, 17);

	jump = sljit_emit_jump(compiler, SLJIT_CALL0);
	/* SLJIT_R0 contains the first value. */
	for (i = 1; i < SLJIT_NUMBER_OF_REGISTERS; i++)
		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R(i), 0);

	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_R0, 0);

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_enter(compiler, 0, 0, SLJIT_NUMBER_OF_REGISTERS, 0, 0, 0, 0);
	for (i = 0; i < SLJIT_NUMBER_OF_REGISTERS; i++)
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R(i), 0, SLJIT_IMM, 35);
	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func0() != (SLJIT_NUMBER_OF_SCRATCH_REGISTERS * 35 + SLJIT_NUMBER_OF_SAVED_REGISTERS * 17), "test51 case 2 failed\n");

	sljit_free_code(code.code);

	/* Next test. */

	compiler = sljit_create_compiler();

	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, 0, SLJIT_NUMBER_OF_SCRATCH_REGISTERS, SLJIT_NUMBER_OF_SAVED_REGISTERS, 0, 0, 0);

	for (i = 0; i < SLJIT_NUMBER_OF_REGISTERS; i++)
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R(i), 0, SLJIT_IMM, 68);

	jump = sljit_emit_jump(compiler, SLJIT_CALL0);
	/* SLJIT_R0 contains the first value. */
	for (i = 1; i < SLJIT_NUMBER_OF_REGISTERS; i++)
		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R(i), 0);

	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_R0, 0);

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_enter(compiler, 0, 0, 0, SLJIT_NUMBER_OF_REGISTERS, 0, 0, 0);
	for (i = 0; i < SLJIT_NUMBER_OF_REGISTERS; i++)
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S(i), 0, SLJIT_IMM, 43);
	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func0() != (SLJIT_NUMBER_OF_SCRATCH_REGISTERS * 43 + SLJIT_NUMBER_OF_SAVED_REGISTERS * 68), "test51 case 3 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test52(void)
{
	/* Test all registers provided by the CPU. */
	executable_code code;
	struct sljit_compiler* compiler;
	struct sljit_jump* jump;
	sljit_d buf[3];
	sljit_si i;

	if (!sljit_is_fpu_available()) {
		if (verbose)
			printf("no fpu available, test52 skipped\n");
		successful_tests++;
		return;
	}

	/* Next test. */

	compiler = sljit_create_compiler();
	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 6.25;
	buf[1] = 17.75;

	sljit_emit_enter(compiler, 0, 1, 0, 1, SLJIT_NUMBER_OF_SCRATCH_FLOAT_REGISTERS, SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS, 0);

	for (i = 0; i < SLJIT_NUMBER_OF_FLOAT_REGISTERS; i++)
		sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR(i), 0, SLJIT_MEM1(SLJIT_S0), 0);

	jump = sljit_emit_jump(compiler, SLJIT_CALL0);
	/* SLJIT_FR0 contains the first value. */
	for (i = 1; i < SLJIT_NUMBER_OF_FLOAT_REGISTERS; i++)
		sljit_emit_fop2(compiler, SLJIT_DADD, SLJIT_FR0, 0, SLJIT_FR0, 0, SLJIT_FR(i), 0);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_d), SLJIT_FR0, 0);

	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_enter(compiler, 0, 0, 1, 0, SLJIT_NUMBER_OF_FLOAT_REGISTERS, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&buf[1]);
	for (i = 0; i < SLJIT_NUMBER_OF_FLOAT_REGISTERS; i++)
		sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR(i), 0, SLJIT_MEM1(SLJIT_R0), 0);
	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);
	FAILED(buf[2] != (SLJIT_NUMBER_OF_SCRATCH_FLOAT_REGISTERS * 17.75 + SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS * 6.25), "test52 case 1 failed\n");

	sljit_free_code(code.code);

	/* Next test. */

	compiler = sljit_create_compiler();
	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = -32.5;
	buf[1] = -11.25;

	sljit_emit_enter(compiler, 0, 1, 0, 1, SLJIT_NUMBER_OF_SCRATCH_FLOAT_REGISTERS, SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS, 0);

	for (i = 0; i < SLJIT_NUMBER_OF_FLOAT_REGISTERS; i++)
		sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR(i), 0, SLJIT_MEM1(SLJIT_S0), 0);

	jump = sljit_emit_jump(compiler, SLJIT_CALL0);
	/* SLJIT_FR0 contains the first value. */
	for (i = 1; i < SLJIT_NUMBER_OF_FLOAT_REGISTERS; i++)
		sljit_emit_fop2(compiler, SLJIT_DADD, SLJIT_FR0, 0, SLJIT_FR0, 0, SLJIT_FR(i), 0);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_d), SLJIT_FR0, 0);

	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_enter(compiler, 0, 0, 1, 0, 0, SLJIT_NUMBER_OF_FLOAT_REGISTERS, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&buf[1]);
	for (i = 0; i < SLJIT_NUMBER_OF_FLOAT_REGISTERS; i++)
		sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FS(i), 0, SLJIT_MEM1(SLJIT_R0), 0);
	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);
	FAILED(buf[2] != (SLJIT_NUMBER_OF_SCRATCH_FLOAT_REGISTERS * -11.25 + SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS * -32.5), "test52 case 2 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

static void test53(void)
{
	/* Check SLJIT_DOUBLE_ALIGNMENT. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_sw buf[1];

	if (verbose)
		printf("Run test53\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = -1;

	sljit_emit_enter(compiler, SLJIT_DOUBLE_ALIGNMENT, 1, 1, 1, 0, 0, 2 * sizeof(sljit_sw));

	sljit_get_local_base(compiler, SLJIT_R0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);

	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);

	FAILED((buf[0] & (sizeof(sljit_d) - 1)) != 0, "test53 case 1 failed\n");

	sljit_free_code(code.code);

	/* Next test. */

	compiler = sljit_create_compiler();
	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = -1;

	/* One more saved register to break the alignment on x86-32. */
	sljit_emit_enter(compiler, SLJIT_DOUBLE_ALIGNMENT, 1, 1, 2, 0, 0, 2 * sizeof(sljit_sw));

	sljit_get_local_base(compiler, SLJIT_R0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);

	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);

	FAILED((buf[0] & (sizeof(sljit_d) - 1)) != 0, "test53 case 2 failed\n");

	sljit_free_code(code.code);
	successful_tests++;
}

void sljit_test(int argc, char* argv[])
{
	sljit_si has_arg = (argc >= 2 && argv[1][0] == '-' && argv[1][2] == '\0');
	verbose = has_arg && argv[1][1] == 'v';
	silent = has_arg && argv[1][1] == 's';

	if (!verbose && !silent)
		printf("Pass -v to enable verbose, -s to disable this hint.\n\n");

#if !(defined SLJIT_CONFIG_UNSUPPORTED && SLJIT_CONFIG_UNSUPPORTED)
	test_exec_allocator();
#endif
	test1();
	test2();
	test3();
	test4();
	test5();
	test6();
	test7();
	test8();
	test9();
	test10();
	test11();
	test12();
	test13();
	test14();
	test15();
	test16();
	test17();
	test18();
	test19();
	test20();
	test21();
	test22();
	test23();
	test24();
	test25();
	test26();
	test27();
	test28();
	test29();
	test30();
	test31();
	test32();
	test33();
	test34();
	test35();
	test36();
	test37();
	test38();
	test39();
	test40();
	test41();
	test42();
	test43();
	test44();
	test45();
	test46();
	test47();
	test48();
	test49();
	test50();
	test51();
	test52();
	test53();

#if (defined SLJIT_EXECUTABLE_ALLOCATOR && SLJIT_EXECUTABLE_ALLOCATOR)
	sljit_free_unused_memory_exec();
#endif

#	define TEST_COUNT 53

	printf("SLJIT tests: ");
	if (successful_tests == TEST_COUNT)
		printf("all tests are " COLOR_GREEN "PASSED" COLOR_DEFAULT " ");
	else
		printf(COLOR_RED "%d" COLOR_DEFAULT " (" COLOR_RED "%d%%" COLOR_DEFAULT ") tests are failed ", TEST_COUNT - successful_tests, (TEST_COUNT - successful_tests) * 100 / 47);
	printf("on " COLOR_ARCH "%s" COLOR_DEFAULT "%s\n", sljit_get_platform_name(), sljit_is_fpu_available() ? " (with fpu)" : " (without fpu)");

#	undef TEST_COUNT
}
