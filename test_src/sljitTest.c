/*
 *    Stack-less Just-In-Time compiler
 *
 *    Copyright Zoltan Herczeg (hzmester@freemail.hu). All rights reserved.
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

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4127) /* conditional expression is constant */
#endif

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
	sljit_sw (SLJIT_FUNC *func0)(void);
	sljit_sw (SLJIT_FUNC *func1)(sljit_sw a);
	sljit_sw (SLJIT_FUNC *func2)(sljit_sw a, sljit_sw b);
	sljit_sw (SLJIT_FUNC *func3)(sljit_sw a, sljit_sw b, sljit_sw c);

	void (SLJIT_FUNC *test70_f1)(sljit_s32 a, sljit_uw b, sljit_u32 c, sljit_sw d);
	void (SLJIT_FUNC *test70_f2)(sljit_s32 a, sljit_u32 b, sljit_sw c, sljit_sw d);
	void (SLJIT_FUNC *test70_f3)(sljit_s32 a, sljit_f32 b, sljit_uw c, sljit_f64 d);
	void (SLJIT_FUNC *test70_f4)(sljit_f32 a, sljit_f64 b, sljit_f32 c, sljit_s32 d);
	void (SLJIT_FUNC *test70_f5)(sljit_f64 a, sljit_f32 b, sljit_u32 c, sljit_f32 d);
	void (SLJIT_FUNC *test70_f6)(sljit_f64 a, sljit_s32 b, sljit_f32 c, sljit_f64 d);
	void (SLJIT_FUNC *test70_f7)(sljit_f32 a, sljit_s32 b, sljit_uw c, sljit_u32 d);
	void (SLJIT_FUNC *test70_f8)(sljit_f64 a, sljit_f64 b, sljit_uw c, sljit_sw d);
	void (SLJIT_FUNC *test70_f9)(sljit_f64 a, sljit_f64 b, sljit_uw c, sljit_f64 d);
	void (SLJIT_FUNC *test70_f10)(sljit_f64 a, sljit_f64 b, sljit_f64 c, sljit_s32 d);

	sljit_sw (SLJIT_FUNC *test71_f1)(sljit_f32 a, sljit_f64 b);
	sljit_sw (SLJIT_FUNC *test71_f2)(sljit_f64 a, sljit_f64 b, sljit_f64 c, sljit_f64 d);
	sljit_sw (SLJIT_FUNC *test71_f3)(sljit_f64 a, sljit_f64 b, sljit_f64 c);

	void (SLJIT_FUNC *test73_f1)(sljit_s32 a, sljit_sw b, sljit_sw c, sljit_s32 d);
	void (SLJIT_FUNC *test73_f2)(sljit_sw a, sljit_sw b, sljit_s32 c, sljit_s32 d);
	void (SLJIT_FUNC *test73_f3)(sljit_f64 a, sljit_f64 b, sljit_f64 c, sljit_sw d);
	void (SLJIT_FUNC *test73_f4)(sljit_f64 a, sljit_f64 b, sljit_sw c, sljit_sw d);

	sljit_f32 (SLJIT_FUNC *test81_f1)(sljit_sw a);
	sljit_f64 (SLJIT_FUNC *test81_f2)(sljit_sw a);
};
typedef union executable_code executable_code;

static sljit_s32 successful_tests = 0;
static sljit_s32 verbose = 0;
static sljit_s32 silent = 0;

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

#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
#define IS_32BIT 0
#define IS_64BIT 1
#define WCONST(const64, const32) ((sljit_sw)SLJIT_W(const64))
#else /* !SLJIT_64BIT_ARCHITECTURE */
#define IS_32BIT 1
#define IS_64BIT 0
#define WCONST(const64, const32) ((sljit_sw)const32)
#endif /* SLJIT_64BIT_ARCHITECTURE */

#if (defined SLJIT_CONFIG_X86 && SLJIT_CONFIG_X86)
#define IS_X86 1
#else /* !SLJIT_CONFIG_X86 */
#define IS_X86 0
#endif /* SLJIT_CONFIG_X86 */

#if (defined SLJIT_CONFIG_ARM && SLJIT_CONFIG_ARM)
#define IS_ARM 1
#else /* !SLJIT_CONFIG_ARM */
#define IS_ARM 0
#endif /* SLJIT_CONFIG_ARM */

static void cond_set(struct sljit_compiler *compiler, sljit_s32 dst, sljit_sw dstw, sljit_s32 type)
{
	/* Testing both sljit_emit_op_flags and sljit_emit_jump. */
	struct sljit_jump* jump;
	struct sljit_label* label;

	sljit_emit_op_flags(compiler, SLJIT_MOV, dst, dstw, type);
	jump = sljit_emit_jump(compiler, type);
	sljit_emit_op2(compiler, SLJIT_ADD, dst, dstw, dst, dstw, SLJIT_IMM, 2);
	label = sljit_emit_label(compiler);
	sljit_set_label(jump, label);
}

#if !(defined SLJIT_CONFIG_UNSUPPORTED && SLJIT_CONFIG_UNSUPPORTED)

/* For interface testing and for test64. */
void *sljit_test_malloc_exec(sljit_uw size, void *exec_allocator_data)
{
	if (exec_allocator_data)
		return exec_allocator_data;

	return SLJIT_BUILTIN_MALLOC_EXEC(size, exec_allocator_data);
}

/* For interface testing. */
void sljit_test_free_code(void* code, void *exec_allocator_data)
{
	SLJIT_UNUSED_ARG(exec_allocator_data);
	SLJIT_BUILTIN_FREE_EXEC(code, exec_allocator_data);
}

#define MALLOC_EXEC(result, size) \
	result = SLJIT_MALLOC_EXEC(size, NULL); \
	if (!result) { \
		printf("Cannot allocate executable memory\n"); \
		return; \
	} \
	memset(result, 255, size);

#define FREE_EXEC(ptr) \
	SLJIT_FREE_EXEC(((sljit_u8*)(ptr)) + SLJIT_EXEC_OFFSET(ptr), NULL);

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
	FREE_EXEC(ptr2);
	FREE_EXEC(ptr3);
	FREE_EXEC(ptr1);
	MALLOC_EXEC(ptr1, 262104);
	MALLOC_EXEC(ptr2, 32000);
	FREE_EXEC(ptr1);
	MALLOC_EXEC(ptr1, 262104);
	FREE_EXEC(ptr1);
	FREE_EXEC(ptr2);
	MALLOC_EXEC(ptr1, 512);
	MALLOC_EXEC(ptr2, 512);
	MALLOC_EXEC(ptr3, 512);
	FREE_EXEC(ptr2);
	MALLOC_EXEC(ptr2, 512);
#if (defined SLJIT_EXECUTABLE_ALLOCATOR && SLJIT_EXECUTABLE_ALLOCATOR)
	sljit_free_unused_memory_exec();
#endif
	FREE_EXEC(ptr3);
	FREE_EXEC(ptr1);
	FREE_EXEC(ptr2);

#if (defined SLJIT_EXECUTABLE_ALLOCATOR && SLJIT_EXECUTABLE_ALLOCATOR)
	sljit_free_unused_memory_exec();
#endif
}

#undef MALLOC_EXEC

#endif /* !(defined SLJIT_CONFIG_UNSUPPORTED && SLJIT_CONFIG_UNSUPPORTED) */

static void test_macros(void)
{
	SLJIT_ASSERT(SLJIT_IS_MEM(SLJIT_MEM0()));
	SLJIT_ASSERT(SLJIT_IS_MEM(SLJIT_MEM1(SLJIT_S0)));
	SLJIT_ASSERT(SLJIT_IS_MEM(SLJIT_MEM2(SLJIT_R0, SLJIT_S0)));
	SLJIT_ASSERT(SLJIT_IS_MEM0(SLJIT_MEM0()));
	SLJIT_ASSERT(!SLJIT_IS_MEM0(SLJIT_MEM1(SLJIT_S0)));
	SLJIT_ASSERT(!SLJIT_IS_MEM0(SLJIT_MEM2(SLJIT_R0, SLJIT_S0)));
	SLJIT_ASSERT(!SLJIT_IS_MEM1(SLJIT_MEM0()));
	SLJIT_ASSERT(SLJIT_IS_MEM1(SLJIT_MEM1(SLJIT_S0)));
	SLJIT_ASSERT(!SLJIT_IS_MEM1(SLJIT_MEM2(SLJIT_R0, SLJIT_S0)));
	SLJIT_ASSERT(!SLJIT_IS_MEM2(SLJIT_MEM0()));
	SLJIT_ASSERT(!SLJIT_IS_MEM2(SLJIT_MEM1(SLJIT_R0)));
	SLJIT_ASSERT(SLJIT_IS_MEM2(SLJIT_MEM2(SLJIT_R0, SLJIT_R1)));

	SLJIT_ASSERT(!SLJIT_IS_REG(SLJIT_IMM));
	SLJIT_ASSERT(!SLJIT_IS_MEM(SLJIT_IMM));
	SLJIT_ASSERT(SLJIT_IS_IMM(SLJIT_IMM));
	SLJIT_ASSERT(!SLJIT_IS_REG_PAIR(SLJIT_IMM));
	SLJIT_ASSERT(SLJIT_IS_REG(SLJIT_S0));
	SLJIT_ASSERT(!SLJIT_IS_MEM(SLJIT_S0));
	SLJIT_ASSERT(!SLJIT_IS_IMM(SLJIT_S0));
	SLJIT_ASSERT(!SLJIT_IS_REG_PAIR(SLJIT_S0));
	SLJIT_ASSERT(!SLJIT_IS_REG(SLJIT_REG_PAIR(SLJIT_R0, SLJIT_S0)));
	SLJIT_ASSERT(!SLJIT_IS_MEM(SLJIT_REG_PAIR(SLJIT_R0, SLJIT_S0)));
	SLJIT_ASSERT(!SLJIT_IS_IMM(SLJIT_REG_PAIR(SLJIT_R0, SLJIT_S0)));
	SLJIT_ASSERT(SLJIT_IS_REG_PAIR(SLJIT_REG_PAIR(SLJIT_R0, SLJIT_S0)));
}

static void test1(void)
{
	/* Enter and return from an sljit function. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);

	if (verbose)
		printf("Run test1\n");

	FAILED(!compiler, "cannot create compiler\n");

	/* 3 arguments passed, 3 arguments used. */
	sljit_emit_enter(compiler, 0, SLJIT_ARGS3(W, W, W, W), 3, 3, 0, 0, 0);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_S1, 0);

	SLJIT_ASSERT(sljit_get_generated_code_size(compiler) == 0);
	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	SLJIT_ASSERT(compiler->error == SLJIT_ERR_COMPILED);
	SLJIT_ASSERT(sljit_get_generated_code_size(compiler) > 0);
	sljit_free_compiler(compiler);

	FAILED(code.func3(3, -21, 86) != -21, "test1 case 1 failed\n");
	FAILED(code.func3(4789, 47890, 997) != 47890, "test1 case 2 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test2(void)
{
	/* Test mov. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_sw buf[10];
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
	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(W, P), 3, 2, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 9999);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_S0, 0);
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_MEM2(SLJIT_S1, SLJIT_R1), 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, 2);
	/* buf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM2(SLJIT_S1, SLJIT_S0), SLJIT_WORD_SHIFT, SLJIT_MEM2(SLJIT_S1, SLJIT_R1), 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, 3);
	/* buf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM2(SLJIT_S1, SLJIT_S0), SLJIT_WORD_SHIFT, SLJIT_MEM0(), (sljit_sw)&buf);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R0), (sljit_sw)&data);
	/* buf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&buf - 0x12345678);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R0), 0x12345678);
	/* buf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 5 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 3456);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)&buf - 0xff890 + 6 * (sljit_sw)sizeof(sljit_sw));
	/* buf[6] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), 0xff890, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)&buf + 0xff890 + 7 * (sljit_sw)sizeof(sljit_sw));
	/* buf[7] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), -0xff890, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&buf - 0xfff0ff + 8 * (sljit_sw)sizeof(sljit_sw));
	/* buf[8] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), 0xfff0ff, SLJIT_IMM, 7896);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&buf + 0xfff100 + 9 * (sljit_sw)sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, -2450);
	/* buf[9] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), -0xfff100, SLJIT_R1, 0);
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
	FAILED(buf[6] != 3456, "test2 case 7 failed\n");
	FAILED(buf[7] != 3456, "test2 case 8 failed\n");
	FAILED(buf[8] != 7896, "test2 case 9 failed\n");
	FAILED(buf[9] != -2450, "test2 case 10 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test3(void)
{
	/* Test not. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_sw buf[5];

	if (verbose)
		printf("Run test3\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 1234;
	buf[1] = 0;
	buf[2] = 9876;
	buf[3] = 0;
	buf[4] = 0x12345678;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(W, P), 3, 1, 0, 0, 0);
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op2(compiler, SLJIT_XOR, SLJIT_MEM0(), (sljit_sw)&buf[1], SLJIT_MEM0(), (sljit_sw)&buf[1], SLJIT_IMM, -1);
	/* buf[3] */
	sljit_emit_op2(compiler, SLJIT_XOR, SLJIT_RETURN_REG, 0, SLJIT_IMM, -1, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op2(compiler, SLJIT_XOR, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 2, SLJIT_IMM, -1);
	/* buf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)&buf[4] - 0xff0000 - 0x20);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, (sljit_sw)&buf[4] - 0xff0000);
	sljit_emit_op2(compiler, SLJIT_XOR, SLJIT_MEM1(SLJIT_R1), 0xff0000 + 0x20, SLJIT_IMM, -1, SLJIT_MEM1(SLJIT_R2), 0xff0000);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_sw)&buf) != ~1234, "test3 case 1 failed\n");
	FAILED(buf[1] != ~1234, "test3 case 2 failed\n");
	FAILED(buf[3] != ~9876, "test3 case 3 failed\n");
	FAILED(buf[4] != ~0x12345678, "test3 case 4 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test4(void)
{
	/* Test negate. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_sw buf[4];

	if (verbose)
		printf("Run test4\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	buf[1] = 1234;
	buf[2] = 0;
	buf[3] = 0;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(W, P, W), 3, 2, 0, 0, 0);
	/* buf[0] */
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_MEM0(), (sljit_sw)&buf[0], SLJIT_IMM, 0, SLJIT_MEM0(), (sljit_sw)&buf[1]);
	/* buf[2] */
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 2, SLJIT_IMM, 0, SLJIT_S1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 299);
	/* buf[3] */
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3, SLJIT_IMM, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 2);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func2((sljit_sw)&buf, 4567) != -4567, "test4 case 1 failed\n");
	FAILED(buf[0] != -1234, "test4 case 2 failed\n");
	FAILED(buf[2] != -4567, "test4 case 3 failed\n");
	FAILED(buf[3] != -299, "test4 case 4 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test5(void)
{
	/* Test add. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
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

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(W, P), 3, 2, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 50);
	/* buf[2] */
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), 1, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), 1, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), 0);
	/* buf[0] */
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, sizeof(sljit_sw) + 2);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 50);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);
	/* buf[5] */
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_IMM, 4, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_IMM, 50, SLJIT_R1, 0);
	/* buf[4] */
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_IMM, 50, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw));
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_R1, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_R1, 0, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw));
	/* buf[5] */
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw));
	/* buf[3] */
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0x1e7d39f2);
	/* buf[6] */
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_R1, 0, SLJIT_IMM, 0x23de7c06);
	/* buf[7] */
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_IMM, 0x3d72e452, SLJIT_R1, 0);
	/* buf[8] */
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw), SLJIT_IMM, -43, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw));
	/* Return value */
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

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test6(void)
{
	/* Test addc, sub, subc. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_sw buf[21];
	sljit_s32 i;

	if (verbose)
		printf("Run test6\n");

	FAILED(!compiler, "cannot create compiler\n");

	for (i = 0; i < 21; i++)
		buf[i] = 0;
	buf[10] = 4000;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(W, P), 3, 1, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -1);
	sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, -1);
	/* buf[0] */
	sljit_emit_op2(compiler, SLJIT_ADDC, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_IMM, 0, SLJIT_IMM, 0);
	sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R0, 0);
	/* buf[1] */
	sljit_emit_op2(compiler, SLJIT_ADDC, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_IMM, 4);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 100);
	/* buf[2] */
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 2, SLJIT_R0, 0, SLJIT_IMM, 50);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 6000);
	/* buf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3, SLJIT_IMM, 10);
	sljit_emit_op2(compiler, SLJIT_SUBC, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3, SLJIT_IMM, 5);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 100);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 2);
	/* buf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 4, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 5000);
	sljit_emit_op2(compiler, SLJIT_SUBC, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 4, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 4, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_R1, 0);
	/* buf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 5, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 5000);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R0, 0, SLJIT_IMM, 6000, SLJIT_R0, 0);
	/* buf[6] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 6, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 100);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, 32768);
	/* buf[7] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 7, SLJIT_R1, 0);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, -32767);
	/* buf[8] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 8, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x52cd3bf4);
	/* buf[9] */
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 9, SLJIT_R0, 0, SLJIT_IMM, 0x3da297c6);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 6000);
	/* buf[10] */
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 10, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 10, SLJIT_R1, 0);
	/* buf[11] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, -1);
	sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_SET_CARRY, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 1);
	sljit_emit_op2(compiler, SLJIT_ADDC, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 11, SLJIT_IMM, 0xff00ff, SLJIT_R1, 0);
	/* buf[12] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_CARRY, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 1);
	sljit_emit_op2(compiler, SLJIT_SUBC, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 12, SLJIT_R1, 0, SLJIT_IMM, 0xff00ff);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 5);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 4);
	/* buf[13] */
	sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_SET_CARRY, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 13, SLJIT_R0, 0, SLJIT_IMM, -2);
	/* buf[14] */
	sljit_emit_op2(compiler, SLJIT_ADDC, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 14, SLJIT_R1, 0, SLJIT_IMM, -2);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -5);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, -4);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, -2);
	sljit_emit_op2(compiler, SLJIT_SUBC, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, -2);
	/* buf[15] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 15, SLJIT_R0, 0);
	/* buf[16] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 16, SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 3);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 4);
	sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_SET_CARRY, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, -4);
	sljit_emit_op2(compiler, SLJIT_ADDC, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_IMM, -4);
	/* buf[17] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 17, SLJIT_R1, 0);
	/* buf[18] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 18, SLJIT_R2, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, -3);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, -4);
	/* buf[19] */
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_CARRY, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 19, SLJIT_R1, 0, SLJIT_IMM, -4);
	/* buf[20] */
	sljit_emit_op2(compiler, SLJIT_SUBC, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 20, SLJIT_R2, 0, SLJIT_IMM, -4);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0, SLJIT_IMM, 10);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_CARRY, SLJIT_RETURN_REG, 0, SLJIT_RETURN_REG, 0, SLJIT_IMM, 5);
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
	FAILED(buf[10] != -2000, "test6 case 12 failed\n");
	FAILED(buf[11] != 0xff0100, "test6 case 13 failed\n");
	FAILED(buf[12] != -0xff0101, "test6 case 14 failed\n");
	FAILED(buf[13] != 3, "test6 case 15 failed\n");
	FAILED(buf[14] != 3, "test6 case 16 failed\n");
	FAILED(buf[15] != -3, "test6 case 17 failed\n");
	FAILED(buf[16] != -3, "test6 case 18 failed\n");
	FAILED(buf[17] != -1, "test6 case 19 failed\n");
	FAILED(buf[18] != 0, "test6 case 20 failed\n");
	FAILED(buf[19] != 1, "test6 case 21 failed\n");
	FAILED(buf[20] != 0, "test6 case 22 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test7(void)
{
	/* Test logical operators. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
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
	buf[7] = (sljit_sw)0xc43a7f95;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(W, P), 3, 1, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0xf0C000);
	/* buf[2] */
	sljit_emit_op2(compiler, SLJIT_OR, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 2, SLJIT_R0, 0, SLJIT_IMM, 0x308f);
	/* buf[0] */
	sljit_emit_op2(compiler, SLJIT_XOR, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw));
	/* buf[3] */
	sljit_emit_op2(compiler, SLJIT_AND, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3, SLJIT_IMM, 0xf0f0f0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3);
	/* buf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0xC0F0);
	sljit_emit_op2(compiler, SLJIT_XOR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 5);
	sljit_emit_op2(compiler, SLJIT_OR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 0xff0000);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 4, SLJIT_R0, 0);
	/* buf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 0xC0F0);
	sljit_emit_op2(compiler, SLJIT_AND, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 5);
	sljit_emit_op2(compiler, SLJIT_OR, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 5, SLJIT_R2, 0, SLJIT_IMM, 0xff0000);
	/* buf[1] */
	sljit_emit_op2(compiler, SLJIT_XOR, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_IMM, 0xFFFFFF, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw));
	/* buf[6] */
	sljit_emit_op2(compiler, SLJIT_OR, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 6, SLJIT_IMM, (sljit_sw)0xa56c82c0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 6);
	/* buf[7] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 7);
	sljit_emit_op2(compiler, SLJIT_XOR, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 7, SLJIT_IMM, (sljit_sw)0xff00ff00, SLJIT_R0, 0);
	/* Return vaue */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)0xff00ff00);
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
	FAILED(buf[6] != (sljit_sw)0xa56c82c0, "test7 case 8 failed\n");
	FAILED(buf[7] != 0x3b3a8095, "test7 case 9 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test8(void)
{
	/* Test flags (neg, cmp, test). */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_sw buf[22];
	sljit_s32 i;

	if (verbose)
		printf("Run test8\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 100;
	for (i = 1; i < 21; i++)
		buf[i] = 3;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 3, 2, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 20);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 10);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_Z, SLJIT_IMM, 6, SLJIT_IMM, 5);
	/* buf[1] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_NOT_EQUAL);
	/* buf[2] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 2, SLJIT_EQUAL);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_GREATER, SLJIT_R0, 0, SLJIT_IMM, 3000);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_GREATER);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_LESS, SLJIT_R0, 0, SLJIT_IMM, 3000);
	/* buf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3, SLJIT_S1, 0);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_LESS);
	/* buf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 4, SLJIT_R2, 0);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_SIG_GREATER, SLJIT_R0, 0, SLJIT_IMM, -15);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_SIG_GREATER);
	/* buf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 5, SLJIT_R2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -(sljit_sw)(~(sljit_uw)0 >> 1) - 1);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_Z | SLJIT_SET_OVERFLOW, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_Z | SLJIT_SET_OVERFLOW, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_Z | SLJIT_SET_OVERFLOW, SLJIT_R1, 0, SLJIT_IMM, 0, SLJIT_R0, 0);
	/* buf[6] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 6, SLJIT_OVERFLOW);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -1);
	sljit_emit_op2(compiler, SLJIT_XOR | SLJIT_SET_Z, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, -1);
	/* buf[7] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 7, SLJIT_ZERO);
	sljit_emit_op2(compiler, SLJIT_XOR | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_IMM, -1, SLJIT_R1, 0);
	/* buf[8] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 8, SLJIT_ZERO);
	sljit_emit_op2u(compiler, SLJIT_AND | SLJIT_SET_Z, SLJIT_IMM, 0xffff, SLJIT_R0, 0);
	sljit_emit_op2u(compiler, SLJIT_AND | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_IMM, 0xffff);
	/* buf[9] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 9, SLJIT_NOT_ZERO);
	sljit_emit_op2u(compiler, SLJIT_AND | SLJIT_SET_Z, SLJIT_IMM, 0xffff, SLJIT_R1, 0);
	sljit_emit_op2u(compiler, SLJIT_AND | SLJIT_SET_Z, SLJIT_R1, 0, SLJIT_IMM, 0xffff);
	sljit_emit_op2u(compiler, SLJIT_AND | SLJIT_SET_Z, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R1, 0);
	sljit_emit_op2u(compiler, SLJIT_AND | SLJIT_SET_Z, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op2u(compiler, SLJIT_AND | SLJIT_SET_Z, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op2u(compiler, SLJIT_AND | SLJIT_SET_Z, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_IMM, 0x1);
	/* buf[10] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 10, SLJIT_NOT_ZERO);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -(sljit_sw)(~(sljit_uw)0 >> 1) - 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_OVERFLOW, SLJIT_R1, 0, SLJIT_R0, 0);
	/* buf[11] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 11, SLJIT_OVERFLOW);
	sljit_emit_op2u(compiler, SLJIT_ADD | SLJIT_SET_OVERFLOW, SLJIT_R1, 0, SLJIT_R0, 0);
	/* buf[12] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 12, SLJIT_OVERFLOW);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 9);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_Z | SLJIT_SET_LESS, SLJIT_R0, 0, SLJIT_IMM, 0, SLJIT_R0, 0);
	/* buf[13] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 13, SLJIT_LESS);
	/* buf[14] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 14, SLJIT_ZERO);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_Z | SLJIT_SET_LESS, SLJIT_R0, 0, SLJIT_IMM, 0, SLJIT_R0, 0);
	/* buf[15] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 15, SLJIT_LESS);
	/* buf[16] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 16, SLJIT_ZERO);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -9);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_SIG_LESS, SLJIT_R0, 0, SLJIT_IMM, 0, SLJIT_R0, 0);
	/* buf[17] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 17, SLJIT_SIG_LESS);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -9);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_SIG_GREATER, SLJIT_R0, 0, SLJIT_IMM, 0, SLJIT_R0, 0);
	/* buf[18] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 18, SLJIT_SIG_GREATER);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op2(compiler, SLJIT_OR | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 0);
	/* buf[19] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 19, SLJIT_ZERO);
	/* buf[20] */
	sljit_emit_op2u(compiler, SLJIT_OR | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 20, SLJIT_ZERO);
	sljit_emit_op2(compiler, SLJIT_XOR | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 0);
	/* buf[21] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 21, SLJIT_ZERO);

	sljit_emit_return_void(compiler);

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
	FAILED(buf[13] != 1, "test8 case 13 failed\n");
	FAILED(buf[14] != 0, "test8 case 14 failed\n");
	FAILED(buf[15] != 0, "test8 case 15 failed\n");
	FAILED(buf[16] != 1, "test8 case 16 failed\n");
	FAILED(buf[17] != 0, "test8 case 17 failed\n");
	FAILED(buf[18] != 1, "test8 case 18 failed\n");
	FAILED(buf[19] != 1, "test8 case 19 failed\n");
	FAILED(buf[20] != 0, "test8 case 20 failed\n");
	FAILED(buf[21] != 1, "test8 case 21 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test9(void)
{
	/* Test shift. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_sw buf[15];
	sljit_s32 i;
#ifdef SLJIT_PREF_SHIFT_REG
	sljit_s32 shift_reg = SLJIT_PREF_SHIFT_REG;
#else
	sljit_s32 shift_reg = SLJIT_R2;
#endif

	SLJIT_ASSERT(shift_reg >= SLJIT_R2 && shift_reg <= SLJIT_R3);

	if (verbose)
		printf("Run test9\n");

	FAILED(!compiler, "cannot create compiler\n");

	for (i = 0; i < 15; i++)
		buf[i] = -1;

	buf[4] = 1 << 10;
	buf[9] = 3;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 4, 2, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0xf);
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 3);
	sljit_emit_op2(compiler, SLJIT_LSHR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
	/* buf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
	/* buf[1] */
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R1, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -64);
	sljit_emit_op1(compiler, SLJIT_MOV, shift_reg, 0, SLJIT_IMM, 2);
	/* buf[2] */
	sljit_emit_op2(compiler, SLJIT_ASHR, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 2, SLJIT_R0, 0, shift_reg, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, shift_reg, 0, SLJIT_IMM, 0xff);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 4);
	sljit_emit_op2(compiler, SLJIT_SHL, shift_reg, 0, shift_reg, 0, SLJIT_R0, 0);
	/* buf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3, shift_reg, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, shift_reg, 0, SLJIT_IMM, 0xff);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 8);
	/* buf[4] */
	sljit_emit_op2(compiler, SLJIT_LSHR, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 4, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 4, SLJIT_R0, 0);
	/* buf[5] */
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 5, shift_reg, 0, SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, 0xf);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 2);
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_S1, 0, SLJIT_S1, 0, SLJIT_R0, 0);
	/* buf[6] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 6, SLJIT_S1, 0);
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_R0, 0, SLJIT_S1, 0, SLJIT_R0, 0);
	/* buf[7] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 7, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 0xf00);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 4);
	sljit_emit_op2(compiler, SLJIT_LSHR, SLJIT_R1, 0, SLJIT_R2, 0, SLJIT_R0, 0);
	/* buf[8] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 8, SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)buf);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 9);
	/* buf[9] */
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_MEM2(SLJIT_R0, SLJIT_R1), SLJIT_WORD_SHIFT, SLJIT_MEM2(SLJIT_R0, SLJIT_R1), SLJIT_WORD_SHIFT, SLJIT_MEM2(SLJIT_R0, SLJIT_R1), SLJIT_WORD_SHIFT);

	sljit_emit_op1(compiler, SLJIT_MOV, shift_reg, 0, SLJIT_IMM, 4);
	sljit_emit_op2(compiler, SLJIT_SHL, shift_reg, 0, SLJIT_IMM, 2, shift_reg, 0);
	/* buf[10] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 10, shift_reg, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0xa9);
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 0x7d00);
	sljit_emit_op2(compiler, SLJIT_LSHR32, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 32);
#if IS_64BIT
	sljit_emit_op1(compiler, SLJIT_MOV_U32, SLJIT_R0, 0, SLJIT_R0, 0);
#endif
	sljit_emit_op2(compiler, SLJIT_OR, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0xe30000);
#if IS_64BIT
	sljit_emit_op2(compiler, SLJIT_ASHR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 0xffc0);
#else
	sljit_emit_op2(compiler, SLJIT_ASHR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 0xffe0);
#endif
	sljit_emit_op2(compiler, SLJIT_OR, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 0x25000000);
	sljit_emit_op2(compiler, SLJIT_SHL32, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 0xfffe1);
#if IS_64BIT
	sljit_emit_op1(compiler, SLJIT_MOV_U32, SLJIT_R0, 0, SLJIT_R0, 0);
#endif
	/* buf[11] */
	sljit_emit_op2(compiler, SLJIT_OR, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 11, SLJIT_R1, 0, SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, shift_reg, 0, SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x5c);
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_R1, 0, SLJIT_R0, 0, shift_reg, 0);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 0xf600);
	sljit_emit_op2(compiler, SLJIT_LSHR32, SLJIT_R0, 0, SLJIT_R0, 0, shift_reg, 0);
#if IS_64BIT
	/* Alternative form of uint32 type cast. */
	sljit_emit_op2(compiler, SLJIT_AND, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 0xffffffff);
#endif
	sljit_emit_op2(compiler, SLJIT_OR, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x630000);
	sljit_emit_op2(compiler, SLJIT_ASHR, SLJIT_R0, 0, SLJIT_R0, 0, shift_reg, 0);
	/* buf[12] */
	sljit_emit_op2(compiler, SLJIT_OR, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 12, SLJIT_R1, 0, SLJIT_R0, 0);

	/* Test shift_reg keeps 64 bit value after 32 bit operation. */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, shift_reg, 0, SLJIT_IMM, -3062);
	sljit_emit_op2(compiler, SLJIT_ASHR32, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
	/* buf[13] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 13, shift_reg, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, shift_reg, 0, SLJIT_IMM, -4691);
	sljit_emit_op2(compiler, SLJIT_LSHR32, SLJIT_R0, 0, SLJIT_R1, 0, SLJIT_R0, 0);
	/* buf[14] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 14, shift_reg, 0);

	sljit_emit_return_void(compiler);

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
	FAILED(buf[13] != -3062, "test9 case 14 failed\n");
	FAILED(buf[14] != -4691, "test9 case 15 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test10(void)
{
	/* Test multiplications. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
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

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(W, P), 3, 1, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 5);
	sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);
	/* buf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 7);
	sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_R0, 0, SLJIT_R2, 0, SLJIT_IMM, 8);
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_R0, 0, SLJIT_IMM, -3, SLJIT_IMM, -4);
	/* buf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 2, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -2);
	/* buf[3] */
	sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, sizeof(sljit_sw) / 2);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)&buf[3]);
	/* buf[4] */
	sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_MEM2(SLJIT_R1, SLJIT_R0), 1, SLJIT_MEM2(SLJIT_R1, SLJIT_R0), 1, SLJIT_MEM2(SLJIT_R1, SLJIT_R0), 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 9);
	sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R0, 0);
	/* buf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 5, SLJIT_R0, 0);
#if IS_64BIT
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 3);
	sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_R0, 0, SLJIT_R1, 0, SLJIT_IMM, SLJIT_W(0x123456789));
	/* buf[6] */
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
#if IS_64BIT
	FAILED(buf[6] != SLJIT_W(0x123456789) * 3, "test10 case 8 failed\n");
#endif

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test11(void)
{
	/* Test rewritable constants. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	struct sljit_const* const1;
	struct sljit_const* const2;
	struct sljit_const* const3;
	struct sljit_const* const4;
	void* value;
	sljit_sw executable_offset;
	sljit_uw const1_addr;
	sljit_uw const2_addr;
	sljit_uw const3_addr;
	sljit_uw const4_addr;
	sljit_sw word_value1 = WCONST(0xaaaaaaaaaaaaaaaa, 0xaaaaaaaa);
	sljit_sw word_value2 = WCONST(0xfee1deadfbadf00d, 0xfbadf00d);
	sljit_sw buf[3];

	if (verbose)
		printf("Run test11\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(W, P), 3, 1, 0, 0, 0);

	SLJIT_ASSERT(!sljit_alloc_memory(compiler, 0));
	SLJIT_ASSERT(!sljit_alloc_memory(compiler, 16 * sizeof(sljit_sw) + 1));

	/* buf[0] */
	const1 = sljit_emit_const(compiler, SLJIT_MEM0(), (sljit_sw)&buf[0], -0x81b9);

	value = sljit_alloc_memory(compiler, 16 * sizeof(sljit_sw));
	if (value != NULL)
	{
		SLJIT_ASSERT(!((sljit_sw)value & ((sljit_sw)sizeof(sljit_sw) - 1)));
		memset(value, 255, 16 * sizeof(sljit_sw));
	}

	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 2);
	const2 = sljit_emit_const(compiler, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), SLJIT_WORD_SHIFT - 1, -65535);
	/* buf[2] */
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&buf[0] + 2 * (sljit_sw)sizeof(sljit_sw) - 2);
	const3 = sljit_emit_const(compiler, SLJIT_MEM1(SLJIT_R0), 0, word_value1);

	value = sljit_alloc_memory(compiler, 17);
	if (value != NULL)
	{
		SLJIT_ASSERT(!((sljit_sw)value & ((sljit_sw)sizeof(sljit_sw) - 1)));
		memset(value, 255, 16);
	}

	/* Return vaue */
	const4 = sljit_emit_const(compiler, SLJIT_RETURN_REG, 0, (sljit_sw)0xf7afcdb7);

	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	executable_offset = sljit_get_executable_offset(compiler);
	const1_addr = sljit_get_const_addr(const1);
	const2_addr = sljit_get_const_addr(const2);
	const3_addr = sljit_get_const_addr(const3);
	const4_addr = sljit_get_const_addr(const4);
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_sw)&buf) != (sljit_sw)0xf7afcdb7, "test11 case 1 failed\n");
	FAILED(buf[0] != -0x81b9, "test11 case 2 failed\n");
	FAILED(buf[1] != -65535, "test11 case 3 failed\n");
	FAILED(buf[2] != word_value1, "test11 case 4 failed\n");

	/* buf[0] */
	sljit_set_const(const1_addr, -1, executable_offset);
	/* buf[1] */
	sljit_set_const(const2_addr, word_value2, executable_offset);
	/* buf[2] */
	sljit_set_const(const3_addr, (sljit_sw)0xbab0fea1, executable_offset);
	/* Return vaue */
	sljit_set_const(const4_addr, -60089, executable_offset);

	FAILED(code.func1((sljit_sw)&buf) != -60089, "test11 case 5 failed\n");
	FAILED(buf[0] != -1, "test11 case 6 failed\n");
	FAILED(buf[1] != word_value2, "test11 case 7 failed\n");
	FAILED(buf[2] != (sljit_sw)0xbab0fea1, "test11 case 8 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test12(void)
{
	/* Test rewriteable jumps. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	struct sljit_label *label1;
	struct sljit_label *label2;
	struct sljit_label *label3;
	struct sljit_jump *jump1;
	struct sljit_jump *jump2;
	struct sljit_jump *jump3;
	sljit_sw executable_offset;
	void* value;
	sljit_uw jump1_addr;
	sljit_uw label1_addr;
	sljit_uw label2_addr;
	sljit_sw buf[1];

	if (verbose)
		printf("Run test12\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(VOID, P, W), 3, 2, 0, 0, 0);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_SIG_GREATER, SLJIT_S1, 0, SLJIT_IMM, 10);
	jump1 = sljit_emit_jump(compiler, SLJIT_REWRITABLE_JUMP | SLJIT_SIG_GREATER);
	/* Default handler. */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_IMM, 5);
	jump2 = sljit_emit_jump(compiler, SLJIT_JUMP);

	value = sljit_alloc_memory(compiler, 15);
	if (value != NULL)
	{
		SLJIT_ASSERT(!((sljit_sw)value & ((sljit_sw)sizeof(sljit_sw) - 1)));
		memset(value, 255, 15);
	}

	/* Handler 1. */
	label1 = sljit_emit_label(compiler);
	sljit_emit_op0(compiler, SLJIT_ENDBR);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_IMM, 6);
	jump3 = sljit_emit_jump(compiler, SLJIT_JUMP);
	/* Handler 2. */
	label2 = sljit_emit_label(compiler);
	sljit_emit_op0(compiler, SLJIT_ENDBR);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_IMM, 7);
	/* Exit. */
	label3 = sljit_emit_label(compiler);
	sljit_emit_op0(compiler, SLJIT_ENDBR);
	sljit_set_label(jump2, label3);
	sljit_set_label(jump3, label3);
	/* By default, set to handler 1. */
	sljit_set_label(jump1, label1);
	sljit_emit_return_void(compiler);

	value = sljit_alloc_memory(compiler, 8);
	if (value != NULL)
	{
		SLJIT_ASSERT(!((sljit_sw)value & ((sljit_sw)sizeof(sljit_sw) - 1)));
		memset(value, 255, 8);
	}

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	executable_offset = sljit_get_executable_offset(compiler);
	jump1_addr = sljit_get_jump_addr(jump1);
	label1_addr = sljit_get_label_addr(label1);
	label2_addr = sljit_get_label_addr(label2);
	sljit_free_compiler(compiler);

	code.func2((sljit_sw)&buf, 4);
	FAILED(buf[0] != 5, "test12 case 1 failed\n");

	code.func2((sljit_sw)&buf, 11);
	FAILED(buf[0] != 6, "test12 case 2 failed\n");

	sljit_set_jump_addr(jump1_addr, label2_addr, executable_offset);
	code.func2((sljit_sw)&buf, 12);
	FAILED(buf[0] != 7, "test12 case 3 failed\n");

	sljit_set_jump_addr(jump1_addr, label1_addr, executable_offset);
	code.func2((sljit_sw)&buf, 13);
	FAILED(buf[0] != 6, "test12 case 4 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test13(void)
{
	/* Test fpu monadic functions. */
	executable_code code;
	struct sljit_compiler* compiler;
	sljit_f64 buf[7];
	sljit_sw buf2[6];

	if (verbose)
		printf("Run test13\n");

	if (!sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		if (verbose)
			printf("no fpu available, test13 skipped\n");
		successful_tests++;
		return;
	}

	compiler = sljit_create_compiler(NULL, NULL);
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

	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(VOID, P, P), 3, 2, 6, 0, 0);
	/* buf[2] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM0(), (sljit_sw)&buf[2], SLJIT_MEM0(), (sljit_sw)&buf[1]);
	/* buf[3] */
	sljit_emit_fop1(compiler, SLJIT_ABS_F64, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_f64), SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64));
	/* buf[4] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM0(), (sljit_sw)&buf[0]);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 2 * sizeof(sljit_f64));
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), 0);
	sljit_emit_fop1(compiler, SLJIT_NEG_F64, SLJIT_FR2, 0, SLJIT_FR0, 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR3, 0, SLJIT_FR2, 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM0(), (sljit_sw)&buf[4], SLJIT_FR3, 0);
	/* buf[5] */
	sljit_emit_fop1(compiler, SLJIT_ABS_F64, SLJIT_FR4, 0, SLJIT_FR1, 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_f64), SLJIT_FR4, 0);
	/* buf[6] */
	sljit_emit_fop1(compiler, SLJIT_NEG_F64, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_f64), SLJIT_FR4, 0);

	/* buf2[0] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR5, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_CMP_F64 | SLJIT_SET_F_GREATER, SLJIT_FR5, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64));
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_F_GREATER);
	/* buf2[1] */
	sljit_emit_fop1(compiler, SLJIT_CMP_F64 | SLJIT_SET_F_GREATER, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64), SLJIT_FR5, 0);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_sw), SLJIT_F_GREATER);
	/* buf2[2] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, SLJIT_FR5, 0);
	sljit_emit_fop1(compiler, SLJIT_CMP_F64 | SLJIT_SET_F_EQUAL, SLJIT_FR1, 0, SLJIT_FR1, 0);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_sw), SLJIT_F_EQUAL);
	/* buf2[3] */
	sljit_emit_fop1(compiler, SLJIT_CMP_F64 | SLJIT_SET_F_LESS, SLJIT_FR1, 0, SLJIT_FR1, 0);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_sw), SLJIT_F_LESS);
	/* buf2[4] */
	sljit_emit_fop1(compiler, SLJIT_CMP_F64 | SLJIT_SET_F_EQUAL, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64));
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_sw), SLJIT_F_EQUAL);
	/* buf2[5] */
	sljit_emit_fop1(compiler, SLJIT_CMP_F64 | SLJIT_SET_F_NOT_EQUAL, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64));
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 5 * sizeof(sljit_sw), SLJIT_F_NOT_EQUAL);

	sljit_emit_return_void(compiler);

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

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test14(void)
{
	/* Test fpu diadic functions. */
	executable_code code;
	struct sljit_compiler* compiler;
	sljit_f64 buf[15];

	if (verbose)
		printf("Run test14\n");

	if (!sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		if (verbose)
			printf("no fpu available, test14 skipped\n");
		successful_tests++;
		return;
	}

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

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

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 3, 1, 6, 0, 0);

	/* ADD */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, sizeof(sljit_f64));
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64));
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64) * 2);
	/* buf[3] */
	sljit_emit_fop2(compiler, SLJIT_ADD_F64, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64) * 3, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop2(compiler, SLJIT_ADD_F64, SLJIT_FR0, 0, SLJIT_FR0, 0, SLJIT_FR1, 0);
	sljit_emit_fop2(compiler, SLJIT_ADD_F64, SLJIT_FR1, 0, SLJIT_FR0, 0, SLJIT_FR1, 0);
	/* buf[4] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64) * 4, SLJIT_FR0, 0);
	/* buf[5] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64) * 5, SLJIT_FR1, 0);

	/* SUB */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR3, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64) * 2);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 2);
	/* buf[6] */
	sljit_emit_fop2(compiler, SLJIT_SUB_F64, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64) * 6, SLJIT_FR3, 0, SLJIT_MEM2(SLJIT_S0, SLJIT_R1), SLJIT_F64_SHIFT);
	sljit_emit_fop2(compiler, SLJIT_SUB_F64, SLJIT_FR2, 0, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64) * 2);
	sljit_emit_fop2(compiler, SLJIT_SUB_F64, SLJIT_FR3, 0, SLJIT_FR2, 0, SLJIT_FR3, 0);
	/* buf[7] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64) * 7, SLJIT_FR2, 0);
	/* buf[8] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64) * 8, SLJIT_FR3, 0);

	/* MUL */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 1);
	/* buf[9] */
	sljit_emit_fop2(compiler, SLJIT_MUL_F64, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64) * 9, SLJIT_MEM2(SLJIT_S0, SLJIT_R1), SLJIT_F64_SHIFT, SLJIT_FR1, 0);
	sljit_emit_fop2(compiler, SLJIT_MUL_F64, SLJIT_FR1, 0, SLJIT_FR1, 0, SLJIT_FR2, 0);
	sljit_emit_fop2(compiler, SLJIT_MUL_F64, SLJIT_FR5, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64) * 2, SLJIT_FR2, 0);
	/* buf[10] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64) * 10, SLJIT_FR1, 0);
	/* buf[11] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64) * 11, SLJIT_FR5, 0);

	/* DIV */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR5, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64) * 12);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64) * 13);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR4, 0, SLJIT_FR5, 0);
	/* buf[12] */
	sljit_emit_fop2(compiler, SLJIT_DIV_F64, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64) * 12, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64) * 12, SLJIT_FR1, 0);
	sljit_emit_fop2(compiler, SLJIT_DIV_F64, SLJIT_FR5, 0, SLJIT_FR5, 0, SLJIT_FR1, 0);
	sljit_emit_fop2(compiler, SLJIT_DIV_F64, SLJIT_FR4, 0, SLJIT_FR1, 0, SLJIT_FR4, 0);
	/* buf[13] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64) * 13, SLJIT_FR5, 0);
	/* buf[14] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64) * 14, SLJIT_FR4, 0);

	sljit_emit_return_void(compiler);

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

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static sljit_sw func(sljit_sw a, sljit_sw b, sljit_sw c)
{
	return a + b + c + 5;
}

static sljit_sw func4(sljit_sw a, sljit_sw b, sljit_sw c, sljit_sw d)
{
	return func(a, b, c) + d;
}

static void test15(void)
{
	/* Test function call. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	struct sljit_jump* jump = NULL;
	sljit_sw buf[9];

	if (verbose)
		printf("Run test15\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 0;
	buf[5] = 0;
	buf[6] = 0;
	buf[7] = 0;
	buf[8] = SLJIT_FUNC_ADDR(func);

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(W, P), 4, 2, 0, 0, 0);

	/* buf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 5);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 7);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, -3);
	sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS3(W, W, W, W), SLJIT_IMM, SLJIT_FUNC_ADDR(func));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_RETURN_REG, 0);

	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -5);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, -10);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 2);
	jump = sljit_emit_call(compiler, SLJIT_CALL | SLJIT_REWRITABLE_JUMP, SLJIT_ARGS3(W, W, W, W));
	sljit_set_target(jump, (sljit_uw)-1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_RETURN_REG, 0);

	/* buf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_FUNC_ADDR(func));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 40);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, -3);
	sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS3(W, W, W, W), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_RETURN_REG, 0);

	/* buf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -60);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, SLJIT_FUNC_ADDR(func));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, -30);
	sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS3(W, W, W, W), SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_RETURN_REG, 0);

	/* buf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 10);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 16);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, SLJIT_FUNC_ADDR(func));
	sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS3(W, W, W, W), SLJIT_R2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_RETURN_REG, 0);

	/* buf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 100);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 110);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 120);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, SLJIT_FUNC_ADDR(func));
	sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS3(W, W, W, W), SLJIT_R3, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_RETURN_REG, 0);

	/* buf[6] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 2);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 3);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, SLJIT_FUNC_ADDR(func));
	sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS3(W, W, W, W), SLJIT_S1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_RETURN_REG, 0);

	/* buf[7] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 2);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 3);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, -6);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, SLJIT_FUNC_ADDR(func4));
	sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS4(W, W, W, W, W), SLJIT_S1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_RETURN_REG, 0);

	/* buf[8] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -10);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, -16);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 6);
	sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS3(W, W, W, W), SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw), SLJIT_RETURN_REG, 0);

	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_set_jump_addr(sljit_get_jump_addr(jump), SLJIT_FUNC_UADDR(func), sljit_get_executable_offset(compiler));
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_sw)&buf) != -15, "test15 case 1 failed\n");
	FAILED(buf[0] != 14, "test15 case 2 failed\n");
	FAILED(buf[1] != -8, "test15 case 3 failed\n");
	FAILED(buf[2] != SLJIT_FUNC_ADDR(func) + 42, "test15 case 4 failed\n");
	FAILED(buf[3] != SLJIT_FUNC_ADDR(func) - 85, "test15 case 5 failed\n");
	FAILED(buf[4] != SLJIT_FUNC_ADDR(func) + 31, "test15 case 6 failed\n");
	FAILED(buf[5] != 335, "test15 case 7 failed\n");
	FAILED(buf[6] != 11, "test15 case 8 failed\n");
	FAILED(buf[7] != 5, "test15 case 9 failed\n");
	FAILED(buf[8] != -15, "test15 case 10 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test16(void)
{
	/* Ackermann benchmark. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	struct sljit_label *entry;
	struct sljit_label *label;
	struct sljit_jump *jump;
	struct sljit_jump *jump1;
	struct sljit_jump *jump2;

	if (verbose)
		printf("Run test16\n");

	FAILED(!compiler, "cannot create compiler\n");

	entry = sljit_emit_label(compiler);
	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(W, W, W), 3, 2, 0, 0, 0);
	/* If x == 0. */
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_Z, SLJIT_S0, 0, SLJIT_IMM, 0);
	jump1 = sljit_emit_jump(compiler, SLJIT_EQUAL);
	/* If y == 0. */
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_Z, SLJIT_S1, 0, SLJIT_IMM, 0);
	jump2 = sljit_emit_jump(compiler, SLJIT_EQUAL);

	/* Ack(x,y-1). */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R1, 0, SLJIT_S1, 0, SLJIT_IMM, 1);
	jump = sljit_emit_call(compiler, SLJIT_CALL, SLJIT_ARGS2(W, W, W));
	sljit_set_label(jump, entry);

	/* Returns with Ack(x-1, Ack(x,y-1)). */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_RETURN_REG, 0);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_IMM, 1);
	jump = sljit_emit_call(compiler, SLJIT_CALL, SLJIT_ARGS2(W, W, W));
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
	jump = sljit_emit_call(compiler, SLJIT_CALL, SLJIT_ARGS2(W, W, W));
	sljit_set_label(jump, entry);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func2(3, 3) != 61, "test16 case 1 failed\n");
	/* For benchmarking. */
	/* FAILED(code.func2(3, 11) != 16381, "test16 case 1 failed\n"); */

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test17(void)
{
	/* Test arm constant pool. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_s32 i;
	sljit_sw buf[5];

	if (verbose)
		printf("Run test17\n");

	FAILED(!compiler, "cannot create compiler\n");

	for (i = 0; i < 5; i++)
		buf[i] = 0;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 3, 1, 0, 0, 0);
	for (i = 0; i <= 0xfff; i++) {
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)0x81818000 | i);
		if ((i & 0x3ff) == 0)
			/* buf[0-3] */
			sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), (i >> 10) * (sljit_sw)sizeof(sljit_sw), SLJIT_R0, 0);
	}
	/* buf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);
	FAILED((sljit_uw)buf[0] != 0x81818000, "test17 case 1 failed\n");
	FAILED((sljit_uw)buf[1] != 0x81818400, "test17 case 2 failed\n");
	FAILED((sljit_uw)buf[2] != 0x81818800, "test17 case 3 failed\n");
	FAILED((sljit_uw)buf[3] != 0x81818c00, "test17 case 4 failed\n");
	FAILED((sljit_uw)buf[4] != 0x81818fff, "test17 case 5 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test18(void)
{
	/* Test 64 bit. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
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
#if IS_64BIT && (defined SLJIT_BIG_ENDIAN && SLJIT_BIG_ENDIAN)
	buf[10] = SLJIT_W(1) << 32;
#else
	buf[10] = 1;
#endif

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 3, 2, 0, 0, 0);

#if IS_64BIT
	/* buf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_IMM, SLJIT_W(0x1122334455667788));
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV_U32, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_IMM, SLJIT_W(0x1122334455667788));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(1000000000000));
	/* buf[2] */
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 2, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(1000000000000));
	/* buf[3] */
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3, SLJIT_IMM, SLJIT_W(5000000000000), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, SLJIT_W(0x1108080808));
	/* buf[4] */
	sljit_emit_op2(compiler, SLJIT_ADD32, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 4, SLJIT_R1, 0, SLJIT_IMM, SLJIT_W(0x1120202020));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(0x1108080808));
	sljit_emit_op2u(compiler, SLJIT_AND | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(0x1120202020));
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_ZERO);
	/* buf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 5, SLJIT_S1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op2u(compiler, SLJIT_AND32 | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(0x1120202020));
	/* buf[6] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 6, SLJIT_ZERO);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(0x1108080808));
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_LESS, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(0x2208080808));
	/* buf[7] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 7, SLJIT_LESS);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op2u(compiler, SLJIT_AND32 | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(0x1104040404));
	/* buf[8] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 8, SLJIT_NOT_ZERO);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 4);
	/* buf[9] */
	sljit_emit_op2(compiler, SLJIT_SHL32, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 9, SLJIT_IMM, SLJIT_W(0xffff0000), SLJIT_R0, 0);
	/* buf[10] */
	sljit_emit_op2(compiler, SLJIT_MUL32, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 10, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 10, SLJIT_IMM, -1);
#else /* !IS_64BIT */
	/* buf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_IMM, 0x11223344);
	/* buf[1] */
	sljit_emit_op2(compiler, SLJIT_ADD32, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_IMM, 0x44332211);
#endif /* IS_64BIT */

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);
#if IS_64BIT
	FAILED(buf[0] != SLJIT_W(0x1122334455667788), "test18 case 1 failed\n");
#if (defined SLJIT_LITTLE_ENDIAN && SLJIT_LITTLE_ENDIAN)
	FAILED(buf[1] != 0x55667788, "test18 case 2 failed\n");
#else /* !SLJIT_LITTLE_ENDIAN */
	FAILED(buf[1] != SLJIT_W(0x5566778800000000), "test18 case 2 failed\n");
#endif /* SLJIT_LITTLE_ENDIAN */
	FAILED(buf[2] != SLJIT_W(2000000000000), "test18 case 3 failed\n");
	FAILED(buf[3] != SLJIT_W(4000000000000), "test18 case 4 failed\n");
#if (defined SLJIT_LITTLE_ENDIAN && SLJIT_LITTLE_ENDIAN)
	FAILED(buf[4] != 0x28282828, "test18 case 5 failed\n");
#else /* !SLJIT_LITTLE_ENDIAN */
	FAILED(buf[4] != SLJIT_W(0x2828282800000000), "test18 case 5 failed\n");
#endif /* SLJIT_LITTLE_ENDIAN */
	FAILED(buf[5] != 0, "test18 case 6 failed\n");
	FAILED(buf[6] != 1, "test18 case 7 failed\n");
	FAILED(buf[7] != 1, "test18 case 8 failed\n");
	FAILED(buf[8] != 0, "test18 case 9 failed\n");
#if (defined SLJIT_LITTLE_ENDIAN && SLJIT_LITTLE_ENDIAN)
	FAILED(buf[9] != (sljit_sw)0xfff00000, "test18 case 10 failed\n");
	FAILED(buf[10] != (sljit_sw)0xffffffff, "test18 case 11 failed\n");
#else /* !SLJIT_LITTLE_ENDIAN */
	FAILED(buf[9] != (sljit_sw)SLJIT_W(0xfff0000000000000), "test18 case 10 failed\n");
	FAILED(buf[10] != (sljit_sw)SLJIT_W(0xffffffff00000000), "test18 case 11 failed\n");
#endif /* SLJIT_LITTLE_ENDIAN */
#else /* !IS_64BIT */
	FAILED(buf[0] != 0x11223344, "test18 case 1 failed\n");
	FAILED(buf[1] != 0x44332211, "test18 case 2 failed\n");
#endif /* IS_64BIT */

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test19(void)
{
	/* Test arm partial instruction caching. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
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

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 3, 1, 0, 0, 0);
	/* buf[0] */
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw));
	/* buf[2] */
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM0(), (sljit_sw)&buf[2], SLJIT_MEM0(), (sljit_sw)&buf[1], SLJIT_MEM0(), (sljit_sw)&buf[0]);
	/* buf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, sizeof(sljit_sw));
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3, SLJIT_MEM1(SLJIT_R0), (sljit_sw)&buf[0], SLJIT_MEM1(SLJIT_R1), (sljit_sw)&buf[0]);
	/* buf[4] */
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 4, SLJIT_MEM0(), (sljit_sw)&buf[0], SLJIT_IMM, 2);
	/* buf[5] */
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 5, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_MEM1(SLJIT_R0), (sljit_sw)&buf[0] + 4 * (sljit_sw)sizeof(sljit_sw));
	/* buf[7] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 7, SLJIT_IMM, 10);
	/* buf[6] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 7);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_R1), (sljit_sw)&buf[5], SLJIT_MEM2(SLJIT_S0, SLJIT_R0), SLJIT_WORD_SHIFT, SLJIT_MEM1(SLJIT_R1), (sljit_sw)&buf[5]);

	sljit_emit_return_void(compiler);

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

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test20(void)
{
	/* Test stack. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	struct sljit_jump* jump;
	struct sljit_label* label;
	sljit_sw buf[6];
	sljit_sw offset_value = WCONST(0x1234567812345678, 0x12345678);

	if (verbose)
		printf("Run test20\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 5;
	buf[1] = 12;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 111;
	buf[5] = -12345;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(W, P), 5, 5, 0, 0, 4 * sizeof(sljit_sw));
	/* buf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_uw), SLJIT_MEM1(SLJIT_S0), 0);
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R4, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S3, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S4, 0, SLJIT_IMM, -1);
	/* buf[2] */
	sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_uw), SLJIT_MEM1(SLJIT_SP), 0, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_uw));
	/* buf[3] */
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_uw), SLJIT_MEM1(SLJIT_SP), sizeof(sljit_uw), SLJIT_MEM1(SLJIT_SP), 0);
	sljit_get_local_base(compiler, SLJIT_R0, 0, -offset_value);
	sljit_get_local_base(compiler, SLJIT_MEM1(SLJIT_S0), 0, -0x1234);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_S0), 0);
	/* buf[4] */
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_uw), SLJIT_MEM1(SLJIT_R0), offset_value, SLJIT_MEM1(SLJIT_R1), 0x1234 + sizeof(sljit_sw));
	/* buf[5] */
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_uw));
	/* Dummy last instructions. */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, 23);
	sljit_emit_label(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_sw)&buf) != -12345, "test20 case 1 failed\n")

	FAILED(buf[2] != 60, "test20 case 2 failed\n");
	FAILED(buf[3] != 17, "test20 case 3 failed\n");
	FAILED(buf[4] != 7, "test20 case 4 failed\n");

	sljit_free_code(code.code, NULL);

	compiler = sljit_create_compiler(NULL, NULL);
	sljit_emit_enter(compiler, 0, SLJIT_ARGS3(W, W, W, W), 3, 3, 0, 0, SLJIT_MAX_LOCAL_SIZE);

	sljit_get_local_base(compiler, SLJIT_R0, 0, SLJIT_MAX_LOCAL_SIZE - sizeof(sljit_sw));
	sljit_get_local_base(compiler, SLJIT_R1, 0, -(sljit_sw)sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, -1);
	label = sljit_emit_label(compiler);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), 0, SLJIT_R2, 0);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_Z, SLJIT_R1, 0, SLJIT_R0, 0);
	jump = sljit_emit_jump(compiler, SLJIT_NOT_EQUAL);
	sljit_set_label(jump, label);

	/* Saved registers should keep their value. */
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_S0, 0, SLJIT_S0, 0, SLJIT_S1, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_S2, 0);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_R0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func3(1234, 5678, 9012) != 15924, "test20 case 5 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test21(void)
{
	/* Test set context. The parts of the jit code can be separated in the memory. */
	executable_code code1;
	executable_code code2;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	struct sljit_jump* jump = NULL;
	sljit_uw addr;
	sljit_sw executable_offset;
	sljit_sw buf[4];

	if (verbose)
		printf("Run test21\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 9;
	buf[1] = -6;
	buf[2] = 0;
	buf[3] = 0;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(W, P), 3, 2, 0, 0, 2 * sizeof(sljit_sw));

	/* Return value */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_IMM, 10);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S0), 0, SLJIT_MEM1(SLJIT_SP), 0);

	jump = sljit_emit_jump(compiler, SLJIT_JUMP | SLJIT_REWRITABLE_JUMP);
	sljit_set_target(jump, 0);

	code1.code = sljit_generate_code(compiler);
	CHECK(compiler);

	executable_offset = sljit_get_executable_offset(compiler);
	addr = sljit_get_jump_addr(jump);

	sljit_free_compiler(compiler);

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	/* Other part of the jit code. */
	sljit_set_context(compiler, 0, 1, 3, 2, 0, 0, 2 * sizeof(sljit_sw));

	sljit_emit_op0(compiler, SLJIT_ENDBR);
	/* buf[2] */
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 2, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_MEM1(SLJIT_SP), 0);
	/* buf[3] */
	sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_MEM1(SLJIT_SP), 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_sw));

	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);

	code2.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	sljit_set_jump_addr(addr, SLJIT_FUNC_UADDR(code2.code), executable_offset);

	FAILED(code1.func1((sljit_sw)&buf) != 19, "test21 case 1 failed\n");
	FAILED(buf[2] != -16, "test21 case 2 failed\n");
	FAILED(buf[3] != 100, "test21 case 3 failed\n");

	sljit_free_code(code1.code, NULL);
	sljit_free_code(code2.code, NULL);
	successful_tests++;
}

static void test22(void)
{
	/* Test simple byte and half-int data transfers. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_sw buf[4];
	sljit_s16 sbuf[9];
	sljit_s8 bbuf[5];

	if (verbose)
		printf("Run test22\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = -1;
	buf[3] = -1;

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

	sljit_emit_enter(compiler, 0, SLJIT_ARGS3(VOID, P, P, P), 3, 3, 0, 0, 0);

	/* sbuf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV_S16, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_IMM, -13);
	/* sbuf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV_U16, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_s16), SLJIT_IMM, 0x1234);
	sljit_emit_op1(compiler, SLJIT_MOV_S16, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_s16));
	/* buf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_S1, 0, SLJIT_S1, 0, SLJIT_IMM, 2 * sizeof(sljit_s16));
	/* sbuf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV_U16, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_s16), SLJIT_MEM1(SLJIT_S1), -(sljit_sw)sizeof(sljit_s16));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0xff0000 + 8000);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 2);
	/* sbuf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV_S16, SLJIT_MEM2(SLJIT_S1, SLJIT_R1), 1, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R2, 0, SLJIT_S1, 0, SLJIT_IMM, 0x1234 - 3 * sizeof(sljit_s16));
	/* sbuf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV_S16, SLJIT_MEM1(SLJIT_R2), 0x1234, SLJIT_IMM, -9317);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R2, 0, SLJIT_S1, 0, SLJIT_IMM, 0x1234 + 4 * sizeof(sljit_s16));
	/* sbuf[6] */
	sljit_emit_op1(compiler, SLJIT_MOV_S16, SLJIT_MEM1(SLJIT_R2), -0x1234, SLJIT_IMM, -9317);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R2, 0, SLJIT_S1, 0, SLJIT_IMM, 0x12348 - 5 * sizeof(sljit_s16));
	/* sbuf[7] */
	sljit_emit_op1(compiler, SLJIT_MOV_S16, SLJIT_MEM1(SLJIT_R2), 0x12348, SLJIT_IMM, -8888);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R2, 0, SLJIT_S1, 0, SLJIT_IMM, 0x12348 + 6 * sizeof(sljit_s16));
	/* sbuf[8] */
	sljit_emit_op1(compiler, SLJIT_MOV_S16, SLJIT_MEM1(SLJIT_R2), -0x12348, SLJIT_IMM, -8888);

	/* bbuf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV_S8, SLJIT_MEM1(SLJIT_S2), 0, SLJIT_IMM, -45);
	/* sbuf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV_U8, SLJIT_MEM1(SLJIT_S2), sizeof(sljit_s8), SLJIT_IMM, 0x12);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 4 * sizeof(sljit_s8));
	sljit_emit_op1(compiler, SLJIT_MOV_S8, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_S2), 2 * sizeof(sljit_s8));
	sljit_emit_op1(compiler, SLJIT_MOV_S8, SLJIT_S1, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_U8, SLJIT_S1, 0, SLJIT_S1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_S8, SLJIT_R2, 0, SLJIT_S1, 0);
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R2, 0);
	/* bbuf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV_U8, SLJIT_MEM1(SLJIT_S2), 3 * sizeof(sljit_s8), SLJIT_S1, 0);
	/* bbuf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV_U8, SLJIT_MEM2(SLJIT_S2, SLJIT_R0), 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV_U16, SLJIT_R0, 0, SLJIT_IMM, 0);
	/* buf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	/* buf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func3((sljit_sw)&buf, (sljit_sw)&sbuf, (sljit_sw)&bbuf);
	FAILED(buf[0] != -9, "test22 case 1 failed\n");
	FAILED(buf[1] != -56, "test22 case 2 failed\n");
	FAILED(buf[2] != 0, "test22 case 3 failed\n");
	FAILED(buf[3] != 0, "test22 case 4 failed\n");

	FAILED(sbuf[0] != -13, "test22 case 5 failed\n");
	FAILED(sbuf[1] != 0x1234, "test22 case 6 failed\n");
	FAILED(sbuf[3] != 0x1234, "test22 case 7 failed\n");
	FAILED(sbuf[4] != 8000, "test22 case 8 failed\n");
	FAILED(sbuf[5] != -9317, "test22 case 9 failed\n");
	FAILED(sbuf[6] != -9317, "test22 case 10 failed\n");
	FAILED(sbuf[7] != -8888, "test22 case 11 failed\n");
	FAILED(sbuf[8] != -8888, "test22 case 12 failed\n");

	FAILED(bbuf[0] != -45, "test22 case 13 failed\n");
	FAILED(bbuf[1] != 0x12, "test22 case 14 failed\n");
	FAILED(bbuf[3] != -56, "test22 case 15 failed\n");
	FAILED(bbuf[4] != 4, "test22 case 16 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test23(void)
{
	/* Test 32 bit / 64 bit signed / unsigned int transfer and conversion.
	   This test has do real things on 64 bit systems, but works on 32 bit systems as well. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_sw buf[9];
	sljit_s32 ibuf[5];
	union {
		sljit_s32 asint;
		sljit_u8 asbytes[4];
	} u;
	sljit_sw garbage = WCONST(0x1234567812345678, 0x12345678);

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

	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(W, P, P), 3, 3, 0, 0, 0);
	/* ibuf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV_U32, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_IMM, 34567);
	/* ibuf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 4);
	sljit_emit_op1(compiler, SLJIT_MOV_S32, SLJIT_MEM2(SLJIT_S1, SLJIT_R0), 0, SLJIT_IMM, -7654);
	/* buf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, garbage);
	sljit_emit_op1(compiler, SLJIT_MOV_S32, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_s32));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, garbage);
	sljit_emit_op1(compiler, SLJIT_MOV_U32, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_s32));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R0, 0);
	/* buf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, garbage);
	sljit_emit_op1(compiler, SLJIT_MOV_S32, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_s32));
	sljit_emit_op1(compiler, SLJIT_MOV_U32, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_R0, 0);
	/* buf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x0f00f00);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R1, 0, SLJIT_S0, 0, SLJIT_IMM, 0x7777);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), 0x7777 + 3 * sizeof(sljit_sw), SLJIT_R0, 0);
	/* buf[4] */
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S0, 0, SLJIT_IMM, 0x7777);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), -0x7777 + 4 * (sljit_sw)sizeof(sljit_sw), SLJIT_R0, 0);
	/* buf[5] */
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S0, 0, SLJIT_IMM, 5 * sizeof(sljit_sw));
	sljit_emit_op2(compiler, SLJIT_LSHR, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM2(SLJIT_R1, SLJIT_R1), 0, SLJIT_IMM, 16);
	/* buf[7] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op2(compiler, SLJIT_OR, SLJIT_MEM2(SLJIT_R0, SLJIT_R1), 1, SLJIT_IMM, 64, SLJIT_MEM2(SLJIT_R0, SLJIT_R1), 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM0(), (sljit_sw)&buf[7], SLJIT_IMM, 0x123456);
	/* buf[6] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), (sljit_sw)&buf[6], SLJIT_MEM0(), (sljit_sw)&buf[7]);
	/* buf[7] */
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S0, 0, SLJIT_IMM, 5 * sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), 2 * sizeof(sljit_sw), SLJIT_R1, 0);
	/* buf[8] */
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R2, 0, SLJIT_S0, 0, SLJIT_IMM, 7 * sizeof(sljit_sw));
	sljit_emit_op2(compiler, SLJIT_LSHR, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_MEM2(SLJIT_R2, SLJIT_R2), 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&buf[8] - 0x12340);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), 0x12340, SLJIT_R2, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_R0), 0x12340, SLJIT_MEM1(SLJIT_R2), 3 * sizeof(sljit_sw), SLJIT_IMM, 6);
	/* ibuf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV_S32, SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_s32), SLJIT_IMM, 0x12345678);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0x2bd700 | 243);
	sljit_emit_return(compiler, SLJIT_MOV_S8, SLJIT_R1, 0);

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
	FAILED(buf[8] != (sljit_sw)&buf[5] + 6, "test23 case 10 failed\n");

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

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test24(void)
{
	/* Some complicated addressing modes. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_sw buf[9];
	sljit_s16 sbuf[5];
	sljit_s8 bbuf[7];

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

	sljit_emit_enter(compiler, 0, SLJIT_ARGS3(VOID, P, P, P), 3, 3, 0, 0, 0);

	/* Nothing should be updated. */
	/* sbuf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV_S16, SLJIT_MEM0(), (sljit_sw)&sbuf[1], SLJIT_MEM0(), (sljit_sw)&sbuf[0]);
	/* bbuf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV_S8, SLJIT_MEM0(), (sljit_sw)&bbuf[1], SLJIT_MEM0(), (sljit_sw)&bbuf[0]);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 2);
	/* sbuf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV_U16, SLJIT_MEM2(SLJIT_S1, SLJIT_R0), 1, SLJIT_MEM0(), (sljit_sw)&sbuf[3]);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&buf[0]);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 2);
	/* buf[2] */
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM2(SLJIT_R0, SLJIT_R2), SLJIT_WORD_SHIFT, SLJIT_MEM0(), (sljit_sw)&buf[0], SLJIT_MEM2(SLJIT_R1, SLJIT_R0), 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, sizeof(sljit_s8));
	/* bbuf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV_U8, SLJIT_MEM1(SLJIT_R0), (sljit_sw)&bbuf[1], SLJIT_MEM1(SLJIT_R0), (sljit_sw)&bbuf[2]);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, sizeof(sljit_s16));
	/* sbuf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV_S16, SLJIT_MEM1(SLJIT_R1), (sljit_sw)&sbuf[3], SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 3);
	/* buf[3] */
	sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), SLJIT_WORD_SHIFT, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), SLJIT_WORD_SHIFT, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), SLJIT_WORD_SHIFT);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 4);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_S0, 0);
	/* buf[4] */
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), SLJIT_WORD_SHIFT, SLJIT_MEM2(SLJIT_R1, SLJIT_R0), SLJIT_WORD_SHIFT, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), SLJIT_WORD_SHIFT);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_IMM, 9 * sizeof(sljit_sw));
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S0, 0, SLJIT_IMM, 4 * sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, -(4 << SLJIT_WORD_SHIFT));
	/* buf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM2(SLJIT_R0, SLJIT_R2), 0, SLJIT_MEM2(SLJIT_R1, SLJIT_R2), 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&buf - 0x7fff8000 + 6 * (sljit_sw)sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 952467);
	/* buf[6] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), 0x7fff8000, SLJIT_R1, 0);
	/* buf[7] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), 0x7fff8000 + sizeof(sljit_sw), SLJIT_MEM1(SLJIT_R0), 0x7fff8000);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&buf + 0x7fff7fff + 6 * (sljit_sw)sizeof(sljit_sw));
	/* buf[8] */
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_R0), -0x7fff7fff + 2 * (sljit_sw)sizeof(sljit_sw), SLJIT_MEM1(SLJIT_R0), -0x7fff7fff + (sljit_sw)sizeof(sljit_sw), SLJIT_MEM1(SLJIT_R0), -0x7fff7fff);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&bbuf - 0x7fff7ffe + 3 * (sljit_sw)sizeof(sljit_s8));
	/* bbuf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV_S8, SLJIT_MEM1(SLJIT_R0), 0x7fff7fff, SLJIT_MEM1(SLJIT_R0), 0x7fff7ffe);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&bbuf + 0x7fff7fff + 5 * (sljit_sw)sizeof(sljit_s8));
	/* bbuf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV_S8, SLJIT_MEM1(SLJIT_R0), -0x7fff7fff, SLJIT_MEM1(SLJIT_R0), -0x7fff8000);
#if IS_64BIT
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&bbuf - SLJIT_W(0x123456123456));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)&bbuf - SLJIT_W(0x123456123456));
	/* bbuf[6] */
	sljit_emit_op1(compiler, SLJIT_MOV_S8, SLJIT_MEM1(SLJIT_R0), SLJIT_W(0x123456123456) + 6 * sizeof(sljit_s8), SLJIT_MEM1(SLJIT_R1), SLJIT_W(0x123456123456));
#endif /* IS_64BIT */

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func3((sljit_sw)&buf, (sljit_sw)&sbuf, (sljit_sw)&bbuf);
	FAILED(buf[2] != 176366, "test24 case 1 failed\n");
	FAILED(buf[3] != 64, "test24 case 2 failed\n");
	FAILED(buf[4] != -100, "test24 case 3 failed\n");
	FAILED(buf[5] != 100567, "test24 case 4 failed\n");
	FAILED(buf[6] != 952467, "test24 case 5 failed\n");
	FAILED(buf[7] != 952467, "test24 case 6 failed\n");
	FAILED(buf[8] != 952467 * 2, "test24 case 7 failed\n");

	FAILED(sbuf[1] != 30000, "test24 case 8 failed\n");
	FAILED(sbuf[2] != -12345, "test24 case 9 failed\n");
	FAILED(sbuf[4] != sizeof(sljit_s16), "test24 case 10 failed\n");

	FAILED(bbuf[1] != -128, "test24 case 11 failed\n");
	FAILED(bbuf[2] != 99, "test24 case 12 failed\n");
	FAILED(bbuf[4] != 99, "test24 case 13 failed\n");
	FAILED(bbuf[5] != 99, "test24 case 14 failed\n");
#if IS_64BIT
	FAILED(bbuf[6] != -128, "test24 case 15 failed\n");
#endif

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test25(void)
{
#if IS_64BIT
	/* 64 bit loads. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
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

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 3, 1, 0, 0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 1 * sizeof(sljit_sw), SLJIT_IMM, 0x7fff);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_IMM, -0x8000);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_IMM, 0x7fffffff);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_IMM, SLJIT_W(-0x80000000));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_IMM, SLJIT_W(0x1234567887654321));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_IMM, SLJIT_W(0xff80000000));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_IMM, SLJIT_W(0x3ff0000000));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw), SLJIT_IMM, (sljit_sw)SLJIT_W(0xfffffff800100000));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_sw), SLJIT_IMM, (sljit_sw)SLJIT_W(0xfffffff80010f000));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 10 * sizeof(sljit_sw), SLJIT_IMM, SLJIT_W(0x07fff00000008001));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 11 * sizeof(sljit_sw), SLJIT_IMM, SLJIT_W(0x07fff00080010000));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 12 * sizeof(sljit_sw), SLJIT_IMM, SLJIT_W(0x07fff00080018001));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 13 * sizeof(sljit_sw), SLJIT_IMM, SLJIT_W(0x07fff00ffff00000));

	sljit_emit_return_void(compiler);

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

	sljit_free_code(code.code, NULL);
#endif /* IS_64BIT */
	successful_tests++;
}

static void test26(void)
{
	/* Aligned access without aligned offsets. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_sw buf[4];
	sljit_s32 ibuf[4];
	sljit_f64 dbuf[4];

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

	sljit_emit_enter(compiler, 0, SLJIT_ARGS3(VOID, P, P, P), 3, 3, 0, 0, 0);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_S0, 0, SLJIT_S0, 0, SLJIT_IMM, 3);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_S1, 0, SLJIT_S1, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), -3);
	/* ibuf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV_S32, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_s32) - 1, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_S32, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S1), -1);
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) - 3, SLJIT_R0, 0);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_IMM, 100);
	/* buf[2] */
	sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_MEM1(SLJIT_R0), (sljit_sw)sizeof(sljit_sw) * 2 - 103, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 2 - 3, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 3 - 3);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S1, 0, SLJIT_IMM, 100);
	/* ibuf[2] */
	sljit_emit_op2(compiler, SLJIT_MUL32, SLJIT_MEM1(SLJIT_R0), (sljit_sw)sizeof(sljit_s32) * 2 - 101, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_s32) * 2 - 1, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_s32) * 3 - 1);

	if (sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_S2, 0, SLJIT_S2, 0, SLJIT_IMM, 3);
		/* dbuf[1] */
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S2), sizeof(sljit_f64) - 3, SLJIT_MEM1(SLJIT_S2), -3);
		/* dbuf[2] */
		sljit_emit_fop2(compiler, SLJIT_ADD_F64, SLJIT_MEM1(SLJIT_S2), sizeof(sljit_f64) * 2 - 3, SLJIT_MEM1(SLJIT_S2), -3, SLJIT_MEM1(SLJIT_S2), sizeof(sljit_f64) - 3);
		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S2, 0, SLJIT_IMM, 2);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sizeof(sljit_f64) * 3 - 4) >> 1);
		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R2, 0, SLJIT_S2, 0, SLJIT_IMM, 1);
		/* dbuf[3] */
		sljit_emit_fop2(compiler, SLJIT_DIV_F64, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_f64) * 3 - 5, SLJIT_MEM1(SLJIT_S2), sizeof(sljit_f64) * 2 - 3, SLJIT_MEM2(SLJIT_R2, SLJIT_R1), 1);
	}

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func3((sljit_sw)&buf, (sljit_sw)&ibuf, (sljit_sw)&dbuf);

	FAILED(buf[1] != -689, "test26 case 1 failed\n");
	FAILED(buf[2] != -16, "test26 case 2 failed\n");
	FAILED(ibuf[1] != -2789, "test26 case 3 failed\n");
	FAILED(ibuf[2] != -18, "test26 case 4 failed\n");

	if (sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		FAILED(dbuf[1] != 5.75, "test26 case 5 failed\n");
		FAILED(dbuf[2] != 11.5, "test26 case 6 failed\n");
		FAILED(dbuf[3] != -2.875, "test26 case 7 failed\n");
	}

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test27(void)
{
#define SET_NEXT_BYTE(type) \
		cond_set(compiler, SLJIT_R2, 0, type); \
		sljit_emit_op1(compiler, SLJIT_MOV_U8, SLJIT_MEM1(SLJIT_S0), 1, SLJIT_R2, 0); \
		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_S0, 0, SLJIT_S0, 0, SLJIT_IMM, 1);
#if IS_64BIT
#define RESULT(i) i
#else
#define RESULT(i) (3 - i)
#endif

	/* Playing with conditional flags. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_u8 buf[41];
	sljit_u32 i;
#ifdef SLJIT_PREF_SHIFT_REG
	sljit_s32 shift_reg = SLJIT_PREF_SHIFT_REG;
#else
	sljit_s32 shift_reg = SLJIT_R2;
#endif

	SLJIT_ASSERT(shift_reg >= SLJIT_R2 && shift_reg <= SLJIT_R3);

	if (verbose)
		printf("Run test27\n");

	for (i = 0; i < sizeof(buf); ++i)
		buf[i] = 10;

	FAILED(!compiler, "cannot create compiler\n");

	/* 3 arguments passed, 3 arguments used. */
	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 4, 3, 0, 0, 0);

	/* buf[0] */
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_S0, 0, SLJIT_S0, 0, SLJIT_IMM, 1);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x1001);
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 20);
	/* 0x100100000 on 64 bit machines, 0x100000 on 32 bit machines. */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0x800000);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_GREATER, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op0(compiler, SLJIT_ENDBR); /* ENDBR should keep the flags. */
	sljit_emit_op0(compiler, SLJIT_NOP); /* Nop should keep the flags. */
	SET_NEXT_BYTE(SLJIT_GREATER);
	/* buf[2] */
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_LESS, SLJIT_R0, 0, SLJIT_R1, 0);
	SET_NEXT_BYTE(SLJIT_LESS);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_R1, 0);
	sljit_emit_op2u(compiler, SLJIT_SUB32 | SLJIT_SET_GREATER, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op0(compiler, SLJIT_ENDBR); /* ENDBR should keep the flags. */
	sljit_emit_op0(compiler, SLJIT_NOP); /* Nop should keep the flags. */
	SET_NEXT_BYTE(SLJIT_GREATER);
	/* buf[4] */
	sljit_emit_op2u(compiler, SLJIT_SUB32 | SLJIT_SET_LESS, SLJIT_R0, 0, SLJIT_R1, 0);
	SET_NEXT_BYTE(SLJIT_LESS);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x1000);
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 20);
	sljit_emit_op2(compiler, SLJIT_OR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 0x10);
	/* 0x100000010 on 64 bit machines, 0x10 on 32 bit machines. */
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_GREATER, SLJIT_R0, 0, SLJIT_IMM, 0x80);
	SET_NEXT_BYTE(SLJIT_GREATER);
	/* buf[6] */
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_LESS, SLJIT_R0, 0, SLJIT_IMM, 0x80);
	SET_NEXT_BYTE(SLJIT_LESS);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op2u(compiler, SLJIT_SUB32 | SLJIT_SET_GREATER, SLJIT_R0, 0, SLJIT_IMM, 0x80);
	SET_NEXT_BYTE(SLJIT_GREATER);
	/* buf[7] */
	sljit_emit_op2u(compiler, SLJIT_SUB32 | SLJIT_SET_LESS, SLJIT_R0, 0, SLJIT_IMM, 0x80);
	SET_NEXT_BYTE(SLJIT_LESS);

	/* buf[8] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
	/* 0xff..ff on all machines. */
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_LESS_EQUAL, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
	SET_NEXT_BYTE(SLJIT_LESS_EQUAL);
	/* buf[9] */
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_GREATER_EQUAL, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
	SET_NEXT_BYTE(SLJIT_GREATER_EQUAL);
	/* buf[10] */
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_SIG_GREATER, SLJIT_R2, 0, SLJIT_R1, 0, SLJIT_IMM, -1);
	SET_NEXT_BYTE(SLJIT_SIG_GREATER);
	/* buf[12] */
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_SIG_LESS, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, -1);
	SET_NEXT_BYTE(SLJIT_SIG_LESS);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_Z, SLJIT_R2, 0, SLJIT_R1, 0, SLJIT_R0, 0);
	SET_NEXT_BYTE(SLJIT_EQUAL);
	/* buf[14] */
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_R1, 0, SLJIT_R0, 0);
	SET_NEXT_BYTE(SLJIT_NOT_EQUAL);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_OVERFLOW, SLJIT_R0, 0, SLJIT_R1, 0, SLJIT_IMM, -2);
	SET_NEXT_BYTE(SLJIT_OVERFLOW);
	/* buf[16] */
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_OVERFLOW, SLJIT_R0, 0, SLJIT_R1, 0, SLJIT_IMM, -2);
	SET_NEXT_BYTE(SLJIT_NOT_OVERFLOW);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_GREATER_EQUAL, SLJIT_R0, 0, SLJIT_R1, 0, SLJIT_IMM, -2);
	SET_NEXT_BYTE(SLJIT_GREATER_EQUAL);
	/* buf[17] */
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_LESS_EQUAL, SLJIT_R0, 0, SLJIT_R1, 0, SLJIT_IMM, -2);
	SET_NEXT_BYTE(SLJIT_LESS_EQUAL);
	/* buf[20] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)((sljit_uw)1 << ((8 * sizeof(sljit_uw)) - 1)));
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_SIG_LESS, SLJIT_R0, 0, SLJIT_R1, 0, SLJIT_IMM, 1);
	SET_NEXT_BYTE(SLJIT_SIG_LESS);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_SIG_GREATER, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, -1);
	SET_NEXT_BYTE(SLJIT_SIG_GREATER);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R0, 0, SLJIT_R1, 0, SLJIT_IMM, 1);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_SIG_GREATER_EQUAL, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, -2);
	SET_NEXT_BYTE(SLJIT_SIG_GREATER_EQUAL);
	/* buf[21] */
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_SIG_GREATER, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 2);
	SET_NEXT_BYTE(SLJIT_SIG_GREATER);

	/* buf[22] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)0x80000000);
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 16);
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 16);
	/* 0x80..0 on 64 bit machines, 0 on 32 bit machines. */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)0xffffffff);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_OVERFLOW, SLJIT_R0, 0, SLJIT_R1, 0);
	SET_NEXT_BYTE(SLJIT_OVERFLOW);
	/* buf[24] */
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_OVERFLOW, SLJIT_R0, 0, SLJIT_R1, 0);
	SET_NEXT_BYTE(SLJIT_NOT_OVERFLOW);
	sljit_emit_op1(compiler, SLJIT_MOV_S32, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_U32, SLJIT_R1, 0, SLJIT_R1, 0);
	sljit_emit_op2u(compiler, SLJIT_SUB32 | SLJIT_SET_OVERFLOW, SLJIT_R0, 0, SLJIT_R1, 0);
	SET_NEXT_BYTE(SLJIT_OVERFLOW);
	/* buf[25] */
	sljit_emit_op2u(compiler, SLJIT_SUB32 | SLJIT_SET_OVERFLOW, SLJIT_R0, 0, SLJIT_R1, 0);
	SET_NEXT_BYTE(SLJIT_NOT_OVERFLOW);

	/* buf[26] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op2u(compiler, SLJIT_SUBC | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op2(compiler, SLJIT_SUBC, SLJIT_R0, 0, SLJIT_IMM, 6, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_U8, SLJIT_MEM1(SLJIT_S0), 1, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_S0, 0, SLJIT_S0, 0, SLJIT_IMM, 1);

	/* buf[27] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -1);
	sljit_emit_op2u(compiler, SLJIT_ADD | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op2u(compiler, SLJIT_ADDC | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op2(compiler, SLJIT_ADDC, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 9);
	sljit_emit_op1(compiler, SLJIT_MOV_U8, SLJIT_MEM1(SLJIT_S0), 1, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_S0, 0, SLJIT_S0, 0, SLJIT_IMM, 1);

	/* buf[28] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, (8 * sizeof(sljit_sw)) - 1);
	sljit_emit_op2u(compiler, SLJIT_ADD | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_IMM, 0);
	SET_NEXT_BYTE(SLJIT_EQUAL);
	/* buf[34] */
	sljit_emit_op2u(compiler, SLJIT_ADD | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_R0, 0);
	SET_NEXT_BYTE(SLJIT_EQUAL);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op2(compiler, SLJIT_ASHR | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 0);
	SET_NEXT_BYTE(SLJIT_EQUAL);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op2(compiler, SLJIT_LSHR | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 0xffffc0);
	SET_NEXT_BYTE(SLJIT_NOT_EQUAL);
	sljit_emit_op1(compiler, SLJIT_MOV, shift_reg, 0, SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op2(compiler, SLJIT_ASHR | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_R0, 0, shift_reg, 0);
	SET_NEXT_BYTE(SLJIT_EQUAL);
	sljit_emit_op1(compiler, SLJIT_MOV, shift_reg, 0, SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op2(compiler, SLJIT_LSHR | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_R0, 0, shift_reg, 0);
	SET_NEXT_BYTE(SLJIT_NOT_EQUAL);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 1);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_SUBC | SLJIT_SET_CARRY, SLJIT_R2, 0, SLJIT_IMM, 1, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_U8, SLJIT_MEM1(SLJIT_S0), 1, SLJIT_R2, 0);
	/* buf[35] */
	sljit_emit_op2u(compiler, SLJIT_SUBC | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op2(compiler, SLJIT_SUBC, SLJIT_R2, 0, SLJIT_IMM, 1, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_U8, SLJIT_MEM1(SLJIT_S0), 2, SLJIT_R2, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_S0, 0, SLJIT_S0, 0, SLJIT_IMM, 2);

	/* buf[36] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -34);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_LESS, SLJIT_R0, 0, SLJIT_IMM, 0x1234);
	SET_NEXT_BYTE(SLJIT_LESS);
	/* buf[37] */
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_SIG_LESS, SLJIT_R0, 0, SLJIT_IMM, 0x1234);
	SET_NEXT_BYTE(SLJIT_SIG_LESS);
	/* buf[38] */
#if IS_64BIT
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(0x12300000000) - 43);
#else
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, -43);
#endif
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, -96);
	sljit_emit_op2u(compiler, SLJIT_SUB32 | SLJIT_SET_LESS, SLJIT_R0, 0, SLJIT_R1, 0);
	SET_NEXT_BYTE(SLJIT_LESS);
	/* buf[39] */
	sljit_emit_op2u(compiler, SLJIT_SUB32 | SLJIT_SET_SIG_GREATER, SLJIT_R0, 0, SLJIT_R1, 0);
	SET_NEXT_BYTE(SLJIT_SIG_GREATER);

	sljit_emit_return_void(compiler);

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
	FAILED(buf[18] != 1, "test27 case 19 failed\n");
	FAILED(buf[19] != 1, "test27 case 20 failed\n");
	FAILED(buf[20] != 1, "test27 case 21 failed\n");
	FAILED(buf[21] != 2, "test27 case 22 failed\n");

	FAILED(buf[22] != RESULT(1), "test27 case 23 failed\n");
	FAILED(buf[23] != RESULT(2), "test27 case 24 failed\n");
	FAILED(buf[24] != 2, "test27 case 25 failed\n");
	FAILED(buf[25] != 1, "test27 case 26 failed\n");

	FAILED(buf[26] != 5, "test27 case 27 failed\n");
	FAILED(buf[27] != 9, "test27 case 28 failed\n");

	FAILED(buf[28] != 2, "test27 case 29 failed\n");
	FAILED(buf[29] != 1, "test27 case 30 failed\n");

	FAILED(buf[30] != 1, "test27 case 31 failed\n");
	FAILED(buf[31] != 1, "test27 case 32 failed\n");
	FAILED(buf[32] != 1, "test27 case 33 failed\n");
	FAILED(buf[33] != 1, "test27 case 34 failed\n");

	FAILED(buf[34] != 1, "test27 case 35 failed\n");
	FAILED(buf[35] != 0, "test27 case 36 failed\n");

	FAILED(buf[36] != 2, "test27 case 37 failed\n");
	FAILED(buf[37] != 1, "test27 case 38 failed\n");
	FAILED(buf[38] != 2, "test27 case 39 failed\n");
	FAILED(buf[39] != 1, "test27 case 40 failed\n");
	FAILED(buf[40] != 10, "test27 case 41 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
#undef SET_NEXT_BYTE
#undef RESULT
}

static void test28(void)
{
	/* Test mov. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	struct sljit_const* const1 = NULL;
	struct sljit_label* label = NULL;
	sljit_uw label_addr = 0;
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
	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(W, P), 5, 5, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, -234);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R4, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw));
	sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_S3, 0, SLJIT_R3, 0, SLJIT_R4, 0);
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_S3, 0);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_Z, SLJIT_S3, 0, SLJIT_IMM, 0);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_S3, 0, SLJIT_NOT_ZERO);
	/* buf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_S3, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S4, 0, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw));
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_S4, 0, SLJIT_S4, 0, SLJIT_R4, 0);
	/* buf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_S4, 0);

	const1 = sljit_emit_const(compiler, SLJIT_S3, 0, 0);
	sljit_emit_ijump(compiler, SLJIT_JUMP, SLJIT_S3, 0);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_S3, 0, SLJIT_S3, 0, SLJIT_IMM, 100);
	label = sljit_emit_label(compiler);
	sljit_emit_op0(compiler, SLJIT_ENDBR);
	/* buf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_S3, 0);

	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_R4, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);

	label_addr = sljit_get_label_addr(label);
	sljit_set_const(sljit_get_const_addr(const1), (sljit_sw)label_addr, sljit_get_executable_offset(compiler));

	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_sw)&buf) != 8, "test28 case 1 failed\n");
	FAILED(buf[1] != -1872, "test28 case 2 failed\n");
	FAILED(buf[2] != 1, "test28 case 3 failed\n");
	FAILED(buf[3] != 2, "test28 case 4 failed\n");
	FAILED(buf[4] != (sljit_sw)label_addr, "test28 case 5 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test29(void)
{
	/* Test signed/unsigned bytes and halfs. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_sw buf[25];
	sljit_s32 i;

	if (verbose)
		printf("Run test29\n");

	for (i = 0; i < 25; i++)
		buf[i] = 0;

	FAILED(!compiler, "cannot create compiler\n");
	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 5, 5, 0, 0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV_S8, SLJIT_R0, 0, SLJIT_IMM, -187);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_S8, SLJIT_R0, 0, SLJIT_IMM, -605);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_U8, SLJIT_R0, 0, SLJIT_IMM, -56);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_U8, SLJIT_R4, 0, SLJIT_IMM, 0xcde5);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_uw), SLJIT_R4, 0);

	sljit_emit_op1(compiler, SLJIT_MOV_S16, SLJIT_R0, 0, SLJIT_IMM, -45896);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_S16, SLJIT_R0, 0, SLJIT_IMM, -1472797);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_U16, SLJIT_R0, 0, SLJIT_IMM, -12890);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_U16, SLJIT_R4, 0, SLJIT_IMM, 0x9cb0a6);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_uw), SLJIT_R4, 0);

#if IS_64BIT
	sljit_emit_op1(compiler, SLJIT_MOV_S32, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(-3580429715));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_S32, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(-100722768662));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_U32, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(-1457052677972));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 10 * sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_U32, SLJIT_R4, 0, SLJIT_IMM, SLJIT_W(0xcef97a70b5));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 11 * sizeof(sljit_uw), SLJIT_R4, 0);
#endif /* IS_64BIT */

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, -187);
	sljit_emit_op1(compiler, SLJIT_MOV_S8, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 12 * sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, -605);
	sljit_emit_op1(compiler, SLJIT_MOV_S8, SLJIT_R0, 0, SLJIT_S2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 13 * sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, -56);
	sljit_emit_op1(compiler, SLJIT_MOV_U8, SLJIT_R0, 0, SLJIT_R2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 14 * sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, 0xcde5);
	sljit_emit_op1(compiler, SLJIT_MOV_U8, SLJIT_R4, 0, SLJIT_R3, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 15 * sizeof(sljit_uw), SLJIT_R4, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, -45896);
	sljit_emit_op1(compiler, SLJIT_MOV_S16, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 16 * sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, -1472797);
	sljit_emit_op1(compiler, SLJIT_MOV_S16, SLJIT_R0, 0, SLJIT_S2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 17 * sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, -12890);
	sljit_emit_op1(compiler, SLJIT_MOV_U16, SLJIT_R0, 0, SLJIT_R2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 18 * sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, 0x9cb0a6);
	sljit_emit_op1(compiler, SLJIT_MOV_U16, SLJIT_R4, 0, SLJIT_R3, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 19 * sizeof(sljit_uw), SLJIT_R4, 0);

#if IS_64BIT
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, SLJIT_W(-3580429715));
	sljit_emit_op1(compiler, SLJIT_MOV_S32, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 20 * sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, SLJIT_W(-100722768662));
	sljit_emit_op1(compiler, SLJIT_MOV_S32, SLJIT_R0, 0, SLJIT_S2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 21 * sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, SLJIT_W(-1457052677972));
	sljit_emit_op1(compiler, SLJIT_MOV_U32, SLJIT_R0, 0, SLJIT_R2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 22 * sizeof(sljit_uw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, SLJIT_W(0xcef97a70b5));
	sljit_emit_op1(compiler, SLJIT_MOV_U32, SLJIT_R4, 0, SLJIT_R3, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 23 * sizeof(sljit_uw), SLJIT_R4, 0);
#endif /* IS_64BIT */

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, 0x9faa5);
	sljit_emit_op1(compiler, SLJIT_MOV_S8, SLJIT_S2, 0, SLJIT_S2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 24 * sizeof(sljit_uw), SLJIT_S2, 0);

	sljit_emit_return_void(compiler);

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

#if IS_64BIT
	FAILED(buf[8] != SLJIT_W(714537581), "test29 case 9 failed\n");
	FAILED(buf[9] != SLJIT_W(-1938520854), "test29 case 10 failed\n");
	FAILED(buf[10] != SLJIT_W(3236202668), "test29 case 11 failed\n");
	FAILED(buf[11] != SLJIT_W(0xf97a70b5), "test29 case 12 failed\n");
#endif /* IS_64BIT */

	FAILED(buf[12] != 69, "test29 case 13 failed\n");
	FAILED(buf[13] != -93, "test29 case 14 failed\n");
	FAILED(buf[14] != 200, "test29 case 15 failed\n");
	FAILED(buf[15] != 0xe5, "test29 case 16 failed\n");
	FAILED(buf[16] != 19640, "test29 case 17 failed\n");
	FAILED(buf[17] != -31005, "test29 case 18 failed\n");
	FAILED(buf[18] != 52646, "test29 case 19 failed\n");
	FAILED(buf[19] != 0xb0a6, "test29 case 20 failed\n");

#if IS_64BIT
	FAILED(buf[20] != SLJIT_W(714537581), "test29 case 21 failed\n");
	FAILED(buf[21] != SLJIT_W(-1938520854), "test29 case 22 failed\n");
	FAILED(buf[22] != SLJIT_W(3236202668), "test29 case 23 failed\n");
	FAILED(buf[23] != SLJIT_W(0xf97a70b5), "test29 case 24 failed\n");
#endif /* IS_64BIT */

	FAILED(buf[24] != -91, "test29 case 25 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test30(void)
{
	/* Test unused results. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_sw buf[1];

	if (verbose)
		printf("Run test30\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 5, 5, 0, 0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R4, 0, SLJIT_IMM, 1);
#if IS_64BIT
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_S1, 0, SLJIT_IMM, SLJIT_W(-0x123ffffffff));
#else
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_S1, 0, SLJIT_IMM, 1);
#endif
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S3, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S4, 0, SLJIT_IMM, 1);

	/* Some calculations with unused results. */
	sljit_emit_op2u(compiler, SLJIT_ADD | SLJIT_SET_Z, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op2u(compiler, SLJIT_SUB32 | SLJIT_SET_GREATER_EQUAL, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op2u(compiler, SLJIT_MUL | SLJIT_SET_OVERFLOW, SLJIT_S0, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op2u(compiler, SLJIT_SHL | SLJIT_SET_Z, SLJIT_S3, 0, SLJIT_R2, 0);
	sljit_emit_op2u(compiler, SLJIT_LSHR | SLJIT_SET_Z, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_IMM, 5);
	sljit_emit_op2u(compiler, SLJIT_AND | SLJIT_SET_Z, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_IMM, 0xff);

	/* Testing that any change happens. */
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R2, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R3, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R4, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_U32, SLJIT_S1, 0, SLJIT_S1, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_S1, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_S2, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_S3, 0);
	/* buf[0] */
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0, SLJIT_S4, 0);

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);
	FAILED(buf[0] != 9, "test30 case 1 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test31(void)
{
	/* Integer mul and set flags. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_sw buf[12];
	sljit_s32 i;
	sljit_sw big_word = WCONST(0x7fffffff00000000, 0x7fffffff);
	sljit_sw big_word2 = WCONST(0x7fffffff00000012, 0x00000012);

	if (verbose)
		printf("Run test31\n");

	for (i = 0; i < 12; i++)
		buf[i] = 3;

	FAILED(!compiler, "cannot create compiler\n");

	/* buf[0] */
	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 3, 5, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0);
	sljit_emit_op2u(compiler, SLJIT_MUL | SLJIT_SET_OVERFLOW, SLJIT_R1, 0, SLJIT_IMM, -45);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_NOT_OVERFLOW);
	/* buf[1] */
	sljit_emit_op2u(compiler, SLJIT_MUL | SLJIT_SET_OVERFLOW, SLJIT_R1, 0, SLJIT_IMM, -45);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_OVERFLOW);

	/* buf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, big_word);
	sljit_emit_op2(compiler, SLJIT_MUL | SLJIT_SET_OVERFLOW, SLJIT_R2, 0, SLJIT_S2, 0, SLJIT_IMM, -2);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 33); /* Should not change flags. */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0); /* Should not change flags. */
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_OVERFLOW);
	/* buf[3] */
	sljit_emit_op2(compiler, SLJIT_MUL | SLJIT_SET_OVERFLOW, SLJIT_R2, 0, SLJIT_S2, 0, SLJIT_IMM, -2);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_NOT_OVERFLOW);

	/* buf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_S3, 0, SLJIT_IMM, 0x3f6b0);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_S4, 0, SLJIT_IMM, 0x2a783);
	sljit_emit_op2(compiler, SLJIT_MUL32 | SLJIT_SET_OVERFLOW, SLJIT_R1, 0, SLJIT_S3, 0, SLJIT_S4, 0);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_OVERFLOW);
	/* buf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_R1, 0);

	/* buf[6] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, big_word2);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R2, 0, SLJIT_R1, 0);
	sljit_emit_op2(compiler, SLJIT_MUL32 | SLJIT_SET_OVERFLOW, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 23);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_OVERFLOW);

	/* buf[7] */
	sljit_emit_op2u(compiler, SLJIT_MUL32 | SLJIT_SET_OVERFLOW, SLJIT_R2, 0, SLJIT_IMM, -23);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_NOT_OVERFLOW);
	/* buf[8] */
	sljit_emit_op2u(compiler, SLJIT_MUL | SLJIT_SET_OVERFLOW, SLJIT_R2, 0, SLJIT_IMM, -23);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw), SLJIT_NOT_OVERFLOW);

	/* buf[9] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 67);
	sljit_emit_op2(compiler, SLJIT_MUL | SLJIT_SET_OVERFLOW, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, -23);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);

	FAILED(buf[0] != 1, "test31 case 1 failed\n");
	FAILED(buf[1] != 2, "test31 case 2 failed\n");
	FAILED(buf[2] != 1, "test31 case 3 failed\n");
	FAILED(buf[3] != 2, "test31 case 4 failed\n");
	FAILED(buf[4] != 1, "test31 case 5 failed\n");
	FAILED((buf[5] & (sljit_sw)0xffffffff) != (sljit_sw)0x85540c10, "test31 case 6 failed\n");
	FAILED(buf[6] != 2, "test31 case 7 failed\n");
	FAILED(buf[7] != 1, "test31 case 8 failed\n");
	FAILED(buf[8] != 1, "test31 case 9 failed\n");
	FAILED(buf[9] != -1541, "test31 case 10 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test32(void)
{
	/* Floating point set flags. */
	executable_code code;
	struct sljit_compiler* compiler;
	sljit_s32 i;

	sljit_sw buf[16];
	union {
		sljit_f64 value;
		struct {
			sljit_s32 value1;
			sljit_s32 value2;
		} u;
	} dbuf[4];

	if (verbose)
		printf("Run test32\n");

	if (!sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		if (verbose)
			printf("no fpu available, test32 skipped\n");
		successful_tests++;
		return;
	}

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	for (i = 0; i < 16; i++)
		buf[i] = 5;

	/* Two NaNs */
	dbuf[0].u.value1 = 0x7fffffff;
	dbuf[0].u.value2 = 0x7fffffff;
	dbuf[1].u.value1 = 0x7fffffff;
	dbuf[1].u.value2 = 0x7fffffff;
	dbuf[2].value = -13.0;
	dbuf[3].value = 27.0;

	SLJIT_ASSERT(sizeof(sljit_f64) == 8 && sizeof(sljit_s32) == 4 && sizeof(dbuf[0]) == 8);

	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(VOID, P, P), 1, 2, 4, 0, 0);

	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S1), 0);
	sljit_emit_fop1(compiler, SLJIT_CMP_F64 | SLJIT_SET_UNORDERED, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_f64), SLJIT_FR0, 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_f64));
	/* buf[0] */
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_UNORDERED);
	sljit_emit_fop1(compiler, SLJIT_CMP_F64 | SLJIT_SET_ORDERED, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_f64), SLJIT_FR0, 0);
	/* buf[1] */
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_ORDERED);

	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_f64));
	sljit_emit_fop1(compiler, SLJIT_CMP_F64 | SLJIT_SET_UNORDERED, SLJIT_FR1, 0, SLJIT_FR2, 0);
	/* buf[2] */
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_UNORDERED);
	sljit_emit_fop1(compiler, SLJIT_CMP_F64 | SLJIT_SET_ORDERED, SLJIT_FR1, 0, SLJIT_FR2, 0);
	/* buf[3] */
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_ORDERED);
	sljit_emit_fop1(compiler, SLJIT_CMP_F64 | SLJIT_SET_F_LESS, SLJIT_FR1, 0, SLJIT_FR2, 0);
	/* buf[4] */
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_F_LESS);
	sljit_emit_fop1(compiler, SLJIT_CMP_F64 | SLJIT_SET_F_GREATER_EQUAL, SLJIT_FR1, 0, SLJIT_FR2, 0);
	/* buf[5] */
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_F_GREATER_EQUAL);
	sljit_emit_fop1(compiler, SLJIT_CMP_F64 | SLJIT_SET_F_GREATER, SLJIT_FR1, 0, SLJIT_FR2, 0);
	/* buf[6] */
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_F_GREATER);
	sljit_emit_fop1(compiler, SLJIT_CMP_F64 | SLJIT_SET_F_LESS_EQUAL, SLJIT_FR1, 0, SLJIT_FR2, 0);
	/* buf[7] */
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_F_LESS_EQUAL);
	sljit_emit_fop1(compiler, SLJIT_CMP_F64 | SLJIT_SET_F_EQUAL, SLJIT_FR1, 0, SLJIT_FR2, 0);
	/* buf[8] */
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw), SLJIT_F_EQUAL);
	sljit_emit_fop1(compiler, SLJIT_CMP_F64 | SLJIT_SET_F_NOT_EQUAL, SLJIT_FR1, 0, SLJIT_FR2, 0);
	/* buf[9] */
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_sw), SLJIT_F_NOT_EQUAL);

	sljit_emit_fop2(compiler, SLJIT_ADD_F64, SLJIT_FR3, 0, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_f64));
	sljit_emit_fop1(compiler, SLJIT_CMP_F64 | SLJIT_SET_UNORDERED, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_f64));
	/* buf[10] */
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 10 * sizeof(sljit_sw), SLJIT_UNORDERED);
	sljit_emit_fop1(compiler, SLJIT_CMP_F64 | SLJIT_SET_F_EQUAL, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_f64));
	/* buf[11] */
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 11 * sizeof(sljit_sw), SLJIT_F_EQUAL);

	sljit_emit_fop1(compiler, SLJIT_CMP_F64 | SLJIT_SET_ORDERED, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_f64), SLJIT_FR0, 0);
	/* buf[12] */
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 12 * sizeof(sljit_sw), SLJIT_ORDERED);

	sljit_emit_fop1(compiler, SLJIT_CMP_F64 | SLJIT_SET_UNORDERED, SLJIT_FR3, 0, SLJIT_FR2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_U8, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S1), 0);
	/* buf[13] */
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 13 * sizeof(sljit_sw), SLJIT_UNORDERED);

	sljit_emit_return_void(compiler);

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

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test33(void)
{
	/* Test setting multiple flags. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	struct sljit_jump* jump;
	sljit_sw buf[10];

	if (verbose)
		printf("Run test33\n");

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

	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 3, 3, 0, 0, 0);

	/* buf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 20);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 10);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_Z | SLJIT_SET_LESS, SLJIT_R2, 0, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_ZERO);
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, -10);
	jump = sljit_emit_jump(compiler, SLJIT_LESS);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_IMM, 11);
	sljit_set_label(jump, sljit_emit_label(compiler));

	/* buf[2] */
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_Z | SLJIT_SET_SIG_GREATER, SLJIT_R2, 0, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_SIG_GREATER);
	/* buf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_IMM, 45);
	jump = sljit_emit_jump(compiler, SLJIT_NOT_EQUAL);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_IMM, 55);
	sljit_set_label(jump, sljit_emit_label(compiler));

	/* buf[4-5] */
#if IS_64BIT
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)SLJIT_W(0x8000000000000000));
#else
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)SLJIT_W(0x80000000));
#endif
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 1);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_Z | SLJIT_SET_OVERFLOW, SLJIT_R2, 0, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_IMM, 33);
	jump = sljit_emit_jump(compiler, SLJIT_NOT_OVERFLOW);
	/* buf[5] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_ZERO);
	/* buf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_IMM, 13);
	sljit_set_label(jump, sljit_emit_label(compiler));

	/* buf[6] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)0x80000000);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_SUB32 | SLJIT_SET_Z | SLJIT_SET_OVERFLOW, SLJIT_R2, 0, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, 0);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_NOT_ZERO);
	/* buf[7] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_IMM, 78);
	jump = sljit_emit_jump(compiler, SLJIT_OVERFLOW);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_IMM, 48);
	sljit_set_label(jump, sljit_emit_label(compiler));

	/* buf[8] */
#if IS_64BIT
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)SLJIT_W(0x8000000000000000));
#else
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)SLJIT_W(0x80000000));
#endif
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_SET_Z | SLJIT_SET_OVERFLOW, SLJIT_R2, 0, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw), SLJIT_IMM, 30);
	jump = sljit_emit_jump(compiler, SLJIT_NOT_OVERFLOW);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw), SLJIT_IMM, 50);
	/* buf[9] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_sw), SLJIT_ZERO);
	sljit_set_label(jump, sljit_emit_label(compiler));

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);

	FAILED(buf[0] != 0, "test33 case 1 failed\n");
	FAILED(buf[1] != 11, "test33 case 2 failed\n");
	FAILED(buf[2] != 1, "test33 case 3 failed\n");
	FAILED(buf[3] != 45, "test33 case 4 failed\n");
	FAILED(buf[4] != 13, "test33 case 5 failed\n");
	FAILED(buf[5] != 0, "test33 case 6 failed\n");
	FAILED(buf[6] != 0, "test33 case 7 failed\n");
	FAILED(buf[7] != 48, "test33 case 8 failed\n");
	FAILED(buf[8] != 50, "test33 case 9 failed\n");
	FAILED(buf[9] != 1, "test33 case 10 failed\n");

	sljit_free_code(code.code, NULL);
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
	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");
	sljit_set_context(compiler, 0, 1, 5, 5, 0, 0, 2 * sizeof(sljit_p));

	sljit_emit_op0(compiler, SLJIT_ENDBR);
	sljit_emit_op_dst(compiler, SLJIT_FAST_ENTER, SLJIT_R1, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 4);
	sljit_emit_op_src(compiler, SLJIT_FAST_RETURN, SLJIT_R1, 0);

	codeA.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	/* B */
	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");
	sljit_set_context(compiler, 0, 1, 5, 5, 0, 0, 2 * sizeof(sljit_p));

	sljit_emit_op0(compiler, SLJIT_ENDBR);
	sljit_emit_op_dst(compiler, SLJIT_FAST_ENTER, SLJIT_R4, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 6);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, SLJIT_FUNC_ADDR(codeA.code));
	sljit_emit_ijump(compiler, SLJIT_FAST_CALL, SLJIT_R1, 0);
	sljit_emit_op_src(compiler, SLJIT_FAST_RETURN, SLJIT_R4, 0);

	codeB.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	/* C */
	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");
	sljit_set_context(compiler, 0, 1, 5, 5, 0, 0, 2 * sizeof(sljit_p));

	sljit_emit_op0(compiler, SLJIT_ENDBR);
	sljit_emit_op_dst(compiler, SLJIT_FAST_ENTER, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_p));
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 8);
	jump = sljit_emit_jump(compiler, SLJIT_FAST_CALL | SLJIT_REWRITABLE_JUMP);
	sljit_set_target(jump, SLJIT_FUNC_UADDR(codeB.code));
	sljit_emit_op_src(compiler, SLJIT_FAST_RETURN, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_p));

	codeC.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	/* D */
	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");
	sljit_set_context(compiler, 0, 1, 5, 5, 0, 0, 2 * sizeof(sljit_p));

	sljit_emit_op0(compiler, SLJIT_ENDBR);
	sljit_emit_op_dst(compiler, SLJIT_FAST_ENTER, SLJIT_MEM1(SLJIT_SP), 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 10);
	sljit_emit_ijump(compiler, SLJIT_FAST_CALL, SLJIT_IMM, SLJIT_FUNC_ADDR(codeC.code));
	sljit_emit_op_src(compiler, SLJIT_FAST_RETURN, SLJIT_MEM1(SLJIT_SP), 0);

	codeD.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	/* E */
	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");
	sljit_set_context(compiler, 0, 1, 5, 5, 0, 0, 2 * sizeof(sljit_p));

	sljit_emit_op_dst(compiler, SLJIT_FAST_ENTER, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 12);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_p), SLJIT_IMM, SLJIT_FUNC_ADDR(codeD.code));
	sljit_emit_ijump(compiler, SLJIT_FAST_CALL, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_p));
	sljit_emit_op_src(compiler, SLJIT_FAST_RETURN, SLJIT_MEM1(SLJIT_S0), 0);

	codeE.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	/* F */
	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(W, P), 5, 5, 0, 0, 2 * sizeof(sljit_p));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_ijump(compiler, SLJIT_FAST_CALL, SLJIT_IMM, SLJIT_FUNC_ADDR(codeE.code));
	label = sljit_emit_label(compiler);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_R0, 0);

	codeF.code = sljit_generate_code(compiler);
	CHECK(compiler);
	addr = sljit_get_label_addr(label);
	sljit_free_compiler(compiler);

	FAILED(codeF.func1((sljit_sw)&buf) != 40, "test34 case 1 failed\n");
	FAILED(buf[0] != addr - SLJIT_RETURN_ADDRESS_OFFSET, "test34 case 2 failed\n");

	sljit_free_code(codeA.code, NULL);
	sljit_free_code(codeB.code, NULL);
	sljit_free_code(codeC.code, NULL);
	sljit_free_code(codeD.code, NULL);
	sljit_free_code(codeE.code, NULL);
	sljit_free_code(codeF.code, NULL);
	successful_tests++;
}

static void test35(void)
{
	/* More complicated tests for fast calls. */
	executable_code codeA;
	executable_code codeB;
	executable_code codeC;
	struct sljit_compiler* compiler;
	struct sljit_jump *jump = NULL;
	struct sljit_label* label;
	sljit_sw executable_offset;
	sljit_uw return_addr;
	sljit_uw jump_addr = 0;
	sljit_p buf[1];

	if (verbose)
		printf("Run test35\n");

	buf[0] = 0;

	/* A */
	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");
	sljit_set_context(compiler, 0, 0, 2, 2, 0, 0, 0);

	sljit_emit_op_dst(compiler, SLJIT_FAST_ENTER, SLJIT_MEM0(), (sljit_sw)&buf[0]);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 5);

	jump = sljit_emit_jump(compiler, SLJIT_FAST_CALL | SLJIT_REWRITABLE_JUMP);
	sljit_set_target(jump, 0);

	label = sljit_emit_label(compiler);
	sljit_emit_op_src(compiler, SLJIT_FAST_RETURN, SLJIT_MEM0(), (sljit_sw)&buf[0]);

	codeA.code = sljit_generate_code(compiler);
	CHECK(compiler);
	executable_offset = sljit_get_executable_offset(compiler);
	jump_addr = sljit_get_jump_addr(jump);
	sljit_free_compiler(compiler);

	/* B */
	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");
	sljit_set_context(compiler, 0, 0, 2, 2, 0, 0, 0);

	sljit_emit_op0(compiler, SLJIT_ENDBR);
	sljit_emit_op_dst(compiler, SLJIT_FAST_ENTER, SLJIT_R1, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 7);
	sljit_emit_op_src(compiler, SLJIT_FAST_RETURN, SLJIT_R1, 0);

	codeB.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	sljit_set_jump_addr(jump_addr, SLJIT_FUNC_UADDR(codeB.code), executable_offset);

	/* C */
	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(W), 2, 2, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_ijump(compiler, SLJIT_FAST_CALL, SLJIT_IMM, SLJIT_FUNC_ADDR(codeA.code));
	label = sljit_emit_label(compiler);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_R0, 0);

	codeC.code = sljit_generate_code(compiler);
	CHECK(compiler);
	return_addr = sljit_get_label_addr(label);
	sljit_free_compiler(compiler);

	FAILED(codeC.func0() != 12, "test35 case 1 failed\n");
	FAILED(buf[0] != return_addr - SLJIT_RETURN_ADDRESS_OFFSET, "test35 case 2 failed\n");

	sljit_free_code(codeA.code, NULL);
	sljit_free_code(codeB.code, NULL);
	sljit_free_code(codeC.code, NULL);
	successful_tests++;
}

static void cmp_test(struct sljit_compiler *compiler, sljit_s32 type, sljit_s32 src1, sljit_sw src1w, sljit_s32 src2, sljit_sw src2w)
{
	/* 2 = true, 1 = false */
	struct sljit_jump* jump;
	struct sljit_label* label;

	sljit_emit_op1(compiler, SLJIT_MOV_U8, SLJIT_MEM1(SLJIT_S0), 1, SLJIT_IMM, 2);
	jump = sljit_emit_cmp(compiler, type, src1, src1w, src2, src2w);
	sljit_emit_op1(compiler, SLJIT_MOV_U8, SLJIT_MEM1(SLJIT_S0), 1, SLJIT_IMM, 1);
	label = sljit_emit_label(compiler);
	sljit_emit_op0(compiler, SLJIT_ENDBR);
	sljit_set_label(jump, label);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_S0, 0, SLJIT_S0, 0, SLJIT_IMM, 1);
}

#define TEST_CASES	(7 + 10 + 12 + 11 + 4)
static void test36(void)
{
	/* Compare instruction. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);

	sljit_s8 buf[TEST_CASES];
	sljit_s8 compare_buf[TEST_CASES] = {
		1, 1, 2, 2, 1, 2, 2,
		1, 1, 2, 2, 2, 1, 2, 2, 1, 1,
		2, 2, 2, 1, 2, 2, 2, 2, 1, 1, 2, 2,
		2, 1, 2, 1, 1, 1, 2, 1, 2, 1, 2,
		2, 1, 1, 2
	};
	sljit_sw data[4];
	sljit_s32 i;

	if (verbose)
		printf("Run test36\n");

	FAILED(!compiler, "cannot create compiler\n");
	for (i = 0; i < TEST_CASES; ++i)
		buf[i] = 100;
	data[0] = 32;
	data[1] = -9;
	data[2] = 43;
	data[3] = -13;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(VOID, P, P), 3, 2, 0, 0, 0);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_S0, 0, SLJIT_S0, 0, SLJIT_IMM, 1);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 13);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 15);
	/* buf[0], compare_buf[0-6] */
	cmp_test(compiler, SLJIT_EQUAL, SLJIT_IMM, 9, SLJIT_R0, 0);
	/* buf[1] */
	cmp_test(compiler, SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 3);
	/* buf[2] */
	cmp_test(compiler, SLJIT_EQUAL, SLJIT_MEM2(SLJIT_S1, SLJIT_R0), SLJIT_WORD_SHIFT, SLJIT_IMM, -13);
	/* buf[3] */
	cmp_test(compiler, SLJIT_NOT_EQUAL, SLJIT_IMM, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	/* buf[4] */
	cmp_test(compiler, SLJIT_NOT_EQUAL | SLJIT_REWRITABLE_JUMP, SLJIT_IMM, 0, SLJIT_R0, 0);
	/* buf[5] */
	cmp_test(compiler, SLJIT_EQUAL, SLJIT_MEM2(SLJIT_S1, SLJIT_R0), SLJIT_WORD_SHIFT, SLJIT_MEM2(SLJIT_S1, SLJIT_R0), SLJIT_WORD_SHIFT);
	/* buf[6] */
	cmp_test(compiler, SLJIT_EQUAL | SLJIT_REWRITABLE_JUMP, SLJIT_R0, 0, SLJIT_IMM, 0);

	/* buf[7-16], compare_buf[7-16] */
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

	/* buf[17-28], compare_buf[17-28] */
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

	/* buf[29-39], compare_buf[29-39] */
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

#if IS_64BIT
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(0xf00000004));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_R1, 0);
	/* buf[40-43] */
	cmp_test(compiler, SLJIT_LESS | SLJIT_32, SLJIT_R1, 0, SLJIT_IMM, 5);
	cmp_test(compiler, SLJIT_LESS, SLJIT_R0, 0, SLJIT_IMM, 5);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(0xff0000004));
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_R0, 0);
	cmp_test(compiler, SLJIT_SIG_GREATER | SLJIT_32, SLJIT_R1, 0, SLJIT_IMM, 5);
	cmp_test(compiler, SLJIT_SIG_GREATER, SLJIT_R0, 0, SLJIT_IMM, 5);
#else /* !IS_64BIT */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 4);
	/* buf[40-43] */
	cmp_test(compiler, SLJIT_LESS | SLJIT_32, SLJIT_R0, 0, SLJIT_IMM, 5);
	cmp_test(compiler, SLJIT_GREATER | SLJIT_32, SLJIT_R0, 0, SLJIT_IMM, 5);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)0xf0000004);
	cmp_test(compiler, SLJIT_SIG_GREATER | SLJIT_32, SLJIT_R0, 0, SLJIT_IMM, 5);
	cmp_test(compiler, SLJIT_SIG_LESS | SLJIT_32, SLJIT_R0, 0, SLJIT_IMM, 5);
#endif /* IS_64BIT */

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func2((sljit_sw)&buf, (sljit_sw)&data);

	for (i = 0; i < TEST_CASES; ++i)
		if (SLJIT_UNLIKELY(buf[i] != compare_buf[i])) {
			printf("test36 case %d failed\n", i + 1);
			return;
		}

	sljit_free_code(code.code, NULL);
	successful_tests++;
}
#undef TEST_CASES

#if IS_64BIT
#define BITN(n) (SLJIT_W(1) << (63 - (n)))
#else /* !IS_64BIT */
#define BITN(n) (1 << (31 - ((n) & 0x1f)))
#endif /* IS_64BIT */

static void test37(void)
{
	/* Test count leading zeroes. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_sw buf[9];
	sljit_s32 ibuf[3];
	sljit_s32 i;

	if (verbose)
		printf("Run test37\n");

	FAILED(!compiler, "cannot create compiler\n");

	for (i = 0; i < 9; i++)
		buf[i] = -1;
	for (i = 0; i < 3; i++)
		ibuf[i] = -1;
	buf[2] = 0;
	buf[4] = BITN(13);

	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(VOID, P, P), 2, 3, 0, 0, 0);
	/* buf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, BITN(27));
	sljit_emit_op1(compiler, SLJIT_CLZ, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, BITN(47));
	sljit_emit_op1(compiler, SLJIT_CLZ, SLJIT_R0, 0, SLJIT_S2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R0, 0);
	/* buf[2] */
	sljit_emit_op1(compiler, SLJIT_CLZ, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw));
	/* buf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_CLZ, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_R0, 0);
	/* ibuf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_CLZ32, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_R0, 0);
	/* buf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_CLZ, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_R0, 0);
	/* buf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, BITN(58));
	sljit_emit_op1(compiler, SLJIT_CLZ, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_R0, 0);
	/* buf[6] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_CLZ, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_R0, 0);
	/* ibuf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, WCONST(0xff08a00000, 0x08a00000));
	sljit_emit_op1(compiler, SLJIT_CLZ32, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_s32), SLJIT_R0, 0);
	/* buf[7] */
	sljit_emit_op1(compiler, SLJIT_CLZ32, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_R0, 0);
	/* buf[8] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, WCONST(0xffc8a00000, 0xc8a00000));
	sljit_emit_op1(compiler, SLJIT_CLZ32, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, 0xa00a);
	sljit_emit_op2(compiler, SLJIT_SHL32, SLJIT_R0, 0, SLJIT_R1, 0, SLJIT_IMM, 8);
	/* ibuf[2] */
	sljit_emit_op1(compiler, SLJIT_CLZ32, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_s32), SLJIT_R0, 0);

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func2((sljit_sw)&buf, (sljit_sw)&ibuf);
	FAILED(buf[0] != 27, "test37 case 1 failed\n");
	FAILED(buf[1] != WCONST(47, 15), "test37 case 2 failed\n");
	FAILED(buf[2] != WCONST(64, 32), "test37 case 3 failed\n");
	FAILED(buf[3] != 0, "test37 case 4 failed\n");
	FAILED(ibuf[0] != 32, "test37 case 5 failed\n");
	FAILED(buf[4] != 13, "test37 case 6 failed\n");
	FAILED(buf[5] != WCONST(58, 26), "test37 case 7 failed\n");
	FAILED(buf[6] != WCONST(64, 32), "test37 case 8 failed\n");
	FAILED(ibuf[1] != 4, "test37 case 9 failed\n");
	FAILED((buf[7] & (sljit_sw)0xffffffff) != 4, "test37 case 10 failed\n");
	FAILED((buf[8] & (sljit_sw)0xffffffff) != 0, "test37 case 11 failed\n");
	FAILED(ibuf[2] != 8, "test37 case 12 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

#undef BITN

static void test38(void)
{
#if (defined SLJIT_UTIL_STACK && SLJIT_UTIL_STACK)
	/* Test stack utility. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	struct sljit_jump* alloc1_fail;
	struct sljit_jump* alloc2_fail;
	struct sljit_jump* alloc3_fail;
	struct sljit_jump* sanity1_fail;
	struct sljit_jump* sanity2_fail;
	struct sljit_jump* sanity3_fail;
	struct sljit_jump* sanity4_fail;
	struct sljit_jump* jump;
	struct sljit_label* label;

	if (verbose)
		printf("Run test38\n");

	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(W), 3, 1, 0, 0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 8192);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 65536);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 0);
	sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS3(P, W, W, P), SLJIT_IMM, SLJIT_FUNC_ADDR(sljit_allocate_stack));
	alloc1_fail = sljit_emit_cmp(compiler, SLJIT_EQUAL, SLJIT_RETURN_REG, 0, SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_RETURN_REG, 0);

	/* Write 8k data. */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), SLJIT_OFFSETOF(struct sljit_stack, start));
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, 8192);
	label = sljit_emit_label(compiler);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_IMM, -1);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, sizeof(sljit_sw));
	jump = sljit_emit_cmp(compiler, SLJIT_LESS, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_set_label(jump, label);

	/* Grow stack. */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_S0), SLJIT_OFFSETOF(struct sljit_stack, end), SLJIT_IMM, 65536);
	sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS2(P, P, P), SLJIT_IMM, SLJIT_FUNC_ADDR(sljit_stack_resize));
	alloc2_fail = sljit_emit_cmp(compiler, SLJIT_EQUAL, SLJIT_RETURN_REG, 0, SLJIT_IMM, 0);
	sanity1_fail = sljit_emit_cmp(compiler, SLJIT_NOT_EQUAL, SLJIT_RETURN_REG, 0, SLJIT_MEM1(SLJIT_S0), SLJIT_OFFSETOF(struct sljit_stack, start));

	/* Write 64k data. */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), SLJIT_OFFSETOF(struct sljit_stack, start));
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, 65536);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_S0), SLJIT_OFFSETOF(struct sljit_stack, min_start));
	sanity2_fail = sljit_emit_cmp(compiler, SLJIT_NOT_EQUAL, SLJIT_R0, 0, SLJIT_R2, 0);
	label = sljit_emit_label(compiler);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_IMM, -1);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, sizeof(sljit_sw));
	jump = sljit_emit_cmp(compiler, SLJIT_LESS, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_set_label(jump, label);

	/* Shrink stack. */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_S0), SLJIT_OFFSETOF(struct sljit_stack, end), SLJIT_IMM, 32768);
	sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS2(P, P, P), SLJIT_IMM, SLJIT_FUNC_ADDR(sljit_stack_resize));
	alloc3_fail = sljit_emit_cmp(compiler, SLJIT_EQUAL, SLJIT_RETURN_REG, 0, SLJIT_IMM, 0);
	sanity3_fail = sljit_emit_cmp(compiler, SLJIT_NOT_EQUAL, SLJIT_RETURN_REG, 0, SLJIT_MEM1(SLJIT_S0), SLJIT_OFFSETOF(struct sljit_stack, start));

	/* Write 32k data. */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), SLJIT_OFFSETOF(struct sljit_stack, start));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_S0), SLJIT_OFFSETOF(struct sljit_stack, end));
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R2, 0, SLJIT_R1, 0, SLJIT_IMM, 32768);
	sanity4_fail = sljit_emit_cmp(compiler, SLJIT_NOT_EQUAL, SLJIT_R0, 0, SLJIT_R2, 0);
	label = sljit_emit_label(compiler);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_IMM, -1);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, sizeof(sljit_sw));
	jump = sljit_emit_cmp(compiler, SLJIT_LESS, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_set_label(jump, label);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0);
	sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS2(VOID, P, P), SLJIT_IMM, SLJIT_FUNC_ADDR(sljit_free_stack));

	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_IMM, 4567);

	label = sljit_emit_label(compiler);
	sljit_set_label(alloc1_fail, label);
	sljit_set_label(alloc2_fail, label);
	sljit_set_label(alloc3_fail, label);
	sljit_set_label(sanity1_fail, label);
	sljit_set_label(sanity2_fail, label);
	sljit_set_label(sanity3_fail, label);
	sljit_set_label(sanity4_fail, label);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_IMM, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	/* Just survive this. */
	FAILED(code.func0() != 4567, "test38 case 1 failed\n");

	sljit_free_code(code.code, NULL);
#endif
	successful_tests++;
}

static void test39(void)
{
	/* Test error handling. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	struct sljit_jump* jump;

	if (verbose)
		printf("Run test39\n");

	FAILED(!compiler, "cannot create compiler\n");

	/* Such assignment should never happen in a regular program. */
	compiler->error = -3967;

	SLJIT_ASSERT(sljit_emit_enter(compiler, 0, SLJIT_ARGS2(VOID, W, W), 5, 5, 6, 0, 32) == -3967);
	SLJIT_ASSERT(sljit_emit_return(compiler, SLJIT_MOV, SLJIT_R1, 0) == -3967);
	SLJIT_ASSERT(sljit_emit_op0(compiler, SLJIT_NOP) == -3967);
	SLJIT_ASSERT(sljit_emit_op0(compiler, SLJIT_ENDBR) == -3967);
	SLJIT_ASSERT(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM2(SLJIT_R0, SLJIT_R1), 1) == -3967);
	SLJIT_ASSERT(sljit_emit_op2(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R0), 64, SLJIT_MEM1(SLJIT_S0), -64) == -3967);
	SLJIT_ASSERT(sljit_emit_fop1(compiler, SLJIT_ABS_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_R1), 0) == -3967);
	SLJIT_ASSERT(sljit_emit_fop2(compiler, SLJIT_DIV_F64, SLJIT_FR2, 0, SLJIT_MEM2(SLJIT_R0, SLJIT_S0), 0, SLJIT_FR2, 0) == -3967);
	SLJIT_ASSERT(!sljit_emit_label(compiler));
	jump = sljit_emit_call(compiler, SLJIT_CALL, SLJIT_ARGS4(W, 32, P, F32, F64));
	SLJIT_ASSERT(!jump);
	sljit_set_label(jump, (struct sljit_label*)0x123450);
	sljit_set_target(jump, 0x123450);
	jump = sljit_emit_cmp(compiler, SLJIT_SIG_LESS_EQUAL, SLJIT_R0, 0, SLJIT_R1, 0);
	SLJIT_ASSERT(!jump);
	SLJIT_ASSERT(sljit_emit_ijump(compiler, SLJIT_JUMP, SLJIT_MEM1(SLJIT_R0), 8) == -3967);
	SLJIT_ASSERT(sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_OVERFLOW) == -3967);
	SLJIT_ASSERT(!sljit_emit_const(compiler, SLJIT_R0, 0, 99));

	SLJIT_ASSERT(!compiler->labels && !compiler->jumps && !compiler->consts);
	SLJIT_ASSERT(!compiler->last_label && !compiler->last_jump && !compiler->last_const);
	SLJIT_ASSERT(!compiler->buf->next && !compiler->buf->used_size);
	SLJIT_ASSERT(!compiler->abuf->next && !compiler->abuf->used_size);

	sljit_set_compiler_memory_error(compiler);
	FAILED(sljit_get_compiler_error(compiler) != -3967, "test39 case 1 failed\n");

	code.code = sljit_generate_code(compiler);
	FAILED(sljit_get_compiler_error(compiler) != -3967, "test39 case 2 failed\n");
	FAILED(!!code.code, "test39 case 3 failed\n");
	sljit_free_compiler(compiler);

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	FAILED(sljit_get_compiler_error(compiler) != SLJIT_SUCCESS, "test39 case 4 failed\n");
	sljit_set_compiler_memory_error(compiler);
	FAILED(sljit_get_compiler_error(compiler) != SLJIT_ERR_ALLOC_FAILED, "test39 case 5 failed\n");
	sljit_free_compiler(compiler);

	successful_tests++;
}

static void test40(void)
{
	/* Test emit_op_flags. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
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

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(W, P), 3, 4, 0, 0, sizeof(sljit_sw));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -5);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_SIG_LESS, SLJIT_IMM, -6, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0x123456);
	sljit_emit_op_flags(compiler, SLJIT_OR, SLJIT_R1, 0, SLJIT_SIG_LESS);
	/* buf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -13);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_Z, SLJIT_IMM, -13, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_IMM, 0);
	sljit_emit_op_flags(compiler, SLJIT_OR | SLJIT_SET_Z, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_EQUAL);
	/* buf[1] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_NOT_EQUAL);
	sljit_emit_op2(compiler, SLJIT_OR, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_MEM1(SLJIT_SP), 0);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_Z, SLJIT_IMM, -13, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0);
	sljit_emit_op_flags(compiler, SLJIT_OR | SLJIT_SET_Z, SLJIT_R1, 0, SLJIT_EQUAL);
	/* buf[2] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 2, SLJIT_EQUAL);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -13);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 3);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_SIG_LESS, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
	/* buf[3] */
	sljit_emit_op_flags(compiler, SLJIT_OR, SLJIT_MEM2(SLJIT_S0, SLJIT_R1), SLJIT_WORD_SHIFT, SLJIT_SIG_LESS);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -8);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 33);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_GREATER, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, 0);
	sljit_emit_op_flags(compiler, SLJIT_OR | SLJIT_SET_Z, SLJIT_R2, 0, SLJIT_GREATER);
	sljit_emit_op_flags(compiler, SLJIT_OR, SLJIT_S1, 0, SLJIT_EQUAL);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S3, 0, SLJIT_IMM, 0x88);
	sljit_emit_op_flags(compiler, SLJIT_OR, SLJIT_S3, 0, SLJIT_NOT_EQUAL);
	/* buf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 4, SLJIT_S1, 0);
	/* buf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 5, SLJIT_S3, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x84);
	sljit_emit_op2u(compiler, SLJIT_AND | SLJIT_SET_Z, SLJIT_IMM, 0x180, SLJIT_R0, 0);
	/* buf[6] */
	sljit_emit_op_flags(compiler, SLJIT_OR | SLJIT_SET_Z, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 6, SLJIT_EQUAL);
	/* buf[7] */
	sljit_emit_op_flags(compiler, SLJIT_OR, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 7, SLJIT_EQUAL);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op_flags(compiler, SLJIT_OR | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_NOT_EQUAL);
	/* buf[8] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw) * 8, SLJIT_NOT_EQUAL);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x123456);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_GREATER, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op_flags(compiler, SLJIT_OR, SLJIT_R0, 0, SLJIT_GREATER);
	/* buf[9] */
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

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test41(void)
{
	/* Test inline assembly. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_s32 i;
	sljit_f64 buf[3];
#if (defined SLJIT_CONFIG_X86_32 && SLJIT_CONFIG_X86_32)
	sljit_u8 inst[16];
#elif (defined SLJIT_CONFIG_X86_64 && SLJIT_CONFIG_X86_64)
	sljit_u8 inst[16];
	sljit_s32 reg;
#else
	sljit_u32 inst;
#endif

	if (verbose)
		printf("Run test41\n");

#if !(defined SLJIT_CONFIG_X86_32 && SLJIT_CONFIG_X86_32)
	SLJIT_ASSERT(sljit_has_cpu_feature(SLJIT_HAS_VIRTUAL_REGISTERS) == 0);
#endif

	for (i = 0; i < SLJIT_NUMBER_OF_REGISTERS; i++) {
#if (defined SLJIT_CONFIG_X86_32 && SLJIT_CONFIG_X86_32)
		if (SLJIT_R(i) >= SLJIT_R3 && SLJIT_R(i) <= SLJIT_R8) {
			SLJIT_ASSERT(sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_R(i)) == -1);
			continue;
		}
#endif
		SLJIT_ASSERT(sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_R(i)) >= 0
			&& sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_R(i)) < 64);
	}

	FAILED(!compiler, "cannot create compiler\n");
	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(W, W, W), 3, 3, 0, 0, 0);

	/* Returns with the sum of SLJIT_S0 and SLJIT_S1. */
#if (defined SLJIT_CONFIG_X86_32 && SLJIT_CONFIG_X86_32)
	/* lea SLJIT_RETURN_REG, [SLJIT_S0, SLJIT_S1] */
	inst[0] = 0x48;
	inst[1] = 0x8d;
	inst[2] = (sljit_u8)(0x04 | ((sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_RETURN_REG) & 0x7) << 3));
	inst[3] = (sljit_u8)((sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_S0) & 0x7)
		| ((sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_S1) & 0x7) << 3));
	sljit_emit_op_custom(compiler, inst, 4);
#elif (defined SLJIT_CONFIG_X86_64 && SLJIT_CONFIG_X86_64)
	/* lea SLJIT_RETURN_REG, [SLJIT_S0, SLJIT_S1] */
	inst[0] = 0x48; /* REX_W */
	inst[1] = 0x8d;
	reg = sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_RETURN_REG);
	inst[2] = (sljit_u8)(0x04 | ((reg & 0x7) << 3));
	if (reg > 7)
		inst[0] |= 0x04; /* REX_R */
	reg = sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_S0);
	inst[3] = (sljit_u8)(reg & 0x7);
	if (reg > 7)
		inst[0] |= 0x01; /* REX_B */
	reg = sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_S1);
	inst[3] = (sljit_u8)(inst[3] | ((reg & 0x7) << 3));
	if (reg > 7)
		inst[0] |= 0x02; /* REX_X */
	sljit_emit_op_custom(compiler, inst, 4);
#elif (defined SLJIT_CONFIG_ARM_V6 && SLJIT_CONFIG_ARM_V6) || (defined SLJIT_CONFIG_ARM_V7 && SLJIT_CONFIG_ARM_V7)
	/* add rd, rn, rm */
	inst = 0xe0800000 | ((sljit_u32)sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_RETURN_REG) << 12)
		| ((sljit_u32)sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_S0) << 16)
		| (sljit_u32)sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_S1);
	sljit_emit_op_custom(compiler, &inst, sizeof(sljit_u32));
#elif (defined SLJIT_CONFIG_ARM_THUMB2 && SLJIT_CONFIG_ARM_THUMB2)
	/* add rd, rn, rm */
	inst = 0xeb000000 | ((sljit_u32)sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_RETURN_REG) << 8)
		| ((sljit_u32)sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_S0) << 16)
		| (sljit_u32)sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_S1);
	sljit_emit_op_custom(compiler, &inst, sizeof(sljit_u32));
#elif (defined SLJIT_CONFIG_ARM_64 && SLJIT_CONFIG_ARM_64)
	/* add rd, rn, rm */
	inst = 0x8b000000 | (sljit_u32)sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_RETURN_REG)
		| ((sljit_u32)sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_S0) << 5)
		| ((sljit_u32)sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_S1) << 16);
	sljit_emit_op_custom(compiler, &inst, sizeof(sljit_u32));
#elif (defined SLJIT_CONFIG_PPC && SLJIT_CONFIG_PPC)
	/* add rD, rA, rB */
	inst = (31 << 26) | (266 << 1) | ((sljit_u32)sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_RETURN_REG) << 21)
		| ((sljit_u32)sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_S0) << 16)
		| ((sljit_u32)sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_S1) << 11);
	sljit_emit_op_custom(compiler, &inst, sizeof(sljit_u32));
#elif (defined SLJIT_CONFIG_MIPS_32 && SLJIT_CONFIG_MIPS_32)
	/* addu rd, rs, rt */
	inst = 33 | ((sljit_u32)sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_RETURN_REG) << 11)
		| ((sljit_u32)sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_S0) << 21)
		| ((sljit_u32)sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_S1) << 16);
	sljit_emit_op_custom(compiler, &inst, sizeof(sljit_u32));
#elif (defined SLJIT_CONFIG_MIPS_64 && SLJIT_CONFIG_MIPS_64)
	/* daddu rd, rs, rt */
	inst = 45 | ((sljit_u32)sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_RETURN_REG) << 11)
		| ((sljit_u32)sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_S0) << 21)
		| ((sljit_u32)sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_S1) << 16);
	sljit_emit_op_custom(compiler, &inst, sizeof(sljit_u32));
#elif (defined SLJIT_CONFIG_RISCV && SLJIT_CONFIG_RISCV)
	/* add rd, rs1, rs2 */
	inst = 0x33 | (0 << 12) | (0 << 25) | ((sljit_u32)sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_RETURN_REG) << 7)
		| ((sljit_u32)sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_S0) << 15)
		| ((sljit_u32)sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_S1) << 20);
	sljit_emit_op_custom(compiler, &inst, sizeof(sljit_u32));
#elif (defined SLJIT_CONFIG_S390X && SLJIT_CONFIG_S390X)
	/* agrk rd, rs1, rs2 */
	inst = (0xb9e8u << 16)
		| ((sljit_u32)sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_RETURN_REG) << 4)
		| ((sljit_u32)sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_S0) << 12)
		| (sljit_u32)sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_S1);
	sljit_emit_op_custom(compiler, &inst, sizeof(inst));
#elif (defined SLJIT_CONFIG_LOONGARCH && SLJIT_CONFIG_LOONGARCH)
	/* add.d rd, rs1, rs2 */
	inst = (0x21u << 15) | (sljit_u32)sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_RETURN_REG)
		| ((sljit_u32)sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_S0) << 5)
		| ((sljit_u32)sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_S1) << 10);
	sljit_emit_op_custom(compiler, &inst, sizeof(sljit_u32));
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
#if IS_64BIT
	FAILED(code.func2(SLJIT_W(0x20f0a04090c06070), SLJIT_W(0x020f0a04090c0607)) != SLJIT_W(0x22ffaa4499cc6677), "test41 case 3 failed\n");
#endif

	sljit_free_code(code.code, NULL);

	if (sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		buf[0] = 13.5;
		buf[1] = -2.25;
		buf[2] = 0.0;

		compiler = sljit_create_compiler(NULL, NULL);
		sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 0, 1, 2, 0, 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S0), 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64));
#if (defined SLJIT_CONFIG_X86_32 && SLJIT_CONFIG_X86_32)
		/* addsd x, xm */
		inst[0] = 0xf2;
		inst[1] = 0x0f;
		inst[2] = 0x58;
		inst[3] = (sljit_u8)(0xc0 | (sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR0) << 3)
			| sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR1));
		sljit_emit_op_custom(compiler, inst, 4);
#elif (defined SLJIT_CONFIG_X86_64 && SLJIT_CONFIG_X86_64)
		/* addsd x, xm */
		if (sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR0) > 7 || sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR1) > 7) {
			inst[0] = 0;
			if (sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR0) > 7)
				inst[0] |= 0x04; /* REX_R */
			if (sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR1) > 7)
				inst[0] |= 0x01; /* REX_B */
			inst[1] = 0xf2;
			inst[2] = 0x0f;
			inst[3] = 0x58;
			inst[4] = (sljit_u8)(0xc0 | ((sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR0) & 0x7) << 3)
				| (sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR1) & 0x7));
			sljit_emit_op_custom(compiler, inst, 5);
		}
		else {
			inst[0] = 0xf2;
			inst[1] = 0x0f;
			inst[2] = 0x58;
			inst[3] = (sljit_u8)(0xc0 | (sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR0) << 3)
				| sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR1));
			sljit_emit_op_custom(compiler, inst, 4);
		}
#elif (defined SLJIT_CONFIG_ARM_32 && SLJIT_CONFIG_ARM_32)
		/* vadd.f64 dd, dn, dm */
		inst = 0xee300b00 | ((sljit_u32)sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR0) << 12)
			| ((sljit_u32)sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR0) << 16)
			| (sljit_u32)sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR1);
		sljit_emit_op_custom(compiler, &inst, sizeof(sljit_u32));
#elif (defined SLJIT_CONFIG_ARM_64 && SLJIT_CONFIG_ARM_64)
		/* fadd rd, rn, rm */
		inst = 0x1e602800 | (sljit_u32)sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR0)
			| ((sljit_u32)sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR0) << 5)
			| ((sljit_u32)sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR1) << 16);
		sljit_emit_op_custom(compiler, &inst, sizeof(sljit_u32));
#elif (defined SLJIT_CONFIG_PPC && SLJIT_CONFIG_PPC)
		/* fadd frD, frA, frB */
		inst = (63u << 26) | (21u << 1) | ((sljit_u32)sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR0) << 21)
			| ((sljit_u32)sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR0) << 16)
			| ((sljit_u32)sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR1) << 11);
		sljit_emit_op_custom(compiler, &inst, sizeof(sljit_u32));
#elif (defined SLJIT_CONFIG_MIPS && SLJIT_CONFIG_MIPS)
		/* add.d fd, fs, ft */
		inst = (17u << 26) | (17u << 21) | ((sljit_u32)sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR0) << 6)
			| ((sljit_u32)sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR0) << 11)
			| ((sljit_u32)sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR1) << 16);
		sljit_emit_op_custom(compiler, &inst, sizeof(sljit_u32));
#elif (defined SLJIT_CONFIG_RISCV && SLJIT_CONFIG_RISCV)
		/* fadd.d rd, rs1, rs2 */
		inst = (0x1u << 25) | (0x7u << 12) | (0x53u)
			| ((sljit_u32)sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR0) << 7)
			| ((sljit_u32)sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR0) << 15)
			| (sljit_u32)sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR1) << 20;
		sljit_emit_op_custom(compiler, &inst, sizeof(sljit_u32));
#elif (defined SLJIT_CONFIG_S390X && SLJIT_CONFIG_S390X)
		/* adbr r1, r2 */
		inst = 0xb31a0000
			| ((sljit_u32)sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR0) << 4)
			| (sljit_u32)sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR1);
		sljit_emit_op_custom(compiler, &inst, sizeof(sljit_u32));
#elif (defined SLJIT_CONFIG_LOONGARCH && SLJIT_CONFIG_LOONGARCH)
		/* fadd.d rd, rs1, rs2 */
		inst = (0x202u << 15)
			| ((sljit_u32)sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR0))
			| ((sljit_u32)sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR0) << 5)
			| (sljit_u32)sljit_get_register_index(SLJIT_FLOAT_REGISTER, SLJIT_FR1) << 10;
		sljit_emit_op_custom(compiler, &inst, sizeof(sljit_u32));
#endif
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_f64), SLJIT_FR0, 0);
		sljit_emit_return_void(compiler);

		code.code = sljit_generate_code(compiler);
		CHECK(compiler);
		sljit_free_compiler(compiler);

		code.func1((sljit_sw)&buf);
		FAILED(buf[2] != 11.25, "test41 case 4 failed\n");

		sljit_free_code(code.code, NULL);
	}

	successful_tests++;
}

static void test42(void)
{
	/* Test long multiply and division. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_s32 i;
	sljit_sw buf[7 + 4 + 8 + 8];

	if (verbose)
		printf("Run test42\n");

	FAILED(!compiler, "cannot create compiler\n");
	for (i = 0; i < 7 + 4 + 8 + 8; i++)
		buf[i] = -1;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 5, 5, 0, 0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, -0x1fb308a);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, 0xf50c873);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R4, 0, SLJIT_IMM, 0x8a0475b);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, 0x9dc849b);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, -0x7c69a35);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S3, 0, SLJIT_IMM, 0x5a4d0c4);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S4, 0, SLJIT_IMM, 0x9a3b06d);

#if IS_64BIT
	/* buf[7-26] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(-0x5dc4f897b8cd67f5));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, SLJIT_W(0x3f8b5c026cb088df));
	sljit_emit_op0(compiler, SLJIT_LMUL_UW);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(-0x5dc4f897b8cd67f5));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, SLJIT_W(0x3f8b5c026cb088df));
	sljit_emit_op0(compiler, SLJIT_LMUL_SW);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 10 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(-0x5dc4f897b8cd67f5));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, SLJIT_W(0x3f8b5c026cb088df));
	sljit_emit_op0(compiler, SLJIT_DIVMOD_UW);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 11 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 12 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(-0x5dc4f897b8cd67f5));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, SLJIT_W(0x3f8b5c026cb088df));
	sljit_emit_op0(compiler, SLJIT_DIVMOD_SW);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 13 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 14 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(0x5cf783d3cf0a74b0));
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, SLJIT_W(0x3d5df42d03a28fc7));
	sljit_emit_op0(compiler, SLJIT_DIVMOD_U32);
	sljit_emit_op1(compiler, SLJIT_MOV_U32, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_U32, SLJIT_R1, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 15 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 16 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(0x371df5197ba26a28));
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, SLJIT_W(0x46c78a5cfd6a420c));
	sljit_emit_op0(compiler, SLJIT_DIVMOD_S32);
	sljit_emit_op1(compiler, SLJIT_MOV_S32, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_S32, SLJIT_R1, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 17 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 18 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)SLJIT_W(0xc456f048c28a611b));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, SLJIT_W(0x3d4af2c543));
	sljit_emit_op0(compiler, SLJIT_DIV_UW);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 19 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 20 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(-0x720fa4b74c329b14));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, SLJIT_W(0xa64ae42b7d6));
	sljit_emit_op0(compiler, SLJIT_DIV_SW);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 21 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 22 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(0x4af51c027b34));
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, SLJIT_W(0x9ba4ff2906b14));
	sljit_emit_op0(compiler, SLJIT_DIV_U32);
	sljit_emit_op1(compiler, SLJIT_MOV_U32, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_U32, SLJIT_R1, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 23 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 24 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(0xc40b58a3f20d));
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, SLJIT_W(-0xa63c923));
	sljit_emit_op0(compiler, SLJIT_DIV_S32);
	sljit_emit_op1(compiler, SLJIT_MOV_S32, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_S32, SLJIT_R1, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 25 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 26 * sizeof(sljit_sw), SLJIT_R1, 0);

#else /* !IS_64BIT */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -0x58cd67f5);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0x3cb088df);
	sljit_emit_op0(compiler, SLJIT_LMUL_UW);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -0x58cd67f5);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0x3cb088df);
	sljit_emit_op0(compiler, SLJIT_LMUL_SW);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 10 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -0x58cd67f5);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0x3cb088df);
	sljit_emit_op0(compiler, SLJIT_DIVMOD_UW);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 11 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 12 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -0x58cd67f5);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0x3cb088df);
	sljit_emit_op0(compiler, SLJIT_DIVMOD_SW);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 13 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 14 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)0xcf0a74b0);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, 0x03a28fc7);
	sljit_emit_op0(compiler, SLJIT_DIVMOD_U32);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 15 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 16 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 0x7ba26a28);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)0xfd6a420c);
	sljit_emit_op0(compiler, SLJIT_DIVMOD_S32);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 17 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 18 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)0x9d4b7036);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0xb86d0);
	sljit_emit_op0(compiler, SLJIT_DIV_UW);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 19 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 20 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -0x58b0692c);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0xd357);
	sljit_emit_op0(compiler, SLJIT_DIV_SW);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 21 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 22 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 0x1c027b34);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)0xf2906b14);
	sljit_emit_op0(compiler, SLJIT_DIV_U32);
	sljit_emit_op1(compiler, SLJIT_MOV_U32, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_U32, SLJIT_R1, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 23 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 24 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 0x58a3f20d);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, -0xa63c923);
	sljit_emit_op0(compiler, SLJIT_DIV_S32);
	sljit_emit_op1(compiler, SLJIT_MOV_S32, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_S32, SLJIT_R1, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 25 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 26 * sizeof(sljit_sw), SLJIT_R1, 0);
#endif /* IS_64BIT */

	/* buf[0-6] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R3, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_R4, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_S1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_S2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_S3, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_S4, 0);

	sljit_emit_return_void(compiler);

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

	FAILED(buf[7] != WCONST(-4388959407985636971, -1587000939), "test42 case 8 failed\n");
	FAILED(buf[8] != WCONST(2901680654366567099, 665003983), "test42 case 9 failed\n");
	FAILED(buf[9] != WCONST(-4388959407985636971, -1587000939), "test42 case 10 failed\n");
	FAILED(buf[10] != WCONST(-1677173957268872740, -353198352), "test42 case 11 failed\n");
	FAILED(buf[11] != 2, "test42 case 12 failed\n");
	FAILED(buf[12] != WCONST(2532236178951865933, 768706125), "test42 case 13 failed\n");
	FAILED(buf[13] != -1, "test42 case 14 failed\n");
	FAILED(buf[14] != WCONST(-2177944059851366166, -471654166), "test42 case 15 failed\n");

	FAILED(buf[15] != 56, "test42 case 16 failed\n");
	FAILED(buf[16] != 58392872, "test42 case 17 failed\n");
	FAILED(buf[17] != -47, "test42 case 18 failed\n");
	FAILED(buf[18] != 35949148, "test42 case 19 failed\n");

	FAILED(buf[19] != WCONST(0x3340bfc, 0xda5), "test42 case 20 failed\n");
	FAILED(buf[20] != WCONST(0x3d4af2c543, 0xb86d0), "test42 case 21 failed\n");
	FAILED(buf[21] != WCONST(-0xaf978, -0x6b6e), "test42 case 22 failed\n");
	FAILED(buf[22] != WCONST(0xa64ae42b7d6, 0xd357), "test42 case 23 failed\n");

	FAILED(buf[23] != 0x0, "test42 case 24 failed\n");
	FAILED(buf[24] != (sljit_sw)0xf2906b14, "test42 case 25 failed\n");
	FAILED(buf[25] != -0x8, "test42 case 26 failed\n");
	FAILED(buf[26] != -0xa63c923, "test42 case 27 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test43(void)
{
	/* Test floating point compare. */
	executable_code code;
	struct sljit_compiler* compiler;
	struct sljit_jump* jump;

	union {
		sljit_f64 value;
		struct {
			sljit_u32 value1;
			sljit_u32 value2;
		} u;
	} dbuf[4];

	if (verbose)
		printf("Run test43\n");

	if (!sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		if (verbose)
			printf("no fpu available, test43 skipped\n");
		successful_tests++;
		return;
	}

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	dbuf[0].value = 12.125;
	/* a NaN */
	dbuf[1].u.value1 = 0x7fffffff;
	dbuf[1].u.value2 = 0x7fffffff;
	dbuf[2].value = -13.5;
	dbuf[3].value = 12.125;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(W, P), 1, 1, 3, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 2);
	/* dbuf[0] < dbuf[2] -> -2 */
	jump = sljit_emit_fcmp(compiler, SLJIT_F_GREATER_EQUAL, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), SLJIT_F64_SHIFT);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_IMM, -2);

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S0), 0);
	/* dbuf[0] and dbuf[1] is not NaN -> 5 */
	jump = sljit_emit_fcmp(compiler, SLJIT_UNORDERED, SLJIT_MEM0(), (sljit_sw)&dbuf[1], SLJIT_FR1, 0);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_IMM, 5);

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_f64));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0, SLJIT_IMM, 11);
	/* dbuf[0] == dbuf[3] -> 11 */
	jump = sljit_emit_fcmp(compiler, SLJIT_F_EQUAL, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_FR2, 0);

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

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test44(void)
{
	/* Test mov. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	void *buf[5];

	if (verbose)
		printf("Run test44\n");

	FAILED(!compiler, "cannot create compiler\n");

	buf[0] = buf + 2;
	buf[1] = NULL;
	buf[2] = NULL;
	buf[3] = NULL;
	buf[4] = NULL;
	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(P, P), 3, 2, 0, 0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV_P, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), 0);
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV_P, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_p), SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, sizeof(sljit_p));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 2);
	/* buf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV_P, SLJIT_MEM2(SLJIT_S0, SLJIT_R1), SLJIT_POINTER_SHIFT, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, sizeof(sljit_p));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 3 << SLJIT_POINTER_SHIFT);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_S0, 0);
	/* buf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV_P, SLJIT_MEM2(SLJIT_R2, SLJIT_R1), 0, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 2 * sizeof(sljit_p));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 1 << SLJIT_POINTER_SHIFT);
	/* buf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV_P, SLJIT_MEM2(SLJIT_R2, SLJIT_R1), 2, SLJIT_R0, 0);

	sljit_emit_return(compiler, SLJIT_MOV_P, SLJIT_R0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_sw)&buf) != (sljit_sw)(buf + 2), "test44 case 1 failed\n");
	FAILED(buf[1] != buf + 2, "test44 case 2 failed\n");
	FAILED(buf[2] != buf + 3, "test44 case 3 failed\n");
	FAILED(buf[3] != buf + 4, "test44 case 4 failed\n");
	FAILED(buf[4] != buf + 2, "test44 case 5 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test45(void)
{
	/* Test single precision floating point. */

	executable_code code;
	struct sljit_compiler* compiler;
	sljit_f32 buf[12];
	sljit_sw buf2[6];
	struct sljit_jump* jump;

	if (verbose)
		printf("Run test45\n");

	if (!sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		if (verbose)
			printf("no fpu available, test45 skipped\n");
		successful_tests++;
		return;
	}

	compiler = sljit_create_compiler(NULL, NULL);
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

	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(VOID, P, P), 3, 2, 6, 0, 0);

	/* buf[2] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR5, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f32));
	sljit_emit_fop1(compiler, SLJIT_NEG_F32, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_f32), SLJIT_FR0, 0);
	/* buf[3] */
	sljit_emit_fop1(compiler, SLJIT_ABS_F32, SLJIT_FR1, 0, SLJIT_FR5, 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_f32), SLJIT_FR1, 0);
	/* buf[4] */
	sljit_emit_fop1(compiler, SLJIT_ABS_F32, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_f32), SLJIT_FR5, 0);
	/* buf[5] */
	sljit_emit_fop1(compiler, SLJIT_NEG_F32, SLJIT_FR4, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_f32), SLJIT_FR4, 0);

	/* buf[6] */
	sljit_emit_fop2(compiler, SLJIT_ADD_F32, SLJIT_FR0, 0, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f32));
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_f32), SLJIT_FR0, 0);
	/* buf[7] */
	sljit_emit_fop2(compiler, SLJIT_SUB_F32, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_f32), SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_f32), SLJIT_FR5, 0);
	/* buf[8] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop2(compiler, SLJIT_MUL_F32, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_f32), SLJIT_FR0, 0, SLJIT_FR0, 0);
	/* buf[9] */
	sljit_emit_fop2(compiler, SLJIT_DIV_F32, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_f32), SLJIT_FR0, 0);
	sljit_emit_fop1(compiler, SLJIT_ABS_F32, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_f32), SLJIT_FR2, 0);
	/* buf[10] */
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_IMM, 0x3d0ac);
	sljit_emit_fop1(compiler, SLJIT_NEG_F32, SLJIT_MEM1(SLJIT_S0), 10 * sizeof(sljit_f32), SLJIT_MEM1(SLJIT_R0), 0x3d0ac);
	/* buf[11] */
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_IMM, 0x3d0ac + sizeof(sljit_f32));
	sljit_emit_fop1(compiler, SLJIT_ABS_F32, SLJIT_MEM1(SLJIT_S0), 11 * sizeof(sljit_f32), SLJIT_MEM1(SLJIT_R0), -0x3d0ac);

	/* buf2[0] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f32));
	sljit_emit_fop1(compiler, SLJIT_CMP_F32 | SLJIT_SET_F_EQUAL, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S0), 0);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_F_EQUAL);
	/* buf2[1] */
	sljit_emit_fop1(compiler, SLJIT_CMP_F32 | SLJIT_SET_F_LESS, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S0), 0);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_sw), SLJIT_F_LESS);
	/* buf2[2] */
	sljit_emit_fop1(compiler, SLJIT_CMP_F32 | SLJIT_SET_F_EQUAL, SLJIT_FR1, 0, SLJIT_FR2, 0);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_sw), SLJIT_F_EQUAL);
	/* buf2[3] */
	sljit_emit_fop1(compiler, SLJIT_CMP_F32 | SLJIT_SET_F_GREATER_EQUAL, SLJIT_FR1, 0, SLJIT_FR2, 0);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_sw), SLJIT_F_GREATER_EQUAL);

	/* buf2[4] */
	jump = sljit_emit_fcmp(compiler, SLJIT_F_LESS_EQUAL | SLJIT_32, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f32));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_sw), SLJIT_IMM, 7);
	sljit_set_label(jump, sljit_emit_label(compiler));

	/* buf2[5] */
	jump = sljit_emit_fcmp(compiler, SLJIT_F_GREATER | SLJIT_32, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_FR2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 5 * sizeof(sljit_sw), SLJIT_IMM, 6);
	sljit_set_label(jump, sljit_emit_label(compiler));

	sljit_emit_return_void(compiler);

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

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test46(void)
{
	/* Test sljit_emit_op_flags with 32 bit operations. */

	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_s32 buf[24];
	sljit_sw buf2[6];
	sljit_s32 i;

	if (verbose)
		printf("Run test46\n");

	for (i = 0; i < 24; ++i)
		buf[i] = -17;
	buf[16] = 0;
	for (i = 0; i < 6; ++i)
		buf2[i] = -13;
	buf2[4] = -124;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(VOID, P, P), 3, 3, 0, 0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, -7);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_Z | SLJIT_SET_LESS, SLJIT_R2, 0, SLJIT_IMM, 13);
	/* buf[0] */
	sljit_emit_op_flags(compiler, SLJIT_MOV32, SLJIT_MEM0(), (sljit_sw)&buf, SLJIT_LESS);
	/* buf[2] */
	sljit_emit_op_flags(compiler, SLJIT_AND32, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_s32), SLJIT_NOT_ZERO);

	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_Z, SLJIT_R2, 0, SLJIT_IMM, -7);
	/* buf[4] */
	sljit_emit_op_flags(compiler, SLJIT_AND32 | SLJIT_SET_Z, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_s32), SLJIT_EQUAL);
	/* buf[6] */
	sljit_emit_op_flags(compiler, SLJIT_AND32, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_s32), SLJIT_NOT_EQUAL);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x1235);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_IMM, 0x1235);
	sljit_emit_op_flags(compiler, SLJIT_AND32 | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_ZERO);
	/* buf[8] */
	sljit_emit_op_flags(compiler, SLJIT_AND32, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_s32), SLJIT_ZERO);
	/* buf[10] */
	sljit_emit_op1(compiler, SLJIT_MOV_U32, SLJIT_MEM1(SLJIT_S0), 10 * sizeof(sljit_s32), SLJIT_R0, 0);

	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_Z, SLJIT_R2, 0, SLJIT_IMM, -7);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 12);
	/* buf[12] */
	sljit_emit_op_flags(compiler, SLJIT_MOV32, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), 2, SLJIT_EQUAL);
	sljit_emit_op_flags(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_EQUAL);
	/* buf[14] */
	sljit_emit_op1(compiler, SLJIT_MOV_S32, SLJIT_MEM1(SLJIT_S0), 14 * sizeof(sljit_s32), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 16);
	/* buf[16] */
	sljit_emit_op_flags(compiler, SLJIT_AND32 | SLJIT_SET_Z, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), 2, SLJIT_EQUAL);
	/* buf[18] */
	sljit_emit_op_flags(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S0), 18 * sizeof(sljit_s32), SLJIT_NOT_EQUAL);

	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_Z, SLJIT_R2, 0, SLJIT_IMM, -7);
	/* buf[20] */
	sljit_emit_op_flags(compiler, SLJIT_XOR32 | SLJIT_SET_Z, SLJIT_MEM1(SLJIT_S0), 20 * sizeof(sljit_s32), SLJIT_ZERO);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 39);
	sljit_emit_op_flags(compiler, SLJIT_XOR32, SLJIT_R0, 0, SLJIT_NOT_ZERO);
	/* buf[22] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S0), 22 * sizeof(sljit_s32), SLJIT_R0, 0);

	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_GREATER, SLJIT_R2, 0, SLJIT_IMM, -7);
	/* buf2[0] */
	sljit_emit_op_flags(compiler, SLJIT_AND, SLJIT_MEM0(), (sljit_sw)&buf2, SLJIT_GREATER);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_SIG_LESS, SLJIT_R2, 0, SLJIT_IMM, 5);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1);
	/* buf2[1] */
	sljit_emit_op_flags(compiler, SLJIT_AND | SLJIT_SET_Z, SLJIT_MEM2(SLJIT_S1, SLJIT_R0), SLJIT_WORD_SHIFT, SLJIT_SIG_LESS);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 2);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_Z | SLJIT_SET_LESS, SLJIT_R2, 0, SLJIT_IMM, 5);
	/* buf2[2] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM2(SLJIT_S1, SLJIT_R0), SLJIT_WORD_SHIFT, SLJIT_LESS);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_NOT_EQUAL);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_SIG_LESS, SLJIT_R2, 0, SLJIT_IMM, 5);
	/* buf2[3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_sw), SLJIT_S2, 0);
	sljit_emit_op_flags(compiler, SLJIT_AND | SLJIT_SET_Z, SLJIT_MEM2(SLJIT_S1, SLJIT_R0), SLJIT_WORD_SHIFT, SLJIT_SIG_LESS);
	/* buf2[4] */
	sljit_emit_op_flags(compiler, SLJIT_OR, SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_sw), SLJIT_ZERO);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_GREATER, SLJIT_R2, 0, SLJIT_IMM, 0);
	/* buf2[5] */
	sljit_emit_op_flags(compiler, SLJIT_XOR, SLJIT_MEM1(SLJIT_S1), 5 * sizeof(sljit_sw), SLJIT_GREATER);

	sljit_emit_return_void(compiler);

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

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test47(void)
{
	/* Test jump optimizations. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_sw buf[3];

	if (verbose)
		printf("Run test47\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(W, P), 3, 1, 0, 0, 0);
	/* buf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x3a5c6f);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_LESS, SLJIT_R0, 0, SLJIT_IMM, 3);
	sljit_set_target(sljit_emit_jump(compiler, SLJIT_LESS), 0x11223344);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0xd37c10);
#if IS_64BIT
	sljit_set_target(sljit_emit_jump(compiler, SLJIT_LESS), SLJIT_W(0x112233445566));
#endif
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R0, 0);
	/* buf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x59b48e);
#if IS_64BIT
	sljit_set_target(sljit_emit_jump(compiler, SLJIT_LESS), SLJIT_W(0x1122334455667788));
#endif
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_sw)&buf) != 0x59b48e, "test47 case 1 failed\n");
	FAILED(buf[0] != 0x3a5c6f, "test47 case 2 failed\n");
	FAILED(buf[1] != 0xd37c10, "test47 case 3 failed\n");
	FAILED(buf[2] != 0x59b48e, "test47 case 4 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test48(void)
{
	/* Test floating point conversions. */
	executable_code code;
	struct sljit_compiler* compiler;
	int i;
	sljit_f64 dbuf[10];
	sljit_f32 sbuf[10];
	sljit_sw wbuf[10];
	sljit_s32 ibuf[10];

	if (verbose)
		printf("Run test48\n");

	if (!sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		if (verbose)
			printf("no fpu available, test48 skipped\n");
		successful_tests++;
		return;
	}

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	for (i = 0; i < 10; i++) {
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

	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(VOID), 3, 3, 6, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, (sljit_sw)&dbuf);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, (sljit_sw)&sbuf);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, (sljit_sw)&wbuf);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, (sljit_sw)&ibuf);

	/* sbuf[2] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F32_FROM_F64, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_f32), SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR5, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 3);
	/* sbuf[3] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F32_FROM_F64, SLJIT_MEM2(SLJIT_S1, SLJIT_R0), SLJIT_F32_SHIFT, SLJIT_FR5, 0);
	sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_F32, SLJIT_FR4, 0, SLJIT_MEM1(SLJIT_S1), 0);
	/* dbuf[3] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_f64), SLJIT_FR4, 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR3, 0, SLJIT_MEM1(SLJIT_S1), 0);
	sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_F32, SLJIT_FR2, 0, SLJIT_FR3, 0);
	/* dbuf[4] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_f64), SLJIT_FR2, 0);
	/* sbuf[4] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_f32), SLJIT_FR3, 0);

	/* wbuf[1] */
	sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F64, SLJIT_MEM1(SLJIT_S2), sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 2);
	sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F64, SLJIT_R0, 0, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), SLJIT_F64_SHIFT);
	/* wbuf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S2), 2 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR5, 0, SLJIT_MEM1(SLJIT_S1), 0);
	/* wbuf[3] */
	sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F32, SLJIT_MEM1(SLJIT_S2), 3 * sizeof(sljit_sw), SLJIT_FR5, 0);
	sljit_emit_fop1(compiler, SLJIT_NEG_F32, SLJIT_FR0, 0, SLJIT_FR5, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 4);
	/* wbuf[4] */
	sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F32, SLJIT_MEM2(SLJIT_S2, SLJIT_R1), SLJIT_WORD_SHIFT, SLJIT_FR0, 0);
	sljit_emit_fop1(compiler, SLJIT_NEG_F64, SLJIT_FR4, 0, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_f64));
	/* ibuf[2] */
	sljit_emit_fop1(compiler, SLJIT_CONV_S32_FROM_F64, SLJIT_MEM1(SLJIT_R2), 2 * sizeof(sljit_s32), SLJIT_FR4, 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_f32));
	sljit_emit_fop1(compiler, SLJIT_CONV_S32_FROM_F32, SLJIT_R0, 0, SLJIT_FR1, 0);
	/* ibuf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV_S32, SLJIT_MEM1(SLJIT_R2), 3 * sizeof(sljit_s32), SLJIT_R0, 0);

	/* dbuf[5] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_SW, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_f64), SLJIT_MEM1(SLJIT_S2), 0);
	sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_SW, SLJIT_FR2, 0, SLJIT_IMM, -6213);
	/* dbuf[6] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_f64), SLJIT_FR2, 0);
	/* dbuf[7] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_S32, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_f64), SLJIT_MEM0(), (sljit_sw)&ibuf[0]);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R2), sizeof(sljit_s32));
	sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_S32, SLJIT_FR1, 0, SLJIT_R0, 0);
	/* dbuf[8] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_f64), SLJIT_FR1, 0);
	/* dbuf[9] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_SW, SLJIT_MEM0(), (sljit_sw)(dbuf + 9), SLJIT_IMM, -77);
	/* sbuf[5] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F32_FROM_SW, SLJIT_MEM1(SLJIT_S1), 5 * sizeof(sljit_f32), SLJIT_IMM, -123);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 7190);
	sljit_emit_fop1(compiler, SLJIT_CONV_F32_FROM_SW, SLJIT_FR3, 0, SLJIT_R0, 0);
	/* sbuf[6] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S1), 6 * sizeof(sljit_f32), SLJIT_FR3, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 123);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R1, 0, SLJIT_R2, 0, SLJIT_IMM, 123 * sizeof(sljit_s32));
	sljit_emit_fop1(compiler, SLJIT_CONV_F32_FROM_S32, SLJIT_FR1, 0, SLJIT_MEM2(SLJIT_R1, SLJIT_R0), 2);
	/* sbuf[7] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S1), 7 * sizeof(sljit_f32), SLJIT_FR1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 8);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, 3812);
	/* sbuf[8] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F32_FROM_S32, SLJIT_MEM2(SLJIT_S1, SLJIT_R0), SLJIT_F32_SHIFT, SLJIT_R1, 0);
	/* sbuf[9] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F32_FROM_SW, SLJIT_MEM0(), (sljit_sw)(sbuf + 9), SLJIT_IMM, -79);

	sljit_emit_return_void(compiler);

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
	FAILED(dbuf[9] != -77.0, "test48 case 7 failed\n");

	FAILED(sbuf[2] != 123.5, "test48 case 8 failed\n");
	FAILED(sbuf[3] != 123.5, "test48 case 9 failed\n");
	FAILED(sbuf[4] != 476.25, "test48 case 10 failed\n");
	FAILED(sbuf[5] != -123, "test48 case 11 failed\n");
	FAILED(sbuf[6] != 7190, "test48 case 12 failed\n");
	FAILED(sbuf[7] != 312, "test48 case 13 failed\n");
	FAILED(sbuf[8] != 3812, "test48 case 14 failed\n");
	FAILED(sbuf[9] != -79.0, "test48 case 15 failed\n");

	FAILED(wbuf[1] != -367, "test48 case 16 failed\n");
	FAILED(wbuf[2] != 917, "test48 case 17 failed\n");
	FAILED(wbuf[3] != 476, "test48 case 18 failed\n");
	FAILED(wbuf[4] != -476, "test48 case 19 failed\n");

	FAILED(ibuf[2] != -917, "test48 case 20 failed\n");
	FAILED(ibuf[3] != -1689, "test48 case 21 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test49(void)
{
	/* Test floating point conversions. */
	executable_code code;
	struct sljit_compiler* compiler;
	int i;
	sljit_f64 dbuf[10];
	sljit_f32 sbuf[9];
	sljit_sw wbuf[9];
	sljit_s32 ibuf[9];
	sljit_s32* dbuf_ptr = (sljit_s32*)dbuf;
	sljit_s32* sbuf_ptr = (sljit_s32*)sbuf;

	if (verbose)
		printf("Run test49\n");

	if (!sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		if (verbose)
			printf("no fpu available, test49 skipped\n");
		successful_tests++;
		return;
	}

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	for (i = 0; i < 9; i++) {
		dbuf_ptr[i << 1] = -1;
		dbuf_ptr[(i << 1) + 1] = -1;
		sbuf_ptr[i] = -1;
		wbuf[i] = -1;
		ibuf[i] = -1;
	}

#if IS_64BIT
	dbuf[9] = (sljit_f64)SLJIT_W(0x1122334455);
#endif
	dbuf[0] = 673.75;
	sbuf[0] = -879.75;
	wbuf[0] = 345;
	ibuf[0] = -249;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(VOID), 3, 3, 3, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, (sljit_sw)&dbuf);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, (sljit_sw)&sbuf);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, (sljit_sw)&wbuf);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, (sljit_sw)&ibuf);

	/* dbuf[2] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_F32, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_f64), SLJIT_MEM1(SLJIT_S1), 0);
	/* sbuf[2] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F32_FROM_F64, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_f32), SLJIT_MEM1(SLJIT_S0), 0);
	/* wbuf[2] */
	sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F64, SLJIT_MEM1(SLJIT_S2), 2 * sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S0), 0);
	/* wbuf[4] */
	sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F32, SLJIT_MEM1(SLJIT_S2), 4 * sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S1), 0);
	/* ibuf[2] */
	sljit_emit_fop1(compiler, SLJIT_CONV_S32_FROM_F64, SLJIT_MEM1(SLJIT_R2), 2 * sizeof(sljit_s32), SLJIT_MEM1(SLJIT_S0), 0);
	/* ibuf[4] */
	sljit_emit_fop1(compiler, SLJIT_CONV_S32_FROM_F32, SLJIT_MEM1(SLJIT_R2), 4 * sizeof(sljit_s32), SLJIT_MEM1(SLJIT_S1), 0);
	/* dbuf[4] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_SW, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_f64), SLJIT_MEM1(SLJIT_S2), 0);
	/* sbuf[4] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F32_FROM_SW, SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_f32), SLJIT_MEM1(SLJIT_S2), 0);
	/* dbuf[6] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_S32, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_f64), SLJIT_MEM1(SLJIT_R2), 0);
	/* sbuf[6] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F32_FROM_S32, SLJIT_MEM1(SLJIT_S1), 6 * sizeof(sljit_f32), SLJIT_MEM1(SLJIT_R2), 0);

#if IS_64BIT
	sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F64, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_f64));
	/* wbuf[8] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S2), 8 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_f64));
	sljit_emit_fop1(compiler, SLJIT_CONV_S32_FROM_F64, SLJIT_R0, 0, SLJIT_FR2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_AND32, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 0xffff);
	/* ibuf[8] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_R2), 8 * sizeof(sljit_s32), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_W(0x4455667788));
	/* dbuf[8] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_SW, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_f64), SLJIT_R0, 0);
	/* dbuf[9] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_S32, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_f64), SLJIT_IMM, SLJIT_W(0x7766554433));
#endif /* IS_64BIT */

	sljit_emit_return_void(compiler);

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

#if IS_64BIT
	FAILED(dbuf[8] != (sljit_f64)SLJIT_W(0x4455667788), "test49 case 29 failed\n");
	FAILED(dbuf[9] != (sljit_f64)SLJIT_W(0x66554433), "test49 case 30 failed\n");
	FAILED(wbuf[8] != SLJIT_W(0x1122334455), "test48 case 31 failed\n");
	FAILED(ibuf[8] == 0x4455, "test48 case 32 failed\n");
#endif /* IS_64BIT */

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test50(void)
{
	/* Test stack and floating point operations. */
	executable_code code;
	struct sljit_compiler* compiler;
#if !IS_X86
	sljit_uw size1, size2, size3;
	int result;
#endif
	sljit_f32 sbuf[7];

	if (verbose)
		printf("Run test50\n");

	if (!sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		if (verbose)
			printf("no fpu available, test50 skipped\n");
		successful_tests++;
		return;
	}

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sbuf[0] = 245.5;
	sbuf[1] = -100.25;
	sbuf[2] = 713.75;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 3, 3, 6, 0, 8 * sizeof(sljit_f32));

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_f32), SLJIT_MEM1(SLJIT_SP), 0);
	/* sbuf[3] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_f32), SLJIT_MEM1(SLJIT_SP), sizeof(sljit_f32));
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_f32), SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f32));
	sljit_emit_fop2(compiler, SLJIT_ADD_F32, SLJIT_MEM1(SLJIT_SP), 2 * sizeof(sljit_f32), SLJIT_MEM1(SLJIT_SP), 0, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_f32));
	/* sbuf[4] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_f32), SLJIT_MEM1(SLJIT_SP), 2 * sizeof(sljit_f32));
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_SP), 2 * sizeof(sljit_f32), SLJIT_IMM, 5934);
	sljit_emit_fop1(compiler, SLJIT_CONV_F32_FROM_S32, SLJIT_MEM1(SLJIT_SP), 3 * sizeof(sljit_f32), SLJIT_MEM1(SLJIT_SP), 2 * sizeof(sljit_f32));
	/* sbuf[5] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_f32), SLJIT_MEM1(SLJIT_SP), 3 * sizeof(sljit_f32));

#if !IS_X86
	size1 = compiler->size;
#endif
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_f32));
#if !IS_X86
	size2 = compiler->size;
#endif
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR5, 0, SLJIT_FR2, 0);
#if !IS_X86
	size3 = compiler->size;
#endif
	/* sbuf[6] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_f32), SLJIT_FR5, 0);
#if !IS_X86
	result = (compiler->size - size3) == (size3 - size2) && (size3 - size2) == (size2 - size1);
#endif

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&sbuf);

	FAILED(sbuf[3] != 245.5, "test50 case 1 failed\n");
	FAILED(sbuf[4] != 145.25, "test50 case 2 failed\n");
	FAILED(sbuf[5] != 5934, "test50 case 3 failed\n");
	FAILED(sbuf[6] != 713.75, "test50 case 4 failed\n");
#if !IS_X86
	FAILED(!result, "test50 case 5 failed\n");
#endif

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test51(void)
{
	/* Test all registers provided by the CPU. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	struct sljit_jump* jump;
	sljit_sw buf[2];
	sljit_s32 i;

	if (verbose)
		printf("Run test51\n");

	FAILED(!compiler, "cannot create compiler\n");

	buf[0] = 39;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(VOID), SLJIT_NUMBER_OF_REGISTERS, 0, 0, 0, 0);

	for (i = 0; i < SLJIT_NUMBER_OF_REGISTERS; i++)
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R(i), 0, SLJIT_IMM, 32);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)buf);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0);

	for (i = 2; i < SLJIT_NUMBER_OF_REGISTERS; i++) {
		if (sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_R(i)) >= 0) {
			sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R(i), 0, SLJIT_R0, 0);
			sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R(i)), 0);
		} else
			sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, buf[0]);
	}

	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 32);
	for (i = 2; i < SLJIT_NUMBER_OF_REGISTERS; i++) {
		if (sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_R(i)) >= 0) {
			sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R(i), 0, SLJIT_R0, 0);
			sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R(i)), 32);
		} else
			sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, buf[0]);
	}

	for (i = 2; i < SLJIT_NUMBER_OF_REGISTERS; i++) {
		if (sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_R(i)) >= 0) {
			sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R(i), 0, SLJIT_IMM, 32);
			sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_MEM2(SLJIT_R(i), SLJIT_R0), 0);
			sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_MEM2(SLJIT_R0, SLJIT_R(i)), 0);
			sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R(i), 0, SLJIT_IMM, 8);
			sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_MEM2(SLJIT_R0, SLJIT_R(i)), 2);
		} else
			sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 3 * buf[0]);
	}

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), 32 + sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func0();

	FAILED(buf[1] != (39 * 5 * (SLJIT_NUMBER_OF_REGISTERS - 2)), "test51 case 1 failed\n");

	sljit_free_code(code.code, NULL);

	/* Next test. */

	compiler = sljit_create_compiler(NULL, NULL);

	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(W), SLJIT_NUMBER_OF_SCRATCH_REGISTERS, SLJIT_NUMBER_OF_SAVED_REGISTERS, 0, 0, 0);

	for (i = 0; i < SLJIT_NUMBER_OF_REGISTERS; i++)
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R(i), 0, SLJIT_IMM, 17);

	jump = sljit_emit_call(compiler, SLJIT_CALL, SLJIT_ARGS0(W));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	for (i = 0; i < SLJIT_NUMBER_OF_SAVED_REGISTERS; i++)
		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_S(i), 0);

	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_R0, 0);

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(VOID), SLJIT_NUMBER_OF_REGISTERS, 0, 0, 0, 0);
	for (i = 0; i < SLJIT_NUMBER_OF_REGISTERS; i++)
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R(i), 0, SLJIT_IMM, 35);
	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func0() != (SLJIT_NUMBER_OF_SAVED_REGISTERS * 17), "test51 case 2 failed\n");

	sljit_free_code(code.code, NULL);

	/* Next test. */

	compiler = sljit_create_compiler(NULL, NULL);

	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(W), SLJIT_NUMBER_OF_SCRATCH_REGISTERS, SLJIT_NUMBER_OF_SAVED_REGISTERS, 0, 0, 0);

	for (i = 0; i < SLJIT_NUMBER_OF_SAVED_REGISTERS; i++)
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S(i), 0, SLJIT_IMM, 68);
	for (i = 0; i < SLJIT_NUMBER_OF_SCRATCH_REGISTERS; i++)
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R(i), 0, SLJIT_IMM, 0);

	jump = sljit_emit_call(compiler, SLJIT_CALL, SLJIT_ARGS0(W));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	for (i = 0; i < SLJIT_NUMBER_OF_SAVED_REGISTERS; i++)
		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_S(i), 0);

	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_R0, 0);

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(VOID), SLJIT_NUMBER_OF_REGISTERS, 0, 0, 0, 0);
	for (i = 0; i < SLJIT_NUMBER_OF_REGISTERS; i++)
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R(i), 0, SLJIT_IMM, 43);
	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func0() != (SLJIT_NUMBER_OF_SAVED_REGISTERS * 68), "test51 case 3 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test52(void)
{
	/* Test all registers provided by the CPU. */
	executable_code code;
	struct sljit_compiler* compiler;
	struct sljit_jump* jump;
	sljit_f64 buf[3];
	sljit_s32 i;

	if (verbose)
		printf("Run test52\n");

	if (!sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		if (verbose)
			printf("no fpu available, test52 skipped\n");
		successful_tests++;
		return;
	}

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 6.25;
	buf[1] = 17.75;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 0, 1, SLJIT_NUMBER_OF_SCRATCH_FLOAT_REGISTERS, SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS, 0);

	for (i = 0; i < SLJIT_NUMBER_OF_FLOAT_REGISTERS; i++)
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR(i), 0, SLJIT_MEM1(SLJIT_S0), 0);

	jump = sljit_emit_call(compiler, SLJIT_CALL, SLJIT_ARGS0(VOID));
	/* SLJIT_FR0 contains the first value. */
	for (i = 1; i < SLJIT_NUMBER_OF_FLOAT_REGISTERS; i++)
		sljit_emit_fop2(compiler, SLJIT_ADD_F64, SLJIT_FR0, 0, SLJIT_FR0, 0, SLJIT_FR(i), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_f64), SLJIT_FR0, 0);

	sljit_emit_return_void(compiler);

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(VOID), 1, 0, SLJIT_NUMBER_OF_FLOAT_REGISTERS, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&buf[1]);
	for (i = 0; i < SLJIT_NUMBER_OF_FLOAT_REGISTERS; i++)
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR(i), 0, SLJIT_MEM1(SLJIT_R0), 0);
	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);
	FAILED(buf[2] != (SLJIT_NUMBER_OF_SCRATCH_FLOAT_REGISTERS * 17.75 + SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS * 6.25), "test52 case 1 failed\n");

	sljit_free_code(code.code, NULL);

	/* Next test. */

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = -32.5;
	buf[1] = -11.25;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 0, 1, SLJIT_NUMBER_OF_SCRATCH_FLOAT_REGISTERS, SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS, 0);

	for (i = 0; i < SLJIT_NUMBER_OF_SCRATCH_FLOAT_REGISTERS; i++)
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR(i), 0, SLJIT_MEM1(SLJIT_S0), 0);
	for (i = 0; i < SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS; i++)
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FS(i), 0, SLJIT_MEM1(SLJIT_S0), 0);

	jump = sljit_emit_call(compiler, SLJIT_CALL, SLJIT_ARGS0(VOID));
	/* SLJIT_FR0 contains the first value. */
	for (i = 1; i < SLJIT_NUMBER_OF_FLOAT_REGISTERS; i++)
		sljit_emit_fop2(compiler, SLJIT_ADD_F64, SLJIT_FR0, 0, SLJIT_FR0, 0, SLJIT_FR(i), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_f64), SLJIT_FR0, 0);

	sljit_emit_return_void(compiler);

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(VOID), 1, 0, SLJIT_NUMBER_OF_FLOAT_REGISTERS, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&buf[1]);
	for (i = 0; i < SLJIT_NUMBER_OF_FLOAT_REGISTERS; i++)
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR(i), 0, SLJIT_MEM1(SLJIT_R0), 0);
	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);
	FAILED(buf[2] != (SLJIT_NUMBER_OF_SCRATCH_FLOAT_REGISTERS * -11.25 + SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS * -32.5), "test52 case 2 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test53(void)
{
	/* Test addressing modes. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_u8 buf[3 + SLJIT_NUMBER_OF_REGISTERS * 3];
	sljit_u8 *buf_start;
	sljit_uw addr = (sljit_uw)&buf;
	sljit_s32 i;

	addr = addr + 3 - (addr % 3);
	buf_start = (sljit_u8*)addr;

	SLJIT_ASSERT((addr % 3) == 0);

	for (i = 0; i < (sljit_s32)sizeof(buf); i++)
		buf[i] = 0;

	if (verbose)
		printf("Run test53\n");

	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(W), SLJIT_NUMBER_OF_REGISTERS, 0, 0, 0, 0);

	addr /= 3;
	for (i = 0; i < SLJIT_NUMBER_OF_REGISTERS; i++, addr++) {
		if (sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_R(i)) == -1)
			continue;
		/* buf_start[i * 3] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R(i), 0, SLJIT_IMM, (sljit_sw)addr);
		sljit_emit_op1(compiler, SLJIT_MOV_U8, SLJIT_MEM2(SLJIT_R(i), SLJIT_R(i)), 1, SLJIT_IMM, 88 + i);
		/* buf_start[i * 3 + 1] */
		if (i != 0) {
			sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)(addr * 2 + 1));
			sljit_emit_op1(compiler, SLJIT_MOV_U8, SLJIT_MEM2(SLJIT_R(i), SLJIT_R0), 0, SLJIT_IMM, 147 + i);
		}
		/* buf_start[i * 3 + 2] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R(i), 0, SLJIT_IMM, (sljit_sw)(addr * 3 + 2));
		sljit_emit_op1(compiler, SLJIT_MOV_U8, SLJIT_MEM1(SLJIT_R(i)), 0, SLJIT_IMM, 191 + i);
	}

	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_R2, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func0();

	for (i = 0; i < SLJIT_NUMBER_OF_REGISTERS; i++) {
		if (sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_R(i)) == -1)
			continue;

		FAILED(buf_start[i * 3] != 88 + i, "test53 case 1 failed\n");
		if (i != 0) {
			FAILED(buf_start[i * 3 + 1] != 147 + i, "test53 case 2 failed\n");
		}
		FAILED(buf_start[i * 3 + 2] != 191 + i, "test53 case 3 failed\n");
	}

	sljit_free_code(code.code, NULL);

	successful_tests++;
}

static void test54(void)
{
	/* Test select operation. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	int i;
	sljit_sw buf[25];
	sljit_s32 ibuf[6];

	union {
		sljit_f32 value;
		sljit_s32 s32_value;
	} sbuf[3];

	sbuf[0].s32_value = 0x7fffffff;
	sbuf[1].value = 7.5;
	sbuf[2].value = -14.75;

	if (verbose)
		printf("Run test54\n");

	FAILED(!compiler, "cannot create compiler\n");

	for (i = 0; i < 25; i++)
		buf[i] = 0;
	for (i = 0; i < 6; i++)
		ibuf[i] = 0;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS3(VOID, P, P, P), 5, 3, 3, 0, 2 * sizeof(sljit_sw));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 17);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 34);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_SIG_LESS, SLJIT_R0, 0, SLJIT_IMM, -10);
	sljit_emit_select(compiler, SLJIT_SIG_LESS, SLJIT_R0, SLJIT_R1, 0, SLJIT_R0);
	/* buf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_SIG_GREATER, SLJIT_R0, 0, SLJIT_IMM, -10);
	sljit_emit_select(compiler, SLJIT_SIG_GREATER, SLJIT_R0, SLJIT_R1, 0, SLJIT_R0);
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -67);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 81);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_SIG_GREATER, SLJIT_R0, 0, SLJIT_IMM, -10);
	sljit_emit_select(compiler, SLJIT_SIG_LESS_EQUAL, SLJIT_R0, SLJIT_R0, 0, SLJIT_R1);
	/* buf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_SIG_GREATER, SLJIT_R0, 0, SLJIT_IMM, -66);
	sljit_emit_select(compiler, SLJIT_SIG_GREATER, SLJIT_R0, SLJIT_R0, 0, SLJIT_R1);
	/* buf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 24);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_IMM, 23);
	sljit_emit_select(compiler, SLJIT_EQUAL, SLJIT_R0, SLJIT_IMM, 66, SLJIT_R0);
	/* buf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_IMM, 78);
	sljit_emit_select(compiler, SLJIT_NOT_EQUAL, SLJIT_R0, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_R0);
	/* buf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_select(compiler, SLJIT_NOT_EQUAL, SLJIT_R0, SLJIT_IMM, WCONST(0x1234567812345678, 0x12345678), SLJIT_R0);
	/* buf[6] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_R0, 0);

#if (defined SLJIT_CONFIG_X86_32 && SLJIT_CONFIG_X86_32)
	SLJIT_ASSERT(sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_R3) == -1 && sljit_get_register_index(SLJIT_GP_REGISTER, SLJIT_R4) == -1);
#endif
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 7);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, -45);
	sljit_emit_op2(compiler, SLJIT_MUL | SLJIT_SET_OVERFLOW, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, 8);
	sljit_emit_select(compiler, SLJIT_OVERFLOW, SLJIT_R3, SLJIT_IMM, 35, SLJIT_R3);
	/* buf[7] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_R3, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, WCONST(0x1010000000, 0x100000));
	sljit_emit_op2u(compiler, SLJIT_MUL | SLJIT_SET_OVERFLOW, SLJIT_R0, 0, SLJIT_IMM, WCONST(0x1010000000, 0x100000));
	sljit_emit_select(compiler, SLJIT_OVERFLOW, SLJIT_R3, SLJIT_IMM, 35, SLJIT_R3);
	/* buf[8] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw), SLJIT_R3, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 71);
	sljit_emit_op2(compiler, SLJIT_LSHR | SLJIT_SET_Z, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, 8);
	sljit_emit_select(compiler, SLJIT_NOT_ZERO, SLJIT_R3, SLJIT_IMM, 13, SLJIT_R0);
	/* buf[9] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_sw), SLJIT_R3, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 12);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, -29);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 10 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_MUL | SLJIT_SET_OVERFLOW, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 8);
	sljit_emit_select(compiler, SLJIT_OVERFLOW, SLJIT_R0, SLJIT_MEM1(SLJIT_S0), 10 * sizeof(sljit_sw), SLJIT_R3);
	/* buf[10] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 10 * sizeof(sljit_sw), SLJIT_R3, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 16);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 11 * sizeof(sljit_sw), SLJIT_IMM, -12);
	sljit_emit_op2u(compiler, SLJIT_AND | SLJIT_SET_Z, SLJIT_R1, 0, SLJIT_IMM, 8);
	sljit_emit_select(compiler, SLJIT_NOT_EQUAL, SLJIT_R0, SLJIT_MEM1(SLJIT_R0), 11 * sizeof(sljit_sw), SLJIT_R1);
	/* buf[11] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 11 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 99);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 12 * sizeof(sljit_sw), SLJIT_IMM, -21);
	sljit_emit_select(compiler, SLJIT_EQUAL, SLJIT_R0, SLJIT_MEM1(SLJIT_R0), 12 * sizeof(sljit_sw), SLJIT_R1);
	/* buf[12] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 12 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_IMM, 43);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 90);
	sljit_emit_op2u(compiler, SLJIT_XOR | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_IMM, 2);
	sljit_emit_select(compiler, SLJIT_ZERO, SLJIT_R1, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_R1);
	/* buf[13] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 13 * sizeof(sljit_sw), SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_sw), SLJIT_IMM, -62);
	sljit_emit_select(compiler, SLJIT_NOT_ZERO, SLJIT_R2, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_sw), SLJIT_R1);
	/* buf[14] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 14 * sizeof(sljit_sw), SLJIT_R2, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 2);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 15);
	sljit_emit_op2u(compiler, SLJIT_ADD | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 15 * sizeof(sljit_sw), SLJIT_IMM, 38);
	sljit_emit_select(compiler, SLJIT_CARRY, SLJIT_R0, SLJIT_MEM2(SLJIT_S0, SLJIT_R1), SLJIT_WORD_SHIFT, SLJIT_R0);
	/* buf[15] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 15 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 2);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S0, 0, SLJIT_IMM, WCONST(0x8800000000, 0x8800000) + 16 * sizeof(sljit_sw));
	sljit_emit_op2u(compiler, SLJIT_ADD | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 16 * sizeof(sljit_sw), SLJIT_IMM, 77);
	sljit_emit_select(compiler, SLJIT_CARRY, SLJIT_R0, SLJIT_MEM1(SLJIT_R1), WCONST(-0x8800000000, -0x8800000), SLJIT_R0);
	/* buf[16] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 16 * sizeof(sljit_sw), SLJIT_R0, 0);

	if (sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S2), 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S2), sizeof(sljit_f32));
		sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S2), 2 * sizeof(sljit_f32));

		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 16);
		sljit_emit_fop1(compiler, SLJIT_CMP_F32 | SLJIT_SET_F_EQUAL, SLJIT_FR1, 0, SLJIT_FR2, 0);
		sljit_emit_select(compiler, SLJIT_F_EQUAL, SLJIT_R0, SLJIT_IMM, -45, SLJIT_R0);
		/* buf[17] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 17 * sizeof(sljit_sw), SLJIT_R0, 0);
		sljit_emit_fop1(compiler, SLJIT_CMP_F32 | SLJIT_SET_F_GREATER, SLJIT_FR1, 0, SLJIT_FR2, 0);
		sljit_emit_select(compiler, SLJIT_F_GREATER, SLJIT_R0, SLJIT_IMM, -45, SLJIT_R0);
		/* buf[18] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 18 * sizeof(sljit_sw), SLJIT_R0, 0);
		sljit_emit_fop1(compiler, SLJIT_CMP_F32 | SLJIT_SET_F_GREATER_EQUAL, SLJIT_FR1, 0, SLJIT_FR2, 0);
		sljit_emit_select(compiler, SLJIT_F_GREATER_EQUAL, SLJIT_R0, SLJIT_IMM, 33, SLJIT_R0);
		/* buf[19] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 19 * sizeof(sljit_sw), SLJIT_R0, 0);

		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -70);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 8);
		sljit_emit_fop1(compiler, SLJIT_CMP_F32 | SLJIT_SET_F_LESS, SLJIT_FR1, 0, SLJIT_FR2, 0);
		sljit_emit_select(compiler, SLJIT_F_LESS, SLJIT_R0, SLJIT_R0, 0, SLJIT_R1);
		/* buf[20] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 20 * sizeof(sljit_sw), SLJIT_R0, 0);
		sljit_emit_fop1(compiler, SLJIT_CMP_F32 | SLJIT_SET_F_LESS_EQUAL, SLJIT_FR2, 0, SLJIT_FR1, 0);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -60);
		sljit_emit_select(compiler, SLJIT_F_GREATER, SLJIT_R0, SLJIT_IMM, 8, SLJIT_R0);
		/* buf[21] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 21 * sizeof(sljit_sw), SLJIT_R0, 0);
		sljit_emit_fop1(compiler, SLJIT_CMP_F32 | SLJIT_SET_F_NOT_EQUAL, SLJIT_FR1, 0, SLJIT_FR2, 0);
		sljit_emit_select(compiler, SLJIT_F_NOT_EQUAL, SLJIT_R0, SLJIT_IMM, 31, SLJIT_R0);
		/* buf[22] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 22 * sizeof(sljit_sw), SLJIT_R0, 0);

		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 23 * sizeof(sljit_sw), SLJIT_IMM, 1);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 53);
		sljit_emit_fop1(compiler, SLJIT_CMP_F32 | SLJIT_SET_ORDERED, SLJIT_FR1, 0, SLJIT_FR0, 0);
		sljit_emit_select(compiler, SLJIT_ORDERED, SLJIT_R0, SLJIT_MEM1(SLJIT_S0), 23 * sizeof(sljit_sw), SLJIT_R1);
		/* buf[23] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 23 * sizeof(sljit_sw), SLJIT_R0, 0);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 24 * sizeof(sljit_sw), SLJIT_IMM, 59);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 1);
		sljit_emit_fop1(compiler, SLJIT_CMP_F32 | SLJIT_SET_UNORDERED, SLJIT_FR0, 0, SLJIT_FR1, 0);
		sljit_emit_select(compiler, SLJIT_UNORDERED, SLJIT_R0, SLJIT_MEM1(SLJIT_R0), 24 * sizeof(sljit_sw), SLJIT_R1);
		/* buf[24] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 24 * sizeof(sljit_sw), SLJIT_R0, 0);
	}

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 177);
	sljit_emit_op2u(compiler, SLJIT_SUB32 | SLJIT_SET_LESS, SLJIT_R0, 0, SLJIT_IMM, 178);
	sljit_emit_select(compiler, SLJIT_LESS | SLJIT_32, SLJIT_R0, SLJIT_IMM, 200, SLJIT_R0);
	/* ibuf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, sizeof(sljit_s32) >> 1);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_s32), SLJIT_IMM, 177);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, 95);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_LESS_EQUAL, SLJIT_R0, 0, SLJIT_IMM, sizeof(sljit_s32));
	sljit_emit_select(compiler, SLJIT_LESS_EQUAL | SLJIT_32, SLJIT_R0, SLJIT_MEM2(SLJIT_S1, SLJIT_R0), 1, SLJIT_R1);
	/* ibuf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_s32), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R3, 0, SLJIT_IMM, 56);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R4, 0, SLJIT_IMM, -63);
	sljit_emit_op2u(compiler, SLJIT_SUB32 | SLJIT_SET_SIG_LESS, SLJIT_R3, 0, SLJIT_R4, 0);
	sljit_emit_select(compiler, SLJIT_SIG_LESS | SLJIT_32, SLJIT_R3, SLJIT_R4, 0, SLJIT_R3);
	/* ibuf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_s32), SLJIT_R3, 0);
	sljit_emit_op2u(compiler, SLJIT_SUB32 | SLJIT_SET_SIG_LESS, SLJIT_R3, 0, SLJIT_R4, 0);
	sljit_emit_select(compiler, SLJIT_SIG_GREATER_EQUAL | SLJIT_32, SLJIT_R3, SLJIT_R4, 0, SLJIT_R3);
	/* ibuf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_s32), SLJIT_R3, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_s32), SLJIT_IMM, 467);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 10);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_SIG_LESS, SLJIT_R2, 0, SLJIT_IMM, 20);
	sljit_emit_select(compiler, SLJIT_SIG_LESS | SLJIT_32, SLJIT_R2, SLJIT_MEM0(), (sljit_sw)(ibuf + 4), SLJIT_R2);
	/* ibuf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_s32), SLJIT_R2, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 5 * sizeof(sljit_s32));
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 5 * sizeof(sljit_s32), SLJIT_IMM, -29);
	sljit_emit_op2u(compiler, SLJIT_ADD | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_IMM, -1);
	sljit_emit_select(compiler, SLJIT_CARRY | SLJIT_32, SLJIT_R2, SLJIT_MEM2(SLJIT_S1, SLJIT_R0), 0, SLJIT_R2);
	/* ibuf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 5 * sizeof(sljit_s32), SLJIT_R2, 0);

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func3((sljit_sw)&buf, (sljit_sw)&ibuf, (sljit_sw)&sbuf);

	FAILED(buf[0] != 17, "test54 case 1 failed\n");
	FAILED(buf[1] != 34, "test54 case 2 failed\n");
	FAILED(buf[2] != -67, "test54 case 3 failed\n");
	FAILED(buf[3] != 81, "test54 case 4 failed\n");
	FAILED(buf[4] != 24, "test54 case 5 failed\n");
	FAILED(buf[5] != 78, "test54 case 6 failed\n");
	FAILED(buf[6] != WCONST(0x1234567812345678, 0x12345678), "test54 case 7 failed\n");
	FAILED(buf[7] != -45, "test54 case 8 failed\n");
	FAILED(buf[8] != 35, "test54 case 9 failed\n");
	FAILED(buf[9] != 71, "test54 case 10 failed\n");
	FAILED(buf[10] != -29, "test54 case 11 failed\n");
	FAILED(buf[11] != 16, "test54 case 12 failed\n");
	FAILED(buf[12] != -21, "test54 case 13 failed\n");
	FAILED(buf[13] != 90, "test54 case 14 failed\n");
	FAILED(buf[14] != -62, "test54 case 15 failed\n");
	FAILED(buf[15] != 38, "test54 case 16 failed\n");
	FAILED(buf[16] != 77, "test54 case 17 failed\n");

	if (sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		FAILED(buf[17] != 16, "test54 case 18 failed\n");
		FAILED(buf[18] != -45, "test54 case 19 failed\n");
		FAILED(buf[19] != 33, "test54 case 20 failed\n");
		FAILED(buf[20] != 8, "test54 case 21 failed\n");
		FAILED(buf[21] != -60, "test54 case 22 failed\n");
		FAILED(buf[22] != 31, "test54 case 23 failed\n");
		FAILED(buf[23] != 53, "test54 case 24 failed\n");
		FAILED(buf[24] != 59, "test54 case 25 failed\n");
	}

	FAILED(ibuf[0] != 200, "test54 case 26 failed\n");
	FAILED(ibuf[1] != 177, "test54 case 27 failed\n");
	FAILED(ibuf[2] != 56, "test54 case 28 failed\n");
	FAILED(ibuf[3] != -63, "test54 case 29 failed\n");
	FAILED(ibuf[4] != 467, "test54 case 30 failed\n");
	FAILED(ibuf[5] != -29, "test54 case 31 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test55(void)
{
	/* Check value preservation. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_sw buf[2];
	sljit_s32 i;

	if (verbose)
		printf("Run test55\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	buf[1] = 0;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(VOID), SLJIT_NUMBER_OF_REGISTERS, 0, 0, 0, sizeof (sljit_sw));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_IMM, 217);

	/* Check 1 */
	for (i = 0; i < SLJIT_NUMBER_OF_REGISTERS; i++)
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R(i), 0, SLJIT_IMM, 118);

	sljit_emit_op0(compiler, SLJIT_DIVMOD_SW);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);

	for (i = 2; i < SLJIT_NUMBER_OF_REGISTERS; i++)
		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R(i), 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_SP), 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM0(), (sljit_sw)(buf + 0), SLJIT_R0, 0);

	/* Check 2 */
	for (i = 0; i < SLJIT_NUMBER_OF_REGISTERS; i++)
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R(i), 0, SLJIT_IMM, 146);

	sljit_emit_op0(compiler, SLJIT_DIV_SW);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);

	for (i = 1; i < SLJIT_NUMBER_OF_REGISTERS; i++)
		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R(i), 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_SP), 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM0(), (sljit_sw)(buf + 1), SLJIT_R0, 0);

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func0();

	FAILED(buf[0] != (SLJIT_NUMBER_OF_REGISTERS - 2) * 118 + 217, "test55 case 1 failed\n");
	FAILED(buf[1] != (SLJIT_NUMBER_OF_REGISTERS - 1) * 146 + 217, "test55 case 2 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test56(void)
{
	/* Check integer subtraction with negative immediate. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_sw buf[13];
	sljit_s32 i;

	if (verbose)
		printf("Run test56\n");

	for (i = 0; i < 13; i++)
		buf[i] = 77;

	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 3, 1, 0, 0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 90 << 12);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_SIG_GREATER, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, -(91 << 12));
	/* buf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R1, 0);
	/* buf[1] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_SIG_GREATER);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_LESS, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, -(91 << 12));
	/* buf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_R1, 0);
	/* buf[3] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_LESS);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_SIG_GREATER_EQUAL, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, -(91 << 12));
	/* buf[4] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_SIG_GREATER_EQUAL);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_LESS_EQUAL, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, -(91 << 12));
	/* buf[5] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_LESS_EQUAL);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_GREATER, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, -(91 << 12));
	/* buf[6] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_GREATER);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_SIG_LESS, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, -(91 << 12));
	/* buf[7] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_SIG_LESS);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 90);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_SIG_GREATER, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, -91);
	/* buf[8] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw), SLJIT_R0, 0);
	/* buf[9] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_sw), SLJIT_SIG_GREATER);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 90);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_LESS_EQUAL, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, -91);
	/* buf[10] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 10 * sizeof(sljit_sw), SLJIT_LESS_EQUAL);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, -0x7fffffff);
	sljit_emit_op2(compiler, SLJIT_ADD32 | SLJIT_SET_OVERFLOW, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, -(91 << 12));
	/* buf[11] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 11 * sizeof(sljit_sw), SLJIT_OVERFLOW);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, -0x7fffffff-1);
	sljit_emit_op2(compiler, SLJIT_SUB32 | SLJIT_SET_OVERFLOW, SLJIT_R0, 0, SLJIT_IMM, 0, SLJIT_R0, 0);
	/* buf[12] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 12 * sizeof(sljit_sw), SLJIT_OVERFLOW);

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);

	FAILED(buf[0] != (181 << 12), "test56 case 1 failed\n");
	FAILED(buf[1] != 1, "test56 case 2 failed\n");
	FAILED(buf[2] != (181 << 12), "test56 case 3 failed\n");
	FAILED(buf[3] != 1, "test56 case 4 failed\n");
	FAILED(buf[4] != 1, "test56 case 5 failed\n");
	FAILED(buf[5] != 1, "test56 case 6 failed\n");
	FAILED(buf[6] != 0, "test56 case 7 failed\n");
	FAILED(buf[7] != 0, "test56 case 8 failed\n");
	FAILED(buf[8] != 181, "test56 case 9 failed\n");
	FAILED(buf[9] != 1, "test56 case 10 failed\n");
	FAILED(buf[10] != 1, "test56 case 11 failed\n");
	FAILED(buf[11] != 1, "test56 case 12 failed\n");
	FAILED(buf[12] != 1, "test56 case 13 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test57(void)
{
	/* Check prefetch instructions. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	struct sljit_label* labels[5];
	sljit_p addr[5];
	int i;

	if (verbose)
		printf("Run test57\n");

	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(VOID), 3, 1, 0, 0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	labels[0] = sljit_emit_label(compiler);
	/* Should never crash. */
	sljit_emit_op_src(compiler, SLJIT_PREFETCH_L1, SLJIT_MEM2(SLJIT_R0, SLJIT_R0), 2);
	labels[1] = sljit_emit_label(compiler);
	sljit_emit_op_src(compiler, SLJIT_PREFETCH_L2, SLJIT_MEM0(), 0);
	labels[2] = sljit_emit_label(compiler);
#if IS_64BIT
	sljit_emit_op_src(compiler, SLJIT_PREFETCH_L3, SLJIT_MEM1(SLJIT_R0), SLJIT_W(0x1122334455667788));
#else
	sljit_emit_op_src(compiler, SLJIT_PREFETCH_L3, SLJIT_MEM1(SLJIT_R0), 0x11223344);
#endif
	labels[3] = sljit_emit_label(compiler);
	sljit_emit_op_src(compiler, SLJIT_PREFETCH_ONCE, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_sw));
	labels[4] = sljit_emit_label(compiler);

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);

	for (i = 0; i < 5; i++)
		addr[i] = sljit_get_label_addr(labels[i]);

	sljit_free_compiler(compiler);

	code.func0();

	if (sljit_has_cpu_feature(SLJIT_HAS_PREFETCH)) {
		FAILED(addr[0] == addr[1], "test57 case 1 failed\n");
		FAILED(addr[1] == addr[2], "test57 case 2 failed\n");
		FAILED(addr[2] == addr[3], "test57 case 3 failed\n");
		FAILED(addr[3] == addr[4], "test57 case 4 failed\n");
	} else {
		FAILED(addr[0] != addr[1], "test57 case 1 failed\n");
		FAILED(addr[1] != addr[2], "test57 case 2 failed\n");
		FAILED(addr[2] != addr[3], "test57 case 3 failed\n");
		FAILED(addr[3] != addr[4], "test57 case 4 failed\n");
	}

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static sljit_f64 test58_f1(sljit_f32 a, sljit_f32 b, sljit_f64 c)
{
	return (sljit_f64)a + (sljit_f64)b + c;
}

static sljit_f32 test58_f2(sljit_sw a, sljit_f64 b, sljit_f32 c)
{
	return (sljit_f32)((sljit_f64)a + b + (sljit_f64)c);
}

static sljit_f64 test58_f3(sljit_sw a, sljit_f32 b, sljit_sw c)
{
	return (sljit_f64)a + (sljit_f64)b + (sljit_f64)c;
}

static sljit_f64 test58_f4(sljit_f32 a, sljit_sw b)
{
	return (sljit_f64)a + (sljit_f64)b;
}

static sljit_f32 test58_f5(sljit_f32 a, sljit_f64 b, sljit_s32 c)
{
	return (sljit_f32)((sljit_f64)a + b + (sljit_f64)c);
}

static sljit_f64 test58_f6(sljit_f64 a, sljit_f64 b, sljit_uw c)
{
	return a * b * (sljit_f64)c;
}

static sljit_sw test58_f7(sljit_f64 a, sljit_sw b)
{
	return (sljit_sw)(a + (sljit_f64)b);
}

static void test58(void)
{
	/* Check function calls with floating point arguments. */
	executable_code code;
	struct sljit_compiler* compiler;
	struct sljit_jump* jump = NULL;
	sljit_f64 dbuf[8];
	sljit_f32 sbuf[7];
	sljit_sw wbuf[2];

	if (verbose)
		printf("Run test58\n");

	if (!sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		if (verbose)
			printf("no fpu available, test58 skipped\n");
		successful_tests++;
		return;
	}

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	dbuf[0] = 5.25;
	dbuf[1] = 0.0;
	dbuf[2] = 2.5;
	dbuf[3] = 0.0;
	dbuf[4] = 0.0;
	dbuf[5] = 0.0;
	dbuf[7] = -18.0;

	sbuf[0] = 6.75f;
	sbuf[1] = -3.5f;
	sbuf[2] = 1.5f;
	sbuf[3] = 0.0f;
	sbuf[4] = 0.0f;

	wbuf[0] = 0;
	wbuf[1] = 0;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS3(VOID, P, P, P), 4, 3, 4, 0, sizeof(sljit_sw));

	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S1), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_f32));
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS3(F64, F32, F32, F64), SLJIT_IMM, SLJIT_FUNC_ADDR(test58_f1));
	/* dbuf[1] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64), SLJIT_FR0, 0);

	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_f32));
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_f32));
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_f64));
	jump = sljit_emit_call(compiler, SLJIT_CALL, SLJIT_ARGS3(F64, F32, F32, F64));
	sljit_set_target(jump, SLJIT_FUNC_UADDR(test58_f1));
	/* dbuf[3] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_f64), SLJIT_FR0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_IMM, SLJIT_FUNC_ADDR(test58_f2));
	sljit_get_local_base(compiler, SLJIT_R1, 0, -16);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 16);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_f32));
	sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS3(F32, W, F64, F32), SLJIT_MEM2(SLJIT_R1, SLJIT_R0), 0);
	/* sbuf[3] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_f32), SLJIT_FR0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -4);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 9);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S1), 0);
	jump = sljit_emit_call(compiler, SLJIT_CALL, SLJIT_ARGS3(F64, W, F32, W));
	sljit_set_target(jump, SLJIT_FUNC_UADDR(test58_f3));
	/* dbuf[4] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_f64), SLJIT_FR0, 0);

	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_f32));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -6);
	jump = sljit_emit_call(compiler, SLJIT_CALL, SLJIT_ARGS2(F64, F32, W));
	sljit_set_target(jump, SLJIT_FUNC_UADDR(test58_f4));
	/* dbuf[5] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_f64), SLJIT_FR0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_IMM, SLJIT_FUNC_ADDR(test58_f5));
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_f32));
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_f64));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 8);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, 0);
	sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS3(F32, F32, F64, 32), SLJIT_MEM1(SLJIT_SP), 0);
	/* sbuf[4] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_f32), SLJIT_FR0, 0);

	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_f64));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 10000000);
	sljit_set_target(sljit_emit_call(compiler, SLJIT_CALL, SLJIT_ARGS3(F64, F64, F64, W)), SLJIT_FUNC_UADDR(test58_f6));
	/* dbuf[6] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_f64), SLJIT_FR0, 0);

	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_f64));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_FUNC_ADDR(test58_f7));
	sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS2(W, F64, W), SLJIT_R0, 0);
	/* wbuf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S2), 0, SLJIT_R0, 0);

	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_f64));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 319);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, SLJIT_FUNC_ADDR(test58_f7));
	sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS2(W, F64, W), SLJIT_R1, 0);
	/* wbuf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S2), sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func3((sljit_sw)&dbuf, (sljit_sw)&sbuf, (sljit_sw)&wbuf);

	FAILED(dbuf[1] != 8.5, "test58 case 1 failed\n");
	FAILED(dbuf[3] != 0.5, "test58 case 2 failed\n");
	FAILED(sbuf[3] != 17.75f, "test58 case 3 failed\n");
	FAILED(dbuf[4] != 11.75, "test58 case 4 failed\n");
	FAILED(dbuf[5] != -9.5, "test58 case 5 failed\n");
	FAILED(sbuf[4] != 12.0f, "test58 case 6 failed\n");
	FAILED(dbuf[6] != -945000000.0, "test58 case 7 failed\n");
	FAILED(wbuf[0] != SLJIT_FUNC_ADDR(test58_f7) - 18, "test58 case 8 failed\n");
	FAILED(wbuf[1] != 301, "test58 case 9 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static sljit_sw test59_f1(sljit_sw a, sljit_s32 b, sljit_sw c, sljit_sw d)
{
	return (sljit_sw)(a + b + c + d - SLJIT_FUNC_ADDR(test59_f1));
}

static sljit_s32 test59_f2(sljit_f64 a, sljit_f32 b, sljit_f64 c, sljit_sw d)
{
	return (sljit_s32)(a + b + c + (sljit_f64)d);
}

static sljit_f32 test59_f3(sljit_f32 a, sljit_s32 b, sljit_f64 c, sljit_sw d)
{
	return (sljit_f32)(a + (sljit_f64)b + c + (sljit_f64)d);
}

static sljit_f32 test59_f4(sljit_f32 a, sljit_f64 b, sljit_f32 c, sljit_f64 d)
{
	return (sljit_f32)(a + b + c + (sljit_f64)d);
}

static sljit_f64 test59_f5(sljit_f64 a, sljit_f64 b, sljit_f64 c, sljit_s32 d)
{
	return a + b + c + (sljit_f64)d;
}

static void test59(void)
{
	/* Check function calls with four arguments. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	struct sljit_jump* jump = NULL;
	sljit_sw wbuf[5];
	sljit_f64 dbuf[4];
	sljit_f32 sbuf[4];

	if (verbose)
		printf("Run test59\n");

	wbuf[0] = 0;
	wbuf[1] = 0;
	wbuf[2] = SLJIT_FUNC_ADDR(test59_f1);
	wbuf[3] = 0;
	wbuf[4] = 0;

	if (sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		dbuf[0] = 5.125;
		dbuf[1] = 6.125;
		dbuf[2] = 4.25;

		sbuf[0] = 0.75f;
		sbuf[1] = -1.5f;
		sbuf[2] = 0.0f;
		sbuf[3] = 0.0f;
	}

	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS3(VOID, P, P, P), 4, 3, 4, 0, sizeof(sljit_sw));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 33);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, -20);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, SLJIT_FUNC_ADDR(test59_f1));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, -40);
	sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS4(W, W, 32, W, W), SLJIT_R2, 0);
	/* wbuf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_FUNC_ADDR(test59_f1));
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, -25);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 100);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, -10);
	sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS4(W, W, 32, W, W), SLJIT_R0, 0);
	/* wbuf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, 231);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 2);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, SLJIT_FUNC_ADDR(test59_f1) - 100);
	sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS4(W, W, 32, W, W), SLJIT_MEM2(SLJIT_R0, SLJIT_R2), SLJIT_WORD_SHIFT);
	/* wbuf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_R0, 0);

	if (sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S1), 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S2), 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_f64));
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -100);
		sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS4(32, F64, F32, F64, W), SLJIT_IMM, SLJIT_FUNC_ADDR(test59_f2));
		sljit_emit_op1(compiler, SLJIT_MOV_S32, SLJIT_R0, 0, SLJIT_R0, 0);
		/* wbuf[4] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_R0, 0);

		sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S2), sizeof(sljit_f32));
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_f64));
		sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 36);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 41);
		jump = sljit_emit_call(compiler, SLJIT_CALL, SLJIT_ARGS4(F32, F32, 32, F64, W));
		sljit_set_target(jump, SLJIT_FUNC_UADDR(test59_f3));
		/* sbuf[2] */
		sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S2), 2 * sizeof(sljit_f32), SLJIT_FR0, 0);

		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_FUNC_ADDR(test59_f4));
		sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S2), 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S1), 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S2), sizeof(sljit_f32));
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR3, 0, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_f64));
		sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS4(F32, F32, F64, F32, F64), SLJIT_R0, 0);
		/* sbuf[2] */
		sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S2), 3 * sizeof(sljit_f32), SLJIT_FR0, 0);

		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_f64));
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S1), 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_f64));
		sljit_emit_op1(compiler, SLJIT_MOV_U32, SLJIT_R0, 0, SLJIT_IMM, 42);
		sljit_set_target(sljit_emit_call(compiler, SLJIT_CALL, SLJIT_ARGS4(F64, F64, F64, F64, 32)), SLJIT_FUNC_UADDR(test59_f5));
		/* dbuf[3] */
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_f64), SLJIT_FR0, 0);
	}

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func3((sljit_sw)&wbuf, (sljit_sw)&dbuf, (sljit_sw)&sbuf);

	FAILED(wbuf[0] != -27, "test59 case 1 failed\n");
	FAILED(wbuf[1] != 65, "test59 case 2 failed\n");
	FAILED(wbuf[3] != (sljit_sw)wbuf + 133, "test59 case 3 failed\n");

	if (sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		FAILED(wbuf[4] != -88, "test59 case 4 failed\n");
		FAILED(sbuf[2] != 79.75f, "test59 case 5 failed\n");
		FAILED(sbuf[3] != 8.625f, "test59 case 6 failed\n");
		FAILED(dbuf[3] != 57.5, "test59 case 7 failed\n");
	}

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test60(void)
{
	/* Test memory accesses with pre/post updates. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_u32 i;
	sljit_s32 supported[10];
	sljit_sw wbuf[18];
	sljit_s8 bbuf[4];
	sljit_s32 ibuf[4];

#if (defined SLJIT_CONFIG_ARM_64 && SLJIT_CONFIG_ARM_64) || (defined SLJIT_CONFIG_ARM_THUMB2 && SLJIT_CONFIG_ARM_THUMB2)
	static sljit_u8 expected[10] = { 1, 1, 1, 1, 1, 1, 0, 0, 0, 0 };
#elif (defined SLJIT_CONFIG_ARM_32 && SLJIT_CONFIG_ARM_32)
	static sljit_u8 expected[10] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
#elif (defined SLJIT_CONFIG_PPC_32 && SLJIT_CONFIG_PPC_32)
	static sljit_u8 expected[10] = { 1, 0, 1, 1, 0, 1, 1, 1, 0, 0 };
#elif (defined SLJIT_CONFIG_PPC_64 && SLJIT_CONFIG_PPC_64)
	static sljit_u8 expected[10] = { 1, 0, 0, 1, 0, 0, 1, 1, 0, 0 };
#else
	static sljit_u8 expected[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
#endif

	if (verbose)
		printf("Run test60\n");

	for (i = 0; i < 18; i++)
		wbuf[i] = 0;
	wbuf[2] = -887766;

	bbuf[0] = 0;
	bbuf[1] = 0;
	bbuf[2] = -13;

	ibuf[0] = -5678;
	ibuf[1] = 0;
	ibuf[2] = 0;

	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS3(VOID, P, P, P), 4, 3, 4, 0, sizeof(sljit_sw));

	supported[0] = sljit_emit_mem_update(compiler, SLJIT_MOV | SLJIT_MEM_SUPP, SLJIT_R1, SLJIT_MEM1(SLJIT_R0), 2 * sizeof(sljit_sw));
	if (supported[0] == SLJIT_SUCCESS) {
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
		sljit_emit_mem_update(compiler, SLJIT_MOV, SLJIT_R1, SLJIT_MEM1(SLJIT_R0), 2 * sizeof(sljit_sw));
		/* buf[0] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R1, 0);
		/* buf[1] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R0, 0);
	}

	supported[1] = sljit_emit_mem_update(compiler, SLJIT_MOV_S8 | SLJIT_MEM_SUPP | SLJIT_MEM_POST, SLJIT_R0, SLJIT_MEM1(SLJIT_R2), -2 * (sljit_sw)sizeof(sljit_s8));
	if (supported[1] == SLJIT_SUCCESS) {
		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R2, 0, SLJIT_S1, 0, SLJIT_IMM, 2 * sizeof(sljit_s8));
		sljit_emit_mem_update(compiler, SLJIT_MOV_S8 | SLJIT_MEM_POST, SLJIT_R0, SLJIT_MEM1(SLJIT_R2), -2 * (sljit_sw)sizeof(sljit_s8));
		/* buf[3] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_R0, 0);
		/* buf[4] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_R2, 0);
	}

	supported[2] = sljit_emit_mem_update(compiler, SLJIT_MOV_S32 | SLJIT_MEM_SUPP, SLJIT_R2, SLJIT_MEM1(SLJIT_R1), -2 * (sljit_sw)sizeof(sljit_s32));
	if (supported[2] == SLJIT_SUCCESS) {
		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S2, 0, SLJIT_IMM, 2 * sizeof(sljit_s32));
		sljit_emit_mem_update(compiler, SLJIT_MOV_S32, SLJIT_R2, SLJIT_MEM1(SLJIT_R1), -2 * (sljit_sw)sizeof(sljit_s32));
		/* buf[5] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_R2, 0);
		/* buf[6] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_R1, 0);
	}

	supported[3] = sljit_emit_mem_update(compiler, SLJIT_MOV32 | SLJIT_MEM_SUPP | SLJIT_MEM_STORE, SLJIT_R1, SLJIT_MEM1(SLJIT_R2), 2 * sizeof(sljit_s32));
	if (supported[3] == SLJIT_SUCCESS) {
		sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, -8765);
		sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R2, 0, SLJIT_S2, 0, SLJIT_IMM, sizeof(sljit_s32));
		sljit_emit_mem_update(compiler, SLJIT_MOV32 | SLJIT_MEM_STORE, SLJIT_R1, SLJIT_MEM1(SLJIT_R2), 2 * sizeof(sljit_s32));
		/* buf[7] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_R2, 0);
	}

	supported[4] = sljit_emit_mem_update(compiler, SLJIT_MOV_S8 | SLJIT_MEM_SUPP | SLJIT_MEM_STORE | SLJIT_MEM_POST, SLJIT_R1, SLJIT_MEM1(SLJIT_R2), -128 * (sljit_sw)sizeof(sljit_s8));
	if (supported[4] == SLJIT_SUCCESS) {
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, -121);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_S1, 0);
		sljit_emit_mem_update(compiler, SLJIT_MOV_S8 | SLJIT_MEM_STORE | SLJIT_MEM_POST, SLJIT_R1, SLJIT_MEM1(SLJIT_R2), -128 * (sljit_sw)sizeof(sljit_s8));
		/* buf[8] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw), SLJIT_R2, 0);
	}

	supported[5] = sljit_emit_mem_update(compiler, SLJIT_MOV | SLJIT_MEM_SUPP | SLJIT_MEM_STORE, SLJIT_R1, SLJIT_MEM1(SLJIT_R0), 1);
	if (supported[5] == SLJIT_SUCCESS) {
		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_IMM, 9 * sizeof(sljit_sw) - 1);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, -881199);
		sljit_emit_mem_update(compiler, SLJIT_MOV | SLJIT_MEM_STORE, SLJIT_R1, SLJIT_MEM1(SLJIT_R0), 1);
		/* buf[10] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 10 * sizeof(sljit_sw), SLJIT_R0, 0);
	}

	supported[6] = sljit_emit_mem_update(compiler, SLJIT_MOV_S32 | SLJIT_MEM_SUPP, SLJIT_R0, SLJIT_MEM2(SLJIT_R1, SLJIT_R2), 0);
	if (supported[6] == SLJIT_SUCCESS) {
		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S2, 0, SLJIT_IMM, 213);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, -213);
		sljit_emit_mem_update(compiler, SLJIT_MOV_S32, SLJIT_R0, SLJIT_MEM2(SLJIT_R1, SLJIT_R2), 0);
		/* buf[11] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 11 * sizeof(sljit_sw), SLJIT_R0, 0);
		/* buf[12] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 12 * sizeof(sljit_sw), SLJIT_R1, 0);
	}

	supported[7] = sljit_emit_mem_update(compiler, SLJIT_MOV_S32 | SLJIT_MEM_SUPP | SLJIT_MEM_STORE, SLJIT_R0, SLJIT_MEM2(SLJIT_R1, SLJIT_R2), 0);
	if (supported[7] == SLJIT_SUCCESS) {
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_S2, 0);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 2 * sizeof(sljit_s32));
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -7890);
		sljit_emit_mem_update(compiler, SLJIT_MOV_S32 | SLJIT_MEM_STORE, SLJIT_R0, SLJIT_MEM2(SLJIT_R1, SLJIT_R2), 0);
		/* buf[13] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 13 * sizeof(sljit_sw), SLJIT_R1, 0);
	}

	supported[8] = sljit_emit_mem_update(compiler, SLJIT_MOV | SLJIT_MEM_SUPP | SLJIT_MEM_POST, SLJIT_R0, SLJIT_MEM2(SLJIT_R1, SLJIT_R2), 2);
	if (supported[8] == SLJIT_SUCCESS) {
		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S0, 0, SLJIT_IMM, 2 * sizeof(sljit_sw));
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 2 * sizeof(sljit_sw));
		sljit_emit_mem_update(compiler, SLJIT_MOV | SLJIT_MEM_POST, SLJIT_R0, SLJIT_MEM2(SLJIT_R1, SLJIT_R2), 2);
		/* buf[14] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 14 * sizeof(sljit_sw), SLJIT_R0, 0);
		/* buf[15] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 15 * sizeof(sljit_sw), SLJIT_R1, 0);
	}

	supported[9] = sljit_emit_mem_update(compiler, SLJIT_MOV_S8 | SLJIT_MEM_SUPP | SLJIT_MEM_POST, SLJIT_R0, SLJIT_MEM2(SLJIT_R1, SLJIT_R2), 0);
	if (supported[9] == SLJIT_SUCCESS) {
		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S1, 0, SLJIT_IMM, 2 * sizeof(sljit_s8));
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, -2 * (sljit_sw)sizeof(sljit_s8));
		sljit_emit_mem_update(compiler, SLJIT_MOV_S8 | SLJIT_MEM_POST, SLJIT_R0, SLJIT_MEM2(SLJIT_R1, SLJIT_R2), 0);
		/* buf[16] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 16 * sizeof(sljit_sw), SLJIT_R0, 0);
		/* buf[17] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 17 * sizeof(sljit_sw), SLJIT_R1, 0);
	}

	SLJIT_ASSERT(sljit_emit_mem_update(compiler, SLJIT_MOV_S8 | SLJIT_MEM_SUPP, SLJIT_R0, SLJIT_MEM2(SLJIT_R1, SLJIT_R2), 1) == SLJIT_ERR_UNSUPPORTED);
	SLJIT_ASSERT(sljit_emit_mem_update(compiler, SLJIT_MOV_S8 | SLJIT_MEM_SUPP, SLJIT_R0, SLJIT_MEM2(SLJIT_R1, SLJIT_R2), 1) == SLJIT_ERR_UNSUPPORTED);

#if (defined SLJIT_CONFIG_ARM_THUMB2 && SLJIT_CONFIG_ARM_THUMB2) || (defined SLJIT_CONFIG_ARM_64 && SLJIT_CONFIG_ARM_64)
	/* TODO: at least for ARM (both V5 and V7) the range below needs further fixing */
	SLJIT_ASSERT(sljit_emit_mem_update(compiler, SLJIT_MOV | SLJIT_MEM_SUPP, SLJIT_R1, SLJIT_MEM1(SLJIT_R0), 256) == SLJIT_ERR_UNSUPPORTED);
	SLJIT_ASSERT(sljit_emit_mem_update(compiler, SLJIT_MOV | SLJIT_MEM_SUPP | SLJIT_MEM_POST, SLJIT_R1, SLJIT_MEM1(SLJIT_R0), -257) == SLJIT_ERR_UNSUPPORTED);
#endif

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func3((sljit_sw)&wbuf, (sljit_sw)&bbuf, (sljit_sw)&ibuf);

	FAILED(sizeof(expected) != sizeof(supported) / sizeof(sljit_s32), "test60 case 1 failed\n");

	for (i = 0; i < sizeof(expected); i++) {
		if (expected[i]) {
			if (supported[i] != SLJIT_SUCCESS) {
				printf("tast60 case %d should be supported\n", i + 1);
				return;
			}
		} else {
			if (supported[i] == SLJIT_SUCCESS) {
				printf("test60 case %d should not be supported\n", i + 1);
				return;
			}
		}
	}

	FAILED(supported[0] == SLJIT_SUCCESS && wbuf[0] != -887766, "test60 case 2 failed\n");
	FAILED(supported[0] == SLJIT_SUCCESS && wbuf[1] != (sljit_sw)(wbuf + 2), "test60 case 3 failed\n");
	FAILED(supported[1] == SLJIT_SUCCESS && wbuf[3] != -13, "test60 case 4 failed\n");
	FAILED(supported[1] == SLJIT_SUCCESS && wbuf[4] != (sljit_sw)(bbuf), "test60 case 5 failed\n");
	FAILED(supported[2] == SLJIT_SUCCESS && wbuf[5] != -5678, "test60 case 6 failed\n");
	FAILED(supported[2] == SLJIT_SUCCESS && wbuf[6] != (sljit_sw)(ibuf), "test60 case 7 failed\n");
	FAILED(supported[3] == SLJIT_SUCCESS && ibuf[1] != -8765, "test60 case 8 failed\n");
	FAILED(supported[3] == SLJIT_SUCCESS && wbuf[7] != (sljit_sw)(ibuf + 1), "test60 case 9 failed\n");
	FAILED(supported[4] == SLJIT_SUCCESS && bbuf[0] != -121, "test60 case 10 failed\n");
	FAILED(supported[4] == SLJIT_SUCCESS && wbuf[8] != (sljit_sw)(bbuf) - 128 * (sljit_sw)sizeof(sljit_s8), "test60 case 11 failed\n");
	FAILED(supported[5] == SLJIT_SUCCESS && wbuf[9] != -881199, "test60 case 12 failed\n");
	FAILED(supported[5] == SLJIT_SUCCESS && wbuf[10] != (sljit_sw)(wbuf + 9), "test60 case 13 failed\n");
	FAILED(supported[6] == SLJIT_SUCCESS && wbuf[11] != -5678, "test60 case 14 failed\n");
	FAILED(supported[6] == SLJIT_SUCCESS && wbuf[12] != (sljit_sw)(ibuf), "test60 case 15 failed\n");
	FAILED(supported[7] == SLJIT_SUCCESS && ibuf[2] != -7890, "test60 case 16 failed\n");
	FAILED(supported[7] == SLJIT_SUCCESS && wbuf[13] != (sljit_sw)(ibuf + 2), "test60 case 17 failed\n");
	FAILED(supported[8] == SLJIT_SUCCESS && wbuf[14] != -887766, "test60 case 18 failed\n");
	FAILED(supported[8] == SLJIT_SUCCESS && wbuf[15] != (sljit_sw)(wbuf + 10), "test60 case 19 failed\n");
	FAILED(supported[9] == SLJIT_SUCCESS && wbuf[16] != -13, "test60 case 20 failed\n");
	FAILED(supported[9] == SLJIT_SUCCESS && wbuf[17] != (sljit_sw)(bbuf), "test60 case 21 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test61(void)
{
	/* Test float memory accesses with pre/post updates. */
	executable_code code;
	struct sljit_compiler* compiler;
	sljit_u32 i;
	sljit_s32 supported[6];
	sljit_sw wbuf[6];
	sljit_f64 dbuf[4];
	sljit_f32 sbuf[4];
#if (defined SLJIT_CONFIG_ARM_64 && SLJIT_CONFIG_ARM_64)
	static sljit_u8 expected[6] = { 1, 1, 1, 1, 0, 0 };
#elif (defined SLJIT_CONFIG_PPC && SLJIT_CONFIG_PPC)
	static sljit_u8 expected[6] = { 1, 0, 1, 0, 1, 1 };
#else
	static sljit_u8 expected[6] = { 0, 0, 0, 0, 0, 0 };
#endif

	if (verbose)
		printf("Run test61\n");

	if (!sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		if (verbose)
			printf("no fpu available, test61 skipped\n");
		successful_tests++;
		return;
	}

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	for (i = 0; i < 6; i++)
		wbuf[i] = 0;

	dbuf[0] = 66.725;
	dbuf[1] = 0.0;
	dbuf[2] = 0.0;
	dbuf[3] = 0.0;

	sbuf[0] = 0.0;
	sbuf[1] = -22.125;
	sbuf[2] = 0.0;
	sbuf[3] = 0.0;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS3(VOID, P, P, P), 4, 3, 4, 0, sizeof(sljit_sw));

	supported[0] = sljit_emit_fmem_update(compiler, SLJIT_MOV_F64 | SLJIT_MEM_SUPP, SLJIT_FR0, SLJIT_MEM1(SLJIT_R0), 4 * sizeof(sljit_f64));
	if (supported[0] == SLJIT_SUCCESS) {
		/* dbuf[1] */
		sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R0, 0, SLJIT_S1, 0, SLJIT_IMM, 4 * sizeof(sljit_f64));
		sljit_emit_fmem_update(compiler, SLJIT_MOV_F64, SLJIT_FR0, SLJIT_MEM1(SLJIT_R0), 4 * sizeof(sljit_f64));
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_f64), SLJIT_FR0, 0);
		/* wbuf[0] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);
	}

	supported[1] = sljit_emit_fmem_update(compiler, SLJIT_MOV_F64 | SLJIT_MEM_SUPP | SLJIT_MEM_STORE | SLJIT_MEM_POST, SLJIT_FR2, SLJIT_MEM1(SLJIT_R0), -(sljit_sw)sizeof(sljit_f64));
	if (supported[1] == SLJIT_SUCCESS) {
		/* dbuf[2] */
		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S1, 0, SLJIT_IMM, 2 * sizeof(sljit_f64));
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S1), 0);
		sljit_emit_fmem_update(compiler, SLJIT_MOV_F64 | SLJIT_MEM_STORE | SLJIT_MEM_POST, SLJIT_FR2, SLJIT_MEM1(SLJIT_R0), -(sljit_sw)sizeof(sljit_f64));
		/* wbuf[1] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R0, 0);
	}

	supported[2] = sljit_emit_fmem_update(compiler, SLJIT_MOV_F32 | SLJIT_MEM_SUPP | SLJIT_MEM_STORE, SLJIT_FR1, SLJIT_MEM1(SLJIT_R2), -4 * (sljit_sw)sizeof(sljit_f32));
	if (supported[2] == SLJIT_SUCCESS) {
		/* sbuf[0] */
		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R2, 0, SLJIT_S2, 0, SLJIT_IMM, 4 * sizeof(sljit_f32));
		sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S2), sizeof(sljit_f32));
		sljit_emit_fmem_update(compiler, SLJIT_MOV_F32 | SLJIT_MEM_STORE, SLJIT_FR1, SLJIT_MEM1(SLJIT_R2), -4 * (sljit_sw)sizeof(sljit_f32));
		/* wbuf[2] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_R2, 0);
	}

	supported[3] = sljit_emit_fmem_update(compiler, SLJIT_MOV_F32 | SLJIT_MEM_SUPP | SLJIT_MEM_POST, SLJIT_FR1, SLJIT_MEM1(SLJIT_R1), sizeof(sljit_f32));
	if (supported[3] == SLJIT_SUCCESS) {
		/* sbuf[2] */
		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S2, 0, SLJIT_IMM, sizeof(sljit_f32));
		sljit_emit_fmem_update(compiler, SLJIT_MOV_F32 | SLJIT_MEM_POST, SLJIT_FR1, SLJIT_MEM1(SLJIT_R1), sizeof(sljit_f32));
		sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S2), 2 * sizeof(sljit_f32), SLJIT_FR1, 0);
		/* wbuf[3] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_R1, 0);
	}

	supported[4] = sljit_emit_fmem_update(compiler, SLJIT_MOV_F64 | SLJIT_MEM_SUPP, SLJIT_FR0, SLJIT_MEM2(SLJIT_R1, SLJIT_R0), 0);
	if (supported[4] == SLJIT_SUCCESS) {
		/* dbuf[3] */
		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S1, 0, SLJIT_IMM, 8 * sizeof(sljit_f64));
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -8 * (sljit_sw)sizeof(sljit_f64));
		sljit_emit_fmem_update(compiler, SLJIT_MOV_F64, SLJIT_FR0, SLJIT_MEM2(SLJIT_R1, SLJIT_R0), 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_f64), SLJIT_FR0, 0);
		/* wbuf[4] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_R1, 0);
	}

	supported[5] = sljit_emit_fmem_update(compiler, SLJIT_MOV_F32 | SLJIT_MEM_SUPP | SLJIT_MEM_STORE, SLJIT_FR2, SLJIT_MEM2(SLJIT_R2, SLJIT_R1), 0);
	if (supported[5] == SLJIT_SUCCESS) {
		/* sbuf[3] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_S2, 0);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 3 * sizeof(sljit_f32));
		sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S2), sizeof(sljit_f32));
		sljit_emit_fmem_update(compiler, SLJIT_MOV_F32 | SLJIT_MEM_STORE, SLJIT_FR2, SLJIT_MEM2(SLJIT_R2, SLJIT_R1), 0);
		/* wbuf[5] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_R2, 0);
	}

	SLJIT_ASSERT(sljit_emit_fmem_update(compiler, SLJIT_MOV_F64 | SLJIT_MEM_SUPP | SLJIT_MEM_POST, SLJIT_FR0, SLJIT_MEM2(SLJIT_R1, SLJIT_R2), 0) == SLJIT_ERR_UNSUPPORTED);
	SLJIT_ASSERT(sljit_emit_fmem_update(compiler, SLJIT_MOV_F32 | SLJIT_MEM_SUPP | SLJIT_MEM_STORE | SLJIT_MEM_POST, SLJIT_FR0, SLJIT_MEM2(SLJIT_R1, SLJIT_R2), 0) == SLJIT_ERR_UNSUPPORTED);

#if (defined SLJIT_CONFIG_ARM_64 && SLJIT_CONFIG_ARM_64)
	/* TODO: at least for ARM (both V5 and V7) the range below needs further fixing */
	SLJIT_ASSERT(sljit_emit_fmem_update(compiler, SLJIT_MOV_F64 | SLJIT_MEM_SUPP, SLJIT_FR0, SLJIT_MEM1(SLJIT_R0), 256) == SLJIT_ERR_UNSUPPORTED);
	SLJIT_ASSERT(sljit_emit_fmem_update(compiler, SLJIT_MOV_F64 | SLJIT_MEM_SUPP | SLJIT_MEM_POST, SLJIT_FR0, SLJIT_MEM1(SLJIT_R0), -257) == SLJIT_ERR_UNSUPPORTED);
#endif

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func3((sljit_sw)&wbuf, (sljit_sw)&dbuf, (sljit_sw)&sbuf);

	FAILED(sizeof(expected) != sizeof(supported) / sizeof(sljit_s32), "test61 case 1 failed\n");

	for (i = 0; i < sizeof(expected); i++) {
		if (expected[i]) {
			if (supported[i] != SLJIT_SUCCESS) {
				printf("tast61 case %d should be supported\n", i + 1);
				return;
			}
		} else {
			if (supported[i] == SLJIT_SUCCESS) {
				printf("test61 case %d should not be supported\n", i + 1);
				return;
			}
		}
	}

	FAILED(supported[0] == SLJIT_SUCCESS && dbuf[1] != 66.725, "test61 case 2 failed\n");
	FAILED(supported[0] == SLJIT_SUCCESS && wbuf[0] != (sljit_sw)(dbuf), "test61 case 3 failed\n");
	FAILED(supported[1] == SLJIT_SUCCESS && dbuf[2] != 66.725, "test61 case 4 failed\n");
	FAILED(supported[1] == SLJIT_SUCCESS && wbuf[1] != (sljit_sw)(dbuf + 1), "test61 case 5 failed\n");
	FAILED(supported[2] == SLJIT_SUCCESS && sbuf[0] != -22.125, "test61 case 6 failed\n");
	FAILED(supported[2] == SLJIT_SUCCESS && wbuf[2] != (sljit_sw)(sbuf), "test61 case 7 failed\n");
	FAILED(supported[3] == SLJIT_SUCCESS && sbuf[2] != -22.125, "test61 case 8 failed\n");
	FAILED(supported[3] == SLJIT_SUCCESS && wbuf[3] != (sljit_sw)(sbuf + 2), "test61 case 9 failed\n");
	FAILED(supported[4] == SLJIT_SUCCESS && dbuf[3] != 66.725, "test61 case 10 failed\n");
	FAILED(supported[4] == SLJIT_SUCCESS && wbuf[4] != (sljit_sw)(dbuf), "test61 case 11 failed\n");
	FAILED(supported[5] == SLJIT_SUCCESS && sbuf[3] != -22.125, "test61 case 12 failed\n");
	FAILED(supported[5] == SLJIT_SUCCESS && wbuf[5] != (sljit_sw)(sbuf + 3), "test61 case 13 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test62(void)
{
	/* Test fast calls flag preservation. */
	executable_code code1;
	executable_code code2;
	struct sljit_compiler* compiler;

	if (verbose)
		printf("Run test62\n");

	/* A */
	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");
	sljit_set_context(compiler, 0, SLJIT_ARGS1(W, W), 1, 1, 0, 0, 0);

	sljit_emit_op_dst(compiler, SLJIT_FAST_ENTER, SLJIT_R0, 0);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_Z | SLJIT_SET_LESS, SLJIT_S0, 0, SLJIT_IMM, 42);
	sljit_emit_op_src(compiler, SLJIT_FAST_RETURN, SLJIT_R0, 0);

	code1.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	/* B */
	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(W, W), 1, 1, 0, 0, 0);
	sljit_emit_ijump(compiler, SLJIT_FAST_CALL, SLJIT_IMM, SLJIT_FUNC_ADDR(code1.code));
	sljit_set_current_flags(compiler, SLJIT_CURRENT_FLAGS_SUB | SLJIT_CURRENT_FLAGS_COMPARE | SLJIT_SET_Z | SLJIT_SET_LESS);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_ZERO);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_LESS);
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_S0, 0, SLJIT_S0, 0, SLJIT_IMM, 1);
	sljit_emit_op2(compiler, SLJIT_OR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_S0, 0);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_R0, 0);

	code2.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code2.func1(88) != 0, "test62 case 1 failed\n");
	FAILED(code2.func1(42) != 1, "test62 case 2 failed\n");
	FAILED(code2.func1(0) != 2, "test62 case 3 failed\n");

	sljit_free_code(code1.code, NULL);
	sljit_free_code(code2.code, NULL);
	successful_tests++;
}

static void test63(void)
{
	/* Test put label. */
	executable_code code;
	struct sljit_label *label[2];
	struct sljit_put_label *put_label[5];
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_uw addr[2];
	sljit_uw buf[4];
	sljit_sw offs = WCONST(0x123456789012, 0x12345678);

	if (verbose)
		printf("Run test63\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(W, P), 3, 1, 0, 0, 2 * sizeof(sljit_sw));

	/* buf[0-1] */
	put_label[0] = sljit_emit_put_label(compiler, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);

	put_label[1] = sljit_emit_put_label(compiler, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_uw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw), SLJIT_MEM1(SLJIT_SP), sizeof(sljit_uw));

	label[0] = sljit_emit_label(compiler);
	sljit_set_put_label(put_label[0], label[0]);
	sljit_set_put_label(put_label[1], label[0]);

	/* buf[2-3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)(buf + 2) - offs);
	put_label[2] = sljit_emit_put_label(compiler, SLJIT_MEM1(SLJIT_R0), offs);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (offs + (sljit_sw)sizeof(sljit_uw)) >> 1);
	put_label[3] = sljit_emit_put_label(compiler, SLJIT_MEM2(SLJIT_R0, SLJIT_R1), 1);

	label[1] = sljit_emit_label(compiler);
	sljit_set_put_label(put_label[2], label[1]);
	sljit_set_put_label(put_label[3], label[1]);

	/* Return value */
	put_label[4] = sljit_emit_put_label(compiler, SLJIT_RETURN_REG, 0);
	sljit_set_put_label(put_label[4], label[0]);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);

	addr[0] = sljit_get_label_addr(label[0]);
	addr[1] = sljit_get_label_addr(label[1]);

	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_sw)&buf) != (sljit_sw)addr[0], "test63 case 1 failed\n");
	FAILED(buf[0] != addr[0], "test63 case 2 failed\n");
	FAILED(buf[1] != addr[0], "test63 case 3 failed\n");
	FAILED(buf[2] != addr[1], "test63 case 4 failed\n");
	FAILED(buf[3] != addr[1], "test63 case 5 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test64(void)
{
	/* Test put label with absolute label addresses */
	executable_code code;
	sljit_uw malloc_addr;
	struct sljit_label label[6];
	struct sljit_put_label *put_label[2];
	struct sljit_compiler* compiler;
	sljit_uw buf[7];
	sljit_s32 i;
	/* Must be even because it is also used for addressing. */
	sljit_sw offs1 = WCONST(0x1234567812, 0x80000000);
	sljit_sw offs2 = WCONST(0x123456781122, 0xe0000000);
	sljit_sw offs3 = WCONST(0x7fffffff7ff, 0x87654321);
	sljit_sw offs4 = WCONST(0x1234567811223344, 0xffffffff);

	if (verbose)
		printf("Run test64\n");

	/* lock next allocation; see sljit_test_malloc_exec() */
#if !(defined SLJIT_CONFIG_UNSUPPORTED && SLJIT_CONFIG_UNSUPPORTED)
	malloc_addr = (sljit_uw)SLJIT_MALLOC_EXEC(1024, NULL);

	if (!malloc_addr) {
		printf("Cannot allocate executable memory\n");
		return;
	}

	compiler = sljit_create_compiler(NULL, (void*)malloc_addr);
	malloc_addr += (sljit_uw)SLJIT_EXEC_OFFSET((void*)malloc_addr);
#else /* SLJIT_CONFIG_UNSUPPORTED */
	malloc_addr = 0;
	compiler = sljit_create_compiler(NULL, (void*)malloc_addr);
#endif /* !SLJIT_CONFIG_UNSUPPORTED */

	label[0].addr = 0x1234;
	label[0].size = (sljit_uw)0x1234 - malloc_addr;

	label[1].addr = 0x12345678;
	label[1].size = (sljit_uw)0x12345678 - malloc_addr;

	label[2].addr = (sljit_uw)offs1;
	label[2].size = (sljit_uw)offs1 - malloc_addr;

	label[3].addr = (sljit_uw)offs2;
	label[3].size = (sljit_uw)offs2 - malloc_addr;

	label[4].addr = (sljit_uw)offs3;
	label[4].size = (sljit_uw)offs3 - malloc_addr;

	label[5].addr = (sljit_uw)offs4;
	label[5].size = (sljit_uw)offs4 - malloc_addr;

	FAILED(!compiler, "cannot create compiler\n");

	for (i = 0; i < 6; i++)
		buf[i] = 0;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(W, P), 3, 1, 0, 0, 2 * sizeof(sljit_sw));

	/* buf[0] */
	put_label[0] = sljit_emit_put_label(compiler, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);

	/* buf[1] */
	put_label[1] = sljit_emit_put_label(compiler, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_uw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_uw), SLJIT_MEM1(SLJIT_SP), sizeof(sljit_uw));

	sljit_set_put_label(put_label[0], &label[0]);
	sljit_set_put_label(put_label[1], &label[0]);

	/* buf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)(buf + 2) - offs1);
	put_label[0] = sljit_emit_put_label(compiler, SLJIT_MEM1(SLJIT_R0), offs1);

	/* buf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (offs1 + (sljit_sw)sizeof(sljit_uw)) >> 1);
	put_label[1] = sljit_emit_put_label(compiler, SLJIT_MEM2(SLJIT_R0, SLJIT_R1), 1);

	sljit_set_put_label(put_label[0], &label[1]);
	sljit_set_put_label(put_label[1], &label[1]);

	/* buf[4] */
	put_label[0] = sljit_emit_put_label(compiler, SLJIT_R1, 0);
	sljit_set_put_label(put_label[0], &label[2]);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_uw), SLJIT_R1, 0);

	/* buf[5] */
	put_label[0] = sljit_emit_put_label(compiler, SLJIT_R2, 0);
	sljit_set_put_label(put_label[0], &label[3]);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_uw), SLJIT_R2, 0);

	/* buf[6] */
	put_label[0] = sljit_emit_put_label(compiler, SLJIT_R1, 0);
	sljit_set_put_label(put_label[0], &label[4]);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_uw), SLJIT_R1, 0);

	/* buf[7] */
	put_label[0] = sljit_emit_put_label(compiler, SLJIT_RETURN_REG, 0);
	sljit_set_put_label(put_label[0], &label[5]);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);
	SLJIT_ASSERT(SLJIT_FUNC_UADDR(code.code) >= malloc_addr && SLJIT_FUNC_UADDR(code.code) <= malloc_addr + 8);

	FAILED(code.func1((sljit_sw)&buf) != (sljit_sw)label[5].addr, "test64 case 1 failed\n");
	FAILED(buf[0] != label[0].addr, "test64 case 2 failed\n");
	FAILED(buf[1] != label[0].addr, "test64 case 3 failed\n");
	FAILED(buf[2] != label[1].addr, "test64 case 4 failed\n");
	FAILED(buf[3] != label[1].addr, "test64 case 5 failed\n");
	FAILED(buf[4] != label[2].addr, "test64 case 6 failed\n");
	FAILED(buf[5] != label[3].addr, "test64 case 7 failed\n");
	FAILED(buf[6] != label[4].addr, "test64 case 8 failed\n");

	sljit_free_code(code.code, NULL);

	successful_tests++;
}

static void test65(void)
{
	/* Test jump tables. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_s32 i;
	/* Normally this table is allocated on the heap. */
	sljit_uw addr[64];
	struct sljit_label *labels[64];
	struct sljit_jump *jump;

	if (verbose)
		printf("Run test65\n");

	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(W, W, W), 1, 2, 0, 0, 0);

	jump = sljit_emit_cmp(compiler, SLJIT_GREATER_EQUAL, SLJIT_S0, 0, SLJIT_IMM, 64);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)addr);
	sljit_emit_ijump(compiler, SLJIT_JUMP, SLJIT_MEM2(SLJIT_R0, SLJIT_S0), SLJIT_WORD_SHIFT);

	for (i = 0; i < 64; i++) {
		labels[i] = sljit_emit_label(compiler);
		sljit_emit_op0(compiler, SLJIT_ENDBR);
		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S1, 0, SLJIT_IMM, i * 2);
		sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);
	}

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_IMM, -1);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);

	for (i = 0; i < 64; i++) {
		addr[i] = sljit_get_label_addr(labels[i]);
	}

	sljit_free_compiler(compiler);

	FAILED(code.func2(64, 0) != -1, "test65 case 1 failed\n");

	for (i = 0; i < 64; i++) {
		FAILED(code.func2(i, i * 2) != i * 4, "test65 case 2 failed\n");
	}

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test66(void)
{
	/* Test direct jumps (computed goto). */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_s32 i;
	sljit_uw addr[64];
	struct sljit_label *labels[64];

	if (verbose)
		printf("Run test66\n");

	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(W, W, W), 1, 2, 0, 0, 0);
	sljit_emit_ijump(compiler, SLJIT_JUMP, SLJIT_S0, 0);

	for (i = 0; i < 64; i++) {
		labels[i] = sljit_emit_label(compiler);
		sljit_emit_op0(compiler, SLJIT_ENDBR);
		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S1, 0, SLJIT_IMM, i * 2);
		sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);
	}

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);

	for (i = 0; i < 64; i++) {
		addr[i] = sljit_get_label_addr(labels[i]);
	}

	sljit_free_compiler(compiler);

	for (i = 0; i < 64; i++) {
		FAILED(code.func2((sljit_sw)addr[i], i) != i * 3, "test66 case 1 failed\n");
	}

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test67(void)
{
	/* Test skipping returns from fast calls (return type is fast). */
	executable_code code;
	struct sljit_compiler *compiler = sljit_create_compiler(NULL, NULL);
	struct sljit_jump *call, *jump;
	struct sljit_label *label;

	if (verbose)
		printf("Run test67\n");

	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(W), 3, 1, 0, 0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	call = sljit_emit_jump(compiler, SLJIT_FAST_CALL);

	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_R0, 0);

	/* First function, never returns. */
	label = sljit_emit_label(compiler);
	sljit_set_label(call, label);
	sljit_emit_op_dst(compiler, SLJIT_FAST_ENTER, SLJIT_R1, 0);

	call = sljit_emit_jump(compiler, SLJIT_FAST_CALL);

	/* Should never return here, marked by a segmentation fault if it does. */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM0(), 0);

	/* Second function, skips the first function. */
	sljit_set_label(call, sljit_emit_label(compiler));
	sljit_emit_op_dst(compiler, SLJIT_FAST_ENTER, SLJIT_R2, 0);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 1);

	jump = sljit_emit_cmp(compiler, SLJIT_NOT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 1);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_R1, 0);
	sljit_set_label(sljit_emit_jump(compiler, SLJIT_FAST_CALL), label);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op_src(compiler, SLJIT_SKIP_FRAMES_BEFORE_FAST_RETURN, SLJIT_S0, 0);
	sljit_emit_op_src(compiler, SLJIT_FAST_RETURN, SLJIT_S0, 0);

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_op_src(compiler, SLJIT_SKIP_FRAMES_BEFORE_FAST_RETURN, SLJIT_R1, 0);
	sljit_emit_op_src(compiler, SLJIT_FAST_RETURN, SLJIT_R1, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);

	sljit_free_compiler(compiler);

	FAILED(code.func0() != 3, "test67 case 1 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test68(void)
{
	/* Test skipping returns from fast calls (return type is normal). */
	executable_code code;
	struct sljit_compiler *compiler;
	struct sljit_jump *call, *jump;
	struct sljit_label *label;
	int i;

	if (verbose)
		printf("Run test68\n");

	for (i = 0; i < 6; i++) {
		compiler = sljit_create_compiler(NULL, NULL);
		FAILED(!compiler, "cannot create compiler\n");

		sljit_emit_enter(compiler, 0, SLJIT_ARGS0(W), 2 + (i % 6), (i % 6), 0, 0, 0);

		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
		call = sljit_emit_jump(compiler, SLJIT_FAST_CALL);

		/* Should never return here, marked by a segmentation fault if it does. */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM0(), 0);

		/* Recursive fast call. */
		label = sljit_emit_label(compiler);
		sljit_set_label(call, label);
		sljit_emit_op_dst(compiler, SLJIT_FAST_ENTER, SLJIT_R1, 0);

		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 1);

		jump = sljit_emit_cmp(compiler, SLJIT_GREATER_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 4);

		sljit_set_label(sljit_emit_jump(compiler, SLJIT_FAST_CALL), label);

		sljit_set_label(jump, sljit_emit_label(compiler));
		sljit_emit_op0(compiler, SLJIT_SKIP_FRAMES_BEFORE_RETURN);
		sljit_emit_return(compiler, SLJIT_MOV, SLJIT_R0, 0);

		code.code = sljit_generate_code(compiler);
		CHECK(compiler);

		sljit_free_compiler(compiler);

		if (SLJIT_UNLIKELY(code.func0() != 4)) {
			printf("test68 case %d failed\n", i + 1);
			return;
		}
		sljit_free_code(code.code, NULL);
	}

	successful_tests++;
}

static void test69(void)
{
	/* Test sljit_set_current_flags. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_sw buf[8];
	sljit_s32 i;

	if (verbose)
		printf("Run test69\n");

	for (i = 0; i < 8; i++)
		buf[i] = 4;

	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 3, 1, 0, 0, 0);
	/* buf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)1 << ((sizeof (sljit_sw) * 8) - 2));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_SET_OVERFLOW, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_label(compiler);
	sljit_set_current_flags(compiler, SLJIT_SET_OVERFLOW | SLJIT_CURRENT_FLAGS_ADD);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_OVERFLOW);
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 5);
	sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_SET_OVERFLOW, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_label(compiler);
	sljit_set_current_flags(compiler, SLJIT_SET_OVERFLOW | SLJIT_CURRENT_FLAGS_ADD);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_OVERFLOW);
	/* buf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_MUL | SLJIT_SET_OVERFLOW, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_label(compiler);
	sljit_set_current_flags(compiler, SLJIT_SET_OVERFLOW);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_OVERFLOW);
	/* buf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 5);
	sljit_emit_op2(compiler, SLJIT_MUL | SLJIT_SET_OVERFLOW, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_R1, 0);
	sljit_emit_label(compiler);
	sljit_set_current_flags(compiler, SLJIT_SET_OVERFLOW);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_OVERFLOW);
	/* buf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 6);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 5);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_GREATER, SLJIT_R1, 0, SLJIT_R2, 0);
	sljit_emit_label(compiler);
	sljit_set_current_flags(compiler, SLJIT_SET_GREATER | SLJIT_CURRENT_FLAGS_SUB | SLJIT_CURRENT_FLAGS_COMPARE);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_GREATER);
	/* buf[5] */
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_Z, SLJIT_R1, 0, SLJIT_R2, 0);
	sljit_emit_label(compiler);
	sljit_set_current_flags(compiler, SLJIT_SET_Z | SLJIT_CURRENT_FLAGS_SUB | SLJIT_CURRENT_FLAGS_COMPARE);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_ZERO);
	/* buf[6] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)(1u << 31));
	sljit_emit_op2u(compiler, SLJIT_ADD32 | SLJIT_SET_Z, SLJIT_R1, 0, SLJIT_R1, 0);
	sljit_emit_label(compiler);
	sljit_set_current_flags(compiler, SLJIT_SET_Z | SLJIT_CURRENT_FLAGS_32 | SLJIT_CURRENT_FLAGS_ADD);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_ZERO);
	/* buf[7] */
	sljit_emit_op2u(compiler, SLJIT_SHL32 | SLJIT_SET_Z, SLJIT_R1, 0, SLJIT_IMM, 1);
	sljit_emit_label(compiler);
	sljit_set_current_flags(compiler, SLJIT_SET_Z | SLJIT_CURRENT_FLAGS_32);
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_NOT_ZERO);

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);

	FAILED(buf[0] != 1, "test69 case 1 failed\n");
	FAILED(buf[1] != 2, "test69 case 2 failed\n");
	FAILED(buf[2] != 1, "test69 case 3 failed\n");
	FAILED(buf[3] != 2, "test69 case 4 failed\n");
	FAILED(buf[4] != 1, "test69 case 5 failed\n");
	FAILED(buf[5] != 2, "test69 case 6 failed\n");
	FAILED(buf[6] != 1, "test69 case 7 failed\n");
	FAILED(buf[7] != 2, "test69 case 8 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test70(void)
{
	/* Test argument passing to sljit_emit_enter. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_sw wbuf[2];
	sljit_s32 ibuf[2];
	sljit_f64 dbuf[3];
	sljit_f32 fbuf[2];

	if (verbose)
		printf("Run test70\n");

	wbuf[0] = 0;
	wbuf[1] = 0;
	ibuf[0] = 0;
	ibuf[1] = 0;

	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS4(VOID, 32, W, 32, W), 1, 4, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&wbuf);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_S1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_sw), SLJIT_S3, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&ibuf);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_S0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_s32), SLJIT_S2, 0);
	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.test70_f1(-1478, 9476, 4928, -6832);

	FAILED(wbuf[0] != 9476, "test70 case 1 failed\n");
	FAILED(wbuf[1] != -6832, "test70 case 2 failed\n");
	FAILED(ibuf[0] != -1478, "test70 case 3 failed\n");
	FAILED(ibuf[1] != 4928, "test70 case 4 failed\n");

	sljit_free_code(code.code, NULL);

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS4(VOID, 32, 32, W, W), 1, 4, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&wbuf);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_S0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_sw), SLJIT_S1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&ibuf);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_S2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_s32), SLJIT_S3, 0);
	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.test70_f2(4721, 7892, -3579, -4830);

	FAILED(wbuf[0] != 4721, "test70 case 5 failed\n");
	FAILED(wbuf[1] != 7892, "test70 case 6 failed\n");
	FAILED(ibuf[0] != -3579, "test70 case 7 failed\n");
	FAILED(ibuf[1] != -4830, "test70 case 8 failed\n");

	sljit_free_code(code.code, NULL);

	if (sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		wbuf[0] = 0;
		ibuf[0] = 0;
		dbuf[0] = 0;
		fbuf[0] = 0;

		compiler = sljit_create_compiler(NULL, NULL);
		FAILED(!compiler, "cannot create compiler\n");

		sljit_emit_enter(compiler, 0, SLJIT_ARGS4(VOID, 32, F32, W, F64), 2, 2, 2, 0, 0);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM0(), (sljit_sw)&wbuf, SLJIT_S1, 0);
		sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM0(), (sljit_sw)&ibuf, SLJIT_S0, 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM0(), (sljit_sw)&dbuf, SLJIT_FR1, 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM0(), (sljit_sw)&fbuf, SLJIT_FR0, 0);
		sljit_emit_return_void(compiler);

		code.code = sljit_generate_code(compiler);
		CHECK(compiler);
		sljit_free_compiler(compiler);

		code.test70_f3(-6834, 674.5, 2789, -895.25);

		FAILED(wbuf[0] != 2789, "test70 case 9 failed\n");
		FAILED(ibuf[0] != -6834, "test70 case 10 failed\n");
		FAILED(dbuf[0] != -895.25, "test70 case 11 failed\n");
		FAILED(fbuf[0] != 674.5, "test70 case 12 failed\n");

		ibuf[0] = 0;
		dbuf[0] = 0;
		fbuf[0] = 0;
		fbuf[1] = 0;

		compiler = sljit_create_compiler(NULL, NULL);
		FAILED(!compiler, "cannot create compiler\n");

		sljit_emit_enter(compiler, 0, SLJIT_ARGS4(VOID, F32, F64, F32, 32), 1, 1, 3, 0, 0);
		sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM0(), (sljit_sw)&ibuf, SLJIT_S0, 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM0(), (sljit_sw)&dbuf, SLJIT_FR1, 0);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&fbuf);
		sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_FR0, 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_f32), SLJIT_FR2, 0);
		sljit_emit_return_void(compiler);

		code.code = sljit_generate_code(compiler);
		CHECK(compiler);
		sljit_free_compiler(compiler);

		code.test70_f4(-4712.5, 5342.25, 2904.25, -4607);

		FAILED(ibuf[0] != -4607, "test70 case 13 failed\n");
		FAILED(dbuf[0] != 5342.25, "test70 case 14 failed\n");
		FAILED(fbuf[0] != -4712.5, "test70 case 15 failed\n");
		FAILED(fbuf[1] != 2904.25, "test70 case 16 failed\n");

		ibuf[0] = 0;
		dbuf[0] = 0;
		fbuf[0] = 0;
		fbuf[1] = 0;

		compiler = sljit_create_compiler(NULL, NULL);
		FAILED(!compiler, "cannot create compiler\n");

		sljit_emit_enter(compiler, 0, SLJIT_ARGS4(VOID, F64, F32, 32, F32), 1, 1, 3, 0, 0);
		sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM0(), (sljit_sw)&ibuf, SLJIT_S0, 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM0(), (sljit_sw)&dbuf, SLJIT_FR0, 0);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&fbuf);
		sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_FR1, 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_f32), SLJIT_FR2, 0);

		sljit_emit_return_void(compiler);

		code.code = sljit_generate_code(compiler);
		CHECK(compiler);
		sljit_free_compiler(compiler);

		code.test70_f5(3578.5, 4619.25, 6859, -1807.75);

		FAILED(ibuf[0] != 6859, "test70 case 17 failed\n");
		FAILED(dbuf[0] != 3578.5, "test70 case 18 failed\n");
		FAILED(fbuf[0] != 4619.25, "test70 case 19 failed\n");
		FAILED(fbuf[1] != -1807.75, "test70 case 20 failed\n");

		ibuf[0] = 0;
		dbuf[0] = 0;
		dbuf[1] = 0;
		fbuf[0] = 0;

		compiler = sljit_create_compiler(NULL, NULL);
		FAILED(!compiler, "cannot create compiler\n");

		sljit_emit_enter(compiler, 0, SLJIT_ARGS4(VOID, F64, 32, F32, F64), SLJIT_NUMBER_OF_SCRATCH_REGISTERS + 2, 1, 3, 0, 33);
		sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM0(), (sljit_sw)&ibuf, SLJIT_S0, 0);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&dbuf);
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_FR0, 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_f64), SLJIT_FR2, 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM0(), (sljit_sw)&fbuf, SLJIT_FR1, 0);
		sljit_emit_return_void(compiler);

		code.code = sljit_generate_code(compiler);
		CHECK(compiler);
		sljit_free_compiler(compiler);

		code.test70_f6(2740.75, -2651, -7909.25, 3671.5);

		FAILED(ibuf[0] != -2651, "test70 case 21 failed\n");
		FAILED(dbuf[0] != 2740.75, "test70 case 22 failed\n");
		FAILED(dbuf[1] != 3671.5, "test70 case 23 failed\n");
		FAILED(fbuf[0] != -7909.25, "test70 case 24 failed\n");

		wbuf[0] = 0;
		ibuf[0] = 0;
		ibuf[1] = 0;
		fbuf[0] = 0;

		compiler = sljit_create_compiler(NULL, NULL);
		FAILED(!compiler, "cannot create compiler\n");

		sljit_emit_enter(compiler, 0, SLJIT_ARGS4(VOID, F32, 32, W, 32), 1, 3, 1, 0, 1);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM0(), (sljit_sw)&wbuf, SLJIT_S1, 0);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&ibuf);
		sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_S0, 0);
		sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_s32), SLJIT_S2, 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM0(), (sljit_sw)&fbuf, SLJIT_FR0, 0);
		sljit_emit_return_void(compiler);

		code.code = sljit_generate_code(compiler);
		CHECK(compiler);
		sljit_free_compiler(compiler);

		code.test70_f7(-5219.25, -4530, 7214, 6741);

		FAILED(wbuf[0] != 7214, "test70 case 25 failed\n");
		FAILED(ibuf[0] != -4530, "test70 case 26 failed\n");
		FAILED(ibuf[1] != 6741, "test70 case 27 failed\n");
		FAILED(fbuf[0] != -5219.25, "test70 case 28 failed\n");

		wbuf[0] = 0;
		wbuf[1] = 0;
		dbuf[0] = 0;
		dbuf[1] = 0;

		compiler = sljit_create_compiler(NULL, NULL);
		FAILED(!compiler, "cannot create compiler\n");

		sljit_emit_enter(compiler, 0, SLJIT_ARGS4(VOID, F64, F64, W, W), 1, 5, 2, 0, SLJIT_MAX_LOCAL_SIZE - 1);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_S0, 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_SP), SLJIT_MAX_LOCAL_SIZE - 2 * sizeof(sljit_f64), SLJIT_FR0, 0);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&wbuf);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_S0, 0);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_sw), SLJIT_S1, 0);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&dbuf);
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_FR0, 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_f64), SLJIT_FR1, 0);
		sljit_emit_return_void(compiler);

		code.code = sljit_generate_code(compiler);
		CHECK(compiler);
		sljit_free_compiler(compiler);

		code.test70_f8(-3749.75, 5280.5, 9134, -6506);

		FAILED(wbuf[0] != 9134, "test70 case 29 failed\n");
		FAILED(wbuf[1] != -6506, "test70 case 30 failed\n");
		FAILED(dbuf[0] != -3749.75, "test70 case 31 failed\n");
		FAILED(dbuf[1] != 5280.5, "test70 case 32 failed\n");

		wbuf[0] = 0;
		dbuf[0] = 0;
		dbuf[1] = 0;
		dbuf[2] = 0;

		compiler = sljit_create_compiler(NULL, NULL);
		FAILED(!compiler, "cannot create compiler\n");

		sljit_emit_enter(compiler, 0, SLJIT_ARGS4(VOID, F64, F64, W, F64), 1, 1, 3, 0, SLJIT_MAX_LOCAL_SIZE);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM0(), (sljit_sw)&wbuf, SLJIT_S0, 0);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&dbuf);
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_FR0, 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_f64), SLJIT_FR1, 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), 2 * sizeof(sljit_f64), SLJIT_FR2, 0);

		sljit_emit_return_void(compiler);

		code.code = sljit_generate_code(compiler);
		CHECK(compiler);
		sljit_free_compiler(compiler);

		code.test70_f9(-6049.25, 7301.5, 4610, -4312.75);

		FAILED(wbuf[0] != 4610, "test70 case 33 failed\n");
		FAILED(dbuf[0] != -6049.25, "test70 case 34 failed\n");
		FAILED(dbuf[1] != 7301.5, "test70 case 35 failed\n");
		FAILED(dbuf[2] != -4312.75, "test70 case 36 failed\n");

		ibuf[0] = 0;
		dbuf[0] = 0;
		dbuf[1] = 0;
		dbuf[2] = 0;

		compiler = sljit_create_compiler(NULL, NULL);
		FAILED(!compiler, "cannot create compiler\n");

		sljit_emit_enter(compiler, 0, SLJIT_ARGS4(VOID, F64, F64, F64, 32), 1, 1, 3, 0, 0);
		sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM0(), (sljit_sw)&ibuf, SLJIT_S0, 0);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&dbuf);
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_FR0, 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_f64), SLJIT_FR1, 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), 2 * sizeof(sljit_f64), SLJIT_FR2, 0);

		sljit_emit_return_void(compiler);

		code.code = sljit_generate_code(compiler);
		CHECK(compiler);
		sljit_free_compiler(compiler);

		code.test70_f10(4810.5, -9148.75, 8601.25, 6703);

		FAILED(ibuf[0] != 6703, "test70 case 37 failed\n");
		FAILED(dbuf[0] != 4810.5, "test70 case 38 failed\n");
		FAILED(dbuf[1] != -9148.75, "test70 case 39 failed\n");
		FAILED(dbuf[2] != 8601.25, "test70 case 40 failed\n");
	}

	successful_tests++;
}

static sljit_sw test71_f1(sljit_sw a)
{
	return a + 10000;
}

static sljit_sw test71_f2(sljit_sw a, sljit_s32 b, sljit_s32 c, sljit_sw d)
{
	return a | b | c | d;
}

static sljit_sw test71_f3(sljit_sw a, sljit_s32 b, sljit_s32 c, sljit_sw d)
{
	SLJIT_UNUSED_ARG(a);
	return b | c | d;
}

static sljit_sw test71_f4(void)
{
	return 7461932;
}

static sljit_sw test71_f5(sljit_f64 a, sljit_f64 b, sljit_f64 c, sljit_f64 d)
{
	if (a == 1345.5 && b == -8724.25 && c == 9034.75 && d == 6307.5)
		return 8920567;
	return 0;
}

static sljit_sw test71_f6(sljit_f64 a, sljit_f64 b, sljit_f64 c, sljit_sw d)
{
	if (a == 4061.25 && b == -3291.75 && c == 8703.5 && d == 1706)
		return 5074526;
	return 0;
}

static void test71(void)
{
	/* Test tail calls. */
	executable_code code;
	struct sljit_compiler* compiler;
	struct sljit_jump *jump;
	sljit_uw jump_addr;
	sljit_sw executable_offset;
	sljit_sw wbuf[1];
	sljit_f64 dbuf[4];

	if (verbose)
		printf("Run test71\n");

	/* Next test. */

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(W, W), 4, 4, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S3, 0, SLJIT_IMM, -1);
	sljit_emit_icall(compiler, SLJIT_CALL | SLJIT_CALL_RETURN, SLJIT_ARGS1(W, W), SLJIT_IMM, SLJIT_FUNC_ADDR(test71_f1));
	/* Should crash. */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM0(), 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func1(7987) != 17987, "test71 case 1 failed\n");

	/* Next test. */

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(W, W), 1, 4, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S3, 0, SLJIT_IMM, -1);
	jump = sljit_emit_call(compiler, SLJIT_CALL | SLJIT_REWRITABLE_JUMP | SLJIT_CALL_RETURN, SLJIT_ARGS1(W, W));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM0(), 0);

	sljit_set_target(jump, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);

	executable_offset = sljit_get_executable_offset(compiler);
	jump_addr = sljit_get_jump_addr(jump);
	sljit_free_compiler(compiler);

	sljit_set_jump_addr(jump_addr, SLJIT_FUNC_UADDR(test71_f1), executable_offset);

	FAILED(code.func1(3903) != 13903, "test71 case 2 failed\n");

	/* Next test. */

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(W), 4, 2, 0, 0, SLJIT_MAX_LOCAL_SIZE);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_IMM, SLJIT_FUNC_ADDR(test71_f2));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x28000000);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, 0x00140000);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R2, 0, SLJIT_IMM, 0x00002800);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, 0x00000041);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, -1);
	sljit_emit_icall(compiler, SLJIT_CALL | SLJIT_CALL_RETURN, SLJIT_ARGS4(W, W, 32, 32, W), SLJIT_MEM1(SLJIT_SP), 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM0(), 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func0() != 0x28142841, "test71 case 3 failed\n");

	/* Next test. */

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(W), 4, 4, 0, 0, SLJIT_MAX_LOCAL_SIZE);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S3, 0, SLJIT_IMM, SLJIT_FUNC_ADDR(test71_f2));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)0x81000000);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, 0x00480000);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R2, 0, SLJIT_IMM, 0x00002100);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, 0x00000014);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, -1);
	sljit_emit_icall(compiler, SLJIT_CALL | SLJIT_CALL_RETURN, SLJIT_ARGS4(W, W, 32, 32, W), SLJIT_S3, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM0(), 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func0() != (sljit_sw)0x81482114, "test71 case 4 failed\n");

	sljit_free_code(code.code, NULL);

	/* Next test. */

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(W), 4, 0, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, SLJIT_FUNC_ADDR(test71_f3));
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, 0x342);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R2, 0, SLJIT_IMM, 0x451000);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, 0x21000000);
	sljit_emit_icall(compiler, SLJIT_CALL | SLJIT_CALL_RETURN, SLJIT_ARGS4(W, W, 32, 32, W), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM0(), 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func0() != 0x21451342, "test71 case 5 failed\n");

	sljit_free_code(code.code, NULL);

	/* Next test. */

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(W), 1, 0, 0, 0, 9);
	sljit_emit_icall(compiler, SLJIT_CALL | SLJIT_CALL_RETURN, SLJIT_ARGS0(W), SLJIT_IMM, SLJIT_FUNC_ADDR(test71_f4));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM0(), 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	FAILED(code.func0() != 7461932, "test71 case 6 failed\n");

	sljit_free_code(code.code, NULL);

	if (sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		/* Next test. */

		dbuf[0] = 9034.75;
		dbuf[1] = 6307.5;

		compiler = sljit_create_compiler(NULL, NULL);
		FAILED(!compiler, "cannot create compiler\n");

		sljit_emit_enter(compiler, 0, SLJIT_ARGS2(W, F32, F64), 1, 1, 4, 0, 0);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&dbuf);
		sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_F32, SLJIT_FR0, 0, SLJIT_FR0, 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_R0), 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR3, 0, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_f64));
		sljit_emit_icall(compiler, SLJIT_CALL | SLJIT_CALL_RETURN, SLJIT_ARGS4(W, F64, F64, F64, F64), SLJIT_IMM, SLJIT_FUNC_ADDR(test71_f5));
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM0(), 0);

		code.code = sljit_generate_code(compiler);
		CHECK(compiler);
		sljit_free_compiler(compiler);

		FAILED(code.test71_f1(1345.5, -8724.25) != 8920567, "test71 case 7 failed\n");

		sljit_free_code(code.code, NULL);

		/* Next test. */

		wbuf[0] = SLJIT_FUNC_ADDR(test71_f5);

		compiler = sljit_create_compiler(NULL, NULL);
		FAILED(!compiler, "cannot create compiler\n");

		sljit_emit_enter(compiler, 0, SLJIT_ARGS4(W, F64, F64, F64, F64), 1, 0, 4, 0, 0);
		sljit_emit_icall(compiler, SLJIT_CALL | SLJIT_CALL_RETURN, SLJIT_ARGS4(W, F64, F64, F64, F64), SLJIT_MEM0(), (sljit_sw)wbuf);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM0(), 0);

		code.code = sljit_generate_code(compiler);
		CHECK(compiler);
		sljit_free_compiler(compiler);

		FAILED(code.test71_f2(1345.5, -8724.25, 9034.75, 6307.5) != 8920567, "test71 case 8 failed\n");

		sljit_free_code(code.code, NULL);

		/* Next test. */

		compiler = sljit_create_compiler(NULL, NULL);
		FAILED(!compiler, "cannot create compiler\n");

		sljit_emit_enter(compiler, 0, SLJIT_ARGS3(W, F64, F64, F64), 1, 0, 4, 0, 0);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1706);
		jump = sljit_emit_call(compiler, SLJIT_CALL | SLJIT_CALL_RETURN, SLJIT_ARGS4(W, F64, F64, F64, W));
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM0(), 0);

		sljit_set_target(jump, SLJIT_FUNC_UADDR(test71_f6));

		code.code = sljit_generate_code(compiler);
		CHECK(compiler);
		sljit_free_compiler(compiler);

		FAILED(code.test71_f3(4061.25, -3291.75, 8703.5) != 5074526, "test71 case 9 failed\n");

		sljit_free_code(code.code, NULL);

		/* Next test. */

		compiler = sljit_create_compiler(NULL, NULL);
		FAILED(!compiler, "cannot create compiler\n");

		sljit_emit_enter(compiler, 0, SLJIT_ARGS3(W, F64, F64, F64), SLJIT_NUMBER_OF_SCRATCH_REGISTERS + 1, 0, 4, 0, 0);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1706);
		jump = sljit_emit_call(compiler, SLJIT_CALL | SLJIT_CALL_RETURN, SLJIT_ARGS4(W, F64, F64, F64, W));
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM0(), 0);

		sljit_set_target(jump, SLJIT_FUNC_UADDR(test71_f6));

		code.code = sljit_generate_code(compiler);
		CHECK(compiler);
		sljit_free_compiler(compiler);

		FAILED(code.test71_f3(4061.25, -3291.75, 8703.5) != 5074526, "test71 case 10 failed\n");

		sljit_free_code(code.code, NULL);

		/* Next test. */

		compiler = sljit_create_compiler(NULL, NULL);
		FAILED(!compiler, "cannot create compiler\n");

		sljit_emit_enter(compiler, 0, SLJIT_ARGS3(W, F64, F64, F64), SLJIT_NUMBER_OF_SCRATCH_REGISTERS + 1, 1, 3, 0, SLJIT_MAX_LOCAL_SIZE);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1706);
		jump = sljit_emit_call(compiler, SLJIT_CALL | SLJIT_CALL_RETURN, SLJIT_ARGS4(W, F64, F64, F64, W));
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM0(), 0);

		sljit_set_target(jump, SLJIT_FUNC_UADDR(test71_f6));

		code.code = sljit_generate_code(compiler);
		CHECK(compiler);
		sljit_free_compiler(compiler);

		FAILED(code.test71_f3(4061.25, -3291.75, 8703.5) != 5074526, "test71 case 11 failed\n");

		sljit_free_code(code.code, NULL);
	}

	successful_tests++;
}

static void test72(void)
{
	/* Test using all fpu registers. */
	executable_code code;
	struct sljit_compiler* compiler;
	sljit_f64 buf[SLJIT_NUMBER_OF_FLOAT_REGISTERS];
	sljit_f64 buf2[2];
	struct sljit_jump *jump;
	sljit_s32 i;

	if (verbose)
		printf("Run test72\n");

	if (!sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		if (verbose)
			printf("no fpu available, test72 skipped\n");
		successful_tests++;
		return;
	}

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	buf2[0] = 7.75;
	buf2[1] = -8.25;

	for (i = 0; i < SLJIT_NUMBER_OF_FLOAT_REGISTERS; i++)
		buf[i] = 0.0;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(VOID, P, P), 1, 2, SLJIT_NUMBER_OF_FLOAT_REGISTERS, 0, 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S1), 0);
	for (i = 1; i < SLJIT_NUMBER_OF_FLOAT_REGISTERS; i++)
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR(i), 0, SLJIT_FR0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S1, 0);
	jump = sljit_emit_call(compiler, SLJIT_CALL, SLJIT_ARGS1(VOID, W));

	for (i = 0; i < SLJIT_NUMBER_OF_FLOAT_REGISTERS; i++)
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), i * (sljit_sw)sizeof(sljit_f64), SLJIT_FR(i), 0);
	sljit_emit_return_void(compiler);

	/* Called function. */
	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 0, 1, SLJIT_NUMBER_OF_FLOAT_REGISTERS, 0, 0);

	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64));
	for (i = 1; i < SLJIT_NUMBER_OF_FLOAT_REGISTERS; i++)
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR(i), 0, SLJIT_FR0, 0);

	sljit_set_context(compiler, 0, SLJIT_ARGS1(VOID, P), 0, 1, SLJIT_NUMBER_OF_FLOAT_REGISTERS, 0, 0);
	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func2((sljit_sw)buf, (sljit_sw)buf2);

	for (i = 0; i < SLJIT_NUMBER_OF_SCRATCH_FLOAT_REGISTERS; i++) {
		FAILED(buf[i] != -8.25, "test72 case 1 failed\n");
	}

	for (i = SLJIT_NUMBER_OF_SCRATCH_FLOAT_REGISTERS; i < SLJIT_NUMBER_OF_FLOAT_REGISTERS; i++) {
		FAILED(buf[i] != 7.75, "test72 case 2 failed\n");
	}

	sljit_free_code(code.code, NULL);

	/* Next test. */
	if (SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS >= 3) {
		compiler = sljit_create_compiler(NULL, NULL);
		FAILED(!compiler, "cannot create compiler\n");

		buf2[0] = -6.25;
		buf2[1] = 3.75;

		for (i = 0; i < SLJIT_NUMBER_OF_FLOAT_REGISTERS; i++)
			buf[i] = 0.0;

		sljit_emit_enter(compiler, 0, SLJIT_ARGS2(VOID, P, P), 1, 2, SLJIT_NUMBER_OF_FLOAT_REGISTERS - 2, 1, SLJIT_MAX_LOCAL_SIZE);
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FS0, 0, SLJIT_MEM1(SLJIT_S1), 0);
		for (i = 0; i < SLJIT_NUMBER_OF_FLOAT_REGISTERS - 2; i++)
			sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR(i), 0, SLJIT_FS0, 0);

		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S1, 0);
		jump = sljit_emit_call(compiler, SLJIT_CALL, SLJIT_ARGS1(VOID, W));

		for (i = 0; i < SLJIT_NUMBER_OF_FLOAT_REGISTERS - 2; i++)
			sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), i * (sljit_sw)sizeof(sljit_f64), SLJIT_FR(i), 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), (SLJIT_NUMBER_OF_FLOAT_REGISTERS - 1) * (sljit_sw)sizeof(sljit_f64), SLJIT_FS0, 0);
		sljit_emit_return_void(compiler);

		/* Called function. */
		sljit_set_label(jump, sljit_emit_label(compiler));
		sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 0, 1, SLJIT_NUMBER_OF_FLOAT_REGISTERS, 0, SLJIT_MAX_LOCAL_SIZE);

		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64));
		for (i = 1; i < SLJIT_NUMBER_OF_FLOAT_REGISTERS; i++)
			sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR(i), 0, SLJIT_FR0, 0);

		sljit_set_context(compiler, 0, SLJIT_ARGS1(VOID, P), 0, 1, SLJIT_NUMBER_OF_FLOAT_REGISTERS, 0, SLJIT_MAX_LOCAL_SIZE);
		sljit_emit_return_void(compiler);

		code.code = sljit_generate_code(compiler);
		CHECK(compiler);
		sljit_free_compiler(compiler);

		code.func2((sljit_sw)buf, (sljit_sw)buf2);

		for (i = 0; i < SLJIT_NUMBER_OF_SCRATCH_FLOAT_REGISTERS; i++) {
			FAILED(buf[i] != 3.75, "test72 case 3 failed\n");
		}

		for (i = SLJIT_NUMBER_OF_SCRATCH_FLOAT_REGISTERS; i < SLJIT_NUMBER_OF_FLOAT_REGISTERS - 2; i++) {
			FAILED(buf[i] != -6.25, "test72 case 4 failed\n");
		}

		FAILED(buf[SLJIT_NUMBER_OF_FLOAT_REGISTERS - 2] != 0, "test72 case 5 failed\n");
		FAILED(buf[SLJIT_NUMBER_OF_FLOAT_REGISTERS - 1] != -6.25, "test72 case 6 failed\n");

		sljit_free_code(code.code, NULL);
	}
	successful_tests++;
}

static void test73(void)
{
	/* Test pasing arguments in registers. */
	executable_code code;
	struct sljit_compiler* compiler;
	sljit_sw wbuf[2];
	sljit_s32 ibuf[2];
	sljit_f64 dbuf[3];

	if (verbose)
		printf("Run test73\n");

	/* Next test. */

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS4(VOID, 32_R, W, W_R, 32), 3, 2, 0, 0, 0);
	/* wbuf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)&wbuf);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), 0, SLJIT_S0, 0);
	/* wbuf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), sizeof(sljit_sw), SLJIT_R2, 0);
	/* ibuf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)&ibuf);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_R1), 0, SLJIT_R0, 0);
	/* ibuf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_R1), sizeof(sljit_s32), SLJIT_S1, 0);
	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.test73_f1(3467, -6781, 5038, 6310);

	FAILED(wbuf[0] != -6781, "test73 case 1 failed\n");
	FAILED(wbuf[1] != 5038, "test73 case 2 failed\n");
	FAILED(ibuf[0] != 3467, "test73 case 3 failed\n");
	FAILED(ibuf[1] != 6310, "test73 case 4 failed\n");
	sljit_free_code(code.code, NULL);

	/* Next test. */

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS4(VOID, 32, W_R, W, 32_R), 4, 2, 0, 0, 8192);
	/* wbuf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&wbuf);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_R1, 0);
	/* wbuf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_sw), SLJIT_S1, 0);
	/* ibuf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&ibuf);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_S0, 0);
	/* ibuf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_s32), SLJIT_R3, 0);
	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.test73_f1(-9723, 5208, 4761, 5084);

	FAILED(wbuf[0] != 5208, "test73 case 5 failed\n");
	FAILED(wbuf[1] != 4761, "test73 case 6 failed\n");
	FAILED(ibuf[0] != -9723, "test73 case 7 failed\n");
	FAILED(ibuf[1] != 5084, "test73 case 8 failed\n");
	sljit_free_code(code.code, NULL);

	/* Next test. */

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS4(VOID, 32_R, W_R, W_R, 32_R), 4, 1, 0, 0, SLJIT_MAX_LOCAL_SIZE);
	/* wbuf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, (sljit_sw)&wbuf);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R1, 0);
	/* wbuf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R2, 0);
	/* ibuf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, (sljit_sw)&ibuf);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);
	/* ibuf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_s32), SLJIT_R3, 0);
	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.test73_f1(5934, 6043, -8572, -3861);

	FAILED(wbuf[0] != 6043, "test73 case 9 failed\n");
	FAILED(wbuf[1] != -8572, "test73 case 10 failed\n");
	FAILED(ibuf[0] != 5934, "test73 case 11 failed\n");
	FAILED(ibuf[1] != -3861, "test73 case 12 failed\n");
	sljit_free_code(code.code, NULL);

	/* Next test. */

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS4(VOID, W_R, W_R, 32_R, 32_R), 4, 1, 0, 0, SLJIT_MAX_LOCAL_SIZE);
	sljit_set_context(compiler, 0, SLJIT_ARGS4(VOID, W_R, W_R, 32_R, 32_R), 4, 1, 0, 0, SLJIT_MAX_LOCAL_SIZE);
	/* wbuf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, (sljit_sw)&wbuf);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);
	/* wbuf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R1, 0);
	/* ibuf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, (sljit_sw)&ibuf);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R2, 0);
	/* ibuf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_s32), SLJIT_R3, 0);
	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.test73_f2(6732, -5916, 2740, -3621);

	FAILED(wbuf[0] != 6732, "test73 case 13 failed\n");
	FAILED(wbuf[1] != -5916, "test73 case 14 failed\n");
	FAILED(ibuf[0] != 2740, "test73 case 15 failed\n");
	FAILED(ibuf[1] != -3621, "test73 case 16 failed\n");
	sljit_free_code(code.code, NULL);

	if (sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		/* Next test. */

		compiler = sljit_create_compiler(NULL, NULL);
		FAILED(!compiler, "cannot create compiler\n");

		sljit_emit_enter(compiler, 0, SLJIT_ARGS4(VOID, F64, F64, F64, W_R), 1, 0, 3, 0, SLJIT_MAX_LOCAL_SIZE);
		/* wbuf[0] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM0(), (sljit_sw)&wbuf, SLJIT_R0, 0);
		/* dbuf[0] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&dbuf);
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_FR0, 0);
		/* dbuf[1] */
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_f64), SLJIT_FR1, 0);
		/* dbuf[2] */
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), 2 * sizeof(sljit_f64), SLJIT_FR2, 0);

		sljit_emit_return_void(compiler);

		code.code = sljit_generate_code(compiler);
		CHECK(compiler);
		sljit_free_compiler(compiler);

		code.test73_f3(7390.25, -8045.5, 1390.75, 8201);

		FAILED(wbuf[0] != 8201, "test73 case 17 failed\n");
		FAILED(dbuf[0] != 7390.25, "test73 case 18 failed\n");
		FAILED(dbuf[1] != -8045.5, "test73 case 19 failed\n");
		FAILED(dbuf[2] != 1390.75, "test73 case 20 failed\n");

		/* Next test. */

		compiler = sljit_create_compiler(NULL, NULL);
		FAILED(!compiler, "cannot create compiler\n");

		sljit_emit_enter(compiler, 0, SLJIT_ARGS4(VOID, F64, F64, W, W_R), 2, 1, 2, 0, SLJIT_MAX_LOCAL_SIZE);
		/* wbuf[0] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&wbuf);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_S0, 0);
		/* wbuf[1] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_sw), SLJIT_R1, 0);
		/* dbuf[0] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&dbuf);
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_FR0, 0);
		/* dbuf[1] */
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_f64), SLJIT_FR1, 0);

		sljit_emit_return_void(compiler);

		code.code = sljit_generate_code(compiler);
		CHECK(compiler);
		sljit_free_compiler(compiler);

		code.test73_f4(4892.75, -3702.5, 4731, 8530);

		FAILED(wbuf[0] != 4731, "test73 case 21 failed\n");
		FAILED(wbuf[1] != 8530, "test73 case 22 failed\n");
		FAILED(dbuf[0] != 4892.75, "test73 case 23 failed\n");
		FAILED(dbuf[1] != -3702.5, "test73 case 24 failed\n");
	}

	successful_tests++;
}

static void test74(void)
{
	/* Test carry flag. */
	executable_code code;
	struct sljit_compiler* compiler;
	sljit_sw wbuf[15];
	sljit_s32 i;

	if (verbose)
		printf("Run test74\n");

	for (i = 0; i < 15; i++)
		wbuf[i] = -1;

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, W), 3, 2, 0, 0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -1);
	sljit_emit_op2u(compiler, SLJIT_ADD | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_CARRY);
	/* wbuf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R1, 0);
	cond_set(compiler, SLJIT_R1, 0, SLJIT_CARRY);
	/* wbuf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 2);
	sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, -1);
	sljit_set_current_flags(compiler, SLJIT_SET_CARRY | SLJIT_CURRENT_FLAGS_ADD);
	/* wbuf[2] */
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_NOT_CARRY);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op2(compiler, SLJIT_SHL32, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 31);
	sljit_emit_op2u(compiler, SLJIT_ADD32 | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_R0, 0);
	cond_set(compiler, SLJIT_R1, 0, SLJIT_CARRY);
	/* wbuf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 5);
	sljit_emit_op2(compiler, SLJIT_ADD32 | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 5);
	cond_set(compiler, SLJIT_R1, 0, SLJIT_CARRY);
	/* wbuf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_IMM, 1);
	cond_set(compiler, SLJIT_R1, 0, SLJIT_CARRY);
	/* wbuf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_IMM, 1);
	cond_set(compiler, SLJIT_R1, 0, SLJIT_NOT_CARRY);
	/* wbuf[6] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op2u(compiler, SLJIT_SUB32 | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_IMM, 1);
	/* wbuf[7] */
	cond_set(compiler, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_CARRY);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op2u(compiler, SLJIT_SUB32 | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_set_current_flags(compiler, SLJIT_SET_CARRY | SLJIT_CURRENT_FLAGS_32 | SLJIT_CURRENT_FLAGS_SUB);
	cond_set(compiler, SLJIT_R1, 0, SLJIT_NOT_CARRY);
	/* wbuf[8] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op2(compiler, SLJIT_SHL32, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 31);
	sljit_emit_op2(compiler, SLJIT_SUB32, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op2(compiler, SLJIT_SUB32 | SLJIT_SET_CARRY, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_R0, 0);
	cond_set(compiler, SLJIT_R1, 0, SLJIT_NOT_CARRY);
	/* wbuf[9] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -1);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, -2);
	cond_set(compiler, SLJIT_R1, 0, SLJIT_NOT_CARRY);
	/* wbuf[10] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 10 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op2(compiler, SLJIT_ADDC | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R0, 0);
	cond_set(compiler, SLJIT_R1, 0, SLJIT_CARRY);
	/* wbuf[11] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 11 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op2(compiler, SLJIT_SHL32, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 31);
	sljit_emit_op2(compiler, SLJIT_ADD32 | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op2(compiler, SLJIT_ADDC32 | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R0, 0);
	cond_set(compiler, SLJIT_R1, 0, SLJIT_CARRY);
	/* wbuf[12] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 12 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op2(compiler, SLJIT_SUBC | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
	cond_set(compiler, SLJIT_R1, 0, SLJIT_CARRY);
	/* wbuf[13] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 13 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op2(compiler, SLJIT_SUB32 | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op2(compiler, SLJIT_SUBC32 | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
	cond_set(compiler, SLJIT_R1, 0, SLJIT_NOT_CARRY);
	/* wbuf[14] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 14 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&wbuf);

	FAILED(wbuf[0] != 1, "test74 case 1 failed\n");
	FAILED(wbuf[1] != 1, "test74 case 2 failed\n");
	FAILED(wbuf[2] != 2, "test74 case 3 failed\n");
	FAILED(wbuf[3] != 1, "test74 case 4 failed\n");
	FAILED(wbuf[4] != 2, "test74 case 5 failed\n");
	FAILED(wbuf[5] != 1, "test74 case 6 failed\n");
	FAILED(wbuf[6] != 1, "test74 case 7 failed\n");
	FAILED(wbuf[7] != 1, "test74 case 8 failed\n");
	FAILED(wbuf[8] != 2, "test74 case 9 failed\n");
	FAILED(wbuf[9] != 2, "test74 case 10 failed\n");
	FAILED(wbuf[10] != 1, "test74 case 11 failed\n");
	FAILED(wbuf[11] != 2, "test74 case 12 failed\n");
	FAILED(wbuf[12] != 1, "test74 case 13 failed\n");
	FAILED(wbuf[13] != 1, "test74 case 14 failed\n");
	FAILED(wbuf[14] != 1, "test74 case 15 failed\n");

	successful_tests++;
}

static void test75_set(struct sljit_compiler *compiler, sljit_s32 compare, sljit_s32 type, sljit_s32 left_fr, sljit_s32 right_fr)
{
	/* Testing both sljit_emit_op_flags and sljit_emit_jump. */
	struct sljit_jump* jump;

	sljit_emit_fop1(compiler, compare | SLJIT_SET(type & 0xfe), left_fr, 0, right_fr, 0);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_R0, 0, type);
	jump = sljit_emit_jump(compiler, type);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 2);
	sljit_set_label(jump, sljit_emit_label(compiler));

	sljit_emit_op1(compiler, SLJIT_MOV_U8, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_S0, 0, SLJIT_S0, 0, SLJIT_IMM, sizeof(sljit_s8));
}

static void test75(void)
{
	/* Test floating point comparison. */
	executable_code code;
	struct sljit_compiler* compiler;
	sljit_s8 bbuf[96];
	sljit_s32 i;

	union {
		sljit_f64 value;
		struct {
			sljit_s32 value1;
			sljit_s32 value2;
		} u;
	} dbuf[3];

	union {
		sljit_f32 value;
		sljit_s32 value1;
	} sbuf[3];

	if (verbose)
		printf("Run test75\n");

	if (!sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		if (verbose)
			printf("no fpu available, test75 skipped\n");
		successful_tests++;
		return;
	}

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	dbuf[0].u.value1 = 0x7fffffff;
	dbuf[0].u.value2 = 0x7fffffff;
	dbuf[1].value = -13.0;
	dbuf[2].value = 27.0;

	sbuf[0].value1 = 0x7fffffff;
	sbuf[1].value = -13.0;
	sbuf[2].value = 27.0;

	for (i = 0; i < 96; i++)
		bbuf[i] = -3;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS3(VOID, P, P, P), 3, 3, 6, 0, 0);

	i = SLJIT_CMP_F64;
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S1), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S1), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_f64));
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR3, 0, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_f64));
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR4, 0, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_f64));

	while (1) {
		/* bbuf[0] and bbuf[48] */
		test75_set(compiler, i, SLJIT_ORDERED_EQUAL, SLJIT_FR2, SLJIT_FR3);
		/* bbuf[1] and bbuf[49] */
		test75_set(compiler, i, SLJIT_ORDERED_EQUAL, SLJIT_FR2, SLJIT_FR4);
		/* bbuf[2] and bbuf[50] */
		test75_set(compiler, i, SLJIT_ORDERED_EQUAL, SLJIT_FR0, SLJIT_FR1);
		/* bbuf[3] and bbuf[51] */
		test75_set(compiler, i, SLJIT_ORDERED_EQUAL, SLJIT_FR0, SLJIT_FR2);

		/* bbuf[4] and bbuf[52] */
		test75_set(compiler, i, SLJIT_UNORDERED_OR_NOT_EQUAL, SLJIT_FR2, SLJIT_FR3);
		/* bbuf[5] and bbuf[53] */
		test75_set(compiler, i, SLJIT_UNORDERED_OR_NOT_EQUAL, SLJIT_FR2, SLJIT_FR4);
		/* bbuf[6] and bbuf[54] */
		test75_set(compiler, i, SLJIT_UNORDERED_OR_NOT_EQUAL, SLJIT_FR0, SLJIT_FR1);
		/* bbuf[7] and bbuf[55] */
		test75_set(compiler, i, SLJIT_UNORDERED_OR_NOT_EQUAL, SLJIT_FR0, SLJIT_FR2);

		/* bbuf[8] and bbuf[56] */
		test75_set(compiler, i, SLJIT_ORDERED_LESS, SLJIT_FR2, SLJIT_FR3);
		/* bbuf[9] and bbuf[57] */
		test75_set(compiler, i, SLJIT_ORDERED_LESS, SLJIT_FR2, SLJIT_FR4);
		/* bbuf[10] and bbuf[58] */
		test75_set(compiler, i, SLJIT_ORDERED_LESS, SLJIT_FR0, SLJIT_FR1);
		/* bbuf[11] and bbuf[59] */
		test75_set(compiler, i, SLJIT_ORDERED_LESS, SLJIT_FR0, SLJIT_FR2);

		/* bbuf[12] and bbuf[60] */
		test75_set(compiler, i, SLJIT_UNORDERED_OR_GREATER_EQUAL, SLJIT_FR2, SLJIT_FR4);
		/* bbuf[13] and bbuf[61] */
		test75_set(compiler, i, SLJIT_UNORDERED_OR_GREATER_EQUAL, SLJIT_FR4, SLJIT_FR2);
		/* bbuf[14] and bbuf[62] */
		test75_set(compiler, i, SLJIT_UNORDERED_OR_GREATER_EQUAL, SLJIT_FR0, SLJIT_FR1);
		/* bbuf[15] and bbuf[63] */
		test75_set(compiler, i, SLJIT_UNORDERED_OR_GREATER_EQUAL, SLJIT_FR0, SLJIT_FR2);

		/* bbuf[16] and bbuf[64] */
		test75_set(compiler, i, SLJIT_ORDERED_GREATER, SLJIT_FR2, SLJIT_FR4);
		/* bbuf[17] and bbuf[65] */
		test75_set(compiler, i, SLJIT_ORDERED_GREATER, SLJIT_FR4, SLJIT_FR2);
		/* bbuf[18] and bbuf[66] */
		test75_set(compiler, i, SLJIT_ORDERED_GREATER, SLJIT_FR0, SLJIT_FR1);
		/* bbuf[19] and bbuf[67] */
		test75_set(compiler, i, SLJIT_ORDERED_GREATER, SLJIT_FR0, SLJIT_FR2);

		/* bbuf[20] and bbuf[68] */
		test75_set(compiler, i, SLJIT_UNORDERED_OR_LESS_EQUAL, SLJIT_FR2, SLJIT_FR4);
		/* bbuf[21] and bbuf[69] */
		test75_set(compiler, i, SLJIT_UNORDERED_OR_LESS_EQUAL, SLJIT_FR4, SLJIT_FR2);
		/* bbuf[22] and bbuf[70] */
		test75_set(compiler, i, SLJIT_UNORDERED_OR_LESS_EQUAL, SLJIT_FR0, SLJIT_FR1);
		/* bbuf[23] and bbuf[71] */
		test75_set(compiler, i, SLJIT_UNORDERED_OR_LESS_EQUAL, SLJIT_FR0, SLJIT_FR2);

		/* bbuf[24] and bbuf[72] */
		test75_set(compiler, i, SLJIT_UNORDERED_OR_EQUAL, SLJIT_FR2, SLJIT_FR4);
		/* bbuf[25] and bbuf[73] */
		test75_set(compiler, i, SLJIT_UNORDERED_OR_EQUAL, SLJIT_FR2, SLJIT_FR3);
		/* bbuf[26] and bbuf[74] */
		test75_set(compiler, i, SLJIT_UNORDERED_OR_EQUAL, SLJIT_FR0, SLJIT_FR1);
		/* bbuf[27] and bbuf[75] */
		test75_set(compiler, i, SLJIT_UNORDERED_OR_EQUAL, SLJIT_FR0, SLJIT_FR2);

		/* bbuf[28] and bbuf[76] */
		test75_set(compiler, i, SLJIT_ORDERED_NOT_EQUAL, SLJIT_FR2, SLJIT_FR3);
		/* bbuf[29] and bbuf[77] */
		test75_set(compiler, i, SLJIT_ORDERED_NOT_EQUAL, SLJIT_FR2, SLJIT_FR4);
		/* bbuf[30] and bbuf[78] */
		test75_set(compiler, i, SLJIT_ORDERED_NOT_EQUAL, SLJIT_FR0, SLJIT_FR1);
		/* bbuf[31] and bbuf[79] */
		test75_set(compiler, i, SLJIT_ORDERED_NOT_EQUAL, SLJIT_FR0, SLJIT_FR2);

		/* bbuf[32] and bbuf[80] */
		test75_set(compiler, i, SLJIT_UNORDERED_OR_LESS, SLJIT_FR2, SLJIT_FR4);
		/* bbuf[33] and bbuf[81] */
		test75_set(compiler, i, SLJIT_UNORDERED_OR_LESS, SLJIT_FR2, SLJIT_FR3);
		/* bbuf[34] and bbuf[82] */
		test75_set(compiler, i, SLJIT_UNORDERED_OR_LESS, SLJIT_FR0, SLJIT_FR1);
		/* bbuf[35] and bbuf[83] */
		test75_set(compiler, i, SLJIT_UNORDERED_OR_LESS, SLJIT_FR0, SLJIT_FR2);

		/* bbuf[36] and bbuf[84] */
		test75_set(compiler, i, SLJIT_ORDERED_GREATER_EQUAL, SLJIT_FR2, SLJIT_FR4);
		/* bbuf[37] and bbuf[85] */
		test75_set(compiler, i, SLJIT_ORDERED_GREATER_EQUAL, SLJIT_FR4, SLJIT_FR2);
		/* bbuf[38] and bbuf[86] */
		test75_set(compiler, i, SLJIT_ORDERED_GREATER_EQUAL, SLJIT_FR0, SLJIT_FR1);
		/* bbuf[39] and bbuf[87] */
		test75_set(compiler, i, SLJIT_ORDERED_GREATER_EQUAL, SLJIT_FR0, SLJIT_FR2);

		/* bbuf[40] and bbuf[88] */
		test75_set(compiler, i, SLJIT_UNORDERED_OR_GREATER, SLJIT_FR2, SLJIT_FR4);
		/* bbuf[41] and bbuf[89] */
		test75_set(compiler, i, SLJIT_UNORDERED_OR_GREATER, SLJIT_FR4, SLJIT_FR2);
		/* bbuf[42] and bbuf[90] */
		test75_set(compiler, i, SLJIT_UNORDERED_OR_GREATER, SLJIT_FR0, SLJIT_FR1);
		/* bbuf[43] and bbuf[91] */
		test75_set(compiler, i, SLJIT_UNORDERED_OR_GREATER, SLJIT_FR0, SLJIT_FR2);

		/* bbuf[44] and bbuf[92] */
		test75_set(compiler, i, SLJIT_ORDERED_LESS_EQUAL, SLJIT_FR2, SLJIT_FR3);
		/* bbuf[45] and bbuf[93] */
		test75_set(compiler, i, SLJIT_ORDERED_LESS_EQUAL, SLJIT_FR4, SLJIT_FR2);
		/* bbuf[46] and bbuf[94] */
		test75_set(compiler, i, SLJIT_ORDERED_LESS_EQUAL, SLJIT_FR0, SLJIT_FR1);
		/* bbuf[47] and bbuf[95] */
		test75_set(compiler, i, SLJIT_ORDERED_LESS_EQUAL, SLJIT_FR0, SLJIT_FR2);

		if (i == SLJIT_CMP_F32)
			break;

		i = SLJIT_CMP_F32;
		sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S2), 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S2), 0);
		sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S2), sizeof(sljit_f32));
		sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR3, 0, SLJIT_MEM1(SLJIT_S2), sizeof(sljit_f32));
		sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR4, 0, SLJIT_MEM1(SLJIT_S2), 2 * sizeof(sljit_f32));
	}

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func3((sljit_sw)&bbuf, (sljit_sw)&dbuf, (sljit_sw)&sbuf);

	/* SLJIT_CMP_F64 */
	FAILED(bbuf[0] != 1, "test75 case 1 failed\n");
	FAILED(bbuf[1] != 2, "test75 case 2 failed\n");
	FAILED(bbuf[2] != 2, "test75 case 3 failed\n");
	FAILED(bbuf[3] != 2, "test75 case 4 failed\n");

	FAILED(bbuf[4] != 2, "test75 case 5 failed\n");
	FAILED(bbuf[5] != 1, "test75 case 6 failed\n");
	FAILED(bbuf[6] != 1, "test75 case 7 failed\n");
	FAILED(bbuf[7] != 1, "test75 case 8 failed\n");

	FAILED(bbuf[8] != 2, "test75 case 9 failed\n");
	FAILED(bbuf[9] != 1, "test75 case 10 failed\n");
	FAILED(bbuf[10] != 2, "test75 case 11 failed\n");
	FAILED(bbuf[11] != 2, "test75 case 12 failed\n");

	FAILED(bbuf[12] != 2, "test75 case 13 failed\n");
	FAILED(bbuf[13] != 1, "test75 case 14 failed\n");
	FAILED(bbuf[14] != 1, "test75 case 15 failed\n");
	FAILED(bbuf[15] != 1, "test75 case 16 failed\n");

	FAILED(bbuf[16] != 2, "test75 case 17 failed\n");
	FAILED(bbuf[17] != 1, "test75 case 18 failed\n");
	FAILED(bbuf[18] != 2, "test75 case 19 failed\n");
	FAILED(bbuf[19] != 2, "test75 case 20 failed\n");

	FAILED(bbuf[20] != 1, "test75 case 21 failed\n");
	FAILED(bbuf[21] != 2, "test75 case 22 failed\n");
	FAILED(bbuf[22] != 1, "test75 case 23 failed\n");
	FAILED(bbuf[23] != 1, "test75 case 24 failed\n");

	FAILED(bbuf[24] != 2, "test75 case 25 failed\n");
	FAILED(bbuf[25] != 1, "test75 case 26 failed\n");
	FAILED(bbuf[26] != 1, "test75 case 27 failed\n");
	FAILED(bbuf[27] != 1, "test75 case 28 failed\n");

	FAILED(bbuf[28] != 2, "test75 case 29 failed\n");
	FAILED(bbuf[29] != 1, "test75 case 30 failed\n");
	FAILED(bbuf[30] != 2, "test75 case 31 failed\n");
	FAILED(bbuf[31] != 2, "test75 case 32 failed\n");

	FAILED(bbuf[32] != 1, "test75 case 33 failed\n");
	FAILED(bbuf[33] != 2, "test75 case 34 failed\n");
	FAILED(bbuf[34] != 1, "test75 case 35 failed\n");
	FAILED(bbuf[35] != 1, "test75 case 36 failed\n");

	FAILED(bbuf[36] != 2, "test75 case 37 failed\n");
	FAILED(bbuf[37] != 1, "test75 case 38 failed\n");
	FAILED(bbuf[38] != 2, "test75 case 39 failed\n");
	FAILED(bbuf[39] != 2, "test75 case 40 failed\n");

	FAILED(bbuf[40] != 2, "test75 case 41 failed\n");
	FAILED(bbuf[41] != 1, "test75 case 42 failed\n");
	FAILED(bbuf[42] != 1, "test75 case 43 failed\n");
	FAILED(bbuf[43] != 1, "test75 case 44 failed\n");

	FAILED(bbuf[44] != 1, "test75 case 45 failed\n");
	FAILED(bbuf[45] != 2, "test75 case 46 failed\n");
	FAILED(bbuf[46] != 2, "test75 case 47 failed\n");
	FAILED(bbuf[47] != 2, "test75 case 48 failed\n");

	/* SLJIT_CMP_F32 */
	FAILED(bbuf[48] != 1, "test75 case 49 failed\n");
	FAILED(bbuf[49] != 2, "test75 case 50 failed\n");
	FAILED(bbuf[50] != 2, "test75 case 51 failed\n");
	FAILED(bbuf[51] != 2, "test75 case 52 failed\n");

	FAILED(bbuf[52] != 2, "test75 case 53 failed\n");
	FAILED(bbuf[53] != 1, "test75 case 54 failed\n");
	FAILED(bbuf[54] != 1, "test75 case 55 failed\n");
	FAILED(bbuf[55] != 1, "test75 case 56 failed\n");

	FAILED(bbuf[56] != 2, "test75 case 57 failed\n");
	FAILED(bbuf[57] != 1, "test75 case 58 failed\n");
	FAILED(bbuf[58] != 2, "test75 case 59 failed\n");
	FAILED(bbuf[59] != 2, "test75 case 60 failed\n");

	FAILED(bbuf[60] != 2, "test75 case 61 failed\n");
	FAILED(bbuf[61] != 1, "test75 case 62 failed\n");
	FAILED(bbuf[62] != 1, "test75 case 63 failed\n");
	FAILED(bbuf[63] != 1, "test75 case 64 failed\n");

	FAILED(bbuf[64] != 2, "test75 case 65 failed\n");
	FAILED(bbuf[65] != 1, "test75 case 66 failed\n");
	FAILED(bbuf[66] != 2, "test75 case 67 failed\n");
	FAILED(bbuf[67] != 2, "test75 case 68 failed\n");

	FAILED(bbuf[68] != 1, "test75 case 69 failed\n");
	FAILED(bbuf[69] != 2, "test75 case 70 failed\n");
	FAILED(bbuf[70] != 1, "test75 case 71 failed\n");
	FAILED(bbuf[71] != 1, "test75 case 72 failed\n");

	FAILED(bbuf[72] != 2, "test75 case 73 failed\n");
	FAILED(bbuf[73] != 1, "test75 case 74 failed\n");
	FAILED(bbuf[74] != 1, "test75 case 75 failed\n");
	FAILED(bbuf[75] != 1, "test75 case 76 failed\n");

	FAILED(bbuf[76] != 2, "test75 case 77 failed\n");
	FAILED(bbuf[77] != 1, "test75 case 78 failed\n");
	FAILED(bbuf[78] != 2, "test75 case 79 failed\n");
	FAILED(bbuf[79] != 2, "test75 case 80 failed\n");

	FAILED(bbuf[80] != 1, "test75 case 81 failed\n");
	FAILED(bbuf[81] != 2, "test75 case 82 failed\n");
	FAILED(bbuf[82] != 1, "test75 case 83 failed\n");
	FAILED(bbuf[83] != 1, "test75 case 84 failed\n");

	FAILED(bbuf[84] != 2, "test75 case 85 failed\n");
	FAILED(bbuf[85] != 1, "test75 case 86 failed\n");
	FAILED(bbuf[86] != 2, "test75 case 87 failed\n");
	FAILED(bbuf[87] != 2, "test75 case 88 failed\n");

	FAILED(bbuf[88] != 2, "test75 case 89 failed\n");
	FAILED(bbuf[89] != 1, "test75 case 90 failed\n");
	FAILED(bbuf[90] != 1, "test75 case 91 failed\n");
	FAILED(bbuf[91] != 1, "test75 case 92 failed\n");

	FAILED(bbuf[92] != 1, "test75 case 93 failed\n");
	FAILED(bbuf[93] != 2, "test75 case 94 failed\n");
	FAILED(bbuf[94] != 2, "test75 case 95 failed\n");
	FAILED(bbuf[95] != 2, "test75 case 96 failed\n");

	successful_tests++;
}

static void test76(void)
{
	/* Test register argument and keep saved registers. */
	executable_code code;
	struct sljit_compiler* compiler;
	struct sljit_jump* jump;
	sljit_sw buf[9];
	sljit_f64 dbuf[3];
	sljit_s32 i;

	if (verbose)
		printf("Run test76\n");

	for (i = 0; i < 9; i++)
		buf[i] = -1;

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(VOID), 4, 2, 0, 0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 7945);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, -9267);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 4309);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, -8321);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, 6803);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, -5497);

	jump = sljit_emit_call(compiler, SLJIT_CALL_REG_ARG, SLJIT_ARGS4(W, W, W, W, W));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)&buf);
	/* buf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), 0, SLJIT_R0, 0);
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), sizeof(sljit_sw), SLJIT_S0, 0);
	/* buf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), 2 * sizeof(sljit_sw), SLJIT_S1, 0);
	sljit_emit_return_void(compiler);

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_enter(compiler, SLJIT_ENTER_REG_ARG, SLJIT_ARGS4(W, W_R, W_R, W_R, W_R), 4, 2, 0, 0, 32);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, (sljit_sw)&buf);
	/* buf[3-6] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_R2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_R3, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 6028);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, 4982);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, -1289);

	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_R0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);

	FAILED(buf[0] != 6028, "test76 case 1 failed\n");
	FAILED(buf[1] != 6803, "test76 case 2 failed\n");
	FAILED(buf[2] != -5497, "test76 case 3 failed\n");
	FAILED(buf[3] != 7945, "test76 case 4 failed\n");
	FAILED(buf[4] != -9267, "test76 case 5 failed\n");
	FAILED(buf[5] != 4309, "test76 case 6 failed\n");
	FAILED(buf[6] != -8321, "test76 case 7 failed\n");

	/* Next test. */

	for (i = 0; i < 9; i++)
		buf[i] = -1;

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(VOID), 4, 2, 0, 0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -2608);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 4751);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 5740);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, -9704);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, -8749);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, 9213);

	jump = sljit_emit_call(compiler, SLJIT_CALL_REG_ARG, SLJIT_ARGS4(W, W, W, W, W));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)&buf);
	/* buf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), 0, SLJIT_R0, 0);
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), sizeof(sljit_sw), SLJIT_S0, 0);
	/* buf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), 2 * sizeof(sljit_sw), SLJIT_S1, 0);
	sljit_emit_return_void(compiler);

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_enter(compiler, SLJIT_ENTER_REG_ARG | SLJIT_ENTER_KEEP(1), SLJIT_ARGS4(W, W_R, W_R, W_R, W_R), 6, 2, 0, 0, SLJIT_MAX_LOCAL_SIZE);
	sljit_set_context(compiler, SLJIT_ENTER_REG_ARG | SLJIT_ENTER_KEEP(1), SLJIT_ARGS4(W, W_R, W_R, W_R, W_R), 6, 2, 0, 0, SLJIT_MAX_LOCAL_SIZE);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, (sljit_sw)&buf);
	/* buf[3-7] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_sw), SLJIT_S0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 5 * sizeof(sljit_sw), SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 6 * sizeof(sljit_sw), SLJIT_R2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 7 * sizeof(sljit_sw), SLJIT_R3, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -7351);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, 3628);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, 0);

	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_R0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);

	FAILED(buf[0] != -7351, "test76 case 8 failed\n");
	FAILED(buf[1] != 3628, "test76 case 9 failed\n");
	FAILED(buf[2] != 9213, "test76 case 10 failed\n");
	FAILED(buf[3] != -8749, "test76 case 11 failed\n");
	FAILED(buf[4] != -2608, "test76 case 12 failed\n");
	FAILED(buf[5] != 4751, "test76 case 13 failed\n");
	FAILED(buf[6] != 5740, "test76 case 14 failed\n");
	FAILED(buf[7] != -9704, "test76 case 15 failed\n");
	FAILED(buf[8] != -1, "test76 case 16 failed\n");

	/* Next test. */

	for (i = 0; i < 9; i++)
		buf[i] = -1;

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(VOID), 4, 2, 0, 0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 8653);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 7245);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, -3610);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, 4591);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, -2865);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, 2510);

	jump = sljit_emit_call(compiler, SLJIT_CALL_REG_ARG, SLJIT_ARGS4(VOID, W, W, W, W));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&buf);
	/* buf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_S0, 0);
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_sw), SLJIT_S1, 0);
	sljit_emit_return_void(compiler);

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_enter(compiler, SLJIT_ENTER_REG_ARG | SLJIT_ENTER_KEEP(2), SLJIT_ARGS4(W, W_R, W_R, W_R, W_R), 4, 3, 0, 0, SLJIT_MAX_LOCAL_SIZE);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, (sljit_sw)&buf);
	/* buf[2-7] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S2), 2 * sizeof(sljit_sw), SLJIT_S0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S2), 3 * sizeof(sljit_sw), SLJIT_S1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S2), 4 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S2), 5 * sizeof(sljit_sw), SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S2), 6 * sizeof(sljit_sw), SLJIT_R2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S2), 7 * sizeof(sljit_sw), SLJIT_R3, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, 5789);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, -9214);

	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_R0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);

	FAILED(buf[0] != 5789, "test76 case 17 failed\n");
	FAILED(buf[1] != -9214, "test76 case 18 failed\n");
	FAILED(buf[2] != -2865, "test76 case 19 failed\n");
	FAILED(buf[3] != 2510, "test76 case 20 failed\n");
	FAILED(buf[4] != 8653, "test76 case 21 failed\n");
	FAILED(buf[5] != 7245, "test76 case 22 failed\n");
	FAILED(buf[6] != -3610, "test76 case 23 failed\n");
	FAILED(buf[7] != 4591, "test76 case 24 failed\n");
	FAILED(buf[8] != -1, "test76 case 25 failed\n");

	/* Next test. */

	for (i = 0; i < 9; i++)
		buf[i] = -1;

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(VOID), 2, 3, 0, 0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, 6071);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, -3817);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, 9250);

	jump = sljit_emit_call(compiler, SLJIT_CALL_REG_ARG, SLJIT_ARGS0(W));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)&buf);
	/* buf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), 0, SLJIT_R0, 0);
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), sizeof(sljit_sw), SLJIT_S0, 0);
	/* buf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), 2 * sizeof(sljit_sw), SLJIT_S1, 0);
	/* buf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), 3 * sizeof(sljit_sw), SLJIT_S2, 0);
	sljit_emit_return_void(compiler);

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_enter(compiler, SLJIT_ENTER_REG_ARG | SLJIT_ENTER_KEEP(2), SLJIT_ARGS0(W), 4, 3, 0, 0, SLJIT_MAX_LOCAL_SIZE);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&buf);
	/* buf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), 4 * sizeof(sljit_sw), SLJIT_S0, 0);
	/* buf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), 5 * sizeof(sljit_sw), SLJIT_S1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -6278);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 1467);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 7150 - 1467);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, 8413);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, 4892);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, -7513);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, -1);

	jump = sljit_emit_call(compiler, SLJIT_CALL_REG_ARG | SLJIT_CALL_RETURN, SLJIT_ARGS4(W, W, W, W, W));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM0(), 0);

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_enter(compiler, SLJIT_ENTER_REG_ARG, SLJIT_ARGS4(W, W_R, W_R, W_R, W_R), 4, 2, 0, 0, 256);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, (sljit_sw)&buf);
	/* buf[6] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_R1, 0);
	/* buf[7] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_R2, 0);
	/* buf[8] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw), SLJIT_R3, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, -1);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_IMM, 6923);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);

	FAILED(buf[0] != 6923, "test76 case 26 failed\n");
	FAILED(buf[1] != 4892, "test76 case 27 failed\n");
	FAILED(buf[2] != -7513, "test76 case 28 failed\n");
	FAILED(buf[3] != 9250, "test76 case 29 failed\n");
	FAILED(buf[4] != 6071, "test76 case 30 failed\n");
	FAILED(buf[5] != -3817, "test76 case 31 failed\n");
	FAILED(buf[6] != -6278, "test76 case 32 failed\n");
	FAILED(buf[7] != 7150, "test76 case 33 failed\n");
	FAILED(buf[8] != 8413, "test76 case 34 failed\n");

	if (sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		/* Next test. */

		for (i = 0; i < 9; i++)
			buf[i] = -1;

		dbuf[0] = 4061.25;
		dbuf[1] = -3291.75;
		dbuf[2] = 8703.5;

		compiler = sljit_create_compiler(NULL, NULL);
		FAILED(!compiler, "cannot create compiler\n");

		sljit_emit_enter(compiler, 0, SLJIT_ARGS0(VOID), 2, 3, 3, 0, 0);

		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)&dbuf);
		/* dbuf[0] */
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_R1), 0);
		/* dbuf[1] */
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_R1), sizeof(sljit_f64));
		/* dbuf[2] */
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_R1), 2 * sizeof(sljit_f64));

		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1706);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, -8956);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, 4381);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, -5314);

		jump = sljit_emit_call(compiler, SLJIT_CALL_REG_ARG, SLJIT_ARGS4(W, F64, F64, F64, W));

		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)&buf);
		/* buf[0] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), 0, SLJIT_R0, 0);
		/* buf[1] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), sizeof(sljit_sw), SLJIT_S0, 0);
		/* buf[2] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), 2 * sizeof(sljit_sw), SLJIT_S1, 0);
		/* buf[3] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), 3 * sizeof(sljit_sw), SLJIT_S2, 0);
		sljit_emit_return_void(compiler);

		sljit_set_label(jump, sljit_emit_label(compiler));
		sljit_emit_enter(compiler, SLJIT_ENTER_REG_ARG | SLJIT_ENTER_KEEP(1), SLJIT_ARGS4(W, F64, F64, F64, W_R), 1, 3, 3, 0, SLJIT_MAX_LOCAL_SIZE);

		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, (sljit_sw)&buf);
		/* buf[4] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_sw), SLJIT_S0, 0);
		/* buf[5] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 5 * sizeof(sljit_sw), SLJIT_R0, 0);

		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, (sljit_sw)&dbuf);
		/* dbuf[0] */
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_FR2, 0);
		/* dbuf[1] */
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_f64), SLJIT_FR0, 0);
		/* dbuf[2] */
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_f64), SLJIT_FR1, 0);

		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 2784);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, 1503);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, -1);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, -1);

		sljit_emit_return(compiler, SLJIT_MOV, SLJIT_R0, 0);

		code.code = sljit_generate_code(compiler);
		CHECK(compiler);
		sljit_free_compiler(compiler);

		code.func1((sljit_sw)&buf);

		FAILED(buf[0] != 2784, "test76 case 35 failed\n");
		FAILED(buf[1] != 1503, "test76 case 36 failed\n");
		FAILED(buf[2] != 4381, "test76 case 37 failed\n");
		FAILED(buf[3] != -5314, "test76 case 38 failed\n");
		FAILED(buf[4] != -8956, "test76 case 39 failed\n");
		FAILED(buf[5] != 1706, "test76 case 40 failed\n");
		FAILED(buf[6] != -1, "test76 case 41 failed\n");
		FAILED(dbuf[0] != 8703.5, "test76 case 42 failed\n");
		FAILED(dbuf[1] != 4061.25, "test76 case 43 failed\n");
		FAILED(dbuf[2] != -3291.75, "test76 case 44 failed\n");

		/* Next test. */

		for (i = 0; i < 9; i++)
			buf[i] = -1;

		dbuf[0] = 4061.25;

		compiler = sljit_create_compiler(NULL, NULL);
		FAILED(!compiler, "cannot create compiler\n");

		sljit_emit_enter(compiler, 0, SLJIT_ARGS0(VOID), 3, 3, 1, 0, 0);

		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM0(), (sljit_sw)&dbuf);

		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 8793);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, -4027);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 2910);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, 4619);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, -1502);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, 5316);

		jump = sljit_emit_call(compiler, SLJIT_CALL_REG_ARG, SLJIT_ARGS4(VOID, F64, W, W, W));

		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&buf);
		/* buf[0] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_S0, 0);
		/* buf[1] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_sw), SLJIT_S1, 0);
		/* buf[2] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), 2 * sizeof(sljit_sw), SLJIT_S2, 0);
		sljit_emit_return_void(compiler);

		sljit_set_label(jump, sljit_emit_label(compiler));
		sljit_emit_enter(compiler, SLJIT_ENTER_REG_ARG | SLJIT_ENTER_KEEP(2), SLJIT_ARGS4(VOID, F64, W_R, W_R, W_R), 3, 3, 3, 0, SLJIT_MAX_LOCAL_SIZE);

		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, (sljit_sw)&buf);
		/* buf[3] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S2), 3 * sizeof(sljit_sw), SLJIT_S0, 0);
		/* buf[4] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S2), 4 * sizeof(sljit_sw), SLJIT_S1, 0);
		/* buf[5] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S2), 5 * sizeof(sljit_sw), SLJIT_R0, 0);
		/* buf[6] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S2), 6 * sizeof(sljit_sw), SLJIT_R1, 0);
		/* buf[7] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S2), 7 * sizeof(sljit_sw), SLJIT_R2, 0);

		sljit_emit_fop1(compiler, SLJIT_NEG_F64, SLJIT_MEM0(), (sljit_sw)&dbuf, SLJIT_FR0, 0);

		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, 7839);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, -9215);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, -1);

		sljit_emit_return_void(compiler);

		code.code = sljit_generate_code(compiler);
		CHECK(compiler);
		sljit_free_compiler(compiler);

		code.func1((sljit_sw)&buf);

		FAILED(buf[0] != 7839, "test76 case 45 failed\n");
		FAILED(buf[1] != -9215, "test76 case 46 failed\n");
		FAILED(buf[2] != 5316, "test76 case 47 failed\n");
		FAILED(buf[3] != 4619, "test76 case 48 failed\n");
		FAILED(buf[4] != -1502, "test76 case 49 failed\n");
		FAILED(buf[5] != 8793, "test76 case 50 failed\n");
		FAILED(buf[6] != -4027, "test76 case 51 failed\n");
		FAILED(buf[7] != 2910, "test76 case 52 failed\n");
		FAILED(buf[8] != -1, "test76 case 53 failed\n");
		FAILED(dbuf[0] != -4061.25, "test76 case 54 failed\n");

		/* Next test. */

		for (i = 0; i < 9; i++)
			buf[i] = -1;

		dbuf[0] = 4061.25;
		dbuf[1] = -3291.75;
		dbuf[2] = 8703.5;

		compiler = sljit_create_compiler(NULL, NULL);
		FAILED(!compiler, "cannot create compiler\n");

		sljit_emit_enter(compiler, 0, SLJIT_ARGS0(VOID), 2, 3, 0, 0, 0);

		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, 7869);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, -5406);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, 4951);

		jump = sljit_emit_call(compiler, SLJIT_CALL_REG_ARG, SLJIT_ARGS0(W));

		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)&buf);
		/* buf[0] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), 0, SLJIT_R0, 0);
		/* buf[1] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), sizeof(sljit_sw), SLJIT_S0, 0);
		/* buf[2] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), 2 * sizeof(sljit_sw), SLJIT_S1, 0);
		/* buf[3] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), 3 * sizeof(sljit_sw), SLJIT_S2, 0);
		sljit_emit_return_void(compiler);

		sljit_set_label(jump, sljit_emit_label(compiler));
		sljit_emit_enter(compiler, SLJIT_ENTER_REG_ARG | SLJIT_ENTER_KEEP(2), SLJIT_ARGS0(W), 1, 3, 3, 0, SLJIT_MAX_LOCAL_SIZE);

		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&buf);
		/* buf[4] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), 4 * sizeof(sljit_sw), SLJIT_S0, 0);
		/* buf[5] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), 5 * sizeof(sljit_sw), SLJIT_S1, 0);

		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&dbuf);
		/* dbuf[0] */
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_R0), 0);
		/* dbuf[1] */
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_f64));
		/* dbuf[2] */
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_R0), 2 * sizeof(sljit_f64));

		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1706);

		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, 4713);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, -2078);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, -1);

		jump = sljit_emit_call(compiler, SLJIT_CALL_REG_ARG | SLJIT_CALL_RETURN, SLJIT_ARGS4(W, F64, F64, F64, W));
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM0(), 0);

		sljit_set_label(jump, sljit_emit_label(compiler));
		sljit_emit_enter(compiler, SLJIT_ENTER_REG_ARG, SLJIT_ARGS4(W, F64, F64, F64, W_R), 1, 0, 3, 0, 256);

		/* buf[6] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM0(), (sljit_sw)&buf[6], SLJIT_R0, 0);

		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&dbuf);
		/* dbuf[0] */
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_FR2, 0);
		/* dbuf[1] */
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_f64), SLJIT_FR0, 0);
		/* dbuf[2] */
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), 2 * sizeof(sljit_f64), SLJIT_FR1, 0);

		sljit_emit_return(compiler, SLJIT_MOV, SLJIT_IMM, 5074);

		code.code = sljit_generate_code(compiler);
		CHECK(compiler);
		sljit_free_compiler(compiler);

		code.func1((sljit_sw)&buf);

		FAILED(buf[0] != 5074, "test76 case 55 failed\n");
		FAILED(buf[1] != 4713, "test76 case 56 failed\n");
		FAILED(buf[2] != -2078, "test76 case 57 failed\n");
		FAILED(buf[3] != 4951, "test76 case 58 failed\n");
		FAILED(buf[4] != 7869, "test76 case 59 failed\n");
		FAILED(buf[5] != -5406, "test76 case 60 failed\n");
		FAILED(buf[6] != 1706, "test76 case 61 failed\n");
		FAILED(buf[7] != -1, "test76 case 62 failed\n");
		FAILED(dbuf[0] != 8703.5, "test76 case 63 failed\n");
		FAILED(dbuf[1] != 4061.25, "test76 case 64 failed\n");
		FAILED(dbuf[2] != -3291.75, "test76 case 65 failed\n");
	}

	successful_tests++;
}

static void copy_u8(void *dst, sljit_sw offset, const void *src, sljit_uw length)
{
	sljit_u8 *dst_p = (sljit_u8 *)dst + offset;
	const sljit_u8 *src_p = (sljit_u8 *)src;

	while (length-- != 0)
		*dst_p++ = *src_p++;
}

static int cmp_u8(const void *src1, sljit_sw offset, const void *src2, sljit_uw length)
{
	const sljit_u8 *src1_p = (sljit_u8 *)src1 + offset;
	const sljit_u8 *src2_p = (sljit_u8 *)src2;

	while (--length != 0) {
		if (*src1_p != *src2_p)
			return 0;
		src1_p++;
		src2_p++;
	}
	return 1;
}

static void test77(void)
{
	/* Test unaligned accesses. */
	executable_code code;
	struct sljit_compiler* compiler;
	sljit_sw i;
	sljit_sw wbuf[13];
	sljit_s32 ibuf[1];
	sljit_s16 hbuf[1];
	sljit_f64 dbuf[5];
	sljit_f32 sbuf[3];
	sljit_s8 bbuf_start[40 + 8 /* for alignment */];
	sljit_s8 *bbuf = (sljit_s8 *)(((sljit_uw)bbuf_start + 7) & ~(sljit_uw)7);

	SLJIT_ASSERT(((sljit_uw)bbuf & 0x7) == 0);

	if (verbose)
		printf("Run test77\n");

	for (i = 0; i < 13; i++)
		wbuf[i] = -3;

	for (i = 0; i < 40; i++)
		bbuf[i] = -3;

	wbuf[0] = -46870;
	ibuf[0] = -38512;
	hbuf[0] = -28531;
	copy_u8(bbuf, 3, hbuf, sizeof(sljit_s16));
	copy_u8(bbuf, 5, ibuf, sizeof(sljit_s32));
	copy_u8(bbuf, 9, wbuf, sizeof(sljit_sw));
	copy_u8(bbuf, 18, ibuf, sizeof(sljit_s32));
	copy_u8(bbuf, 22, wbuf, sizeof(sljit_sw));
	copy_u8(bbuf, 32, wbuf, sizeof(sljit_sw));

	wbuf[0] = -62945;
	ibuf[0] = -90678;
	hbuf[0] = -17249;
	bbuf[0] = -73;

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(VOID, P, P), 2, 2, 0, 0, 0);

	sljit_emit_mem(compiler, SLJIT_MOV_S8 | SLJIT_MEM_UNALIGNED, SLJIT_R0, SLJIT_MEM0(), (sljit_sw)bbuf);
	/* wbuf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_mem(compiler, SLJIT_MOV_U8 | SLJIT_MEM_UNALIGNED, SLJIT_R0, SLJIT_MEM2(SLJIT_S1, SLJIT_R0), 0);
	/* wbuf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV_S8, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S1), 0);
	/* bbuf[1] */
	sljit_emit_mem(compiler, SLJIT_MOV_U8 | SLJIT_MEM_STORE | SLJIT_MEM_UNALIGNED, SLJIT_R0, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_s8));

	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R1, 0, SLJIT_S1, 0, SLJIT_IMM, 100000);
	sljit_emit_mem(compiler, SLJIT_MOV_S16 | SLJIT_MEM_UNALIGNED, SLJIT_R0, SLJIT_MEM1(SLJIT_R1), 100000 + 3);
	/* wbuf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S1, 0, SLJIT_IMM, 1000);
	sljit_emit_mem(compiler, SLJIT_MOV_U16 | SLJIT_MEM_UNALIGNED, SLJIT_R0, SLJIT_MEM1(SLJIT_R1), -1000 + 3);
	/* wbuf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV_S16, SLJIT_R0, 0, SLJIT_MEM0(), (sljit_sw)&hbuf);
	/* bbuf[3] */
	sljit_emit_mem(compiler, SLJIT_MOV_S16 | SLJIT_MEM_STORE | SLJIT_MEM_UNALIGNED, SLJIT_R0, SLJIT_MEM1(SLJIT_S1), 3);

	sljit_emit_mem(compiler, SLJIT_MOV_S32 | SLJIT_MEM_UNALIGNED, SLJIT_R0, SLJIT_MEM1(SLJIT_S1), 5);
	/* wbuf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 5);
	sljit_emit_mem(compiler, SLJIT_MOV_U32 | SLJIT_MEM_UNALIGNED, SLJIT_R0, SLJIT_MEM2(SLJIT_R0, SLJIT_S1), 0);
	/* wbuf[6] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R1, 0, SLJIT_S1, 0, SLJIT_IMM, 100000);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_MEM0(), (sljit_sw)&ibuf);
	/* bbuf[5] */
	sljit_emit_mem(compiler, SLJIT_MOV32 | SLJIT_MEM_STORE | SLJIT_MEM_UNALIGNED, SLJIT_R0, SLJIT_MEM1(SLJIT_R1), 100000 + 5);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S1, 0, SLJIT_IMM, 100000);
#if (defined SLJIT_UNALIGNED && SLJIT_UNALIGNED)
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R1), -100000 + 9);
#else /* !SLJIT_UNALIGNED */
	sljit_emit_mem(compiler, SLJIT_MOV | SLJIT_MEM_UNALIGNED, SLJIT_R0, SLJIT_MEM1(SLJIT_R1), -100000 + 9);
#endif /* SLJIT_UNALIGNED */
	/* wbuf[7] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), 0);
	/* bbuf[9] */
#if (defined SLJIT_UNALIGNED && SLJIT_UNALIGNED)
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), -100000 + 9, SLJIT_R0, 0);
#else /* !SLJIT_UNALIGNED */
	sljit_emit_mem(compiler, SLJIT_MOV | SLJIT_MEM_STORE | SLJIT_MEM_UNALIGNED, SLJIT_R0, SLJIT_MEM1(SLJIT_R1), -100000 + 9);
#endif /* SLJIT_UNALIGNED */

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 18 >> 1);
	sljit_emit_mem(compiler, SLJIT_MOV_S32 | SLJIT_MEM_ALIGNED_16, SLJIT_R0, SLJIT_MEM2(SLJIT_S1, SLJIT_R1), 1);
	/* wbuf[8] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV_S32, SLJIT_R0, 0, SLJIT_MEM0(), (sljit_sw)&ibuf);
	/* bbuf[18] */
	sljit_emit_mem(compiler, SLJIT_MOV_S32 | SLJIT_MEM_STORE | SLJIT_MEM_ALIGNED_16, SLJIT_R0, SLJIT_MEM2(SLJIT_S1, SLJIT_R1), 1);

	sljit_emit_mem(compiler, SLJIT_MOV | SLJIT_MEM_ALIGNED_16, SLJIT_R0, SLJIT_MEM0(), (sljit_sw)bbuf + 22);
	/* wbuf[9] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), 0);
	/* bbuf[22] */
	sljit_emit_mem(compiler, SLJIT_MOV | SLJIT_MEM_STORE | SLJIT_MEM_ALIGNED_16, SLJIT_R0, SLJIT_MEM0(), (sljit_sw)bbuf + 22);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S1, 0, SLJIT_IMM, 128);
	sljit_emit_mem(compiler, SLJIT_MOV | SLJIT_MEM_ALIGNED_32, SLJIT_R0, SLJIT_MEM1(SLJIT_R0), -128 + 32);
	/* wbuf[10] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 10 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S1, 0, SLJIT_IMM, 128);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), 0);
	/* bbuf[32] */
	sljit_emit_mem(compiler, SLJIT_MOV | SLJIT_MEM_STORE | SLJIT_MEM_ALIGNED_32, SLJIT_R0, SLJIT_MEM1(SLJIT_R1), -128 + 32);

	sljit_emit_mem(compiler, SLJIT_MOV | SLJIT_MEM_UNALIGNED, SLJIT_R0, SLJIT_MEM1(SLJIT_S0), 0);
	/* wbuf[11] */
	sljit_emit_mem(compiler, SLJIT_MOV | SLJIT_MEM_STORE | SLJIT_MEM_UNALIGNED, SLJIT_R0, SLJIT_MEM1(SLJIT_S0), 11 * sizeof(sljit_sw));
	/* wbuf[12] */
	sljit_emit_mem(compiler, SLJIT_MOV | SLJIT_MEM_STORE | SLJIT_MEM_UNALIGNED, SLJIT_S0, SLJIT_MEM1(SLJIT_S0), 12 * sizeof(sljit_sw));

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func2((sljit_sw)&wbuf, (sljit_sw)bbuf);

	FAILED(wbuf[1] != -73, "test77 case 1 failed\n");
	FAILED(wbuf[2] != (sljit_u8)-73, "test77 case 2 failed\n");
	FAILED(bbuf[1] != -73, "test77 case 3 failed\n");
	FAILED(wbuf[3] != -28531, "test77 case 4 failed\n");
	FAILED(wbuf[4] != (sljit_u16)-28531, "test77 case 5 failed\n");
	FAILED(cmp_u8(bbuf, 3, hbuf, sizeof(sljit_s16)) != 1, "test77 case 6 failed\n");
	FAILED(wbuf[5] != -38512, "test77 case 7 failed\n");
	FAILED(wbuf[6] != WCONST((sljit_u32)-38512, -38512), "test77 case 8 failed\n");
	FAILED(cmp_u8(bbuf, 5, ibuf, sizeof(sljit_s32)) != 1, "test77 case 9 failed\n");
	FAILED(wbuf[7] != -46870, "test77 case 10 failed\n");
	FAILED(cmp_u8(bbuf, 9, wbuf, sizeof(sljit_sw)) != 1, "test77 case 11 failed\n");
	FAILED(wbuf[8] != -38512, "test77 case 12 failed\n");
	FAILED(cmp_u8(bbuf, 18, ibuf, sizeof(sljit_s32)) != 1, "test77 case 13 failed\n");
	FAILED(wbuf[9] != -46870, "test77 case 14 failed\n");
	FAILED(cmp_u8(bbuf, 22, wbuf, sizeof(sljit_sw)) != 1, "test77 case 15 failed\n");
	FAILED(wbuf[10] != -46870, "test77 case 16 failed\n");
	FAILED(cmp_u8(bbuf, 32, wbuf, sizeof(sljit_sw)) != 1, "test77 case 17 failed\n");
	FAILED(wbuf[11] != -62945, "test77 case 18 failed\n");
	FAILED(wbuf[12] != (sljit_sw)&wbuf, "test77 case 19 failed\n");

	sljit_free_code(code.code, NULL);

	if (sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		for (i = 0; i < 40; i++)
			bbuf[i] = -3;

		for (i = 0; i < 5; i++)
			dbuf[i] = 0;

		dbuf[0] = 6897.75;
		sbuf[0] = -8812.25;
		copy_u8(bbuf, 1, sbuf, sizeof(sljit_f32));
		copy_u8(bbuf, 5, dbuf, sizeof(sljit_f64));
		copy_u8(bbuf, 14, sbuf, sizeof(sljit_f32));
		copy_u8(bbuf, 18, dbuf, sizeof(sljit_f64));
		copy_u8(bbuf, 28, dbuf, sizeof(sljit_f64));

		dbuf[0] = -18046.5;
		sbuf[0] = 3751.75;

		compiler = sljit_create_compiler(NULL, NULL);
		FAILED(!compiler, "cannot create compiler\n");

		sljit_emit_enter(compiler, 0, SLJIT_ARGS3(VOID, P, P, P), 1, 3, 1, 0, 0);

		sljit_emit_fmem(compiler, SLJIT_MOV_F32 | SLJIT_MEM_UNALIGNED, SLJIT_FR0, SLJIT_MEM0(), (sljit_sw)bbuf + 1);
		/* sbuf[1] */
		sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_f32), SLJIT_FR0, 0);

		sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S1), 0);
		/* bbuf[1] */
		sljit_emit_fmem(compiler, SLJIT_MOV_F32 | SLJIT_MEM_STORE | SLJIT_MEM_UNALIGNED, SLJIT_FR0, SLJIT_MEM0(), (sljit_sw)bbuf + 1);

		sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R0, 0, SLJIT_S2, 0, SLJIT_IMM, 100000);
#if (defined SLJIT_FPU_UNALIGNED && SLJIT_FPU_UNALIGNED)
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_R0), 100000 + 5);
#else /* !SLJIT_FPU_UNALIGNED */
		sljit_emit_fmem(compiler, SLJIT_MOV_F64 | SLJIT_MEM_UNALIGNED, SLJIT_FR0, SLJIT_MEM1(SLJIT_R0), 100000 + 5);
#endif /* SLJIT_FPU_UNALIGNED */
		/* dbuf[1] */
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64), SLJIT_FR0, 0);

		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S0), 0);
		/* bbuf[5] */
#if (defined SLJIT_FPU_UNALIGNED && SLJIT_FPU_UNALIGNED)
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), 100000 + 5, SLJIT_FR0, 0);
#else /* !SLJIT_FPU_UNALIGNED */
		sljit_emit_fmem(compiler, SLJIT_MOV_F64 | SLJIT_MEM_STORE | SLJIT_MEM_UNALIGNED, SLJIT_FR0, SLJIT_MEM1(SLJIT_R0), 100000 + 5);
#endif /* SLJIT_FPU_UNALIGNED */

		sljit_emit_fmem(compiler, SLJIT_MOV_F32 | SLJIT_MEM_ALIGNED_16, SLJIT_FR0, SLJIT_MEM1(SLJIT_S2), 14);
		/* sbuf[2] */
		sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_f32), SLJIT_FR0, 0);

		sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S1), 0);
		/* bbuf[14] */
		sljit_emit_fmem(compiler, SLJIT_MOV_F32 | SLJIT_MEM_STORE | SLJIT_MEM_ALIGNED_16, SLJIT_FR0, SLJIT_MEM1(SLJIT_S2), 14);

		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 18 >> 1);
		sljit_emit_fmem(compiler, SLJIT_MOV_F64 | SLJIT_MEM_ALIGNED_16, SLJIT_FR0, SLJIT_MEM2(SLJIT_S2, SLJIT_R0), 1);
		/* dbuf[2] */
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_f64), SLJIT_FR0, 0);

		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S0), 0);
		/* bbuf[18] */
		sljit_emit_fmem(compiler, SLJIT_MOV_F64 | SLJIT_MEM_STORE | SLJIT_MEM_ALIGNED_16, SLJIT_FR0, SLJIT_MEM2(SLJIT_S2, SLJIT_R0), 1);

		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S2, 0, SLJIT_IMM, 128);
		sljit_emit_fmem(compiler, SLJIT_MOV_F64 | SLJIT_MEM_ALIGNED_32, SLJIT_FR0, SLJIT_MEM1(SLJIT_R0), -128 + 28);
		/* dbuf[3] */
		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_f64), SLJIT_FR0, 0);

		sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S0), 0);
		/* bbuf[28] */
		sljit_emit_fmem(compiler, SLJIT_MOV_F64 | SLJIT_MEM_STORE | SLJIT_MEM_ALIGNED_32, SLJIT_FR0, SLJIT_MEM1(SLJIT_R0), -128 + 28);

		sljit_emit_fmem(compiler, SLJIT_MOV_F64 | SLJIT_MEM_ALIGNED_32, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 0);
		/* dbuf[4] */
		sljit_emit_fmem(compiler, SLJIT_MOV_F64 | SLJIT_MEM_STORE | SLJIT_MEM_ALIGNED_32, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_f64));

		sljit_emit_return_void(compiler);

		code.code = sljit_generate_code(compiler);
		CHECK(compiler);
		sljit_free_compiler(compiler);

		code.func3((sljit_sw)&dbuf, (sljit_sw)&sbuf, (sljit_sw)bbuf);

		FAILED(sbuf[1] != -8812.25, "test77 case 20 failed\n");
		FAILED(cmp_u8(bbuf, 1, sbuf, sizeof(sljit_f32)) != 1, "test77 case 21 failed\n");
		FAILED(dbuf[1] != 6897.75, "test77 case 22 failed\n");
		FAILED(cmp_u8(bbuf, 5, dbuf, sizeof(sljit_f64)) != 1, "test77 case 23 failed\n");
		FAILED(sbuf[2] != -8812.25, "test77 case 24 failed\n");
		FAILED(cmp_u8(bbuf, 14, sbuf, sizeof(sljit_f32)) != 1, "test77 case 25 failed\n");
		FAILED(dbuf[2] != 6897.75, "test77 case 26 failed\n");
		FAILED(cmp_u8(bbuf, 18, dbuf, sizeof(sljit_f64)) != 1, "test77 case 27 failed\n");
		FAILED(dbuf[3] != 6897.75, "test77 case 28 failed\n");
		FAILED(cmp_u8(bbuf, 28, dbuf, sizeof(sljit_f64)) != 1, "test77 case 29 failed\n");
		FAILED(dbuf[4] != -18046.5, "test77 case 30 failed\n");

		sljit_free_code(code.code, NULL);
	}

	successful_tests++;
}

static void test78(void)
{
	/* Test register register preservation in keep saveds mode. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_sw buf[6 + SLJIT_NUMBER_OF_REGISTERS];
	struct sljit_jump* jump;
	sljit_s32 i;

	if (verbose)
		printf("Run test78\n");

	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(VOID), SLJIT_NUMBER_OF_REGISTERS - 3, 3, 0, 0, 0);

	for (i = 0; i < SLJIT_NUMBER_OF_REGISTERS - 3; i++)
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R(i), 0, SLJIT_IMM, 8469 + 1805 * i);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, 3671);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, 2418);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, 1597);

	jump = sljit_emit_call(compiler, SLJIT_CALL_REG_ARG, SLJIT_ARGS4(VOID, W, W, W, W));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM0(), (sljit_sw)(buf + 6), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)&buf);
	/* buf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), 3 * sizeof(sljit_sw), SLJIT_S0, 0);
	/* buf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), 4 * sizeof(sljit_sw), SLJIT_S1, 0);
	/* buf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), 5 * sizeof(sljit_sw), SLJIT_S2, 0);

	for (i = 1; i < SLJIT_NUMBER_OF_REGISTERS - 3; i++)
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), (6 + i) * (sljit_sw)sizeof(sljit_sw), SLJIT_R(i), 0);

	sljit_emit_return_void(compiler);

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_enter(compiler, SLJIT_ENTER_REG_ARG | SLJIT_ENTER_KEEP(3), SLJIT_ARGS4(VOID, W_R, W_R, W_R, W_R), 4, 3, 0, 0, SLJIT_MAX_LOCAL_SIZE);
	sljit_set_context(compiler, SLJIT_ENTER_REG_ARG | SLJIT_ENTER_KEEP(3), SLJIT_ARGS4(VOID, W_R, W_R, W_R, W_R), 4, 3, 0, 0, SLJIT_MAX_LOCAL_SIZE);

	/* buf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM0(), (sljit_sw)(buf + 0), SLJIT_S0, 0);
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM0(), (sljit_sw)(buf + 1), SLJIT_S1, 0);
	/* buf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM0(), (sljit_sw)(buf + 2), SLJIT_S2, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, 6501);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, 7149);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, 5732);

	jump = sljit_emit_call(compiler, SLJIT_CALL_REG_ARG | SLJIT_CALL_RETURN, SLJIT_ARGS0(VOID));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_MEM0(), 0);

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_enter(compiler, SLJIT_ENTER_REG_ARG | SLJIT_ENTER_KEEP(3), SLJIT_ARGS0(VOID), 4, 3, 0, 0, SLJIT_MAX_LOCAL_SIZE / 2);
	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func0();

	FAILED(buf[0] != 3671, "test78 case 1 failed\n");
	FAILED(buf[1] != 2418, "test78 case 2 failed\n");
	FAILED(buf[2] != 1597, "test78 case 3 failed\n");
	FAILED(buf[3] != 6501, "test78 case 4 failed\n");
	FAILED(buf[4] != 7149, "test78 case 5 failed\n");
	FAILED(buf[5] != 5732, "test78 case 6 failed\n");

	for (i = 0; i < SLJIT_NUMBER_OF_REGISTERS - 3; i++) {
		FAILED(buf[6 + i] != 8469 + 1805 * i, "test78 case 7 failed\n");
	}

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test79(void)
{
	/* Test register pair movement. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_sw buf[33];
	sljit_s8 bbuf_start[66 + 8 /* for alignment */];
	sljit_s8 *bbuf = (sljit_s8 *)(((sljit_uw)bbuf_start + 7) & ~(sljit_uw)7);
	sljit_s32 i;

	if (verbose)
		printf("Run test79\n");

	FAILED(!compiler, "cannot create compiler\n");

	for (i = 0; i < 33; i++)
		buf[i] = -1;
	for (i = 0; i < 66 + 8; i++)
		bbuf_start[i] = -1;

	buf[0] = -5836;
	buf[1] = 3724;
	buf[2] = -9035;
	buf[3] = (sljit_sw)bbuf + 50 + 0xfff;

	copy_u8(bbuf, 1, buf, sizeof(sljit_sw));
	copy_u8(bbuf, 1 + sizeof(sljit_sw), buf + 1, sizeof(sljit_sw));
	copy_u8(bbuf, 34, buf + 2, sizeof(sljit_sw));
	copy_u8(bbuf, 34 + sizeof(sljit_sw), buf + 3, sizeof(sljit_sw));

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, W), 5, 5, 0, 0, 3 * sizeof(sljit_sw));

	sljit_emit_mem(compiler, SLJIT_MOV | SLJIT_MEM_LOAD, SLJIT_REG_PAIR(SLJIT_R0, SLJIT_R1), SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw));
	/* buf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_R0, 0);
	/* buf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 5814);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 7201);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R2, 0, SLJIT_S0, 0, SLJIT_IMM, 6 * sizeof(sljit_sw) + 77);
	/* buf[6-7] */
	sljit_emit_mem(compiler, SLJIT_MOV | SLJIT_MEM_STORE, SLJIT_REG_PAIR(SLJIT_R0, SLJIT_R1), SLJIT_MEM1(SLJIT_R2), -77);

	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_IMM, 36);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 9);
	sljit_emit_mem(compiler, SLJIT_MOV | SLJIT_MEM_LOAD, SLJIT_REG_PAIR(SLJIT_R0, SLJIT_R1), SLJIT_MEM2(SLJIT_R0, SLJIT_R1), 2);
	/* buf[8] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw), SLJIT_R0, 0);
	/* buf[9] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, sizeof(sljit_sw));
	sljit_emit_mem(compiler, SLJIT_MOV | SLJIT_MEM_LOAD, SLJIT_REG_PAIR(SLJIT_R1, SLJIT_R0), SLJIT_MEM2(SLJIT_R0, SLJIT_R1), 1);
	/* buf[10] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 10 * sizeof(sljit_sw), SLJIT_R1, 0);
	/* buf[11] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 11 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, -8402);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, 6257);
	sljit_emit_mem(compiler, SLJIT_MOV | SLJIT_MEM_STORE, SLJIT_REG_PAIR(SLJIT_R2, SLJIT_R3), SLJIT_MEM1(SLJIT_SP), 0);
	/* buf[12] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 12 * sizeof(sljit_sw), SLJIT_MEM1(SLJIT_SP), 0);
	/* buf[13] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 13 * sizeof(sljit_sw), SLJIT_MEM1(SLJIT_SP), sizeof(sljit_sw));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_sw), SLJIT_IMM, 6139);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), 2 * sizeof(sljit_sw), SLJIT_IMM, -7049);
	sljit_emit_mem(compiler, SLJIT_MOV | SLJIT_MEM_LOAD, SLJIT_REG_PAIR(SLJIT_R4, SLJIT_S4), SLJIT_MEM1(SLJIT_SP), sizeof(sljit_sw));
	/* buf[14] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 14 * sizeof(sljit_sw), SLJIT_R4, 0);
	/* buf[15] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 15 * sizeof(sljit_sw), SLJIT_S4, 0);

	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R2, 0, SLJIT_S0, 0, SLJIT_IMM, 0x7f404);
	sljit_emit_mem(compiler, SLJIT_MOV | SLJIT_MEM_LOAD, SLJIT_REG_PAIR(SLJIT_R2, SLJIT_S4), SLJIT_MEM1(SLJIT_R2), 0x7f404);
	/* buf[16] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 16 * sizeof(sljit_sw), SLJIT_R2, 0);
	/* buf[17] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 17 * sizeof(sljit_sw), SLJIT_S4, 0);

	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R1, 0, SLJIT_S0, 0, SLJIT_IMM, 0x7f400 - sizeof(sljit_sw));
	sljit_emit_mem(compiler, SLJIT_MOV | SLJIT_MEM_LOAD, SLJIT_REG_PAIR(SLJIT_S2, SLJIT_S3), SLJIT_MEM1(SLJIT_R1), 0x7f400);
	/* buf[18] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 18 * sizeof(sljit_sw), SLJIT_S2, 0);
	/* buf[19] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 19 * sizeof(sljit_sw), SLJIT_S3, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, 3065);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R4, 0, SLJIT_IMM, 7481);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_S2, 0, SLJIT_S0, 0, SLJIT_IMM, 0x7f7f0 + 20 * sizeof(sljit_sw));
	/* buf[20-21] */
	sljit_emit_mem(compiler, SLJIT_MOV | SLJIT_MEM_STORE, SLJIT_REG_PAIR(SLJIT_R3, SLJIT_R4), SLJIT_MEM1(SLJIT_S2), -0x7f7f0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, 3275);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S3, 0, SLJIT_IMM, -8714);
	/* buf[22-23] */
	sljit_emit_mem(compiler, SLJIT_MOV | SLJIT_MEM_STORE, SLJIT_REG_PAIR(SLJIT_S1, SLJIT_S3), SLJIT_MEM0(), (sljit_sw)(buf + 22));

	sljit_emit_mem(compiler, SLJIT_MOV | SLJIT_MEM_LOAD | SLJIT_MEM_UNALIGNED, SLJIT_REG_PAIR(SLJIT_R0, SLJIT_R1), SLJIT_MEM0(), (sljit_sw)bbuf + 1);
	/* buf[24] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 24 * sizeof(sljit_sw), SLJIT_R0, 0);
	/* buf[25] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 25 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R4, 0, SLJIT_IMM, 3724);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S4, 0, SLJIT_IMM, -9035);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)bbuf + 18 - 0x7f0f);
	/* bbuf[18], buf[18] + sizeof(sljit_sw) */
	sljit_emit_mem(compiler, SLJIT_MOV | SLJIT_MEM_STORE | SLJIT_MEM_ALIGNED_16, SLJIT_REG_PAIR(SLJIT_R4, SLJIT_S4), SLJIT_MEM1(SLJIT_R0), 0x7f0f);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, (sljit_sw)bbuf + 34 - 0xfff);
	sljit_emit_mem(compiler, SLJIT_MOV | SLJIT_MEM_LOAD | SLJIT_MEM_ALIGNED_16, SLJIT_REG_PAIR(SLJIT_S1, SLJIT_R0), SLJIT_MEM1(SLJIT_S1), 0xfff);
	/* buf[26] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 26 * sizeof(sljit_sw), SLJIT_S1, 0);
	/* buf[27] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 27 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, (sljit_sw)bbuf + 34);
	sljit_emit_mem(compiler, SLJIT_MOV | SLJIT_MEM_LOAD | SLJIT_MEM_UNALIGNED, SLJIT_REG_PAIR(SLJIT_S1, SLJIT_R0), SLJIT_MEM1(SLJIT_S1), 0);
	/* buf[28] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 28 * sizeof(sljit_sw), SLJIT_S1, 0);
	/* buf[29] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 29 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)bbuf + 1 + 0x8004);
	sljit_emit_mem(compiler, SLJIT_MOV | SLJIT_MEM_LOAD | SLJIT_MEM_UNALIGNED, SLJIT_REG_PAIR(SLJIT_S1, SLJIT_R0), SLJIT_MEM1(SLJIT_R0), -0x8004);
	/* buf[30] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 30 * sizeof(sljit_sw), SLJIT_S1, 0);
	/* buf[31] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 31 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, (sljit_sw)bbuf + 50 + 0xfff);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, -9035);
	/* bbuf[50], buf[50] + sizeof(sljit_sw) */
	sljit_emit_mem(compiler, SLJIT_MOV | SLJIT_MEM_STORE | SLJIT_MEM_ALIGNED_16, SLJIT_REG_PAIR(SLJIT_R2, SLJIT_R3), SLJIT_MEM1(SLJIT_R2), -0xfff);

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)buf);

	FAILED(buf[3] != (sljit_sw)bbuf + 50 + 0xfff, "test79 case 1 failed\n");
	FAILED(buf[4] != 3724, "test79 case 2 failed\n");
	FAILED(buf[5] != -9035, "test79 case 3 failed\n");
	FAILED(buf[6] != 5814, "test79 case 4 failed\n");
	FAILED(buf[7] != 7201, "test79 case 5 failed\n");
	FAILED(buf[8] != -5836, "test79 case 6 failed\n");
	FAILED(buf[9] != 3724, "test79 case 7 failed\n");
	FAILED(buf[10] != -9035, "test79 case 8 failed\n");
	FAILED(buf[11] != buf[3], "test79 case 9 failed\n");
	FAILED(buf[12] != -8402, "test79 case 10 failed\n");
	FAILED(buf[13] != 6257, "test79 case 11 failed\n");
	FAILED(buf[14] != 6139, "test79 case 12 failed\n");
	FAILED(buf[15] != -7049, "test79 case 13 failed\n");
	FAILED(buf[16] != -5836, "test79 case 14 failed\n");
	FAILED(buf[17] != 3724, "test79 case 15 failed\n");
	FAILED(buf[18] != 3724, "test79 case 16 failed\n");
	FAILED(buf[19] != -9035, "test79 case 17 failed\n");
	FAILED(buf[20] != 3065, "test79 case 18 failed\n");
	FAILED(buf[21] != 7481, "test79 case 19 failed\n");
	FAILED(buf[22] != 3275, "test79 case 20 failed\n");
	FAILED(buf[23] != -8714, "test79 case 21 failed\n");
	FAILED(buf[24] != -5836, "test79 case 22 failed\n");
	FAILED(buf[25] != 3724, "test79 case 23 failed\n");
	FAILED(cmp_u8(bbuf, 18, buf + 1, sizeof(sljit_sw)) != 1, "test79 case 24 failed\n");
	FAILED(cmp_u8(bbuf, 18 + sizeof(sljit_sw), buf + 2, sizeof(sljit_sw)) != 1, "test79 case 25 failed\n");
	FAILED(buf[26] != -9035, "test79 case 26 failed\n");
	FAILED(buf[27] != buf[3], "test79 case 27 failed\n");
	FAILED(buf[28] != -9035, "test79 case 28 failed\n");
	FAILED(buf[29] != buf[3], "test79 case 29 failed\n");
	FAILED(buf[30] != -5836, "test79 case 30 failed\n");
	FAILED(buf[31] != 3724, "test79 case 31 failed\n");
	FAILED(cmp_u8(bbuf, 50, buf + 3, sizeof(sljit_sw)) != 1, "test79 case 32 failed\n");
	FAILED(cmp_u8(bbuf, 50 + sizeof(sljit_sw), buf + 2, sizeof(sljit_sw)) != 1, "test79 case 33 failed\n");
	FAILED(buf[32] != -1, "test79 case 34 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test80(void)
{
	/* Test masked shift. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_sw buf[8];
	sljit_s32 ibuf[8];
	sljit_s32 i;

	if (verbose)
		printf("Run test80\n");

	FAILED(!compiler, "cannot create compiler\n");

	for (i = 0; i < 8; i++)
		buf[i] = -1;
	for (i = 0; i < 8; i++)
		ibuf[i] = -1;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(VOID, W, W), 5, 5, 0, 0, 2 * sizeof(sljit_sw));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x1234);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 8 * sizeof(sljit_sw) + 4);
	sljit_emit_op2(compiler, SLJIT_MSHL, SLJIT_R2, 0, SLJIT_R0, 0, SLJIT_R1, 0);
	/* buf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R2, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_IMM, 8 * sizeof(sljit_sw));
	/* buf[1] */
	sljit_emit_op2(compiler, SLJIT_MSHL, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R0, 0, SLJIT_MEM1(SLJIT_SP), 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 0x5678);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, 32 + 8);
	sljit_emit_op2(compiler, SLJIT_MSHL32, SLJIT_R2, 0, SLJIT_R0, 0, SLJIT_R1, 0);
	/* ibuf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_R2, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R3, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R4, 0, SLJIT_IMM, -2);
	/* ibuf[1] */
	sljit_emit_op2(compiler, SLJIT_MSHL32 | SLJIT_SET_Z, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_s32), SLJIT_R3, 0, SLJIT_R4, 0);
	/* buf[2] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_NOT_ZERO);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 8 * sizeof(sljit_sw) + 4);
	sljit_emit_op2(compiler, SLJIT_MLSHR | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_R1, 0, SLJIT_R2, 0);
	/* buf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_R0, 0);
	/* buf[4] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_NOT_ZERO);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_IMM, 0x5678);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_s32), SLJIT_IMM, -32);
	sljit_emit_op2(compiler, SLJIT_MLSHR32, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_s32));
	/* ibuf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_s32), SLJIT_MEM1(SLJIT_SP), 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 0x345678);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R1, 0, SLJIT_S1, 0, SLJIT_IMM, 0x123000 - 3 * sizeof(sljit_s32));
	/* ibuf[3] */
	sljit_emit_op2(compiler, SLJIT_MLSHR32, SLJIT_MEM1(SLJIT_R1), 0x123000, SLJIT_R0, 0, SLJIT_IMM, 32 + 4);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, -0x100);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 8 * sizeof(sljit_sw) + 4);
	sljit_emit_op2(compiler, SLJIT_MASHR, SLJIT_R1, 0, SLJIT_R3, 0, SLJIT_R2, 0);
	/* buf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_s32), SLJIT_IMM, -0x100);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R2, 0, SLJIT_IMM, -32 + 1);
	sljit_emit_op2(compiler, SLJIT_MASHR32, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_s32), SLJIT_MEM1(SLJIT_SP), sizeof(sljit_s32), SLJIT_R2, 0);
	/* ibuf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_s32), SLJIT_MEM1(SLJIT_SP), sizeof(sljit_s32));

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 5 * sizeof(sljit_s32), SLJIT_IMM, 0x7fffffff);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, -1);
	/* ibuf[5] */
	sljit_emit_op2(compiler, SLJIT_MLSHR32 | SLJIT_SET_Z, SLJIT_MEM1(SLJIT_S1), 5 * sizeof(sljit_s32), SLJIT_MEM1(SLJIT_S1), 5 * sizeof(sljit_s32), SLJIT_R0, 0);
	/* buf[6] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_NOT_ZERO);

#if (defined SLJIT_MASKED_SHIFT && SLJIT_MASKED_SHIFT)
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 12344321);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, (8 * sizeof(sljit_sw)) + 1);
	/* buf[7] */
	sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_R1, 0, SLJIT_R2, 0);
#endif /* SLJIT_MASKED_SHIFT */
#if (defined SLJIT_MASKED_SHIFT32 && SLJIT_MASKED_SHIFT32)
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, 24688643);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R2, 0, SLJIT_IMM, (8 * sizeof(sljit_s32)) + 1);
	/* ibuf[6] */
	sljit_emit_op2(compiler, SLJIT_LSHR32, SLJIT_MEM1(SLJIT_S1), 6 * sizeof(sljit_s32), SLJIT_R1, 0, SLJIT_R2, 0);
#endif /* SLJIT_MASKED_SHIFT32 */

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func2((sljit_sw)buf, (sljit_sw)ibuf);

	FAILED(buf[0] != 0x12340, "test80 case 1 failed\n");
	FAILED(buf[1] != 0x1234, "test80 case 2 failed\n");
	FAILED(ibuf[0] != 0x567800, "test80 case 3 failed\n");
	FAILED(ibuf[1] != (sljit_sw)1 << 30, "test80 case 4 failed\n");
	FAILED(buf[2] != 1, "test80 case 5 failed\n");
	FAILED(buf[3] != ((sljit_uw)-1 >> 4), "test80 case 6 failed\n");
	FAILED(buf[4] != 1, "test80 case 7 failed\n");
	FAILED(ibuf[2] != 0x5678, "test80 case 8 failed\n");
	FAILED(ibuf[3] != 0x34567, "test80 case 9 failed\n");
	FAILED(buf[5] != -0x10, "test80 case 10 failed\n");
	FAILED(ibuf[4] != -0x80, "test80 case 11 failed\n");
	FAILED(ibuf[5] != 0, "test80 case 12 failed\n");
	FAILED(buf[6] != 0, "test80 case 13 failed\n");
#if (defined SLJIT_MASKED_SHIFT && SLJIT_MASKED_SHIFT)
	FAILED(buf[7] != 24688642, "test80 case 14 failed\n");
#endif /* SLJIT_MASKED_SHIFT */
#if (defined SLJIT_MASKED_SHIFT32 && SLJIT_MASKED_SHIFT32)
	FAILED(ibuf[6] != 12344321, "test80 case 15 failed\n");
#endif /* SLJIT_MASKED_SHIFT32 */

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test81(void)
{
	/* Test return with floating point value. */
	executable_code code;
	struct sljit_compiler* compiler;
	struct sljit_jump* jump;
	sljit_f64 dbuf[2];
	sljit_f32 sbuf[2];

	if (verbose)
		printf("Run test81\n");

	if (!sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		if (verbose)
			printf("no fpu available, test81 skipped\n");
		successful_tests++;
		return;
	}

	/* Next test. */

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(F64, W), 0, 1, 3, 0, 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_return(compiler, SLJIT_MOV_F64, SLJIT_FR2, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	dbuf[0] = 35.125;
	FAILED(code.test81_f2((sljit_sw)dbuf) != 35.125, "test81 case 1 failed\n");

	sljit_free_code(code.code, NULL);

	/* Next test. */

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(F32, W), 0, 1, 1, 0, 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_RETURN_FREG, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_return(compiler, SLJIT_MOV_F32, SLJIT_RETURN_FREG, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	sbuf[0] = -9027.5;
	FAILED(code.test81_f1((sljit_sw)sbuf) != -9027.5, "test81 case 2 failed\n");

	sljit_free_code(code.code, NULL);

	/* Next test. */

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(F32, W), 0, 1, 1, 0, sizeof(sljit_f32));
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_return(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_SP), 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	sbuf[0] = -6.75;
	FAILED(code.test81_f1((sljit_sw)sbuf) != -6.75, "test81 case 3 failed\n");

	sljit_free_code(code.code, NULL);

	/* Next test. */

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(F64, W), 0, 1, 1, 0, 2 * sizeof(sljit_f64));
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_f64), SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_return(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_f64));

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	dbuf[0] = 45.125;
	FAILED(code.test81_f2((sljit_sw)dbuf) != 45.125, "test81 case 4 failed\n");

	sljit_free_code(code.code, NULL);

	/* Next test. */

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(VOID), 1, 0, 1, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)dbuf - 33);
	jump = sljit_emit_call(compiler, SLJIT_CALL_REG_ARG, SLJIT_ARGS1(F64, W));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)dbuf);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_f64), SLJIT_RETURN_FREG, 0);
	sljit_emit_return_void(compiler);

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_enter(compiler, SLJIT_ENTER_REG_ARG, SLJIT_ARGS1(F64, W_R), 1, 0, 1, 0, 0);
	sljit_emit_return(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), 33);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	dbuf[0] = 2571.75;
	dbuf[1] = 0;
	code.func0();
	FAILED(dbuf[1] != 2571.75, "test81 case 5 failed\n");

	sljit_free_code(code.code, NULL);

	/* Next test. */

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(VOID), 1, 0, 1, 0, 0);
	jump = sljit_emit_call(compiler, SLJIT_CALL_REG_ARG, SLJIT_ARGS0(F32));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)sbuf);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_f32), SLJIT_RETURN_FREG, 0);
	sljit_emit_return_void(compiler);

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_enter(compiler, SLJIT_ENTER_REG_ARG, SLJIT_ARGS0(F32), 0, 0, 1, 0, 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_RETURN_FREG, 0, SLJIT_MEM0(), (sljit_sw)sbuf);
	sljit_emit_return(compiler, SLJIT_MOV_F32, SLJIT_RETURN_FREG, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	sbuf[0] = 6310.25;
	sbuf[1] = 0;
	code.func0();
	FAILED(sbuf[1] != 6310.25, "test81 case 6 failed\n");

	sljit_free_code(code.code, NULL);

	successful_tests++;
}

static void test82(void)
{
	/* Test return_to operation. */
	executable_code code, code2;
	struct sljit_compiler* compiler;
	struct sljit_jump* jump;
	struct sljit_label* label;
	sljit_s32 i;
	sljit_sw buf[3];

	if (verbose)
		printf("Run test82\n");

	/* Next test. */

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 2, 1, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -7602);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_S0, 0);
	jump = sljit_emit_call(compiler, SLJIT_CALL, SLJIT_ARGS2(W, W, W));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM0(), 0);
	label = sljit_emit_label(compiler);
	/* buf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_RETURN_REG, 0);
	sljit_emit_op0(compiler, SLJIT_SKIP_FRAMES_BEFORE_RETURN);
	sljit_emit_return_void(compiler);

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(VOID, W_R, W_R), 2, 0, 0, 0, 256);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), 256 - sizeof(sljit_sw), SLJIT_IMM, -1);
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), sizeof(sljit_sw), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0, SLJIT_IMM, 8945);
	sljit_emit_return_to(compiler, SLJIT_MEM1(SLJIT_R1), 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);

	buf[0] = (sljit_sw)sljit_get_label_addr(label);
	buf[1] = 0;

	sljit_free_compiler(compiler);

	code.func1((sljit_sw)buf);
	FAILED(buf[0] != 8945, "test82 case 1 failed\n");
	FAILED(buf[1] != -7602, "test82 case 2 failed\n");

	sljit_free_code(code.code, NULL);

	/* Next test. */

	for (i = 0; i < 3; i++) {
		compiler = sljit_create_compiler(NULL, NULL);
		FAILED(!compiler, "cannot create compiler\n");

		sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 2, 1, 0, 0, 0);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 6032);
		jump = sljit_emit_call(compiler, SLJIT_CALL_REG_ARG, SLJIT_ARGS1(W, W));
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM0(), 0);
		label = sljit_emit_label(compiler);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)buf);
		/* buf[0] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), 0, SLJIT_RETURN_REG, 0);
		/* buf[2] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), 2 * sizeof(sljit_sw), SLJIT_S0, 0);
		sljit_emit_op0(compiler, SLJIT_SKIP_FRAMES_BEFORE_RETURN);
		sljit_emit_return_void(compiler);

		sljit_set_label(jump, sljit_emit_label(compiler));
		sljit_emit_enter(compiler, SLJIT_ENTER_REG_ARG | SLJIT_ENTER_KEEP(1), SLJIT_ARGS1(VOID, W_R), 2, i == 1 ? 2 : 1, 0, 0, SLJIT_MAX_LOCAL_SIZE);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_IMM, -1);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_sw), SLJIT_R0, 0);
		/* buf[1] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_MEM1(SLJIT_SP), sizeof(sljit_sw));
		if (i == 2)
			sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), 2 * sizeof(sljit_sw), SLJIT_MEM1(SLJIT_S0), 0);
		else
			sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S(i), 0, SLJIT_MEM1(SLJIT_S0), 0);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), SLJIT_MAX_LOCAL_SIZE - sizeof(sljit_sw), SLJIT_IMM, -1);
		if (i != 0)
			sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, -3890);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0, SLJIT_IMM, 7145);
		if (i == 2)
			sljit_emit_return_to(compiler, SLJIT_MEM1(SLJIT_SP), 2 * sizeof(sljit_sw));
		else
			sljit_emit_return_to(compiler, SLJIT_S(i), 0);

		code.code = sljit_generate_code(compiler);
		CHECK(compiler);

		buf[0] = (sljit_sw)sljit_get_label_addr(label);
		buf[1] = 0;
		buf[2] = 0;

		sljit_free_compiler(compiler);

		code.func1((sljit_sw)buf);
		FAILED(buf[0] != 7145, "test82 case 3 failed\n");
		FAILED(buf[1] != 6032, "test82 case 4 failed\n");
		if (i != 0)
			FAILED(buf[2] != -3890, "test82 case 5 failed\n");

		sljit_free_code(code.code, NULL);
	}

	/* Next test. */

	for (i = 0; i < 3; i++) {
		compiler = sljit_create_compiler(NULL, NULL);
		FAILED(!compiler, "cannot create compiler\n");

		sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P_R), 2, 1, 0, 0, 0);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_R0, 0);
		jump = sljit_emit_call(compiler, SLJIT_CALL, SLJIT_ARGS1(W, W));
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM0(), 0);
		label = sljit_emit_label(compiler);

		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)buf);
		/* buf[0] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_RETURN_REG, 0);
		sljit_emit_op0(compiler, SLJIT_SKIP_FRAMES_BEFORE_RETURN);
		sljit_emit_return_void(compiler);

		sljit_set_label(jump, sljit_emit_label(compiler));
		sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, W_R), 2, 1, 0, 0, (i == 0) ? 0 : (i == 1) ? 512 : 32768);
		/* buf[1] */
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), sizeof(sljit_sw), SLJIT_R0, 0);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, -1);
		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, 0x1000);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0, SLJIT_IMM, -4502);
		sljit_emit_return_to(compiler, SLJIT_MEM1(SLJIT_R1), -0x1000);

		code.code = sljit_generate_code(compiler);
		CHECK(compiler);

		buf[0] = (sljit_sw)sljit_get_label_addr(label);
		buf[1] = 0;

		sljit_free_compiler(compiler);

		code.func1((sljit_sw)buf);
		FAILED(buf[0] != -4502, "test82 case 6 failed\n");
		FAILED(buf[1] != (sljit_sw)buf, "test82 case 7 failed\n");

		sljit_free_code(code.code, NULL);
	}

	/* Next test. */

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

#if (defined SLJIT_CONFIG_X86_32 && SLJIT_CONFIG_X86_32)
	i = SLJIT_S2;
#else
	i = SLJIT_S(SLJIT_NUMBER_OF_SAVED_REGISTERS - 1);
#endif

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 2, SLJIT_NUMBER_OF_SAVED_REGISTERS, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, i, 0, SLJIT_IMM, 2 * sizeof(sljit_sw));
	jump = sljit_emit_call(compiler, SLJIT_CALL, SLJIT_ARGS0(W));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM0(), 0);
	label = sljit_emit_label(compiler);
	/* buf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM2(SLJIT_S0, i), 0, SLJIT_RETURN_REG, 0);
	sljit_emit_op0(compiler, SLJIT_SKIP_FRAMES_BEFORE_RETURN);
	sljit_emit_return_void(compiler);

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(VOID), 2, SLJIT_NUMBER_OF_SAVED_REGISTERS, 0, 0, 16);
	for (i = 0; i < SLJIT_NUMBER_OF_SAVED_REGISTERS; i++)
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S(i), 0, SLJIT_IMM, -1);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0, SLJIT_IMM, (sljit_sw)(buf + 3));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, -3);
	sljit_emit_return_to(compiler, SLJIT_MEM2(SLJIT_RETURN_REG, SLJIT_R1), SLJIT_WORD_SHIFT);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);

	buf[0] = (sljit_sw)sljit_get_label_addr(label);
	buf[1] = 0;
	buf[2] = 0;

	sljit_free_compiler(compiler);

	code.func1((sljit_sw)buf);
	FAILED(buf[2] != (sljit_sw)(buf + 3), "test82 case 8 failed\n");

	sljit_free_code(code.code, NULL);

	/* Next test. */

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(VOID, P_R, P), 2, SLJIT_NUMBER_OF_SAVED_REGISTERS, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_S0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, 586000);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, 392);
	sljit_emit_icall(compiler, SLJIT_CALL_REG_ARG, SLJIT_ARGS0(W), SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM0(), 0);
	label = sljit_emit_label(compiler);
	/* buf[0] */
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_S2), 0, SLJIT_S0, 0, SLJIT_S1, 0);
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S2), sizeof(sljit_sw), SLJIT_RETURN_REG, 0);
	sljit_emit_op0(compiler, SLJIT_SKIP_FRAMES_BEFORE_RETURN);
	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);

	buf[0] = (sljit_sw)sljit_get_label_addr(label);

	sljit_free_compiler(compiler);

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, SLJIT_ENTER_REG_ARG | SLJIT_ENTER_KEEP(2), SLJIT_ARGS0(VOID), 2, SLJIT_NUMBER_OF_SAVED_REGISTERS, 0, 0, 16);
	for (i = 2; i < SLJIT_NUMBER_OF_SAVED_REGISTERS; i++)
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S(i), 0, SLJIT_IMM, -1);
	/* buf[2] */
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM0(), (sljit_sw)(buf + 2), SLJIT_S0, 0, SLJIT_S1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S0, 0, SLJIT_IMM, 416000);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, 931);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0, SLJIT_IMM, 2906);
	sljit_emit_return_to(compiler, SLJIT_IMM, buf[0]);

	code2.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;

	code.func2(SLJIT_FUNC_ADDR(code2.func0), (sljit_sw)buf);
	FAILED(buf[0] != 416931, "test82 case 9 failed\n");
	FAILED(buf[1] != 2906, "test82 case 10 failed\n");
	FAILED(buf[2] != 586392, "test82 case 11 failed\n");

	sljit_free_code(code.code, NULL);
	sljit_free_code(code2.code, NULL);

	successful_tests++;
}

static void test83(void)
{
	/* Test rotate. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_sw buf[13];
	sljit_s32 ibuf[8];
	sljit_s32 i;
#ifdef SLJIT_PREF_SHIFT_REG
	sljit_s32 shift_reg = SLJIT_PREF_SHIFT_REG;
#else
	sljit_s32 shift_reg = SLJIT_R2;
#endif

	if (verbose)
		printf("Run test83\n");

	FAILED(!compiler, "cannot create compiler\n");

	for (i = 0; i < 13; i++)
		buf[i] = -1;
	for (i = 0; i < 8; i++)
		ibuf[i] = -1;

	ibuf[0] = 8;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(VOID, W, W), 5, 5, 0, 0, 2 * sizeof(sljit_sw));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, WCONST(0x1234567812345678, 0x12345678));
	sljit_emit_op1(compiler, SLJIT_MOV, shift_reg, 0, SLJIT_IMM, 12);
	sljit_emit_op2(compiler, SLJIT_ROTL, SLJIT_R0, 0, SLJIT_R0, 0, shift_reg, 0);
	/* buf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, WCONST(0xfedcba0987654321, 0x87654321));
	sljit_emit_op2(compiler, SLJIT_ROTL, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R4, 0, SLJIT_IMM, WCONST(0xfedcba0987654321, 0x87654321));
	sljit_emit_op2(compiler, SLJIT_ROTL, SLJIT_S2, 0, SLJIT_R4, 0, SLJIT_IMM, 0xffff00);
	/* buf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_S2, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, shift_reg, 0, SLJIT_IMM, -1);
	/* buf[3] */
	sljit_emit_op2(compiler, SLJIT_ROTL, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_IMM, WCONST(0x9876543210abcdef, 0x87654321), shift_reg, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_IMM, WCONST(0x9876543210abcdc0, 0x876543e0));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 4);
	sljit_emit_op2(compiler, SLJIT_ROTL, SLJIT_R0, 0, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), SLJIT_WORD_SHIFT, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw));
	/* buf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_sw), SLJIT_IMM, WCONST(0x1234567812345678, 0x12345678));
	sljit_emit_op2(compiler, SLJIT_ROTR, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_sw), SLJIT_IMM, 4);
	/* buf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_MEM1(SLJIT_SP), 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_sw), SLJIT_IMM, WCONST(0x1234567812345678, 0x12345678));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 20);
	sljit_emit_op2(compiler, SLJIT_ROTR, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_sw), SLJIT_R0, 0);
	/* buf[6] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_MEM1(SLJIT_SP), 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, WCONST(0x1234567887654341, 0x17654321));
	sljit_emit_op2(compiler, SLJIT_ROTR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R0, 0);
	/* buf[7] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 8 * sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, WCONST(0xfedcba0987654321, 0x87654321));
	/* buf[8] */
	sljit_emit_op2(compiler, SLJIT_ROTR, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), 0, SLJIT_R1, 0, SLJIT_IMM, 0xff00);

	sljit_emit_op1(compiler, SLJIT_MOV, shift_reg, 0, SLJIT_IMM, 0xffc0);
	sljit_emit_op2(compiler, SLJIT_ROTR, SLJIT_R1, 0, SLJIT_R1, 0, shift_reg, 0);
	/* buf[9] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, shift_reg, 0, SLJIT_IMM, -7834);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)0x87654321);
	sljit_emit_op2(compiler, SLJIT_ROTL32, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S1), 0);
	/* ibuf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_R0, 0);
	/* buf[10] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 10 * sizeof(sljit_sw), shift_reg, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R4, 0, SLJIT_IMM, (sljit_sw)0xabc89def);
	sljit_emit_op2(compiler, SLJIT_ROTL32, SLJIT_S4, 0, SLJIT_R4, 0, SLJIT_IMM, 0xfffe1);
	/* ibuf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_s32), SLJIT_S4, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R4, 0, SLJIT_IMM, (sljit_sw)0xabc89def);
	sljit_emit_op2(compiler, SLJIT_ROTL32, SLJIT_S4, 0, SLJIT_R4, 0, SLJIT_IMM, 0xfffe0);
	/* ibuf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_s32), SLJIT_S4, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, shift_reg, 0, SLJIT_IMM, -6512);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R4, 0, SLJIT_IMM, 0xfffe0);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_s32), SLJIT_IMM, (sljit_sw)0xabc89def);
	/* ibuf[3] */
	sljit_emit_op2(compiler, SLJIT_ROTL32, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_s32), SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_s32), SLJIT_R4, 0);
	/* buf[11] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 11 * sizeof(sljit_sw), shift_reg, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 30);
	sljit_emit_op2(compiler, SLJIT_ROTR32, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)0x87654321, SLJIT_R0, 0);
	/* ibuf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_s32), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_s32), SLJIT_IMM, (sljit_sw)0xfedccdef);
	sljit_emit_op2(compiler, SLJIT_ROTR32, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_s32), SLJIT_IMM, 4);
	/* ibuf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 5 * sizeof(sljit_s32), SLJIT_MEM1(SLJIT_SP), 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 6 * sizeof(sljit_s32), SLJIT_IMM, (sljit_sw)0x89abcdef);
	sljit_emit_op2(compiler, SLJIT_ROTR32, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S1), 6 * sizeof(sljit_s32), SLJIT_IMM, 0xfffe0);
	/* ibuf[6] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 6 * sizeof(sljit_s32), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, shift_reg, 0, SLJIT_IMM, -2647);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)0x89abcde0);
	/* ibuf[7] */
	sljit_emit_op2(compiler, SLJIT_ROTR32, SLJIT_MEM1(SLJIT_S1), 7 * sizeof(sljit_s32), SLJIT_R1, 0, SLJIT_R1, 0);
	/* buf[12] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 12 * sizeof(sljit_sw), shift_reg, 0);

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func2((sljit_sw)buf, (sljit_sw)ibuf);

	FAILED(buf[0] != WCONST(0x4567812345678123, 0x45678123), "test83 case 1 failed\n");
	FAILED(buf[1] != WCONST(0xfdb974130eca8643, 0xeca8643), "test83 case 2 failed\n");
	FAILED(buf[2] != WCONST(0xfedcba0987654321, 0x87654321), "test83 case 3 failed\n");
	FAILED(buf[3] != WCONST(0xcc3b2a190855e6f7, 0xc3b2a190), "test83 case 4 failed\n");
	FAILED(buf[4] != WCONST(0x9876543210abcdc0, 0x876543e0), "test83 case 5 failed\n");
	FAILED(buf[5] != WCONST(0x8123456781234567, 0x81234567), "test83 case 6 failed\n");
	FAILED(buf[6] != WCONST(0x4567812345678123, 0x45678123), "test83 case 7 failed\n");
	FAILED(buf[7] != WCONST(0x891a2b3c43b2a1a0, 0x8bb2a190), "test83 case 8 failed\n");
	FAILED(buf[8] != WCONST(0xfedcba0987654321, 0x87654321), "test83 case 9 failed\n");
	FAILED(buf[9] != WCONST(0xfedcba0987654321, 0x87654321), "test83 case 10 failed\n");
	FAILED(ibuf[0] != (sljit_s32)0x65432187, "test83 case 11 failed\n");
	FAILED(buf[10] != -7834, "test83 case 12 failed\n");
	FAILED(ibuf[1] != (sljit_s32)0x57913bdf, "test83 case 13 failed\n");
	FAILED(ibuf[2] != (sljit_s32)0xabc89def, "test83 case 14 failed\n");
	FAILED(ibuf[3] != (sljit_s32)0xabc89def, "test83 case 15 failed\n");
	FAILED(buf[11] != -6512, "test83 case 16 failed\n");
	FAILED(ibuf[4] != (sljit_s32)0x1d950c86, "test83 case 17 failed\n");
	FAILED(ibuf[5] != (sljit_s32)0xffedccde, "test83 case 18 failed\n");
	FAILED(ibuf[6] != (sljit_s32)0x89abcdef, "test83 case 19 failed\n");
	FAILED(ibuf[7] != (sljit_s32)0x89abcde0, "test83 case 20 failed\n");
	FAILED(buf[12] != -2647, "test83 case 21 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test84(void)
{
	/* Test "shift into". */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_sw buf[19];
	sljit_s32 ibuf[10];
	sljit_s32 i;
#ifdef SLJIT_PREF_SHIFT_REG
	sljit_s32 shift_reg = SLJIT_PREF_SHIFT_REG;
#else
	sljit_s32 shift_reg = SLJIT_R2;
#endif

	if (verbose)
		printf("Run test84\n");

	FAILED(!compiler, "cannot create compiler\n");

	for (i = 0; i < 19; i++)
		buf[i] = -1;
	for (i = 0; i < 10; i++)
		ibuf[i] = -1;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(VOID, W, W), 5, 5, 0, 0, 2 * sizeof(sljit_sw));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, WCONST(0x1234567812345678, 0x12345678));
	sljit_emit_op1(compiler, SLJIT_MOV, shift_reg, 0, SLJIT_IMM, 12);
	sljit_emit_shift_into(compiler, SLJIT_SHL, SLJIT_R0, SLJIT_R1, SLJIT_R1, shift_reg, 0);
	/* buf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, WCONST(0x1234567812345678, 0x12345678));
	sljit_emit_shift_into(compiler, SLJIT_MLSHR, SLJIT_R4, SLJIT_R3, SLJIT_R3, SLJIT_IMM, 0xffd4 /* 20 */);
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R4, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, shift_reg, 0, SLJIT_IMM, (sljit_s32)0x86421357);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_IMM, 0xffeb /* 11 */);
	sljit_emit_shift_into(compiler, SLJIT_MSHL32, SLJIT_R0, shift_reg, shift_reg, SLJIT_MEM1(SLJIT_SP), 0);
	/* ibuf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, shift_reg, 0, SLJIT_IMM, -8762);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R4, 0, SLJIT_IMM, (sljit_s32)0x89abcdef);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_s32), SLJIT_IMM, 0xffff);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R0, 0, SLJIT_S1, 0, SLJIT_IMM, 16 * sizeof(sljit_s32));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 17);
	sljit_emit_shift_into(compiler, SLJIT_MLSHR32, SLJIT_S2, SLJIT_R4, SLJIT_R4, SLJIT_MEM2(SLJIT_R0, SLJIT_R1), 2);
	/* ibuf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_s32), SLJIT_S2, 0);
	/* buf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), shift_reg, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S4, 0, SLJIT_IMM, WCONST(0x1234567812345678, 0x12345678));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, WCONST(0xabcd000000000000, 0xabcd0000));
	sljit_emit_shift_into(compiler, SLJIT_MSHL, SLJIT_S4, SLJIT_S4, SLJIT_R0, SLJIT_IMM, 12);
	/* buf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_S4, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, WCONST(0xaabbccddeeff8899, 0xabcdef89));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R4, 0, SLJIT_IMM, 0xfedcba);
	sljit_emit_shift_into(compiler, SLJIT_LSHR, SLJIT_R1, SLJIT_R0, SLJIT_R4, SLJIT_IMM, 19);
	/* buf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, WCONST(0xfedcba0987654321, 0xfedcba09));
	sljit_emit_op1(compiler, SLJIT_MOV, shift_reg, 0, SLJIT_IMM, WCONST(0x7fffffffffffffff, 0x7fffffff));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_IMM, 1);
	sljit_emit_shift_into(compiler, SLJIT_SHL | SLJIT_SHIFT_INTO_NON_ZERO, SLJIT_R4, SLJIT_R1, shift_reg, SLJIT_MEM1(SLJIT_SP), 0);
	/* buf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_R4, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)0xdeadbeaf);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R4, 0, SLJIT_IMM, (sljit_sw)0xfedcba09);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, -5);
	sljit_emit_shift_into(compiler, SLJIT_MLSHR32 | SLJIT_SHIFT_INTO_NON_ZERO, shift_reg, SLJIT_R1, SLJIT_R4, SLJIT_R0, 0);
	/* ibuf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_s32), shift_reg, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R3, 0, SLJIT_IMM, (sljit_sw)0xabcd6543);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R4, 0, SLJIT_IMM, (sljit_s32)0xc9000000);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 0xffe8);
	sljit_emit_shift_into(compiler, SLJIT_MSHL32, shift_reg, SLJIT_R3, SLJIT_R4, SLJIT_R0, 0);
	/* ibuf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_s32), shift_reg, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, shift_reg, 0, SLJIT_IMM, -6032);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R4, 0, SLJIT_IMM, 0x7cadcad7);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_S3, 0, SLJIT_IMM, (sljit_s32)0xfffffff5);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 3);
	sljit_emit_shift_into(compiler, SLJIT_LSHR32, SLJIT_R4, SLJIT_R4, SLJIT_S3, SLJIT_R0, 0);
	/* ibuf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_s32), SLJIT_R4, 0);
	/* buf[6] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), shift_reg, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -9740);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, -5182);
	sljit_emit_shift_into(compiler, SLJIT_SHL, SLJIT_R0, SLJIT_R0, SLJIT_R1, SLJIT_IMM, 0);
	/* buf[7] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_R0, 0);
	/* buf[8] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, -4072);
	sljit_emit_op1(compiler, SLJIT_MOV32, shift_reg, 0, SLJIT_IMM, -2813);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, 0);
	sljit_emit_shift_into(compiler, SLJIT_LSHR32, SLJIT_R0, SLJIT_R0, shift_reg, SLJIT_R1, 0);
	/* ibuf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 5 * sizeof(sljit_s32), SLJIT_R0, 0);
	/* ibuf[6] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 6 * sizeof(sljit_s32), shift_reg, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -3278);
	sljit_emit_op1(compiler, SLJIT_MOV, shift_reg, 0, SLJIT_IMM, 0);
	sljit_emit_shift_into(compiler, SLJIT_LSHR, SLJIT_R1, SLJIT_R0, SLJIT_R0, shift_reg, 0);
	/* buf[9] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S3, 0, SLJIT_IMM, WCONST(0x1234567890abcdef, 0x12345678));
	sljit_emit_shift_into(compiler, SLJIT_LSHR, SLJIT_R0, SLJIT_S3, SLJIT_S3, SLJIT_IMM, 0xfff8 /* 24/56 */);
	/* buf[10] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 10 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S3, 0, SLJIT_IMM, WCONST(0x1234567890abcdef, 0x12345678));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S4, 0, SLJIT_IMM, WCONST(0xba9876fedcba9800, 0xfedcba00));
	sljit_emit_shift_into(compiler, SLJIT_SHL, SLJIT_S3, SLJIT_S3, SLJIT_S4, SLJIT_IMM, 0xfff8 /* 24/56 */);
	/* buf[11] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 11 * sizeof(sljit_sw), SLJIT_S3, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, WCONST(0x1234567890abcdef, 0x12345678));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, -4986);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R4, 0, SLJIT_IMM, 0);
	sljit_emit_shift_into(compiler, SLJIT_SHL, SLJIT_R0, SLJIT_R0, SLJIT_R1, SLJIT_R4, 0);
	/* buf[12] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 12 * sizeof(sljit_sw), SLJIT_R0, 0);
	/* buf[13] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 13 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, WCONST(0x12345678fedcba09, 0x12348765));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 14 * sizeof(sljit_sw), SLJIT_IMM, -1);
	sljit_emit_shift_into(compiler, SLJIT_MLSHR, shift_reg, SLJIT_R0, SLJIT_R1, SLJIT_MEM1(SLJIT_S0), 14 * sizeof(sljit_sw));
	/* buf[14] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 14 * sizeof(sljit_sw), shift_reg, 0);
	/* buf[15] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 15 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, WCONST(0x8000000000000005, 0x80000005));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 16 * sizeof(sljit_sw), SLJIT_R1, 0);
	sljit_emit_shift_into(compiler, SLJIT_MSHL, SLJIT_R0, SLJIT_R0, SLJIT_R1, SLJIT_MEM0(), (sljit_sw)(buf + 16));
	/* buf[16] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 16 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 2);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, WCONST(0x2345678923456789, 0x23456789));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, -1);
	sljit_emit_shift_into(compiler, SLJIT_SHL, SLJIT_R0, SLJIT_R1, SLJIT_S2, SLJIT_R0, 0);
	/* buf[17] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 17 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)0xabc23456);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)0xef000000);
	sljit_emit_shift_into(compiler, SLJIT_SHL32, SLJIT_R0, SLJIT_R0, SLJIT_R1, SLJIT_IMM, 4);
	/* ibuf[7] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 7 * sizeof(sljit_s32), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)0xabc23456);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)0xfe);
	sljit_emit_shift_into(compiler, SLJIT_LSHR32, SLJIT_S2, SLJIT_R0, SLJIT_R1, SLJIT_IMM, 4);
	/* ibuf[8] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 8 * sizeof(sljit_s32), SLJIT_S2, 0);

#if (defined SLJIT_MASKED_SHIFT && SLJIT_MASKED_SHIFT)
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 12344321);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, (8 * sizeof(sljit_sw)) + 1);
	sljit_emit_shift_into(compiler, SLJIT_SHL, SLJIT_R0, SLJIT_R0, SLJIT_R1, SLJIT_R2, 0);
	/* buf[18] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 18 * sizeof(sljit_sw), SLJIT_R0, 0);
#endif /* SLJIT_MASKED_SHIFT */
#if (defined SLJIT_MASKED_SHIFT32 && SLJIT_MASKED_SHIFT32)
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 24688642);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, 1);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R2, 0, SLJIT_IMM, (8 * sizeof(sljit_s32)) + 1);
	sljit_emit_shift_into(compiler, SLJIT_LSHR32, SLJIT_R0, SLJIT_R0, SLJIT_R1, SLJIT_R2, 0);
	/* ibuf[9] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 9 * sizeof(sljit_s32), SLJIT_R0, 0);
#endif /* SLJIT_MASKED_SHIFT32 */

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func2((sljit_sw)buf, (sljit_sw)ibuf);

	FAILED(buf[0] != WCONST(0x4567812345678123, 0x45678123), "test84 case 1 failed\n");
	FAILED(buf[1] != WCONST(0x4567812345678123, 0x45678123), "test84 case 2 failed\n");
	FAILED(ibuf[0] != 0x109abc32, "test84 case 3 failed\n");
	FAILED(ibuf[1] != 0x13579bdf, "test84 case 4 failed\n");
	FAILED(buf[2] != -8762, "test84 case 5 failed\n");
	FAILED(buf[3] != WCONST(0x4567812345678abc, 0x45678abc), "test84 case 6 failed\n");
	FAILED(buf[4] != WCONST(0xdb975557799bbddf, 0xdb975579), "test84 case 7 failed\n");
	FAILED(buf[5] != WCONST(0xfdb974130eca8642, 0xfdb97412), "test84 case 8 failed\n");
	FAILED(ibuf[2] != (sljit_s32)0xdb97413b, "test84 case 9 failed\n");
	FAILED(ibuf[3] != (sljit_s32)0xcd6543c9, "test84 case 10 failed\n");
	FAILED(ibuf[4] != (sljit_s32)0xaf95b95a, "test84 case 11 failed\n");
	FAILED(buf[6] != -6032, "test84 case 12 failed\n");
	FAILED(buf[7] != -9740, "test84 case 13 failed\n");
	FAILED(buf[8] != -5182, "test84 case 14 failed\n");
	FAILED(ibuf[5] != -4072, "test84 case 15 failed\n");
	FAILED(ibuf[6] != -2813, "test84 case 16 failed\n");
	FAILED(buf[9] != -3278, "test84 case 17 failed\n");
	FAILED(buf[10] != WCONST(0x34567890abcdef12, 0x34567812), "test84 case 18 failed\n");
	FAILED(buf[11] != WCONST(0xefba9876fedcba98, 0x78fedcba), "test84 case 19 failed\n");
	FAILED(buf[12] != WCONST(0x1234567890abcdef, 0x12345678), "test84 case 20 failed\n");
	FAILED(buf[13] != -4986, "test84 case 21 failed\n");
	FAILED(buf[14] != WCONST(0x2468acf1fdb97413, 0x24690ecb), "test84 case 22 failed\n");
	FAILED(buf[15] != WCONST(0x12345678fedcba09, 0x12348765), "test84 case 23 failed\n");
	FAILED(buf[16] != 0x30, "test84 case 24 failed\n");
	FAILED(buf[17] != WCONST(0x8d159e248d159e27, 0x8d159e27), "test84 case 25 failed\n");
	FAILED(ibuf[7] != (sljit_s32)0xbc23456e, "test84 case 26 failed\n");
	FAILED(ibuf[8] != (sljit_s32)0xeabc2345, "test84 case 27 failed\n");
#if (defined SLJIT_MASKED_SHIFT && SLJIT_MASKED_SHIFT)
	FAILED(buf[18] != 24688643, "test84 case 28 failed\n");
#endif /* SLJIT_MASKED_SHIFT */
#if (defined SLJIT_MASKED_SHIFT32 && SLJIT_MASKED_SHIFT32)
	FAILED(ibuf[9] != (sljit_s32)-2135139327, "test84 case 29 failed\n");
#endif /* SLJIT_MASKED_SHIFT32 */

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test85(void)
{
	/* Test count trailing zeroes. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_sw buf[5];
	sljit_s32 ibuf[7];
	sljit_s32 i;

	if (verbose)
		printf("Run test85\n");

	FAILED(!compiler, "cannot create compiler\n");

	for (i = 0; i < 5; i++)
		buf[i] = -1;
	for (i = 0; i < 7; i++)
		ibuf[i] = -1;

	buf[2] = 0;
	ibuf[3] = 1;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(VOID, W, W), 5, 5, 0, 0, 2 * sizeof(sljit_sw));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0x80);
	/* buf[0] */
	sljit_emit_op1(compiler, SLJIT_CTZ, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_IMM, 0x654321);
	sljit_emit_op1(compiler, SLJIT_CTZ, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_SP), 0);
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, 2);
	sljit_emit_op1(compiler, SLJIT_CTZ, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_sw), SLJIT_MEM2(SLJIT_S0, SLJIT_S2), SLJIT_WORD_SHIFT);
	/* buf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_MEM1(SLJIT_SP), sizeof(sljit_sw));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R4, 0, SLJIT_IMM, (sljit_sw)1 << (8 * sizeof(sljit_sw) - 3));
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R1, 0, SLJIT_S0, 0, SLJIT_IMM, 0x100000);
	/* buf[3] */
	sljit_emit_op1(compiler, SLJIT_CTZ, SLJIT_MEM1(SLJIT_R1), 0x100000 + 3 * sizeof(sljit_sw), SLJIT_R4, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_IMM, WCONST(0xabcdef800, 0xcdef800));
	sljit_emit_op1(compiler, SLJIT_CTZ, SLJIT_S4, 0, SLJIT_MEM0(), (sljit_sw)(buf + 4));
	/* buf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_S4, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, 0xa400);
	sljit_emit_op2(compiler, SLJIT_ASHR32, SLJIT_R0, 0, SLJIT_R1, 0, SLJIT_IMM, 4);
	sljit_emit_op1(compiler, SLJIT_CTZ32, SLJIT_R1, 0, SLJIT_R0, 0);
	/* ibuf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_R0, 0);
	/* ibuf[1] */
	sljit_emit_op1(compiler, SLJIT_CTZ32, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_s32), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_s32), SLJIT_IMM, 0xbcdefe0);
	sljit_emit_op1(compiler, SLJIT_CTZ32, SLJIT_S4, 0, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_s32));
	/* ibuf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_s32), SLJIT_S4, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 3);
	sljit_emit_op1(compiler, SLJIT_CTZ32, SLJIT_R0, 0, SLJIT_MEM2(SLJIT_S1, SLJIT_R0), 2);
	/* ibuf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_s32), SLJIT_R0, 0);

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func2((sljit_sw)buf, (sljit_sw)ibuf);

	FAILED(buf[0] != 7, "test85 case 1 failed\n");
	FAILED(buf[1] != 0, "test85 case 2 failed\n");
	FAILED(buf[2] != WCONST(64, 32), "test85 case 3 failed\n");
	FAILED(buf[3] != WCONST(61, 29), "test85 case 4 failed\n");
	FAILED(buf[4] != 11, "test85 case 5 failed\n");
	FAILED(ibuf[0] != 6, "test85 case 6 failed\n");
	FAILED(ibuf[1] != 32, "test85 case 7 failed\n");
	FAILED(ibuf[2] != 5, "test85 case 8 failed\n");
	FAILED(ibuf[3] != 0, "test85 case 9 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test86(void)
{
	/* Test get return address. */
	executable_code code;
	struct sljit_compiler* compiler;
	struct sljit_jump *jump;
	struct sljit_label *label;
	sljit_uw return_addr = 0;
	sljit_uw buf[1];

	if (verbose)
		printf("Run test86\n");

	/* Next test. */

	buf[0] = 0;

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, W), 1, 1, 0, 0, 0);
	jump = sljit_emit_call(compiler, SLJIT_CALL, SLJIT_ARGS0(W));
	label = sljit_emit_label(compiler);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_RETURN_REG, 0);
	sljit_emit_return_void(compiler);

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(W), 1, 0, 0, 0, 0);
	sljit_emit_op_dst(compiler, SLJIT_GET_RETURN_ADDRESS, SLJIT_RETURN_REG, 0);
	sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	return_addr = sljit_get_label_addr(label);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)buf);

	FAILED(buf[0] != return_addr, "test86 case 1 failed\n");
	sljit_free_code(code.code, NULL);

	/* Next test. */

	buf[0] = 0;

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS0(VOID), 2, 0, 0, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, -1);
	jump = sljit_emit_call(compiler, SLJIT_CALL, SLJIT_ARGS2(VOID, W, W));
	label = sljit_emit_label(compiler);
	sljit_emit_return_void(compiler);

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(VOID, W, W), 1, SLJIT_NUMBER_OF_SAVED_REGISTERS - 2, 0, 0, SLJIT_MAX_LOCAL_SIZE);
	sljit_emit_op_dst(compiler, SLJIT_GET_RETURN_ADDRESS, SLJIT_MEM0(), (sljit_sw)buf);
	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	return_addr = sljit_get_label_addr(label);
	sljit_free_compiler(compiler);

	code.func0();

	FAILED(buf[0] != return_addr, "test86 case 2 failed\n");
	sljit_free_code(code.code, NULL);

	/* Next test. */

	buf[0] = 0;

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, W), 1, 3, 0, 0, 0);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_S2, 0, SLJIT_S0, 0, SLJIT_IMM, 16);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 8);
	jump = sljit_emit_call(compiler, SLJIT_CALL_REG_ARG, SLJIT_ARGS1(VOID, W));
	label = sljit_emit_label(compiler);
	sljit_emit_return_void(compiler);

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_enter(compiler, SLJIT_ENTER_REG_ARG | SLJIT_ENTER_KEEP(3), SLJIT_ARGS1(VOID, W_R), 1, SLJIT_NUMBER_OF_SAVED_REGISTERS, 0, 0, SLJIT_MAX_LOCAL_SIZE >> 1);
	sljit_emit_op_dst(compiler, SLJIT_GET_RETURN_ADDRESS, SLJIT_MEM2(SLJIT_S2, SLJIT_R0), 1);
	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	return_addr = sljit_get_label_addr(label);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)buf);

	FAILED(buf[0] != return_addr, "test86 case 3 failed\n");
	sljit_free_code(code.code, NULL);

	/* Next test. */

	buf[0] = 0;

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, W_R), 1, 0, 0, 0, 0);
	jump = sljit_emit_call(compiler, SLJIT_CALL_REG_ARG, SLJIT_ARGS1(VOID, W));
	label = sljit_emit_label(compiler);
	sljit_emit_return_void(compiler);

	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_enter(compiler, SLJIT_ENTER_REG_ARG, SLJIT_ARGS1(VOID, W_R), 1, SLJIT_NUMBER_OF_SAVED_REGISTERS >> 1, 0, 0, 64);
	sljit_emit_op_dst(compiler, SLJIT_GET_RETURN_ADDRESS, SLJIT_MEM1(SLJIT_SP), 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_MEM1(SLJIT_SP), 0);
	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	return_addr = sljit_get_label_addr(label);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)buf);

	FAILED(buf[0] != return_addr, "test86 case 4 failed\n");
	sljit_free_code(code.code, NULL);

	if (sljit_has_cpu_feature(SLJIT_HAS_FPU) && SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS > 0) {
		/* Next test. */

		buf[0] = 0;

		compiler = sljit_create_compiler(NULL, NULL);
		FAILED(!compiler, "cannot create compiler\n");

		sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, W), 1, 1, 0, 0, 0);
		jump = sljit_emit_call(compiler, SLJIT_CALL, SLJIT_ARGS0(W));
		label = sljit_emit_label(compiler);
		sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_RETURN_REG, 0);
		sljit_emit_return_void(compiler);

		sljit_set_label(jump, sljit_emit_label(compiler));
		sljit_emit_enter(compiler, 0, SLJIT_ARGS0(W), 1, 3, 0, SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS, 64);
		sljit_emit_op_dst(compiler, SLJIT_GET_RETURN_ADDRESS, SLJIT_RETURN_REG, 0);
		sljit_emit_return(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0);

		code.code = sljit_generate_code(compiler);
		CHECK(compiler);
		return_addr = sljit_get_label_addr(label);
		sljit_free_compiler(compiler);

		code.func1((sljit_sw)buf);

		FAILED(buf[0] != return_addr, "test86 case 5 failed\n");
		sljit_free_code(code.code, NULL);
	}

	successful_tests++;
}

static void test87(void)
{
	/* Test reverse bytes. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_sw buf[6];
	sljit_s32 ibuf[5];
	sljit_s32 i;

	if (verbose)
		printf("Run test87\n");

	FAILED(!compiler, "cannot create compiler\n");

	for (i = 0; i < 6; i++)
		buf[i] = -1;
	for (i = 0; i < 5; i++)
		ibuf[i] = -1;

	buf[3] = WCONST(0x8070605040302010, 0x40302010);
	ibuf[1] = (sljit_s32)0xffeeddcc;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(VOID, W, W), 5, 5, 0, 0, 2 * sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, WCONST(0xf1e2d3c4b5a69788, 0xf1e2d3c4));
	sljit_emit_op1(compiler, SLJIT_REV, SLJIT_R0, 0, SLJIT_R0, 0);
	/* buf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R4, 0, SLJIT_IMM, WCONST(0xffeeddccbbaa9988, 0xffeeddcc));
	sljit_emit_op1(compiler, SLJIT_REV, SLJIT_R2, 0, SLJIT_R4, 0);
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R2, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, WCONST(0x0102030405060708, 0x01020304));
	/* buf[2] */
	sljit_emit_op1(compiler, SLJIT_REV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_S2, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 3);
	sljit_emit_op1(compiler, SLJIT_REV, SLJIT_R4, 0, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), SLJIT_WORD_SHIFT);
	/* buf[3] */
	sljit_emit_op1(compiler, SLJIT_REV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_R4, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, WCONST(0x1122334455667788, 0x11223344));
	/* buf[4] */
	sljit_emit_op1(compiler, SLJIT_REV, SLJIT_MEM0(), (sljit_sw)&buf[4], SLJIT_S2, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 5 * sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, WCONST(0xaabbccddeeff0011, 0xaabbccdd));
	/* buf[5] */
	sljit_emit_op1(compiler, SLJIT_REV, SLJIT_MEM2(SLJIT_S0, SLJIT_R2), 0, SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, (sljit_s32)0xf1e2d3c4);
	sljit_emit_op1(compiler, SLJIT_REV32, SLJIT_R1, 0, SLJIT_R0, 0);
	/* ibuf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_R1, 0);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R2, 0, SLJIT_S1, 0, SLJIT_IMM, 0x12340 + sizeof(sljit_s32));
	sljit_emit_op1(compiler, SLJIT_REV32, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R2), -0x12340);
	/* ibuf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_s32), SLJIT_R2, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 2);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R4, 0, SLJIT_IMM, (sljit_s32)0x01020304);
	/* ibuf[2] */
	sljit_emit_op1(compiler, SLJIT_REV32, SLJIT_MEM2(SLJIT_S1, SLJIT_R0), 2, SLJIT_R4, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R4, 0, SLJIT_IMM, (sljit_s32)0x11223344);
	sljit_emit_op1(compiler, SLJIT_REV32, SLJIT_R4, 0, SLJIT_R4, 0);
	/* ibuf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_s32), SLJIT_R4, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_IMM, (sljit_s32)0xfeeddccb);
	/* ibuf[4] */
	sljit_emit_op1(compiler, SLJIT_REV32, SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_s32), SLJIT_MEM1(SLJIT_SP), 0);

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func2((sljit_sw)buf, (sljit_sw)ibuf);

	FAILED(buf[0] != WCONST(0x8897a6b5c4d3e2f1, 0xc4d3e2f1), "test87 case 1 failed\n");
	FAILED(buf[1] != WCONST(0x8899aabbccddeeff, 0xccddeeff), "test87 case 2 failed\n");
	FAILED(buf[2] != WCONST(0x0807060504030201, 0x04030201), "test87 case 3 failed\n");
	FAILED(buf[3] != WCONST(0x8070605040302010, 0x40302010), "test87 case 4 failed\n");
	FAILED(buf[4] != WCONST(0x8877665544332211, 0x44332211), "test87 case 5 failed\n");
	FAILED(buf[5] != WCONST(0x1100ffeeddccbbaa, 0xddccbbaa), "test87 case 6 failed\n");
	FAILED(ibuf[0] != (sljit_s32)0xc4d3e2f1, "test87 case 7 failed\n");
	FAILED(ibuf[1] != (sljit_s32)0xccddeeff, "test87 case 8 failed\n");
	FAILED(ibuf[2] != (sljit_s32)0x04030201, "test87 case 9 failed\n");
	FAILED(ibuf[3] != (sljit_s32)0x44332211, "test87 case 10 failed\n");
	FAILED(ibuf[4] != (sljit_s32)0xcbdcedfe, "test87 case 11 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test88(void)
{
	/* Test sljit_emit_fcopy. */
	executable_code code;
	struct sljit_compiler* compiler;
	sljit_f64 dbuf[4];
	sljit_f32 sbuf[2];
#if IS_64BIT
	sljit_sw wbuf[2];
	sljit_s32 ibuf[2];
#else /* !IS_64BIT */
	sljit_s32 ibuf[7];
#endif /* IS_64BIT */

	if (verbose)
		printf("Run test88\n");

	if (!sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		if (verbose)
			printf("no fpu available, test88 skipped\n");
		successful_tests++;
		return;
	}

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sbuf[0] = 12345.0;
	sbuf[1] = -1.0;
	ibuf[0] = -1;
	ibuf[1] = (sljit_s32)0xc7543100;
	dbuf[0] = 123456789012345.0;
	dbuf[1] = -1.0;
#if IS_64BIT
	wbuf[0] = -1;
	wbuf[1] = (sljit_sw)0xc2fee0c29f50cb10;
#else /* !IS_64BIT */
	ibuf[2] = -1;
	ibuf[3] = -1;
	ibuf[4] = -1;
	ibuf[5] = (sljit_sw)0x9f50cb10;
	ibuf[6] = (sljit_sw)0xc2fee0c2;
#endif /* IS_64BIT */

	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(VOID, W, W), 5, 5, 5, 0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)ibuf);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S1), 0);
	sljit_emit_fcopy(compiler, SLJIT_COPY32_FROM_F32, SLJIT_FR2, SLJIT_R0);
	/* ibuf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_R1), 0, SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R3, 0, SLJIT_MEM1(SLJIT_R1), sizeof(sljit_s32));
	sljit_emit_fcopy(compiler, SLJIT_COPY32_TO_F32, SLJIT_FR4, SLJIT_R3);
	/* sbuf[1] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_f32), SLJIT_FR4, 0);

#if IS_64BIT
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)wbuf);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fcopy(compiler, SLJIT_COPY_FROM_F64, SLJIT_FR1, SLJIT_S2);
	/* wbuf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), 0, SLJIT_S2, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_MEM1(SLJIT_R1), sizeof(sljit_sw));
	sljit_emit_fcopy(compiler, SLJIT_COPY_TO_F64, SLJIT_FR0, SLJIT_R3);
	/* dbuf[1] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64), SLJIT_FR0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 0);
	sljit_emit_fcopy(compiler, SLJIT_COPY_TO_F64, SLJIT_FR3, SLJIT_R2);
	/* dbuf[2] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_f64), SLJIT_FR3, 0);
#else /* !IS_64BIT */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fcopy(compiler, SLJIT_COPY_FROM_F64, SLJIT_FR1, SLJIT_REG_PAIR(SLJIT_S3, SLJIT_S2));
	/* ibuf[2-3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), 2 * sizeof(sljit_sw), SLJIT_S2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), 3 * sizeof(sljit_sw), SLJIT_S3, 0);

	sljit_emit_fcopy(compiler, SLJIT_COPY_FROM_F64, SLJIT_FR1, SLJIT_R2);
	/* ibuf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), 4 * sizeof(sljit_sw), SLJIT_R2, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_MEM1(SLJIT_R1), 5 * sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R1), 6 * sizeof(sljit_sw));
	sljit_emit_fcopy(compiler, SLJIT_COPY_TO_F64, SLJIT_FR0, SLJIT_REG_PAIR(SLJIT_R0, SLJIT_R3));
	/* dbuf[1] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64), SLJIT_FR0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 0);
	sljit_emit_fcopy(compiler, SLJIT_COPY_TO_F64, SLJIT_FR3, SLJIT_REG_PAIR(SLJIT_R2, SLJIT_R2));
	/* dbuf[2] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_f64), SLJIT_FR3, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, (sljit_sw)0xc00c0000);
	sljit_emit_fcopy(compiler, SLJIT_COPY_TO_F64, SLJIT_FR3, SLJIT_R2);
	/* dbuf[3] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_f64), SLJIT_FR3, 0);
#endif /* IS_64BIT */

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func2((sljit_sw)dbuf, (sljit_sw)sbuf);

	FAILED(ibuf[0] != (sljit_s32)0x4640e400, "test88 case 1 failed\n");
	FAILED(sbuf[1] != -54321.0, "test88 case 2 failed\n");
#if IS_64BIT
	FAILED(wbuf[0] != (sljit_sw)0x42dc12218377de40, "test88 case 3 failed\n");
	FAILED(dbuf[1] != -543210987654321.0, "test88 case 4 failed\n");
	FAILED(dbuf[2] != 0.0, "test88 case 5 failed\n");
#else /* !IS_64BIT */
	FAILED(ibuf[2] != (sljit_sw)0x8377de40, "test88 case 3 failed\n");
	FAILED(ibuf[3] != (sljit_sw)0x42dc1221, "test88 case 4 failed\n");
	FAILED(ibuf[4] != (sljit_sw)0x42dc1221, "test88 case 5 failed\n");
	FAILED(dbuf[1] != -543210987654321.0, "test88 case 6 failed\n");
	FAILED(dbuf[2] != 0.0, "test88 case 7 failed\n");
	FAILED(dbuf[3] != -3.5, "test88 case 8 failed\n");
#endif /* IS_64BIT */

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test89(void)
{
	/* Test reverse two bytes. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_sw buf[7];
	sljit_s32 ibuf[4];
	sljit_s16 hbuf[11];
	sljit_s32 i;

	if (verbose)
		printf("Run test89\n");

	FAILED(!compiler, "cannot create compiler\n");

	for (i = 0; i < 7; i++)
		buf[i] = -1;
	for (i = 0; i < 4; i++)
		ibuf[i] = -1;
	for (i = 0; i < 11; i++)
		hbuf[i] = -1;

	hbuf[0] = (sljit_s16)0x8c9d;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS3(VOID, W, W, W), 5, 5, 0, 0, 2 * sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0xeeabcd);
	sljit_emit_op1(compiler, SLJIT_REV_U16, SLJIT_R0, 0, SLJIT_R0, 0);
	/* buf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 0xeeabcd);
	sljit_emit_op1(compiler, SLJIT_REV_S16, SLJIT_R1, 0, SLJIT_R2, 0);
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, 0xeeedcb);
	sljit_emit_op1(compiler, SLJIT_REV_U16, SLJIT_R4, 0, SLJIT_R3, 0);
	/* buf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_R4, 0);

	/* hbuf[1] */
	sljit_emit_op1(compiler, SLJIT_REV_U16, SLJIT_MEM1(SLJIT_S2), sizeof(sljit_s16), SLJIT_MEM1(SLJIT_S2), 0);
	/* hbuf[2] */
	sljit_emit_op1(compiler, SLJIT_REV_S16, SLJIT_MEM1(SLJIT_S2), 2 * sizeof(sljit_s16), SLJIT_MEM1(SLJIT_S2), 0);

	sljit_emit_op1(compiler, SLJIT_MOV_U16, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_sw), SLJIT_IMM, 0xedcb);
	sljit_emit_op1(compiler, SLJIT_REV_U16, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_sw));
	/* buf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV_S16, SLJIT_MEM1(SLJIT_S2), 3 * sizeof(sljit_s16), SLJIT_IMM, 0xedcb);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 3);
	sljit_emit_op1(compiler, SLJIT_REV_S16, SLJIT_R1, 0, SLJIT_MEM2(SLJIT_S2, SLJIT_R1), 1);
	/* buf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV_U16, SLJIT_R2, 0, SLJIT_IMM, 0xd9c8);
	/* hbuf[3] */
	sljit_emit_op1(compiler, SLJIT_REV_S16, SLJIT_MEM1(SLJIT_S2), 3 * sizeof(sljit_s16), SLJIT_R2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_U16, SLJIT_R4, 0, SLJIT_R2, 0);
	sljit_emit_op1(compiler, SLJIT_REV_S16, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_s16), SLJIT_R4, 0);
	/* hbuf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV_U16, SLJIT_MEM1(SLJIT_S2), 4 * sizeof(sljit_s16), SLJIT_MEM1(SLJIT_SP), sizeof(sljit_s16));

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 0xeeb1c2);
	sljit_emit_op1(compiler, SLJIT_REV32_U16, SLJIT_R0, 0, SLJIT_R0, 0);
	/* ibuf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R2, 0, SLJIT_IMM, 0xeeb1c2);
	sljit_emit_op1(compiler, SLJIT_REV32_S16, SLJIT_R1, 0, SLJIT_R2, 0);
	/* ibuf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_s32), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R3, 0, SLJIT_IMM, 0xddbfae);
	sljit_emit_op1(compiler, SLJIT_REV32_S16, SLJIT_R4, 0, SLJIT_R3, 0);
	/* ibuf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_s32), SLJIT_R4, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32_U16, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_s32), SLJIT_IMM, 0xbfae);
	sljit_emit_op1(compiler, SLJIT_REV32_U16, SLJIT_R1, 0, SLJIT_MEM0(), (sljit_sw)(ibuf + 3));
	/* ibuf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_s32), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, 0x880102);
	sljit_emit_op1(compiler, SLJIT_REV32_U16, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_s32), SLJIT_R1, 0);
	/* hbuf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV_U16, SLJIT_MEM1(SLJIT_S2), 5 * sizeof(sljit_s16), SLJIT_MEM1(SLJIT_SP), sizeof(sljit_s32));

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R3, 0, SLJIT_IMM, 0x880102);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 6 * sizeof(sljit_s16));
	/* hbuf[6] */
	sljit_emit_op1(compiler, SLJIT_REV32_S16, SLJIT_MEM2(SLJIT_S2, SLJIT_R1), 0, SLJIT_R3, 0);
	/* hbuf[7] */
	hbuf[7] = -367;

#if IS_64BIT
	/* SLJIT_REV truncates memory store, source not sign extended 64bit */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)0xdeadbeef4444aa55);
	sljit_emit_op1(compiler, SLJIT_REV_U16, SLJIT_R1, 0, SLJIT_R0, 0);
	/* buf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_R1, 0);
	/* hbuf[8] */
	sljit_emit_op1(compiler, SLJIT_MOV_U16, SLJIT_MEM1(SLJIT_S2), 8 * sizeof(sljit_u16), SLJIT_R1, 0);
	/* hbuf[9] */
	sljit_emit_op1(compiler, SLJIT_REV_S16, SLJIT_MEM1(SLJIT_S2), 9 * sizeof(sljit_u16), SLJIT_R0, 0);
	/* hbuf[10] */
	hbuf[10] = -42;
	sljit_emit_op1(compiler, SLJIT_REV_S16, SLJIT_R0, 0, SLJIT_R0, 0);
	/* buf[6] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_R0, 0);
#endif /* IS_64BIT */

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func3((sljit_sw)buf, (sljit_sw)ibuf, (sljit_sw)hbuf);

	FAILED(buf[0] != 0xcdab, "test89 case 1 failed\n");
	FAILED(buf[1] != -0x3255, "test89 case 2 failed\n");
	FAILED(buf[2] != 0xcbed, "test89 case 3 failed\n");
	FAILED(hbuf[1] != (sljit_s16)0x9d8c, "test89 case 4 failed\n");
	FAILED(hbuf[2] != (sljit_s16)0x9d8c, "test89 case 5 failed\n");
	FAILED(buf[3] != 0xcbed, "test89 case 6 failed\n");
	FAILED(buf[4] != -0x3413, "test89 case 7 failed\n");
	FAILED(hbuf[3] != (sljit_s16)0xc8d9, "test89 case 8 failed\n");
	FAILED(hbuf[4] != (sljit_s16)0xc8d9, "test89 case 9 failed\n");
	FAILED(ibuf[0] != 0xc2b1, "test89 case 10 failed\n");
	FAILED(ibuf[1] != -0x3d4f, "test89 case 11 failed\n");
	FAILED(ibuf[2] != -0x5141, "test89 case 12 failed\n");
	FAILED(ibuf[3] != 0xaebf, "test89 case 13 failed\n");
	FAILED(hbuf[5] != (sljit_s16)0x0201, "test89 case 14 failed\n");
	FAILED(hbuf[6] != (sljit_s16)0x0201, "test89 case 15 failed\n");
	FAILED(hbuf[7] != -367, "test89 case 16 failed\n");
#if IS_64BIT
	FAILED(hbuf[8] != 0x55aa, "test89 case 17 failed\n");
	FAILED(buf[5] != hbuf[8], "test89 case 18 failed\n");
	FAILED(hbuf[9] != hbuf[8], "test89 case 19 failed\n");
	FAILED(hbuf[10] != -42, "test89 case 20 failed\n");
	FAILED(buf[6] != hbuf[9], "test89 case 21 failed\n");
#endif /* IS_64BIT */

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test90(void)
{
	/* Test reverse four bytes. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	sljit_sw buf[7];
	sljit_s32 ibuf[6];
	sljit_s32 i;

	if (verbose)
		printf("Run test90\n");

	FAILED(!compiler, "cannot create compiler\n");

	for (i = 0; i < 7; i++)
		buf[i] = -1;
	for (i = 0; i < 6; i++)
		ibuf[i] = -1;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(VOID, W, W), 5, 5, 0, 0, 2 * sizeof(sljit_sw));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, WCONST(0xffffa1b2c3d4, 0xa1b2c3d4));
	sljit_emit_op1(compiler, SLJIT_REV_U32, SLJIT_R0, 0, SLJIT_R0, 0);
	/* buf[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R0, 0);

	/* Sign extend negative integer. */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, WCONST(0xffffa1b2c3d4, 0xa1b2c3d4));
	sljit_emit_op1(compiler, SLJIT_REV_S32, SLJIT_R1, 0, SLJIT_R2, 0);
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R1, 0);

	/* Sign extend positive integer. */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, WCONST(0xffff1a2b3c4d,0x1a2b3c4d));
	sljit_emit_op1(compiler, SLJIT_REV_S32, SLJIT_R1, 0, SLJIT_R2, 0);
	/* buf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_s32), SLJIT_IMM, (sljit_s32)0xf9e8d7c6);
	sljit_emit_op1(compiler, SLJIT_REV_U32, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_s32));
	/* buf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_R2, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, sizeof(sljit_s32));
	sljit_emit_op1(compiler, SLJIT_REV_S32, SLJIT_S2, 0, SLJIT_MEM2(SLJIT_S1, SLJIT_R1), 0);
	/* buf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_S2, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_sw), SLJIT_IMM, (sljit_s32)0xaabbccdd);
	sljit_emit_op1(compiler, SLJIT_REV_U32, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_sw));
	/* buf[5] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_sw), SLJIT_R2, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_sw), SLJIT_IMM, (sljit_s32)0xaabbccdd);
	sljit_emit_op1(compiler, SLJIT_REV_S32, SLJIT_R4, 0, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_sw));
	/* buf[6] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_R4, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, WCONST(0xffff01020304, 0x01020304));
	/* ibuf[0] */
	sljit_emit_op1(compiler, SLJIT_REV_U32, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S4, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, 1);
	/* ibuf[1] */
	sljit_emit_op1(compiler, SLJIT_REV_S32, SLJIT_MEM2(SLJIT_S1, SLJIT_S2), 2, SLJIT_S4, 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_s32), SLJIT_IMM, (sljit_s32)0xf0e0d0c0);
	/* ibuf[2] */
	sljit_emit_op1(compiler, SLJIT_REV_U32, SLJIT_MEM0(), (sljit_sw)(ibuf + 2), SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_s32));

	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_S2, 0, SLJIT_S1, 0, SLJIT_IMM, WCONST(0x1234567890ab, 0x12345678) - 3 * sizeof(sljit_s32));
	sljit_emit_op1(compiler, SLJIT_REV_U32, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_s32), SLJIT_MEM1(SLJIT_S2), WCONST(0x1234567890ab, 0x12345678));
	/* ibuf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_s32), SLJIT_MEM1(SLJIT_SP), sizeof(sljit_s32));

	/* SLJIT_REV memory store truncates and does not overflow */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R4, 0, SLJIT_IMM, WCONST(0xffff8192a3b4, 0x8192a3b4));
	/* ibuf[4] */
	sljit_emit_op1(compiler, SLJIT_REV_S32, SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_s32), SLJIT_R4, 0);
	/* ibuf[5] */
	ibuf[5] = 0x55555555;

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func2((sljit_sw)buf, (sljit_sw)ibuf);

	FAILED(buf[0] != WCONST(0xd4c3b2a1, 0xd4c3b2a1), "test90 case 1 failed\n");
	FAILED(buf[1] != WCONST(-0x2b3c4d5f, 0xd4c3b2a1), "test90 case 2 failed\n");
	FAILED(buf[2] != 0x4d3c2b1a, "test90 case 3 failed\n");
	FAILED(buf[3] != WCONST(0xc6d7e8f9, 0xc6d7e8f9), "test90 case 4 failed\n");
	FAILED(buf[4] != WCONST(-0x39281707, 0xc6d7e8f9), "test90 case 5 failed\n");
	FAILED(buf[5] != WCONST(0xddccbbaa, 0xddccbbaa), "test90 case 6 failed\n");
	FAILED(buf[6] != WCONST(-0x22334456, 0xddccbbaa), "test90 case 7 failed\n");
	FAILED(ibuf[0] != 0x04030201, "test90 case 8 failed\n");
	FAILED(ibuf[1] != 0x04030201, "test90 case 9 failed\n");
	FAILED(ibuf[2] != (sljit_s32)0xc0d0e0f0, "test90 case 10 failed\n");
	FAILED(ibuf[3] != (sljit_s32)0xc0d0e0f0, "test90 case 11 failed\n");
	FAILED(ibuf[4] != (sljit_s32)0xb4a39281, "test90 case 12 failed\n");
	FAILED(ibuf[5] != 0x55555555, "test90 case 13 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test91(void)
{
	/* Test fselect operation. */
	executable_code code;
	struct sljit_compiler* compiler;
	sljit_f64 dbuf[10];
	sljit_f32 sbuf[10];
	sljit_s32 i;

	if (verbose)
		printf("Run test91\n");

	if (!sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		if (verbose)
			printf("no fpu available, test91 skipped\n");
		successful_tests++;
		return;
	}

	for (i = 4; i < 10; i++)
		dbuf[i] = -1.0;
	for (i = 4; i < 10; i++)
		sbuf[i] = -1.0;

	dbuf[0] = 759.25;
	dbuf[1] = -316.25;
	dbuf[2] = 591.5;
	dbuf[3] = -801.75;

	sbuf[0] = 630.5;
	sbuf[1] = -912.75;
	sbuf[2] = 264.25;
	sbuf[3] = -407.5;

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(VOID, W, W), 3, 3, 4, 0, 2 * sizeof(sljit_f64));

	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR3, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -1);
	sljit_emit_op2u(compiler, SLJIT_ADD | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_fselect(compiler, SLJIT_CARRY, SLJIT_FR2, SLJIT_FR3, 0, SLJIT_FR2);
	/* dbuf[4] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_f64), SLJIT_FR2, 0);

	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, (sljit_s32)0x80000000);
	sljit_emit_op2u(compiler, SLJIT_ADD32 | SLJIT_SET_OVERFLOW, SLJIT_R0, 0, SLJIT_IMM, -1);
	sljit_emit_fselect(compiler, SLJIT_OVERFLOW, SLJIT_FR2, SLJIT_FR2, 0, SLJIT_FR3);
	/* dbuf[5] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_f64), SLJIT_FR2, 0);

	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_CMP_F64 | SLJIT_SET_UNORDERED, SLJIT_FR2, 0, SLJIT_FR3, 0);
	sljit_emit_fselect(compiler, SLJIT_UNORDERED, SLJIT_FR3, SLJIT_MEM0(), (sljit_sw)(dbuf + 2), SLJIT_FR2);
	/* dbuf[6] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_f64), SLJIT_FR3, 0);
	sljit_emit_fselect(compiler, SLJIT_ORDERED, SLJIT_FR2, SLJIT_MEM0(), (sljit_sw)(dbuf + 2), SLJIT_FR2);
	/* dbuf[7] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_f64), SLJIT_FR2, 0);

	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_f64), SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_f64));
	sljit_emit_fop1(compiler, SLJIT_CMP_F64 | SLJIT_SET_F_GREATER, SLJIT_FR2, 0, SLJIT_FR2, 0);
	sljit_emit_fselect(compiler, SLJIT_F_LESS_EQUAL, SLJIT_FR0, SLJIT_MEM1(SLJIT_SP), sizeof(sljit_f64), SLJIT_FR0);
	/* dbuf[8] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_f64), SLJIT_FR0, 0);

	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_fselect(compiler, SLJIT_F_LESS_EQUAL, SLJIT_FR1, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), 3, SLJIT_FR1);
	/* dbuf[9] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_f64), SLJIT_FR1, 0);

	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S1), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_f32));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 10);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_IMM, 10);
	sljit_emit_fselect(compiler, SLJIT_EQUAL | SLJIT_32, SLJIT_FR0, SLJIT_FR1, 0, SLJIT_FR2);
	/* sbuf[4] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_f32), SLJIT_FR0, 0);
	sljit_emit_fselect(compiler, SLJIT_NOT_EQUAL | SLJIT_32, SLJIT_FR0, SLJIT_FR1, 0, SLJIT_FR2);
	/* sbuf[5] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S1), 5 * sizeof(sljit_f32), SLJIT_FR0, 0);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S1, 0, SLJIT_IMM, WCONST(0x1234000000, 0x123400) + 3 * sizeof(sljit_f32));
	sljit_emit_fop1(compiler, SLJIT_CMP_F32 | SLJIT_SET_ORDERED_GREATER, SLJIT_FR1, 0, SLJIT_FR2, 0);
	sljit_emit_fselect(compiler, SLJIT_ORDERED_GREATER | SLJIT_32, SLJIT_FR1, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_f32), SLJIT_FR2);
	/* sbuf[6] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S1), 6 * sizeof(sljit_f32), SLJIT_FR1, 0);
	sljit_emit_fselect(compiler, SLJIT_ORDERED_GREATER | SLJIT_32, SLJIT_FR2, SLJIT_MEM1(SLJIT_R1), WCONST(-0x1234000000, -0x123400), SLJIT_FR2);
	/* sbuf[7] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S1), 7 * sizeof(sljit_f32), SLJIT_FR2, 0);

	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR3, 0, SLJIT_MEM1(SLJIT_S1), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_f32));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -100);
	sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_SIG_LESS, SLJIT_R0, 0, SLJIT_IMM, 10);
	sljit_emit_fselect(compiler, SLJIT_SIG_LESS | SLJIT_32, SLJIT_FR2, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_FR3);
	/* sbuf[8] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S1), 8 * sizeof(sljit_f32), SLJIT_FR2, 0);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_S2, 0, SLJIT_S1, 0, SLJIT_IMM, -0x5678 + 2 * (sljit_s32)sizeof(sljit_f32));
	sljit_emit_fop1(compiler, SLJIT_CMP_F32 | SLJIT_SET_ORDERED_EQUAL, SLJIT_FR3, 0, SLJIT_FR3, 0);
	sljit_emit_fselect(compiler, SLJIT_ORDERED_EQUAL | SLJIT_32, SLJIT_FR3, SLJIT_MEM1(SLJIT_S2), 0x5678, SLJIT_FR3);
	/* sbuf[9] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S1), 9 * sizeof(sljit_f32), SLJIT_FR3, 0);

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func2((sljit_sw)dbuf, (sljit_sw)sbuf);

	FAILED(dbuf[4] != -316.25, "test91 case 1 failed\n");
	FAILED(dbuf[5] != 759.25, "test91 case 2 failed\n");
	FAILED(dbuf[6] != 759.25, "test91 case 3 failed\n");
	FAILED(dbuf[7] != 591.5, "test91 case 4 failed\n");
	FAILED(dbuf[8] != -801.75, "test91 case 5 failed\n");
	FAILED(dbuf[9] != -316.25, "test91 case 6 failed\n");
	FAILED(sbuf[4] != 630.5, "test91 case 7 failed\n");
	FAILED(sbuf[5] != -912.75, "test91 case 8 failed\n");
	FAILED(sbuf[6] != 264.25, "test91 case 9 failed\n");
	FAILED(sbuf[7] != -407.5, "test91 case 10 failed\n");
	FAILED(sbuf[8] != -912.75, "test91 case 11 failed\n");
	FAILED(sbuf[9] != 264.25, "test91 case 12 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test92(void)
{
#if (defined SLJIT_CONFIG_X86 && SLJIT_CONFIG_X86) \
		|| (defined SLJIT_CONFIG_ARM && SLJIT_CONFIG_ARM) \
		|| (defined SLJIT_CONFIG_S390X && SLJIT_CONFIG_S390X)
	/* Test atomic load and store. */
	executable_code code;
	struct sljit_compiler *compiler = sljit_create_compiler(NULL, NULL);
	struct sljit_label *label;
	struct sljit_jump *jump;
	sljit_sw buf[45];
	sljit_s32 i;

	if (verbose)
		printf("Run test92\n");

	FAILED(!compiler, "cannot create compiler\n");

	for (i = 1; i < 45; i++)
		buf[i] = WCONST(0x5555555555555555, 0x55555555);

	buf[0] = 4678;
	*(sljit_u8*)(buf + 2) = 78;
	*(sljit_u8*)(buf + 5) = 211;
	*(sljit_u16*)(buf + 9) = 17897;
	*(sljit_u16*)(buf + 12) = 57812;
	*(sljit_u32*)(buf + 15) = 1234567890;
	*(sljit_u32*)(buf + 17) = 987609876;
	buf[20] = (sljit_sw)buf;
	*(sljit_u8*)(buf + 22) = 192;
	*(sljit_u16*)(buf + 25) = 6359;
	((sljit_u8*)(buf + 28))[1] = 105;
	((sljit_u8*)(buf + 30))[2] = 13;
	((sljit_u16*)(buf + 33))[1] = 14876;
#if IS_64BIT
	((sljit_u8*)(buf + 35))[7] = 0x88;
	((sljit_u16*)(buf + 37))[3] = 0x1337;
	((sljit_s32*)(buf + 39))[1] = -1;
#endif /* IS_64BIT */
	buf[44] = WCONST(0x1122334444332211, 0x11222211);

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 5, 5, 0, 0, 2 * sizeof(sljit_sw));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_IMM, -9856);
	label = sljit_emit_label(compiler);
	sljit_emit_atomic_load(compiler, SLJIT_MOV, SLJIT_R1, SLJIT_S0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw));
	/* buf[0] */
	sljit_emit_atomic_store(compiler, SLJIT_MOV | SLJIT_SET_ATOMIC_STORED, SLJIT_R1, SLJIT_S0, SLJIT_R0);
	sljit_set_label(sljit_emit_jump(compiler, SLJIT_ATOMIC_NOT_STORED), label);
	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R2, 0);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S0, 0, SLJIT_IMM, 2 * sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_IMM, 203);
	label = sljit_emit_label(compiler);
	sljit_emit_atomic_load(compiler, SLJIT_MOV_U8, SLJIT_R2, SLJIT_R1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_R2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw));
	/* buf[2] */
	sljit_emit_atomic_store(compiler, SLJIT_MOV_U8 | SLJIT_SET_ATOMIC_STORED, SLJIT_R0, SLJIT_R1, SLJIT_R2);
	jump = sljit_emit_jump(compiler, SLJIT_ATOMIC_STORED);
	sljit_set_label(sljit_emit_jump(compiler, SLJIT_JUMP), label);
	sljit_set_label(jump, sljit_emit_label(compiler));
	/* buf[3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_sw), SLJIT_S2, 0);
	/* buf[4] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_sw), SLJIT_R0, 0);

	label = sljit_emit_label(compiler);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_IMM, 5 * sizeof(sljit_sw));
	sljit_emit_atomic_load(compiler, SLJIT_MOV32_U8, SLJIT_R0, SLJIT_R0);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_IMM, 5 * sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, 97);
	/* buf[5] */
	sljit_emit_atomic_store(compiler, SLJIT_MOV32_U8 | SLJIT_SET_ATOMIC_STORED, SLJIT_S1, SLJIT_R0, SLJIT_S2);
	sljit_set_label(sljit_emit_jump(compiler, SLJIT_ATOMIC_NOT_STORED), label);
	/* buf[6] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_sw), SLJIT_R1, 0);
	/* buf[7] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_sw), SLJIT_R0, 0);
	/* buf[8] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_sw), SLJIT_S1, 0);

	label = sljit_emit_label(compiler);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S0, 0, SLJIT_IMM, 9 * sizeof(sljit_sw));
	sljit_emit_atomic_load(compiler, SLJIT_MOV_U16, SLJIT_S2, SLJIT_R1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_S2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_S2, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_IMM, 9 * sizeof(sljit_sw));
	/* buf[9] */
	sljit_emit_atomic_store(compiler, SLJIT_MOV_U16 | SLJIT_SET_ATOMIC_STORED, SLJIT_R0, SLJIT_R0, SLJIT_R1);
	sljit_set_label(sljit_emit_jump(compiler, SLJIT_ATOMIC_NOT_STORED), label);
	/* buf[10] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 10 * sizeof(sljit_sw), SLJIT_R2, 0);
	/* buf[11] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 11 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S0, 0, SLJIT_IMM, 12 * sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 13 * sizeof(sljit_sw), SLJIT_IMM, 41306);
	label = sljit_emit_label(compiler);
	sljit_emit_atomic_load(compiler, SLJIT_MOV32_U16, SLJIT_R0, SLJIT_R1);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R2, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM0(), (sljit_sw)(buf + 13));
	/* buf[12] */
	sljit_emit_atomic_store(compiler, SLJIT_MOV32_U16 | SLJIT_SET_ATOMIC_STORED, SLJIT_R0, SLJIT_R1, SLJIT_R3);
	sljit_set_label(sljit_emit_jump(compiler, SLJIT_ATOMIC_NOT_STORED), label);
	/* buf[13] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S0), 13 * sizeof(sljit_sw), SLJIT_R2, 0);
	/* buf[14] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 14 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_S2, 0, SLJIT_S0, 0, SLJIT_IMM, 15 * sizeof(sljit_sw));
	label = sljit_emit_label(compiler);
	sljit_emit_atomic_load(compiler, SLJIT_MOV_U32, SLJIT_R2, SLJIT_S2);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_R2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S3, 0, SLJIT_R2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 987654321);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_S1, 0, SLJIT_S0, 0, SLJIT_IMM, 15 * sizeof(sljit_sw));
	/* buf[15] */
	sljit_emit_atomic_store(compiler, SLJIT_MOV_U32 | SLJIT_SET_ATOMIC_STORED, SLJIT_R1, SLJIT_S1, SLJIT_S3);
	sljit_set_label(sljit_emit_jump(compiler, SLJIT_ATOMIC_NOT_STORED), label);
	/* buf[16] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 16 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R2, 0, SLJIT_S0, 0, SLJIT_IMM, 17 * sizeof(sljit_sw));
	label = sljit_emit_label(compiler);
	sljit_emit_atomic_load(compiler, SLJIT_MOV32, SLJIT_R0, SLJIT_R2);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_S1, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -573621);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, 678906789);
	/* buf[17] */
	sljit_emit_atomic_store(compiler, SLJIT_MOV32 | SLJIT_SET_ATOMIC_STORED, SLJIT_R1, SLJIT_R2, SLJIT_S2);
	sljit_set_label(sljit_emit_jump(compiler, SLJIT_ATOMIC_NOT_STORED), label);
	/* buf[18] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S0), 18 * sizeof(sljit_sw), SLJIT_S1, 0);
	/* buf[19] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 19 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R2, 0, SLJIT_S0, 0, SLJIT_IMM, 20 * sizeof(sljit_sw));
	label = sljit_emit_label(compiler);
	sljit_emit_atomic_load(compiler, SLJIT_MOV_P, SLJIT_R0, SLJIT_R2);
	sljit_emit_op1(compiler, SLJIT_MOV_P, SLJIT_S1, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_P, SLJIT_R1, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV_P, SLJIT_R0, 0, SLJIT_R2, 0);
	/* buf[20] */
	sljit_emit_atomic_store(compiler, SLJIT_MOV_P | SLJIT_SET_ATOMIC_STORED, SLJIT_R0, SLJIT_R2, SLJIT_R1);
	sljit_set_label(sljit_emit_jump(compiler, SLJIT_ATOMIC_NOT_STORED), label);
	/* buf[21] */
	sljit_emit_op1(compiler, SLJIT_MOV_P, SLJIT_MEM1(SLJIT_S0), 21 * sizeof(sljit_sw), SLJIT_S1, 0);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S0, 0, SLJIT_IMM, 22 * sizeof(sljit_sw));
	label = sljit_emit_label(compiler);
	sljit_emit_atomic_load(compiler, SLJIT_MOV_U8, SLJIT_R3, SLJIT_R1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_R3, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R4, 0, SLJIT_R3, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, 240);
	/* buf[22] */
	sljit_emit_atomic_store(compiler, SLJIT_MOV_U8 | SLJIT_SET_ATOMIC_STORED, SLJIT_R3, SLJIT_R1, SLJIT_R4);
	sljit_set_label(sljit_emit_jump(compiler, SLJIT_ATOMIC_NOT_STORED), label);
	/* buf[23] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 23 * sizeof(sljit_sw), SLJIT_R2, 0);
	/* buf[24] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 24 * sizeof(sljit_sw), SLJIT_R3, 0);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_IMM, 25 * sizeof(sljit_sw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 25 * sizeof(sljit_sw), SLJIT_IMM, 6359);
	label = sljit_emit_label(compiler);
	sljit_emit_atomic_load(compiler, SLJIT_MOV, SLJIT_S3, SLJIT_R0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_S3, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S4, 0, SLJIT_S3, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S3, 0, SLJIT_IMM, 4059);
	/* buf[25] */
	sljit_emit_atomic_store(compiler, SLJIT_MOV | SLJIT_SET_ATOMIC_STORED, SLJIT_S3, SLJIT_R0, SLJIT_S4);
	sljit_set_label(sljit_emit_jump(compiler, SLJIT_ATOMIC_NOT_STORED), label);
	/* buf[26] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 26 * sizeof(sljit_sw), SLJIT_R1, 0);
	/* buf[27] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 27 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S0, 0, SLJIT_IMM, 28 * sizeof(sljit_sw) + 1);
	label = sljit_emit_label(compiler);
	sljit_emit_atomic_load(compiler, SLJIT_MOV_U8, SLJIT_R0, SLJIT_R1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 204);
	/* buf[28] */
	sljit_emit_atomic_store(compiler, SLJIT_MOV_U8 | SLJIT_SET_ATOMIC_STORED, SLJIT_R2, SLJIT_R1, SLJIT_R0);
	jump = sljit_emit_jump(compiler, SLJIT_ATOMIC_STORED);
	sljit_set_label(sljit_emit_jump(compiler, SLJIT_JUMP), label);
	sljit_set_label(jump, sljit_emit_label(compiler));
	/* buf[29] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 29 * sizeof(sljit_sw), SLJIT_S2, 0);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S0, 0, SLJIT_IMM, 30 * sizeof(sljit_sw) + 2);
	label = sljit_emit_label(compiler);
	sljit_emit_atomic_load(compiler, SLJIT_MOV_U8, SLJIT_R0, SLJIT_R1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 240);
	/* buf[30] */
	sljit_emit_atomic_store(compiler, SLJIT_MOV_U8 | SLJIT_SET_ATOMIC_STORED, SLJIT_R2, SLJIT_R1, SLJIT_R0);
	sljit_set_label(sljit_emit_jump(compiler, SLJIT_ATOMIC_NOT_STORED), label);
	/* buf[31] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 31 * sizeof(sljit_sw), SLJIT_S1, 0);
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_ATOMIC_NOT_STORED);
	/* buf[32] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 32 * sizeof(sljit_sw), SLJIT_R0, 0);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S0, 0, SLJIT_IMM, 33 * sizeof(sljit_sw) + 2);
	label = sljit_emit_label(compiler);
	sljit_emit_atomic_load(compiler, SLJIT_MOV_U16, SLJIT_R0, SLJIT_R1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 51403);
	/* buf[33] */
	sljit_emit_atomic_store(compiler, SLJIT_MOV_U16 | SLJIT_SET_ATOMIC_STORED, SLJIT_R2, SLJIT_R1, SLJIT_R0);
	sljit_set_label(sljit_emit_jump(compiler, SLJIT_ATOMIC_NOT_STORED), label);
	/* buf[34] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 34 * sizeof(sljit_sw), SLJIT_S1, 0);

#if IS_64BIT
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S0, 0, SLJIT_IMM, 35 * sizeof(sljit_sw) + 7 * sizeof(sljit_u8));
	label = sljit_emit_label(compiler);
	sljit_emit_atomic_load(compiler, SLJIT_MOV_U8, SLJIT_R0, SLJIT_R1);
	sljit_emit_op1(compiler, SLJIT_MOV_U8, SLJIT_S1, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 0xffa5a5a542);
	/* buf[35] */
	sljit_emit_atomic_store(compiler, SLJIT_MOV_U8 | SLJIT_SET_ATOMIC_STORED, SLJIT_R2, SLJIT_R1, SLJIT_R0);
	sljit_set_label(sljit_emit_jump(compiler, SLJIT_ATOMIC_NOT_STORED), label);
	/* buf[36] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 36 * sizeof(sljit_sw), SLJIT_S1, 0);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S0, 0, SLJIT_IMM, 37 * sizeof(sljit_sw) + 3 * sizeof(sljit_u16));
	label = sljit_emit_label(compiler);
	sljit_emit_atomic_load(compiler, SLJIT_MOV_U16, SLJIT_R0, SLJIT_R1);
	sljit_emit_op1(compiler, SLJIT_MOV_U16, SLJIT_S1, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 0xffa5a56942);
	/* buf[37] */
	sljit_emit_atomic_store(compiler, SLJIT_MOV_U16 | SLJIT_SET_ATOMIC_STORED, SLJIT_R2, SLJIT_R1, SLJIT_R0);
	sljit_set_label(sljit_emit_jump(compiler, SLJIT_ATOMIC_NOT_STORED), label);
	/* buf[38] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 38 * sizeof(sljit_sw), SLJIT_S1, 0);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S0, 0, SLJIT_IMM, 39 * sizeof(sljit_sw) + sizeof(sljit_u32));
	label = sljit_emit_label(compiler);
	sljit_emit_atomic_load(compiler, SLJIT_MOV_U32, SLJIT_R0, SLJIT_R1);
	sljit_emit_op1(compiler, SLJIT_MOV_U32, SLJIT_S1, 0, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 0xffffdeadbeef);
	/* buf[39] */
	sljit_emit_atomic_store(compiler, SLJIT_MOV_U32 | SLJIT_SET_ATOMIC_STORED, SLJIT_R2, SLJIT_R1, SLJIT_R0);
	sljit_set_label(sljit_emit_jump(compiler, SLJIT_ATOMIC_NOT_STORED), label);
	/* buf[40] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 40 * sizeof(sljit_sw), SLJIT_S1, 0);
#endif /* IS_64BIT */

	/* buf[41] */
	sljit_emit_op_flags(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 41 * sizeof(sljit_sw), SLJIT_ATOMIC_STORED);

	/* abandoned atomic load (byte) */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, 8);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S0, 0, SLJIT_IMM, 44 * sizeof(sljit_sw));
	sljit_emit_atomic_load(compiler, SLJIT_MOV_U8, SLJIT_R0, SLJIT_R1);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_R0, 0);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 1);
	label = sljit_emit_label(compiler);
	sljit_emit_atomic_load(compiler, SLJIT_MOV_U8, SLJIT_R0, SLJIT_R1);
	sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_Z, SLJIT_R3, 0, SLJIT_R3, 0, SLJIT_IMM, 1);
	jump = sljit_emit_jump(compiler, SLJIT_ZERO);
	/* buf[44] */
	sljit_emit_atomic_store(compiler, SLJIT_MOV_U8, SLJIT_R2, SLJIT_R1, SLJIT_R0);
	sljit_emit_mem(compiler, SLJIT_MOV_U8 | SLJIT_MEM_UNALIGNED, SLJIT_R4, SLJIT_MEM1(SLJIT_R1), 0); 
	sljit_set_label(sljit_emit_cmp(compiler, SLJIT_NOT_EQUAL, SLJIT_R4, 0, SLJIT_R2, 0), label);
	/* buf[43] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 43 * sizeof(sljit_sw), SLJIT_R2, 0);
	/* buf[42] */
	sljit_set_label(jump, sljit_emit_label(compiler));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 42 * sizeof(sljit_sw), SLJIT_R3, 0);

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);

	FAILED(buf[0] != -9856, "test92 case 1 failed\n");
	FAILED(buf[1] != 4678, "test92 case 2 failed\n");
	FAILED(*(sljit_u8*)(buf + 2) != 203, "test92 case 3 failed\n");
	FAILED(((sljit_u8*)(buf + 2))[1] != 0x55, "test92 case 4 failed\n");
	FAILED(buf[3] != 78, "test92 case 5 failed\n");
	FAILED(buf[4] != 203, "test92 case 6 failed\n");
	FAILED(*(sljit_u8*)(buf + 5) != 97, "test92 case 7 failed\n");
	FAILED(((sljit_u8*)(buf + 5))[1] != 0x55, "test92 case 8 failed\n");
	FAILED(*(sljit_u32*)(buf + 6) != 211, "test92 case 9 failed\n");
	FAILED(buf[7] != (sljit_sw)(buf + 5), "test92 case 10 failed\n");
	FAILED(buf[8] != 97, "test92 case 11 failed\n");
	FAILED(*(sljit_u16*)(buf + 9) != (sljit_u16)(sljit_sw)(buf + 9), "test92 case 12 failed\n");
	FAILED(((sljit_u8*)(buf + 9))[2] != 0x55, "test92 case 13 failed\n");
	FAILED(buf[10] != 17897, "test92 case 14 failed\n");
	FAILED(buf[11] != (sljit_sw)(buf + 9), "test92 case 15 failed\n");
	FAILED(*(sljit_u16*)(buf + 12) != 41306, "test92 case 16 failed\n");
	FAILED(((sljit_u8*)(buf + 12))[2] != 0x55, "test92 case 17 failed\n");
	FAILED(*(sljit_u32*)(buf + 13) != 57812, "test92 case 18 failed\n");
	FAILED(buf[14] != 41306, "test92 case 19 failed\n");
	FAILED(*(sljit_u32*)(buf + 15) != 987654321, "test92 case 20 failed\n");
#if IS_64BIT
	FAILED(((sljit_u8*)(buf + 15))[4] != 0x55, "test92 case 21 failed\n");
#endif /* IS_64BIT */
	FAILED(buf[16] != 1234567890, "test92 case 22 failed\n");
	FAILED(*(sljit_u32*)(buf + 17) != 678906789, "test92 case 23 failed\n");
#if IS_64BIT
	FAILED(((sljit_u8*)(buf + 17))[4] != 0x55, "test92 case 24 failed\n");
#endif /* IS_64BIT */
	FAILED(*(sljit_u32*)(buf + 18) != 987609876, "test92 case 25 failed\n");
#if IS_64BIT
	FAILED(((sljit_u8*)(buf + 18))[4] != 0x55, "test92 case 26 failed\n");
#endif /* IS_64BIT */
	FAILED(buf[19] != -573621, "test92 case 27 failed\n");
	FAILED(buf[20] != (sljit_sw)(buf + 20), "test 92 case 28 failed\n");
	FAILED(buf[21] != (sljit_sw)buf, "test 92 case 29 failed\n");
	FAILED(*(sljit_u8*)(buf + 22) != 240, "test92 case 30 failed\n");
	FAILED(((sljit_u8*)(buf + 22))[1] != 0x55, "test92 case 31 failed\n");
	FAILED(buf[23] != 192, "test92 case 32 failed\n");
	FAILED(buf[24] != 240, "test92 case 33 failed\n");
	FAILED(buf[25] != 4059, "test92 case 34 failed\n");
	FAILED(buf[26] != 6359, "test92 case 35 failed\n");
	FAILED(buf[27] != (sljit_sw)(buf + 25), "test92 case 36 failed\n");
	FAILED(((sljit_u8*)(buf + 28))[0] != 0x55, "test92 case 37 failed\n");
	FAILED(((sljit_u8*)(buf + 28))[1] != 204, "test92 case 38 failed\n");
	FAILED(((sljit_u8*)(buf + 28))[2] != 0x55, "test92 case 39 failed\n");
	FAILED(buf[29] != 105, "test92 case 40 failed\n");
	FAILED(((sljit_u8*)(buf + 30))[1] != 0x55, "test92 case 41 failed\n");
	FAILED(((sljit_u8*)(buf + 30))[2] != 240, "test92 case 42 failed\n");
	FAILED(((sljit_u8*)(buf + 30))[3] != 0x55, "test92 case 43 failed\n");
	FAILED(buf[31] != 13, "test92 case 44 failed\n");
	FAILED(buf[32] != 0, "test92 case 45 failed\n");
	FAILED(((sljit_u16*)(buf + 33))[0] != 0x5555, "test92 case 46 failed\n");
	FAILED(((sljit_u16*)(buf + 33))[1] != 51403, "test92 case 47 failed\n");
	FAILED(buf[34] != 14876, "test92 case 48 failed\n");
#if IS_64BIT
	FAILED(((sljit_u8*)(buf + 35))[7] != 0x42, "test92 case 49 failed\n");
	FAILED(buf[36] != 0x88, "test92 case 50 failed\n");
	FAILED(((sljit_u16*)(buf + 37))[3] != 0x6942, "test92 case 51 failed\n");
	FAILED(buf[38] != 0x1337, "test92 case 52 failed\n");
	FAILED(((sljit_u32*)(buf + 39))[0] != 0x55555555, "test92 case 53 failed\n");
	FAILED(((sljit_u32*)(buf + 39))[1] != 0xdeadbeef, "test92 case 54 failed\n");
	FAILED(buf[40] != 0xffffffff, "test92 case 55 failed\n");
#endif /* IS_64BIT */
	FAILED(buf[41] != 1, "test92 case 56 failed\n");
	FAILED(!buf[42], "test92 case 57 failed\n");
	FAILED(buf[43] != 0x11, "test92 case 58 failed\n");
	FAILED(((sljit_u8*)(buf + 44))[1] != buf[43], "test92 case 59 failed\n");

	sljit_free_code(code.code, NULL);
#endif
	successful_tests++;
}

static void test93(void)
{
	/* Floating point set immediate. */
	executable_code code;
	struct sljit_compiler* compiler;
	sljit_f64 dbuf[6];
	sljit_f32 sbuf[5];
	sljit_s32 check_buf[2];
	sljit_s32 i;

	if (verbose)
		printf("Run test93\n");

	if (!sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		if (verbose)
			printf("no fpu available, test93 skipped\n");
		successful_tests++;
		return;
	}

	for (i = 0; i < 6; i++)
		dbuf[i] = -1.0;

	for (i = 0; i < 5; i++)
		sbuf[i] = -1.0;

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(VOID, P, P), 2, 2, 4, 0, 0);

	sljit_emit_fset64(compiler, SLJIT_FR0, 0.0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_FR0, 0);
	sljit_emit_fset64(compiler, SLJIT_FR1, -0.0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64), SLJIT_FR1, 0);
	sljit_emit_fset64(compiler, SLJIT_FR2, 1.0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_f64), SLJIT_FR2, 0);
	sljit_emit_fset64(compiler, SLJIT_FR2, -31.0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_f64), SLJIT_FR2, 0);
	sljit_emit_fset64(compiler, SLJIT_FR2, 545357837627392.0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_f64), SLJIT_FR2, 0);
	sljit_emit_fset64(compiler, SLJIT_FR0, 983752153845214.5);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_f64), SLJIT_FR0, 0);

	sljit_emit_fset32(compiler, SLJIT_FR0, 0.0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_FR0, 0);
	sljit_emit_fset32(compiler, SLJIT_FR1, -0.0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_f32), SLJIT_FR1, 0);
	sljit_emit_fset32(compiler, SLJIT_FR2, 1.0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_f32), SLJIT_FR2, 0);
	sljit_emit_fset32(compiler, SLJIT_FR2, 31.0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_f32), SLJIT_FR2, 0);
	sljit_emit_fset32(compiler, SLJIT_FR2, -811.5);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_f32), SLJIT_FR2, 0);

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func2((sljit_sw)&dbuf, (sljit_sw)&sbuf);

	copy_u8(check_buf, 0, dbuf + 0, sizeof(sljit_f64));
	FAILED(check_buf[0] != 0, "test93 case 1 failed\n");
	FAILED(check_buf[1] != 0, "test93 case 2 failed\n");
	copy_u8(check_buf, 0, dbuf + 1, sizeof(sljit_f64));
#if (defined SLJIT_LITTLE_ENDIAN && SLJIT_LITTLE_ENDIAN)
	FAILED(check_buf[0] != 0, "test93 case 3 failed\n");
	FAILED(check_buf[1] != (sljit_s32)0x80000000, "test93 case 4 failed\n");
#else /* !SLJIT_LITTLE_ENDIAN */
	FAILED(check_buf[1] != 0, "test93 case 3 failed\n");
	FAILED(check_buf[0] != (sljit_s32)0x80000000, "test93 case 4 failed\n");
#endif /* SLJIT_LITTLE_ENDIAN */
	FAILED(dbuf[2] != 1.0, "test93 case 5 failed\n");
	FAILED(dbuf[3] != -31.0, "test93 case 6 failed\n");
	FAILED(dbuf[4] != 545357837627392.0, "test93 case 7 failed\n");
	FAILED(dbuf[5] != 983752153845214.5, "test93 case 8 failed\n");

	copy_u8(check_buf, 0, sbuf + 0, sizeof(sljit_f32));
	FAILED(check_buf[0] != 0, "test93 case 9 failed\n");
	copy_u8(check_buf, 0, sbuf + 1, sizeof(sljit_f32));
	FAILED(check_buf[0] != (sljit_s32)0x80000000, "test93 case 10 failed\n");
	FAILED(sbuf[2] != 1.0, "test93 case 11 failed\n");
	FAILED(sbuf[3] != 31.0, "test93 case 12 failed\n");
	FAILED(sbuf[4] != -811.5, "test93 case 13 failed\n");

	successful_tests++;
}

static void test94(void)
{
	/* Floating point convert from unsigned. */
	executable_code code;
	struct sljit_compiler* compiler;
	sljit_f64 dbuf[9];
	sljit_f32 sbuf[9];
	sljit_s32 i;
	sljit_sw value1 = WCONST(0xfffffffffffff800, 0xffffff00);
	sljit_sw value2 = WCONST(0x8000000000000801, 0x80000101);

	union {
		sljit_f64 value;
#if (defined SLJIT_LITTLE_ENDIAN && SLJIT_LITTLE_ENDIAN)
		struct {
			sljit_u32 low;
			sljit_u32 high;
		} bin;
#else /* !SLJIT_LITTLE_ENDIAN */
		struct {
			sljit_u32 high;
			sljit_u32 low;
		} bin;
#endif /* SLJIT_LITTLE_ENDIAN */
	} f64_check;

	union {
		sljit_f32 value;
		sljit_u32 bin;
	} f32_check;

	if (verbose)
		printf("Run test94\n");

	if (!sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		if (verbose)
			printf("no fpu available, test94 skipped\n");
		successful_tests++;
		return;
	}

	for (i = 0; i < 9; i++)
		dbuf[i] = -1.0;

	for (i = 0; i < 9; i++)
		sbuf[i] = -1.0;

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(VOID, P, P), 4, 4, 4, 0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0x7fffffff);
	/* dbuf[0] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_U32, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_R1, 0);
	/* sbuf[0] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F32_FROM_U32, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_R1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1);
	/* dbuf[1] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_U32, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), 3, SLJIT_IMM, (sljit_sw)0xfff00000);
	/* sbuf[1] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F32_FROM_U32, SLJIT_MEM2(SLJIT_S1, SLJIT_R0), 2, SLJIT_IMM, (sljit_sw)0xfff00000);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)0xffffff80);
	sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_U32, SLJIT_FR1, 0, SLJIT_R0, 0);
	sljit_emit_fop1(compiler, SLJIT_CONV_F32_FROM_U32, SLJIT_FR3, 0, SLJIT_R0, 0);
	/* dbuf[2] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_f64), SLJIT_FR1, 0);
	/* sbuf[2] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_f32), SLJIT_FR3, 0);

	/* dbuf[3] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_UW, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_f64), SLJIT_IMM, (sljit_sw)0xffffff00);
	/* sbuf[3] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F32_FROM_UW, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_f32), SLJIT_IMM, (sljit_sw)0xffffff00);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, WCONST(0x7fff000000000000, 0x7fff0000));
	/* dbuf[4] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_UW, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_f64), SLJIT_R3, 0);
	/* sbuf[4] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F32_FROM_UW, SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_f32), SLJIT_R3, 0);

	sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_UW, SLJIT_FR2, 0, SLJIT_MEM0(), (sljit_sw)&value1);
	sljit_emit_fop1(compiler, SLJIT_CONV_F32_FROM_UW, SLJIT_FR1, 0, SLJIT_MEM0(), (sljit_sw)&value1);
	/* dbuf[5] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_f64), SLJIT_FR2, 0);
	/* sbuf[5] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S1), 5 * sizeof(sljit_f32), SLJIT_FR1, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S3, 0, SLJIT_IMM, WCONST(0xaaaaaaaaaaaaaaaa, 0xaaaaaaaa));
	/* dbuf[6] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_UW, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_f64), SLJIT_S3, 0);
	/* sbuf[6] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F32_FROM_UW, SLJIT_MEM1(SLJIT_S1), 6 * sizeof(sljit_f32), SLJIT_S3, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, (sljit_sw)&value2 + 64);
	/* dbuf[7] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_UW, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_f64), SLJIT_MEM1(SLJIT_R2), -64);
	/* sbuf[7] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F32_FROM_UW, SLJIT_MEM1(SLJIT_S1), 7 * sizeof(sljit_f32), SLJIT_MEM1(SLJIT_R2), -64);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, WCONST(0x8000000000000401, 0x80000001));
	/* dbuf[8] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F64_FROM_UW, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_f64), SLJIT_R2, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, WCONST(0x8000008000000001, 0x80000081));
	/* sbuf[8] */
	sljit_emit_fop1(compiler, SLJIT_CONV_F32_FROM_UW, SLJIT_MEM1(SLJIT_S1), 8 * sizeof(sljit_f32), SLJIT_R2, 0);

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func2((sljit_sw)&dbuf, (sljit_sw)&sbuf);

	f64_check.value = dbuf[0]; /* 0x7fffffff */
	FAILED(f64_check.bin.low != 0xffc00000 || f64_check.bin.high != 0x41dfffff, "test94 case 1 failed\n");
	f32_check.value = sbuf[0]; /* 0x7fffffff */
	FAILED(f32_check.bin != 0x4f000000, "test94 case 2 failed\n");
	f64_check.value = dbuf[1]; /* 0xfff00000 */
	FAILED(f64_check.bin.low != 0 || f64_check.bin.high != 0x41effe00, "test94 case 3 failed\n");
	f32_check.value = sbuf[1]; /* 0xfff00000 */
	FAILED(f32_check.bin != 0x4f7ff000, "test94 case 4 failed\n");
	f64_check.value = dbuf[2]; /* 0xffffff80 */
	FAILED(f64_check.bin.low != 0xf0000000 || f64_check.bin.high != 0x41efffff, "test94 case 5 failed\n");
	f32_check.value = sbuf[2]; /* 0xffffff80 */
	FAILED(f32_check.bin != 0x4f800000, "test94 case 6 failed\n");
	f64_check.value = dbuf[3]; /* 0xffffff00 */
	FAILED(f64_check.bin.low != 0xe0000000 || f64_check.bin.high != 0x41efffff, "test94 case 7 failed\n");
	f32_check.value = sbuf[3]; /* 0xffffff00 */
	FAILED(f32_check.bin != 0x4f7fffff, "test94 case 8 failed\n");
#if IS_64BIT
	f64_check.value = dbuf[4]; /* 0x7fff000000000000 */
	FAILED(f64_check.bin.low != 0 || f64_check.bin.high != 0x43dfffc0, "test94 case 9 failed\n");
	f32_check.value = sbuf[4]; /* 0x7fff000000000000 */
	FAILED(f32_check.bin != 0x5efffe00, "test94 case 10 failed\n");
	f64_check.value = dbuf[5]; /* 0xfffffffffffff800 */
	FAILED(f64_check.bin.low != 0xffffffff || f64_check.bin.high != 0x43efffff, "test94 case 11 failed\n");
	f32_check.value = sbuf[5]; /* 0xfffffffffffff800 */
	FAILED(f32_check.bin != 0x5f800000, "test94 case 12 failed\n");
	f64_check.value = dbuf[6]; /* 0xffff000000000000 */
	FAILED(f64_check.bin.low != 0x55555555 || f64_check.bin.high != 0x43e55555, "test94 case 13 failed\n");
	f32_check.value = sbuf[6]; /* 0xffff000000000000 */
	FAILED(f32_check.bin != 0x5f2aaaab, "test94 case 14 failed\n");
	f64_check.value = dbuf[7]; /* 0x8000000000000801 */
	FAILED(f64_check.bin.low != 1 || f64_check.bin.high != 0x43e00000, "test94 case 15 failed\n");
	f32_check.value = sbuf[7]; /* 0x8000000000000801 */
	FAILED(f32_check.bin != 0x5f000000, "test94 case 16 failed\n");
	f64_check.value = dbuf[8]; /* 0x8000000000000401 */
	FAILED(f64_check.bin.low != 1 || f64_check.bin.high != 0x43e00000, "test94 case 17 failed\n");
	f32_check.value = sbuf[8]; /* 0x8000008000000001 */
	FAILED(f32_check.bin != 0x5f000001, "test94 case 18 failed\n");
#else /* !IS_64BIT */
	f64_check.value = dbuf[4]; /* 0x7fff0000 */
	FAILED(f64_check.bin.low != 0 || f64_check.bin.high != 0x41dfffc0, "test94 case 9 failed\n");
	f32_check.value = sbuf[4]; /* 0x7fff0000 */
	FAILED(f32_check.bin != 0x4efffe00, "test94 case 10 failed\n");
	f64_check.value = dbuf[5]; /* 0xffffff00 */
	FAILED(f64_check.bin.low != 0xe0000000 || f64_check.bin.high != 0x41efffff, "test94 case 11 failed\n");
	f32_check.value = sbuf[5]; /* 0xffffff00 */
	FAILED(f32_check.bin != 0x4f7fffff, "test94 case 12 failed\n");
	f64_check.value = dbuf[6]; /* 0xaaaaaaaa */
	FAILED(f64_check.bin.low != 0x55400000 || f64_check.bin.high != 0x41e55555, "test94 case 13 failed\n");
	f32_check.value = sbuf[6]; /* 0xaaaaaaaa */
	FAILED(f32_check.bin != 0x4f2aaaab, "test94 case 14 failed\n");
	f64_check.value = dbuf[7]; /* 0x80000101 */
	FAILED(f64_check.bin.low != 0x20200000 || f64_check.bin.high != 0x41e00000, "test94 case 15 failed\n");
	f32_check.value = sbuf[7]; /* 0x80000101 */
	FAILED(f32_check.bin != 0x4f000001, "test94 case 16 failed\n");
	f64_check.value = dbuf[8]; /* 0x80000001 */
	FAILED(f64_check.bin.low != 0x00200000 || f64_check.bin.high != 0x41e00000, "test94 case 17 failed\n");
	f32_check.value = sbuf[8]; /* 0x80000081 */
	FAILED(f32_check.bin != 0x4f000001, "test94 case 18 failed\n");
#endif /* IS_64BIT */

	successful_tests++;
}

static void test95(void)
{
	/* Test fpu copysign. */
	executable_code code;
	struct sljit_compiler* compiler;
	int i;

	union {
		sljit_f64 value;
		struct {
#if defined(SLJIT_LITTLE_ENDIAN) && SLJIT_LITTLE_ENDIAN
			sljit_u32 lo;
			sljit_u32 hi;
#else /* !SLJIT_LITTLE_ENDIAN */
			sljit_u32 hi;
			sljit_u32 lo;
#endif /* SLJIT_LITTLE_ENDIAN */
		} bits;
	} dbuf[8];
	union {
		sljit_f32 value;
		sljit_u32 bits;
	} sbuf[8];

	if (verbose)
		printf("Run test95\n");

	if (!sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
		if (verbose)
			printf("no fpu available, test95 skipped\n");
		successful_tests++;
		return;
	}

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	for (i = 0; i < 8; i++)
		dbuf[i].value = 123.0;

	for (i = 0; i < 8; i++)
		sbuf[i].value = 123.0f;

	dbuf[0].value = 1786.5;
	dbuf[1].value = -8403.25;
	dbuf[2].bits.lo = 0;
	dbuf[2].bits.hi = 0x7fff0000;
	dbuf[3].value = 9054;

	sbuf[0].value = 6371.75f;
	sbuf[1].value = -2713.5f;
	sbuf[2].bits = 0xfff00000;
	sbuf[3].value = -5791.25f;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS2(VOID, P, P), 2, 2, 6, 0, 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f64));
	sljit_emit_fop2r(compiler, SLJIT_COPYSIGN_F64, SLJIT_FR0, SLJIT_FR0, 0, SLJIT_FR1, 0);
	/* dbuf[4] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_f64), SLJIT_FR0, 0);

	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR4, 0, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_f32));
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR3, 0, SLJIT_MEM1(SLJIT_S1), 0);
	sljit_emit_fop2r(compiler, SLJIT_COPYSIGN_F32, SLJIT_FR3, SLJIT_FR4, 0, SLJIT_FR3, 0);
	/* sbuf[4] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S1), 4 * sizeof(sljit_f32), SLJIT_FR3, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 2 * sizeof(sljit_f64));
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S0, 0, SLJIT_IMM, 8 * sizeof(sljit_f64));
	sljit_emit_fop2r(compiler, SLJIT_COPYSIGN_F64, SLJIT_FR2, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), 0, SLJIT_MEM1(SLJIT_R1), -7 * (sljit_sw)sizeof(sljit_f64));
	/* dbuf[5] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_f64), SLJIT_FR2, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 2);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR4, 0, SLJIT_MEM1(SLJIT_S1), 0);
	sljit_emit_fop2r(compiler, SLJIT_COPYSIGN_F32, SLJIT_FR5, SLJIT_MEM2(SLJIT_S1, SLJIT_R1), 2, SLJIT_FR4, 0);
	/* sbuf[5] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S1), 5 * sizeof(sljit_f32), SLJIT_FR5, 0);

	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR3, 0, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_f64));
	sljit_emit_fop2r(compiler, SLJIT_COPYSIGN_F64, SLJIT_FR0, SLJIT_FR3, 0, SLJIT_FR2, 0);
	/* dbuf[6] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_f64), SLJIT_FR0, 0);

	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_f32));
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S1, 0, SLJIT_IMM, 0x12345);
	sljit_emit_fop2r(compiler, SLJIT_COPYSIGN_F32, SLJIT_FR2, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_R0), -0x12345 + (sljit_sw)sizeof(sljit_f32));
	/* sbuf[6] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S1), 6 * sizeof(sljit_f32), SLJIT_FR2, 0);

	sljit_emit_fop2r(compiler, SLJIT_COPYSIGN_F64, SLJIT_FR5, SLJIT_MEM0(), (sljit_sw)(dbuf + 1), SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_f64));
	/* dbuf[7] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_f64), SLJIT_FR5, 0);

	sljit_emit_fop2r(compiler, SLJIT_COPYSIGN_F32, SLJIT_FR4, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_MEM0(), (sljit_sw)(sbuf + 2));
	/* sbuf[7] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S1), 7 * sizeof(sljit_f32), SLJIT_FR4, 0);

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func2((sljit_sw)&dbuf, (sljit_sw)&sbuf);
	FAILED(dbuf[4].value != -1786.5, "test95 case 1 failed\n");
	FAILED(sbuf[4].value != 2713.5, "test95 case 2 failed\n");
	FAILED(dbuf[5].bits.lo != 0, "test95 case 3 failed\n");
	FAILED(dbuf[5].bits.hi != 0xffff0000, "test95 case 4 failed\n");
	FAILED(sbuf[5].bits != 0x7ff00000, "test95 case 5 failed\n");
	FAILED(dbuf[6].value != 9054, "test95 case 6 failed\n");
	FAILED(sbuf[6].value != -5791.25, "test95 case 7 failed\n");
	FAILED(dbuf[7].value != 8403.25, "test95 case 8 failed\n");
	FAILED(sbuf[7].value != -sbuf[0].value, "test95 case 9 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void simd_set(sljit_u8* buf, sljit_u8 start, sljit_s32 length)
{
	do {
		*buf++ = start;
		start = (sljit_u8)(start + 103);

		if (start == 0xaa)
			start = 0xab;
	} while (--length != 0);
}

static sljit_s32 check_simd_mov(sljit_u8* buf, sljit_u8 start, sljit_s32 length)
{
	if (buf[-1] != 0xaa || buf[length] != 0xaa)
		return 0;

	do {
		if (*buf++ != start)
			return 0;

		start = (sljit_u8)(start + 103);

		if (start == 0xaa)
			start = 0xab;
	} while (--length != 0);

	return 1;
}

static void test96(void)
{
	/* Test simd data transfer. */
	executable_code code;
	struct sljit_compiler* compiler;
	sljit_s32 i, type;
	sljit_u8 supported[1];
	sljit_u8* buf;
	sljit_u8 data[63 + 880];
	sljit_s32 fs0 = SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS > 0 ? SLJIT_FS0 : SLJIT_FR5;

	if (verbose)
		printf("Run test96\n");

	if (!sljit_has_cpu_feature(SLJIT_HAS_SIMD)) {
		if (verbose)
			printf("no simd available, test96 skipped\n");
		successful_tests++;
		return;
	}

	/* Buffer is 64 byte aligned. */
	buf = (sljit_u8*)(((sljit_sw)data + (sljit_sw)63) & ~(sljit_sw)63);

	for (i = 0; i < 880; i++)
		buf[i] = 0xaa;

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	simd_set(buf + 0, 81, 16);
	simd_set(buf + 65, 213, 16);
	simd_set(buf + 104, 33, 16);
	simd_set(buf + 160, 140, 16);
	simd_set(buf + 210, 7, 16);
	simd_set(buf + 256, 239, 16);
	simd_set(buf + 312, 176, 16);
	simd_set(buf + 368, 88, 8);
	simd_set(buf + 393, 197, 8);
	simd_set(buf + 416, 58, 16);
	simd_set(buf + 432, 203, 16);
	simd_set(buf + 496, 105, 16);
	simd_set(buf + 560, 19, 16);
	simd_set(buf + 616, 202, 8);
	simd_set(buf + 648, 123, 8);
	simd_set(buf + 704, 85, 32);
	simd_set(buf + 801, 215, 32);

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 2, 2, 6, SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS > 0 ? 1 : 0, 64);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_8 | SLJIT_SIMD_MEM_ALIGNED_128;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 0);
	/* buf[32] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 32);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 65);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 82 >> 1);
	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_8 | SLJIT_SIMD_MEM_UNALIGNED;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), 0);
	/* buf[82] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM2(SLJIT_S0, SLJIT_R1), 1);

	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_IMM, 70001);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S0, 0, SLJIT_IMM, 70001);
	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_32 | SLJIT_SIMD_MEM_ALIGNED_64;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_R0), 70001 + 104);
	/* buf[136] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_R1), 136 - 70001);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_FLOAT | SLJIT_SIMD_ELEM_32 | SLJIT_SIMD_MEM_ALIGNED_128;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, SLJIT_MEM0(), (sljit_sw)(buf + 160));
	/* buf[192] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM0(), (sljit_sw)(buf + 192));

	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_IMM, 1001);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S0, 0, SLJIT_IMM, 1001);
	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_FLOAT | SLJIT_SIMD_ELEM_32 | SLJIT_SIMD_MEM_ALIGNED_16;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_R0), 1001 + 210);
	/* buf[230] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_R1), 230 - 1001);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 256 >> 3);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 288 >> 3);
	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_FLOAT | SLJIT_SIMD_ELEM_64 | SLJIT_SIMD_MEM_ALIGNED_128;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), 3);
	/* buf[288] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM2(SLJIT_S0, SLJIT_R1), 3);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_FLOAT | SLJIT_SIMD_ELEM_64 | SLJIT_SIMD_MEM_ALIGNED_64;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 312);
	/* buf[344] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 344);

	type = SLJIT_SIMD_REG_64 | SLJIT_SIMD_ELEM_32 | SLJIT_SIMD_MEM_ALIGNED_64;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 368);
	/* buf[384] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 384);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 393);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 402);
	type = SLJIT_SIMD_REG_64 | SLJIT_SIMD_ELEM_64 | SLJIT_SIMD_MEM_UNALIGNED;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), 0);
	/* buf[402] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM2(SLJIT_S0, SLJIT_R1), 0);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_16 | SLJIT_SIMD_MEM_ALIGNED_128;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 416);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 432);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 464);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_16;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 496);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 480);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR4, SLJIT_FR3, 0);
	/* buf[528] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 528);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 560);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 544);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_FR0, 0);
	/* buf[592] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 592);

	type = SLJIT_SIMD_REG_64 | SLJIT_SIMD_ELEM_8;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 616);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 608);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR3, SLJIT_FR5, 0);
	/* buf[632] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 632);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 648);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, SLJIT_MEM1(SLJIT_S0), 640);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, fs0, 0);
	/* buf[664] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 664);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_32 | SLJIT_SIMD_MEM_ALIGNED_256;
	supported[0] = sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 704) != SLJIT_ERR_UNSUPPORTED;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_32, SLJIT_FR2, fs0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_S0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, 384);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM2(SLJIT_R1, SLJIT_S1), 1);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_16;
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_IMM, 801 - 32);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_R0), 32);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_SP), 0);
	sljit_get_local_base(compiler, SLJIT_R1, 0, 128);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_R1), -128);
	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_16 | SLJIT_SIMD_MEM_ALIGNED_16;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, SLJIT_MEM0(), (sljit_sw)(buf + 834));

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)buf);

	SLJIT_ASSERT(sljit_has_cpu_feature(SLJIT_HAS_SIMD));
	FAILED(!check_simd_mov(buf + 32, 81, 16), "test96 case 1 failed\n");
	FAILED(!check_simd_mov(buf + 82, 213, 16), "test96 case 2 failed\n");
	FAILED(!check_simd_mov(buf + 136, 33, 16), "test96 case 3 failed\n");
	FAILED(!check_simd_mov(buf + 192, 140, 16), "test96 case 4 failed\n");
	FAILED(!check_simd_mov(buf + 230, 7, 16), "test96 case 5 failed\n");
	FAILED(!check_simd_mov(buf + 288, 239, 16), "test96 case 6 failed\n");
	FAILED(!check_simd_mov(buf + 344, 176, 16), "test96 case 7 failed\n");
#if IS_ARM
	FAILED(!check_simd_mov(buf + 384, 88, 8), "test96 case 8 failed\n");
	FAILED(!check_simd_mov(buf + 402, 197, 8), "test96 case 9 failed\n");
#endif /* IS_ARM */
	FAILED(!check_simd_mov(buf + 464, sljit_has_cpu_feature(SLJIT_SIMD_REGS_ARE_PAIRS) ? 203 : 58, 16), "test96 case 10 failed\n");
	FAILED(!check_simd_mov(buf + 528, 105, 16), "test96 case 11 failed\n");
	FAILED(!check_simd_mov(buf + 592, 19, 16), "test96 case 12 failed\n");
#if IS_ARM
	FAILED(!check_simd_mov(buf + 632, 202, 8), "test96 case 13 failed\n");
	FAILED(!check_simd_mov(buf + 664, 123, 8), "test96 case 14 failed\n");
#endif /* IS_ARM */

	if (supported[0]) {
		FAILED(!check_simd_mov(buf + 768, 85, 32), "test96 case 15 failed\n");
		FAILED(!check_simd_mov(buf + 834, 215, 32), "test96 case 16 failed\n");
	}

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static sljit_s32 check_simd_lane_mov(sljit_u8* buf, sljit_s32 length, sljit_s32 elem_size, sljit_s32 is_odd)
{
	sljit_s32 count = (length / elem_size) >> 1;
	sljit_s32 value = 180 + length - elem_size;
	sljit_s32 i;

	if (!is_odd)
		value -= elem_size;

	do {
		if (is_odd) {
			for (i = 0; i < elem_size; i++)
				if (*buf++ != 0xaa)
					return 0;
		}

		for (i = 0; i < elem_size; i++)
			if (*buf++ != value++)
				return 0;

		if (!is_odd) {
			for (i = 0; i < elem_size; i++)
				if (*buf++ != 0xaa)
					return 0;
		}

		value -= 3 * elem_size;
	} while (--count != 0);

	return 1;
}

static void test97(void)
{
	/* Test simd lane data transfer. */
	executable_code code;
	struct sljit_compiler* compiler;
	sljit_s32 i, type;
	sljit_u8 supported[1];
	sljit_u8* buf;
	sljit_u8 data[63 + 576];
	sljit_f64 tmp[1];
	sljit_u32 f32_result = 0;
	sljit_sw result[6];
	sljit_s32 result32[5];
	sljit_s32 fs0 = SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS > 0 ? SLJIT_FS0 : SLJIT_FR5;

	if (verbose)
		printf("Run test97\n");

	if (!sljit_has_cpu_feature(SLJIT_HAS_SIMD)) {
		if (verbose)
			printf("no simd available, test97 skipped\n");
		successful_tests++;
		return;
	}

	/* Buffer is 64 byte aligned. */
	buf = (sljit_u8*)(((sljit_sw)data + (sljit_sw)63) & ~(sljit_sw)63);

	for (i = 0; i < 64; i++)
		buf[i] = (sljit_u8)(180 + i);

	for (i = 64; i < 576; i++)
		buf[i] = 0xaa;

	for (i = 0; i < 6; i++)
		result[i] = 0;

	for (i = 0; i < 5; i++)
		result32[i] = 0;

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 4, 4, 6, SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS > 0 ? 1 : 0, 16);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)tmp - 100000);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)tmp + 1000);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, 100000 / 2);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_8;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 64);

	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, 14, SLJIT_R2, 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, 0, SLJIT_R2, 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, 12, SLJIT_MEM1(SLJIT_SP), 0);
	sljit_get_local_base(compiler, SLJIT_R2, 0, 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, 2, SLJIT_MEM1(SLJIT_R2), 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, 10, SLJIT_MEM0(), (sljit_sw)tmp);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, 4, SLJIT_MEM0(), (sljit_sw)tmp);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, 8, SLJIT_R3, 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, 6, SLJIT_R3, 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, 6, SLJIT_MEM1(SLJIT_R0), 100000);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, 8, SLJIT_MEM1(SLJIT_R1), -1000);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, 4, SLJIT_MEM2(SLJIT_R0, SLJIT_S1), 1);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, 10, SLJIT_MEM1(SLJIT_R1), -1000);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, 2, SLJIT_MEM1(SLJIT_R1), -1000);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, 12, SLJIT_MEM2(SLJIT_R0, SLJIT_S1), 1);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, 0, SLJIT_S2, 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, 14, SLJIT_S2, 0);
	/* buf[128] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 128);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 64);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, 1, SLJIT_R2, 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR5, 15, SLJIT_IMM, 181 + 0xffff00);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 183 + 0xffff00);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR5, 13, SLJIT_R2, 0);
	for (i = 5; i < 16; i += 2) {
		sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | SLJIT_32 | type, SLJIT_FR0, i, SLJIT_R2, 0);
		sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | SLJIT_32 | type, SLJIT_FR5, 16 - i, SLJIT_R2, 0);
	}
	/* buf[144] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 144);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_16;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 64);

	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, 6, SLJIT_R2, 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, 0, SLJIT_R2, 0);
	sljit_get_local_base(compiler, SLJIT_R2, 0, 4);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, 4, SLJIT_MEM1(SLJIT_R2), 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, 2, SLJIT_MEM1(SLJIT_SP), 4);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, 2, SLJIT_MEM0(), (sljit_sw)tmp);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, 4, SLJIT_MEM0(), (sljit_sw)tmp);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, 0, SLJIT_S3, 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, 6, SLJIT_S3, 0);
	/* buf[160] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 160);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 64);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, 7, SLJIT_MEM1(SLJIT_R0), 100000);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, 1, SLJIT_MEM1(SLJIT_R1), -1000);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, 5, SLJIT_MEM2(SLJIT_R0, SLJIT_S1), 1);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, 3, SLJIT_MEM1(SLJIT_R1), -1000);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, 3, SLJIT_MEM1(SLJIT_R1), -1000);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, 5, SLJIT_MEM2(SLJIT_R0, SLJIT_S1), 1);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, 1, SLJIT_S2, 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, 7, SLJIT_S2, 0);
	/* buf[176] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 176);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_32;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 64);

	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, 2, SLJIT_R2, 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, 0, SLJIT_R2, 0);
	sljit_get_local_base(compiler, SLJIT_R2, 0, 8);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, 0, SLJIT_MEM1(SLJIT_R2), 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, 2, SLJIT_MEM1(SLJIT_SP), 8);
	/* buf[192] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 192);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, SLJIT_MEM1(SLJIT_S0), 64);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, 3, SLJIT_S3, 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, 1, SLJIT_S3, 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, 1, SLJIT_MEM1(SLJIT_R0), 100000);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, 3, SLJIT_MEM1(SLJIT_R1), -1000);
	/* buf[208] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 208);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_64;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 64);

	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, 0, SLJIT_R2, 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR5, 0, SLJIT_R2, 0);
	/* buf[224] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 224);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 64);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, 1, SLJIT_MEM1(SLJIT_R1), -1000);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, 1, SLJIT_MEM2(SLJIT_R0, SLJIT_S1), 1);
	/* buf[240] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 240);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_FLOAT | SLJIT_SIMD_ELEM_32;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 64);

	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, 2, SLJIT_FR1, 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR3, 0, SLJIT_FR1, 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM0(), (sljit_sw)&f32_result, SLJIT_FR1, 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, 0, SLJIT_FR0, 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR3, 2, SLJIT_FR0, 0);
	/* buf[256] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 256);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 64);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, 3, SLJIT_FR2, 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, 1, SLJIT_FR2, 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, 1, SLJIT_FR3, 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, 3, SLJIT_FR3, 0);
	/* buf[272] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 272);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 64);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, 2, SLJIT_MEM1(SLJIT_SP), 4);
	sljit_get_local_base(compiler, SLJIT_R2, 0, 4);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_R2), 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, 0, SLJIT_MEM1(SLJIT_R0), 100000);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, 2, SLJIT_MEM1(SLJIT_R1), -1000);
	/* buf[288] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 288);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 64);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, 3, SLJIT_MEM1(SLJIT_R1), -1000);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, 1, SLJIT_MEM2(SLJIT_R0, SLJIT_S1), 1);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, 1, SLJIT_MEM0(), (sljit_sw)tmp);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, 3, SLJIT_MEM0(), (sljit_sw)tmp);
	/* buf[304] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 304);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_FLOAT | SLJIT_SIMD_ELEM_64;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 64);

	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, 0, SLJIT_FR4, 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, 0, SLJIT_FR4, 0);
	/* buf[320] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 320);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, SLJIT_MEM1(SLJIT_S0), 64);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, 1, SLJIT_FR2, 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, 1, SLJIT_FR2, 0);
	/* buf[336] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 336);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 64);
	sljit_get_local_base(compiler, SLJIT_R2, 0, 8);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_R2), 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR3, 0, SLJIT_MEM1(SLJIT_SP), 8);
	/* buf[352] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 352);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 64);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, 1, SLJIT_MEM1(SLJIT_R0), 100000);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, 1, SLJIT_MEM1(SLJIT_R1), -1000);
	/* buf[368] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 368);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, (sljit_sw)result);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | SLJIT_SIMD_REG_128, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 64);
	type = SLJIT_SIMD_STORE | SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_8;
	sljit_emit_simd_lane_mov(compiler, type, SLJIT_FR1, 6, SLJIT_R0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S3, 0, SLJIT_IMM, -1);
	sljit_emit_simd_lane_mov(compiler, type | SLJIT_SIMD_LANE_SIGNED, SLJIT_FR1, 13, SLJIT_S3, 0);
	/* result[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R2), 0, SLJIT_R0, 0);
	/* result[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R2), sizeof(sljit_sw), SLJIT_S3, 0);

	type = SLJIT_SIMD_STORE | SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_16;
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, -1);
	sljit_emit_simd_lane_mov(compiler, type, SLJIT_FR1, 5, SLJIT_R3, 0);
	sljit_emit_simd_lane_mov(compiler, type | SLJIT_SIMD_LANE_SIGNED, SLJIT_FR1, 7, SLJIT_R1, 0);
	/* result[2] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R2), sizeof(sljit_sw) * 2, SLJIT_R3, 0);
	/* result[3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R2), sizeof(sljit_sw) * 3, SLJIT_R1, 0);

	type = SLJIT_SIMD_STORE | SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_32;
	sljit_emit_simd_lane_mov(compiler, type, SLJIT_FR1, 2, SLJIT_S3, 0);
	sljit_emit_simd_lane_mov(compiler, type | SLJIT_SIMD_LANE_SIGNED, SLJIT_FR1, 3, SLJIT_R0, 0);
	/* result[4] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R2), sizeof(sljit_sw) * 4, SLJIT_S3, 0);
	/* result[5] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_R2), sizeof(sljit_sw) * 5, SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, (sljit_sw)result32);
	type = SLJIT_SIMD_STORE | SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_8 | SLJIT_32;
	sljit_emit_simd_lane_mov(compiler, type, SLJIT_FR1, 0, SLJIT_R3, 0);
	sljit_emit_simd_lane_mov(compiler, type | SLJIT_SIMD_LANE_SIGNED, SLJIT_FR1, 3, SLJIT_S2, 0);
	/* result32[0] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_R2), 0, SLJIT_R3, 0);
	/* result32[1] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_R2), sizeof(sljit_s32), SLJIT_S2, 0);

	type = SLJIT_SIMD_STORE | SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_16 | SLJIT_32;
	sljit_emit_simd_lane_mov(compiler, type, SLJIT_FR1, 0, SLJIT_R1, 0);
	sljit_emit_simd_lane_mov(compiler, type | SLJIT_SIMD_LANE_SIGNED, SLJIT_FR1, 3, SLJIT_S3, 0);
	/* result32[2] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_R2), sizeof(sljit_s32) * 2, SLJIT_R1, 0);
	/* result32[3] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_R2), sizeof(sljit_s32) * 3, SLJIT_S3, 0);

	type = SLJIT_SIMD_STORE | SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_32 | SLJIT_32;
	sljit_emit_simd_lane_mov(compiler, type | SLJIT_SIMD_LANE_SIGNED, SLJIT_FR1, 0, SLJIT_R0, 0);
	/* result32[4] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_R2), sizeof(sljit_s32) * 4, SLJIT_R0, 0);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)tmp - 100000);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)tmp + 1000);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_8;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 64);
	supported[0] = sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, 30, SLJIT_MEM1(SLJIT_R1), -1000) != SLJIT_ERR_UNSUPPORTED;
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR4, 0, SLJIT_MEM1(SLJIT_R1), -1000);

	for (i = 2; i < 32; i += 2) {
		sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, 30 - i, SLJIT_R2, 0);
		sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR4, i, SLJIT_R2, 0);
	}
	/* buf[384] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 384);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_16;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 64);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, 1, SLJIT_MEM1(SLJIT_SP), 8);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, 15, SLJIT_MEM1(SLJIT_SP), 8);

	for (i = 3; i < 16; i += 2) {
		sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, i, SLJIT_R2, 0);
		sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, 16 - i, SLJIT_R2, 0);
	}
	/* buf[416] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 416);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_32;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 64);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, 6, SLJIT_MEM1(SLJIT_R0), 100000);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, 0, SLJIT_MEM0(), (sljit_sw)tmp);

	for (i = 2; i < 8; i += 2) {
		sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, 6 - i, SLJIT_S1, 0);
		sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, i, SLJIT_S1, 0);
	}
	/* buf[448] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 448);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_64;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, SLJIT_MEM1(SLJIT_S0), 64);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, -1000);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, 1, SLJIT_MEM1(SLJIT_R0), 100000);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, 3, SLJIT_MEM2(SLJIT_R1, SLJIT_S1), 0);

	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, 3, SLJIT_S1, 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, 1, SLJIT_S1, 0);
	/* buf[480] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 480);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_32 | SLJIT_SIMD_FLOAT;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 64);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, 1, SLJIT_MEM1(SLJIT_SP), 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR4, 7, SLJIT_MEM1(SLJIT_SP), 0);

	for (i = 3; i < 8; i += 2) {
		sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, i, SLJIT_FR2, 0);
		sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR4, 8 - i, SLJIT_FR2, 0);
	}
	/* buf[512] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 512);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_64 | SLJIT_SIMD_FLOAT;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 64);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, 2, SLJIT_MEM0(), (sljit_sw)tmp);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_R0), 100000);

	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, 0, SLJIT_FR0, 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, 2, SLJIT_FR0, 0);
	/* buf[544] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 544);

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)buf);

	FAILED(!check_simd_lane_mov(buf + 128, 16, 1, 0), "test97 case 1 failed\n");
	FAILED(!check_simd_lane_mov(buf + 144, 16, 1, 1), "test97 case 2 failed\n");
	FAILED(!check_simd_lane_mov(buf + 160, 16, 2, 0), "test97 case 3 failed\n");
	FAILED(!check_simd_lane_mov(buf + 176, 16, 2, 1), "test97 case 4 failed\n");
	FAILED(!check_simd_lane_mov(buf + 192, 16, 4, 0), "test97 case 5 failed\n");
	FAILED(!check_simd_lane_mov(buf + 208, 16, 4, 1), "test97 case 6 failed\n");
#if IS_64BIT
	FAILED(!check_simd_lane_mov(buf + 224, 16, 8, 0), "test97 case 7 failed\n");
	FAILED(!check_simd_lane_mov(buf + 240, 16, 8, 1), "test97 case 8 failed\n");
#endif /* IS_64BIT */
	FAILED(!check_simd_lane_mov(buf + 256, 16, 4, 0), "test97 case 9 failed\n");
	FAILED(!check_simd_lane_mov(buf + 272, 16, 4, 1), "test97 case 10 failed\n");
	FAILED(!check_simd_lane_mov(buf + 288, 16, 4, 0), "test97 case 11 failed\n");
	FAILED(!check_simd_lane_mov(buf + 304, 16, 4, 1), "test97 case 12 failed\n");
	FAILED(f32_result != 0xbfbebdbc, "test97 case 13 failed\n");
	FAILED(!check_simd_lane_mov(buf + 320, 16, 8, 0), "test97 case 14 failed\n");
	FAILED(!check_simd_lane_mov(buf + 336, 16, 8, 1), "test97 case 15 failed\n");
	FAILED(!check_simd_lane_mov(buf + 352, 16, 8, 0), "test97 case 16 failed\n");
	FAILED(!check_simd_lane_mov(buf + 368, 16, 8, 1), "test97 case 17 failed\n");
	FAILED(result[0] != 186, "test97 case 18 failed\n");
	FAILED(result[1] != -63, "test97 case 19 failed\n");
	FAILED(result[2] != 49086, "test97 case 20 failed\n");
	FAILED(result[3] != -15422, "test97 case 21 failed\n");
	FAILED(result[4] != WCONST(3216948668, -1078018628), "test97 case 22 failed\n");
	FAILED(result[5] != -1010646592, "test97 case 23 failed\n");
	FAILED(result32[0] != 180, "test97 case 24 failed\n");
	FAILED(result32[1] != -73, "test97 case 25 failed\n");
	FAILED(result32[2] != 46516, "test97 case 26 failed\n");
	FAILED(result32[3] != -17478, "test97 case 27 failed\n");
	FAILED(result32[4] != -1212762700, "test97 case 28 failed\n");

	if (supported[0]) {
		FAILED(!check_simd_lane_mov(buf + 384, 32, 1, 0), "test97 case 29 failed\n");
		FAILED(!check_simd_lane_mov(buf + 416, 32, 2, 1), "test97 case 30 failed\n");
		FAILED(!check_simd_lane_mov(buf + 448, 32, 4, 0), "test97 case 31 failed\n");
#if IS_64BIT
		FAILED(!check_simd_lane_mov(buf + 480, 32, 8, 1), "test97 case 32 failed\n");
#endif /* IS_64BIT */
		FAILED(!check_simd_lane_mov(buf + 512, 32, 4, 1), "test97 case 33 failed\n");
		FAILED(!check_simd_lane_mov(buf + 544, 32, 8, 0), "test97 case 34 failed\n");
	}

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static sljit_s32 check_simd_replicate(sljit_u8* buf, sljit_s32 length, sljit_s32 elem_size, sljit_s32 value)
{
	sljit_s32 count = length / elem_size;
	sljit_s32 i;

	do {
		for (i = 0; i < elem_size; i++)
			if (*buf++ != value++)
				return 0;

		value -= elem_size;
	} while (--count != 0);

	return 1;
}

static sljit_s32 check_simd_replicate_u32(sljit_u8* buf, sljit_s32 length, sljit_u32 value)
{
	sljit_s32 count = length / 4;
	sljit_u32 start_value = value;
	sljit_s32 i;

	do {
		for (i = 0; i < 4; i++) {
			if (*buf++ != (value & 0xff))
				return 0;
			value >>= 8;
		}

		value = start_value;
	} while (--count != 0);

	return 1;
}

static void test98(void)
{
	/* Test simd replicate scalar to all lanes. */
	executable_code code;
	struct sljit_compiler* compiler;
	sljit_s32 i, type;
	sljit_u8 supported[1];
	sljit_u8* buf;
	sljit_u8 data[63 + 768];
	sljit_s32 fs0 = SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS > 0 ? SLJIT_FS0 : SLJIT_FR5;

	if (verbose)
		printf("Run test98\n");

	if (!sljit_has_cpu_feature(SLJIT_HAS_SIMD)) {
		if (verbose)
			printf("no simd available, test98 skipped\n");
		successful_tests++;
		return;
	}

	/* Buffer is 64 byte aligned. */
	buf = (sljit_u8*)(((sljit_sw)data + (sljit_sw)63) & ~(sljit_sw)63);

	for (i = 0; i < 32; i++)
		buf[i] = (sljit_u8)(200 + i);

	for (i = 32; i < 768; i++)
		buf[i] = 0xaa;

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 4, 4, 6, SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS > 0 ? 1 : 0, 16);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_8;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 32);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 0xffff00 + 78);
	sljit_emit_simd_replicate(compiler, type, SLJIT_FR0, SLJIT_R2, 0);
	/* buf[48] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 48);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 32);
	sljit_emit_simd_replicate(compiler, type, SLJIT_FR3, SLJIT_IMM, 0xffff00 + 253);
	/* buf[64] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 64);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, SLJIT_MEM1(SLJIT_S0), 32);
	sljit_emit_op1(compiler, SLJIT_MOV_U8, SLJIT_MEM1(SLJIT_SP), 3, SLJIT_IMM, 42);
	sljit_emit_simd_replicate(compiler, type, fs0, SLJIT_MEM1(SLJIT_SP), 3);
	/* buf[80] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 80);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 32);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 15);
	sljit_emit_simd_replicate(compiler, type, SLJIT_FR5, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), 0);
	/* buf[96] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 96);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_16;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 32);
	sljit_emit_op1(compiler, SLJIT_MOV_S16, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_S0), 24);
	sljit_emit_simd_replicate(compiler, type, SLJIT_FR3, SLJIT_R1, 0);
	/* buf[112] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 112);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, SLJIT_MEM1(SLJIT_S0), 32);
	sljit_emit_simd_replicate(compiler, type, fs0, SLJIT_MEM0(), (sljit_sw)(buf + 10));
	/* buf[128] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 128);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 32);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_IMM, 10000 + 20);
	sljit_emit_simd_replicate(compiler, type, SLJIT_FR0, SLJIT_MEM1(SLJIT_R0), -10000);
	/* buf[144] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 144);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_32;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 32);
	sljit_emit_op1(compiler, SLJIT_MOV_S32, SLJIT_S3, 0, SLJIT_MEM1(SLJIT_S0), 28);
	sljit_emit_simd_replicate(compiler, type, SLJIT_FR1, SLJIT_S3, 0);
	/* buf[160] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 160);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 32);
	sljit_emit_op1(compiler, SLJIT_MOV_U32, SLJIT_MEM1(SLJIT_SP), 4, SLJIT_MEM1(SLJIT_S0), 12);
	sljit_emit_simd_replicate(compiler, type, SLJIT_FR5, SLJIT_MEM1(SLJIT_SP), 4);
	/* buf[176] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 176);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, SLJIT_MEM1(SLJIT_S0), 32);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R2, 0, SLJIT_S0, 0, SLJIT_IMM, 100000 - 24);
	sljit_emit_simd_replicate(compiler, type, fs0, SLJIT_MEM1(SLJIT_R2), 100000);
	/* buf[192] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 192);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_64;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 32);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_MEM1(SLJIT_S0), 8);
	sljit_emit_simd_replicate(compiler, type, SLJIT_FR2, SLJIT_S1, 0);
	/* buf[208] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 208);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 32);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 3);
	sljit_emit_simd_replicate(compiler, type, SLJIT_FR0, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), 3);
	/* buf[224] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 224);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, SLJIT_MEM1(SLJIT_S0), 32);
	sljit_emit_simd_replicate(compiler, type, fs0, SLJIT_MEM0(), (sljit_sw)buf);
	/* buf[240] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 240);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_FLOAT | SLJIT_SIMD_ELEM_32;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 32);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S0), 4);
	sljit_emit_simd_replicate(compiler, type, SLJIT_FR1, SLJIT_FR2, 0);
	/* buf[256] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 256);

	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR4, 0, SLJIT_MEM1(SLJIT_S0), 20);
	sljit_emit_simd_replicate(compiler, type, SLJIT_FR4, SLJIT_FR4, 0);
	/* buf[272] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 272);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 32);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_SP), 4, SLJIT_MEM1(SLJIT_S0), 12);
	sljit_emit_simd_replicate(compiler, type, SLJIT_FR0, SLJIT_MEM1(SLJIT_SP), 4);
	/* buf[288] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 288);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_FLOAT | SLJIT_SIMD_ELEM_64;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, SLJIT_MEM1(SLJIT_S0), 32);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S0), 16);
	sljit_emit_simd_replicate(compiler, type, fs0, SLJIT_FR0, 0);
	/* buf[304] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 304);

	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR5, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_replicate(compiler, type, SLJIT_FR5, SLJIT_FR5, 0);
	/* buf[320] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 320);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 32);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R2, 0, SLJIT_S0, 0, SLJIT_IMM, 10000 + 8);
	sljit_emit_simd_replicate(compiler, type, SLJIT_FR1, SLJIT_MEM1(SLJIT_R2), -10000);
	/* buf[336] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 336);

	/* Test constant values. */
	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_32;
	sljit_emit_simd_replicate(compiler, type, SLJIT_FR0, SLJIT_IMM, WCONST(0xff00123456, 0x123456));
	/* buf[352] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 352);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_16;
	sljit_emit_simd_replicate(compiler, type, SLJIT_FR1, SLJIT_IMM, 0xff0000);
	/* buf[368] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 368);

	sljit_emit_simd_replicate(compiler, type, SLJIT_FR2, SLJIT_IMM, 0x1ffff);
	/* buf[384] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 384);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_FLOAT | SLJIT_SIMD_ELEM_64;
	sljit_emit_simd_replicate(compiler, type, SLJIT_FR3, SLJIT_IMM, 0);
	/* buf[400] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 400);

	/* Test ARM constant values. */
	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_16;
	sljit_emit_simd_replicate(compiler, type, fs0, SLJIT_IMM, 0xff0034);
	/* buf[416] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 416);

	sljit_emit_simd_replicate(compiler, type, SLJIT_FR5, SLJIT_IMM, 0xff45ff);
	/* buf[432] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 432);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_32;
	sljit_emit_simd_replicate(compiler, type, SLJIT_FR0, SLJIT_IMM, 0xb3);
	/* buf[448] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 448);

	sljit_emit_simd_replicate(compiler, type, SLJIT_FR1, SLJIT_IMM, (sljit_sw)0xffff46ff);
	/* buf[464] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 464);

	sljit_emit_simd_replicate(compiler, type, fs0, SLJIT_IMM, 0x4c0000);
	/* buf[480] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 480);

	sljit_emit_simd_replicate(compiler, type, SLJIT_FR3, SLJIT_IMM, 0x71ffffff);
	/* buf[496] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 496);

	sljit_emit_simd_replicate(compiler, type, SLJIT_FR4, SLJIT_IMM, 0x9eff);
	/* buf[512] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 512);

	sljit_emit_simd_replicate(compiler, type, SLJIT_FR5, SLJIT_IMM, (sljit_sw)0xff070000);
	/* buf[528] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 528);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_8;
	supported[0] = sljit_emit_simd_replicate(compiler, type, SLJIT_FR2, SLJIT_IMM, 0xffff00 + 181) != SLJIT_ERR_UNSUPPORTED;
	/* buf[544] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 544);

	sljit_emit_simd_replicate(compiler, type, fs0, SLJIT_IMM, 0xffff00);
	/* buf[576] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 576);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_16;
	sljit_emit_op1(compiler, SLJIT_MOV_U16, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_S0), 30);
	sljit_emit_simd_replicate(compiler, type, SLJIT_FR1, SLJIT_R1, 0);
	/* buf[608] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 608);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_32;
	sljit_emit_simd_replicate(compiler, type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 4);
	/* buf[640] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 640);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_64;
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_S0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, 4);
	sljit_emit_simd_replicate(compiler, type, SLJIT_FR0, SLJIT_MEM2(SLJIT_R1, SLJIT_S1), 2);
	/* buf[672] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 672);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_32 | SLJIT_SIMD_FLOAT;
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S0), 20);
	sljit_emit_simd_replicate(compiler, type, SLJIT_FR1, SLJIT_FR2, 0);
	/* buf[704] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 704);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_64 | SLJIT_SIMD_FLOAT;
	sljit_emit_simd_replicate(compiler, type, fs0, SLJIT_MEM0(), (sljit_sw)(buf + 8));
	/* buf[736] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 736);

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)buf);

	FAILED(!check_simd_replicate(buf + 48, 16, 1, 78), "test98 case 1 failed\n");
	FAILED(!check_simd_replicate(buf + 64, 16, 1, 253), "test98 case 2 failed\n");
	FAILED(!check_simd_replicate(buf + 80, 16, 1, 42), "test98 case 3 failed\n");
	FAILED(!check_simd_replicate(buf + 96, 16, 1, 215), "test98 case 4 failed\n");
	FAILED(!check_simd_replicate(buf + 112, 16, 2, 224), "test98 case 5 failed\n");
	FAILED(!check_simd_replicate(buf + 128, 16, 2, 210), "test98 case 6 failed\n");
	FAILED(!check_simd_replicate(buf + 144, 16, 2, 220), "test98 case 7 failed\n");
	FAILED(!check_simd_replicate(buf + 160, 16, 4, 228), "test98 case 8 failed\n");
	FAILED(!check_simd_replicate(buf + 176, 16, 4, 212), "test98 case 9 failed\n");
	FAILED(!check_simd_replicate(buf + 192, 16, 4, 224), "test98 case 10 failed\n");
#if IS_64BIT
	FAILED(!check_simd_replicate(buf + 208, 16, 8, 208), "test98 case 11 failed\n");
	FAILED(!check_simd_replicate(buf + 224, 16, 8, 224), "test98 case 12 failed\n");
	FAILED(!check_simd_replicate(buf + 240, 16, 8, 200), "test98 case 13 failed\n");
#endif /* IS_64BIT */
	FAILED(!check_simd_replicate(buf + 256, 16, 4, 204), "test98 case 14 failed\n");
	FAILED(!check_simd_replicate(buf + 272, 16, 4, 220), "test98 case 15 failed\n");
	FAILED(!check_simd_replicate(buf + 288, 16, 4, 212), "test98 case 16 failed\n");
	FAILED(!check_simd_replicate(buf + 304, 16, 8, 216), "test98 case 17 failed\n");
	FAILED(!check_simd_replicate(buf + 320, 16, 8, 200), "test98 case 18 failed\n");
	FAILED(!check_simd_replicate(buf + 336, 16, 8, 208), "test98 case 19 failed\n");
	FAILED(!check_simd_replicate_u32(buf + 352, 16, 0x123456), "test98 case 20 failed\n");
	FAILED(!check_simd_replicate_u32(buf + 368, 16, 0), "test98 case 21 failed\n");
	FAILED(!check_simd_replicate_u32(buf + 384, 16, 0xffffffff), "test98 case 22 failed\n");
	FAILED(!check_simd_replicate_u32(buf + 400, 16, 0), "test98 case 23 failed\n");
	FAILED(!check_simd_replicate_u32(buf + 416, 16, 0x340034), "test98 case 24 failed\n");
	FAILED(!check_simd_replicate_u32(buf + 432, 16, 0x45ff45ff), "test98 case 25 failed\n");
	FAILED(!check_simd_replicate_u32(buf + 448, 16, 0xb3), "test98 case 26 failed\n");
	FAILED(!check_simd_replicate_u32(buf + 464, 16, 0xffff46ff), "test98 case 27 failed\n");
	FAILED(!check_simd_replicate_u32(buf + 480, 16, 0x4c0000), "test98 case 28 failed\n");
	FAILED(!check_simd_replicate_u32(buf + 496, 16, 0x71ffffff), "test98 case 29 failed\n");
	FAILED(!check_simd_replicate_u32(buf + 512, 16, 0x9eff), "test98 case 30 failed\n");
	FAILED(!check_simd_replicate_u32(buf + 528, 16, 0xff070000), "test98 case 31 failed\n");

	if (supported[0]) {
		FAILED(!check_simd_replicate(buf + 544, 32, 1, 181), "test98 case 32 failed\n");
		FAILED(!check_simd_replicate(buf + 576, 32, 1, 0), "test98 case 33 failed\n");
		FAILED(!check_simd_replicate(buf + 608, 32, 2, 230), "test98 case 34 failed\n");
		FAILED(!check_simd_replicate(buf + 640, 32, 4, 204), "test98 case 35 failed\n");
#if IS_64BIT
		FAILED(!check_simd_replicate(buf + 672, 32, 8, 216), "test98 case 36 failed\n");
#endif /* IS_64BIT */
		FAILED(!check_simd_replicate(buf + 704, 32, 4, 220), "test98 case 37 failed\n");
		FAILED(!check_simd_replicate(buf + 736, 32, 8, 208), "test98 case 38 failed\n");
	}

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test99(void)
{
	/* Test simd replicate lane to all lanes. */
	executable_code code;
	struct sljit_compiler* compiler;
	sljit_s32 i, type;
	sljit_u8 supported[1];
	sljit_u8* buf;
	sljit_u8 data[63 + 992];
	sljit_s32 fs0 = SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS > 0 ? SLJIT_FS0 : SLJIT_FR5;

	if (verbose)
		printf("Run test99\n");

	if (!sljit_has_cpu_feature(SLJIT_HAS_SIMD)) {
		if (verbose)
			printf("no simd available, test99 skipped\n");
		successful_tests++;
		return;
	}

	/* Buffer is 64 byte aligned. */
	buf = (sljit_u8*)(((sljit_sw)data + (sljit_sw)63) & ~(sljit_sw)63);

	for (i = 0; i < 32; i++)
		buf[i] = (sljit_u8)(100 + i);

	for (i = 32; i < 992; i++)
		buf[i] = 0xaa;

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 4, 4, 6, SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS > 0 ? 1 : 0, 16);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_8;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR0, SLJIT_FR0, 0);
	/* buf[48] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 48);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 16);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 32);
	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR2, SLJIT_FR1, 12);
	/* buf[64] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 64);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 32);
	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR3, SLJIT_FR5, 6);
	/* buf[80] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 80);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 16);
	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR3, SLJIT_FR3, 9);
	/* buf[96] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 96);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, SLJIT_MEM1(SLJIT_S0), 32);
	sljit_emit_simd_lane_replicate(compiler, type, fs0, SLJIT_FR0, 10);
	/* buf[112] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 112);
	/* buf[128] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 128);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_16;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR0, SLJIT_FR0, 0);
	/* buf[144] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 144);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 16);
	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR1, SLJIT_FR1, 3);
	/* buf[160] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 160);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 16);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 32);
	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR0, SLJIT_FR4, 5);
	/* buf[176] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 176);
	/* buf[192] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 192);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_32;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR3, SLJIT_FR3, 0);
	/* buf[208] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 208);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 16);
	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR1, SLJIT_FR1, 2);
	/* buf[224] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 224);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 32);
	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR5, SLJIT_FR2, 3);
	/* buf[240] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 240);
	/* buf[256] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 256);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_64;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 16);
	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR1, SLJIT_FR1, 0);
	/* buf[272] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 272);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR0, SLJIT_FR0, 1);
	/* buf[288] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 288);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 16);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, SLJIT_MEM1(SLJIT_S0), 32);
	sljit_emit_simd_lane_replicate(compiler, type, fs0, SLJIT_FR3, 1);
	/* buf[304] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 304);
	/* buf[320] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 320);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_FLOAT | SLJIT_SIMD_ELEM_32;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR2, SLJIT_FR2, 0);
	/* buf[336] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 336);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 16);
	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR0, SLJIT_FR0, 3);
	/* buf[352] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 352);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 32);
	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR5, SLJIT_FR0, 1);
	/* buf[368] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 368);
	/* buf[384] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 384);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_FLOAT | SLJIT_SIMD_ELEM_64;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 16);
	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR4, SLJIT_FR4, 0);
	/* buf[400] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 400);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR1, SLJIT_FR1, 1);
	/* buf[416] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 416);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 16);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 32);
	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR0, SLJIT_FR2, 1);
	/* buf[432] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 432);
	/* buf[448] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 448);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_8;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 0);
	supported[0] = sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR2, SLJIT_FR2, 0) != SLJIT_ERR_UNSUPPORTED;
	/* buf[480] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 480);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR0, SLJIT_FR4, 13);
	/* buf[512] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 512);

	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR1, SLJIT_FR4, 6);
	/* buf[544] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 544);

	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR4, SLJIT_FR4, 28);
	/* buf[576] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 576);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_16;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR2, SLJIT_FR1, 0);
	/* buf[608] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 608);

	sljit_emit_simd_lane_replicate(compiler, type, fs0, SLJIT_FR1, 2);
	/* buf[640] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 640);

	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR1, SLJIT_FR1, 13);
	/* buf[672] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 672);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_32;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR0, SLJIT_FR5, 0);
	/* buf[704] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 704);

	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR5, SLJIT_FR5, 5);
	/* buf[736] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 736);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_64;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_lane_replicate(compiler, type, fs0, SLJIT_FR0, 0);
	/* buf[768] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 768);

	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR0, SLJIT_FR0, 1);
	/* buf[800] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 800);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_32 | SLJIT_SIMD_FLOAT;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR2, SLJIT_FR1, 0);
	/* buf[832] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 832);

	sljit_emit_simd_lane_replicate(compiler, type, fs0, SLJIT_FR1, 1);
	/* buf[864] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 864);

	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR1, SLJIT_FR1, 4);
	/* buf[896] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 896);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_64 | SLJIT_SIMD_FLOAT;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_lane_replicate(compiler, type, SLJIT_FR1, fs0, 0);
	/* buf[928] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 928);

	sljit_emit_simd_lane_replicate(compiler, type, fs0, fs0, 2);
	/* buf[960] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 960);

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)buf);

	FAILED(!check_simd_replicate(buf + 48, 16, 1, 100), "test99 case 1 failed\n");
	FAILED(!check_simd_replicate(buf + 64, 16, 1, 128), "test99 case 2 failed\n");
	FAILED(!check_simd_replicate(buf + 80, 16, 1, 106), "test99 case 3 failed\n");
	FAILED(!check_simd_replicate(buf + 96, 16, 1, 125), "test99 case 4 failed\n");
	FAILED(!check_simd_replicate(buf + 112, 16, 1, 110), "test99 case 5 failed\n");
	FAILED(!check_simd_replicate(buf + 128, 16, 16, 100), "test99 case 6 failed\n");
	FAILED(!check_simd_replicate(buf + 144, 16, 2, 100), "test99 case 7 failed\n");
	FAILED(!check_simd_replicate(buf + 160, 16, 2, 122), "test99 case 8 failed\n");
	FAILED(!check_simd_replicate(buf + 176, 16, 2, 126), "test99 case 9 failed\n");
	FAILED(!check_simd_replicate(buf + 192, 16, 16, 116), "test99 case 10 failed\n");
	FAILED(!check_simd_replicate(buf + 208, 16, 4, 100), "test99 case 11 failed\n");
	FAILED(!check_simd_replicate(buf + 224, 16, 4, 124), "test99 case 12 failed\n");
	FAILED(!check_simd_replicate(buf + 240, 16, 4, 112), "test99 case 13 failed\n");
	FAILED(!check_simd_replicate(buf + 256, 16, 16, 100), "test99 case 14 failed\n");
	FAILED(!check_simd_replicate(buf + 272, 16, 8, 116), "test99 case 15 failed\n");
	FAILED(!check_simd_replicate(buf + 288, 16, 8, 108), "test99 case 16 failed\n");
	FAILED(!check_simd_replicate(buf + 304, 16, 8, 124), "test99 case 17 failed\n");
	FAILED(!check_simd_replicate(buf + 320, 16, 16, 116), "test99 case 18 failed\n");
	FAILED(!check_simd_replicate(buf + 336, 16, 4, 100), "test99 case 19 failed\n");
	FAILED(!check_simd_replicate(buf + 352, 16, 4, 128), "test99 case 20 failed\n");
	FAILED(!check_simd_replicate(buf + 368, 16, 4, 104), "test99 case 21 failed\n");
	FAILED(!check_simd_replicate(buf + 384, 16, 16, 100), "test99 case 22 failed\n");
	FAILED(!check_simd_replicate(buf + 400, 16, 8, 116), "test99 case 23 failed\n");
	FAILED(!check_simd_replicate(buf + 416, 16, 8, 108), "test99 case 24 failed\n");
	FAILED(!check_simd_replicate(buf + 432, 16, 8, 124), "test99 case 25 failed\n");
	FAILED(!check_simd_replicate(buf + 448, 16, 16, 116), "test99 case 26 failed\n");

	if (supported[0]) {
		FAILED(!check_simd_replicate(buf + 480, 32, 1, 100), "test99 case 27 failed\n");
		FAILED(!check_simd_replicate(buf + 512, 32, 1, 113), "test99 case 28 failed\n");
		FAILED(!check_simd_replicate(buf + 544, 32, 1, 106), "test99 case 29 failed\n");
		FAILED(!check_simd_replicate(buf + 576, 32, 1, 128), "test99 case 30 failed\n");
		FAILED(!check_simd_replicate(buf + 608, 32, 2, 100), "test99 case 31 failed\n");
		FAILED(!check_simd_replicate(buf + 640, 32, 2, 104), "test99 case 32 failed\n");
		FAILED(!check_simd_replicate(buf + 672, 32, 2, 126), "test99 case 33 failed\n");
		FAILED(!check_simd_replicate(buf + 704, 32, 4, 100), "test99 case 34 failed\n");
		FAILED(!check_simd_replicate(buf + 736, 32, 4, 120), "test99 case 35 failed\n");
		FAILED(!check_simd_replicate(buf + 768, 32, 8, 100), "test99 case 36 failed\n");
		FAILED(!check_simd_replicate(buf + 800, 32, 8, 108), "test99 case 37 failed\n");
		FAILED(!check_simd_replicate(buf + 832, 32, 4, 100), "test99 case 38 failed\n");
		FAILED(!check_simd_replicate(buf + 864, 32, 4, 104), "test99 case 39 failed\n");
		FAILED(!check_simd_replicate(buf + 896, 32, 4, 116), "test99 case 40 failed\n");
		FAILED(!check_simd_replicate(buf + 928, 32, 8, 100), "test99 case 41 failed\n");
		FAILED(!check_simd_replicate(buf + 960, 32, 8, 116), "test99 case 42 failed\n");
	}

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static sljit_s32 check_simd_lane_mov_zero(sljit_u8* buf, sljit_s32 length, sljit_s32 elem_size, sljit_s32 start, sljit_s32 value)
{
	sljit_s32 i;

	for (i = 0; i < start; i++)
		if (*buf++ != 0)
			return 0;

	for (i = 0; i < elem_size; i++)
		if (*buf++ != value++)
			return 0;

	for (i = start + elem_size; i < length; i++)
		if (*buf++ != 0)
			return 0;

	return 1;
}

static void test100(void)
{
	/* Test simd zero register before move to lane. */
	executable_code code;
	struct sljit_compiler* compiler;
	sljit_s32 i, type;
	sljit_u8 supported[1];
	sljit_u8* buf;
	sljit_u8 data[63 + 672];
	sljit_s32 fs0 = SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS > 0 ? SLJIT_FS0 : SLJIT_FR5;

	if (verbose)
		printf("Run test100\n");

	if (!sljit_has_cpu_feature(SLJIT_HAS_SIMD)) {
		if (verbose)
			printf("no simd available, test100 skipped\n");
		successful_tests++;
		return;
	}

	/* Buffer is 64 byte aligned. */
	buf = (sljit_u8*)(((sljit_sw)data + (sljit_sw)63) & ~(sljit_sw)63);

	for (i = 0; i < 64; i++)
		buf[i] = (sljit_u8)(100 + i);

	for (i = 64; i < 672; i++)
		buf[i] = 0xaa;

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 4, 4, 6, SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS > 0 ? 1 : 0, 16);
	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_IMM, 100000);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S0, 0, SLJIT_IMM, 10000);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_8;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 0xffff00 + 85);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | type, SLJIT_FR0, 0, SLJIT_R2, 0);
	/* buf[64] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 64);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | type, fs0, 0, SLJIT_IMM, 0xffff00 + 18);
	/* buf[80] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 80);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op1(compiler, SLJIT_MOV_U8, SLJIT_MEM1(SLJIT_SP), 10, SLJIT_IMM, 170);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | SLJIT_32 | type, SLJIT_FR5, 5, SLJIT_MEM1(SLJIT_SP), 10);
	/* buf[96] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 96);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_16;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_MEM1(SLJIT_S0), 8);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | type, SLJIT_FR1, 0, SLJIT_S2, 0);
	/* buf[112] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 112);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | SLJIT_32 | type, SLJIT_FR4, 0, SLJIT_IMM, 0xff8382);
	/* buf[128] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 128);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | type, fs0, 3, SLJIT_MEM1(SLJIT_R0), 100004);
	/* buf[144] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 144);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_32;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_S0), 4);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | SLJIT_32 | type, SLJIT_FR2, 0, SLJIT_R2, 0);
	/* buf[160] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 160);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | type, SLJIT_FR5, 0, SLJIT_IMM, 0x29282726);
	/* buf[176] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 176);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 3);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | SLJIT_32 | type, SLJIT_FR1, 0, SLJIT_MEM2(SLJIT_S0, SLJIT_R2), 2);
	/* buf[192] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 192);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | type, SLJIT_FR3, 3, SLJIT_MEM1(SLJIT_R1), -10000 + 8);
	/* buf[208] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 208);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_64;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | type, fs0, 0, SLJIT_S2, 0);
	/* buf[224] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 224);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | type, SLJIT_FR4, 0, SLJIT_IMM, WCONST(0xe3e2e1e0dfdedddc, 0));
	/* buf[240] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 240);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 8);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | type, SLJIT_FR3, 0, SLJIT_MEM2(SLJIT_S0, SLJIT_R2), 0);
	/* buf[256] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 256);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | type, SLJIT_FR1, 1, SLJIT_MEM1(SLJIT_R0), 100000);
	/* buf[272] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 272);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_FLOAT | SLJIT_SIMD_ELEM_32;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S0), 12);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | type, SLJIT_FR0, 0, SLJIT_FR0, 0);
	/* buf[288] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 288);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR5, 0, SLJIT_MEM1(SLJIT_S0), 4);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | type, SLJIT_FR2, 0, SLJIT_FR5, 0);
	/* buf[304] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 304);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 1);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | type, SLJIT_FR1, 0, SLJIT_MEM2(SLJIT_S0, SLJIT_R2), 3);
	/* buf[320] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 320);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR4, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | type, SLJIT_FR4, 1, SLJIT_FR4, 0);
	/* buf[336] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 336);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_FLOAT | SLJIT_SIMD_ELEM_64;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S0), 8);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | type, SLJIT_FR2, 0, SLJIT_FR2, 0);
	/* buf[352] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 352);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR4, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | type, SLJIT_FR3, 0, SLJIT_FR4, 0);
	/* buf[368] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 368);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | type, SLJIT_FR1, 0, SLJIT_MEM0(), (sljit_sw)(buf + 8));
	/* buf[384] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 384);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | type, SLJIT_FR2, 1, SLJIT_FR2, 0);
	/* buf[400] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 400);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_8;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 215);
	supported[0] = sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | type, SLJIT_FR0, 0, SLJIT_R2, 0) != SLJIT_ERR_UNSUPPORTED;
	/* buf[416] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 416);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | type, fs0, 17, SLJIT_IMM, 78);
	/* buf[448] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 448);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_16;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, 0xff3433);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | type, SLJIT_FR1, 4, SLJIT_S1, 0);
	/* buf[480] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 480);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_32;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | type, SLJIT_FR5, 5, SLJIT_MEM1(SLJIT_S0), 60);
	/* buf[512] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 512);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_64;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | type, SLJIT_FR4, 3, SLJIT_MEM0(), (sljit_sw)buf + 32);
	/* buf[544] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 544);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_32 | SLJIT_SIMD_FLOAT;
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S0), 48);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | type, SLJIT_FR2, 3, SLJIT_FR2, 0);
	/* buf[576] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 576);

	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR3, 0, SLJIT_MEM1(SLJIT_S0), 8);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | type, SLJIT_FR3, 6, SLJIT_FR3, 0);
	/* buf[608] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 608);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_64 | SLJIT_SIMD_FLOAT;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_SP), 8, SLJIT_MEM1(SLJIT_S0), 40);
	sljit_emit_simd_lane_mov(compiler, SLJIT_SIMD_LANE_ZERO | type, SLJIT_FR0, 3, SLJIT_MEM1(SLJIT_SP), 8);
	/* buf[640] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 640);

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)buf);

	FAILED(!check_simd_lane_mov_zero(buf + 64, 16, 1, 0, 85), "test100 case 1 failed\n");
	FAILED(!check_simd_lane_mov_zero(buf + 80, 16, 1, 0, 18), "test100 case 2 failed\n");
	FAILED(!check_simd_lane_mov_zero(buf + 96, 16, 1, 5, 170), "test100 case 3 failed\n");
	FAILED(!check_simd_lane_mov_zero(buf + 112, 16, 2, 0, 108), "test100 case 4 failed\n");
	FAILED(!check_simd_lane_mov_zero(buf + 128, 16, 2, 0, 130), "test100 case 5 failed\n");
	FAILED(!check_simd_lane_mov_zero(buf + 144, 16, 2, 6, 104), "test100 case 6 failed\n");
	FAILED(!check_simd_lane_mov_zero(buf + 160, 16, 4, 0, 104), "test100 case 7 failed\n");
	FAILED(!check_simd_lane_mov_zero(buf + 176, 16, 4, 0, 38), "test100 case 8 failed\n");
	FAILED(!check_simd_lane_mov_zero(buf + 192, 16, 4, 0, 112), "test100 case 9 failed\n");
	FAILED(!check_simd_lane_mov_zero(buf + 208, 16, 4, 12, 108), "test100 case 10 failed\n");
#if IS_64BIT
	FAILED(!check_simd_lane_mov_zero(buf + 224, 16, 8, 0, 100), "test100 case 11 failed\n");
	FAILED(!check_simd_lane_mov_zero(buf + 240, 16, 8, 0, 220), "test100 case 12 failed\n");
	FAILED(!check_simd_lane_mov_zero(buf + 256, 16, 8, 0, 108), "test100 case 13 failed\n");
	FAILED(!check_simd_lane_mov_zero(buf + 272, 16, 8, 8, 100), "test100 case 14 failed\n");
#endif /* IS_64BIT */
	FAILED(!check_simd_lane_mov_zero(buf + 288, 16, 4, 0, 112), "test100 case 15 failed\n");
	FAILED(!check_simd_lane_mov_zero(buf + 304, 16, 4, 0, 104), "test100 case 16 failed\n");
	FAILED(!check_simd_lane_mov_zero(buf + 320, 16, 4, 0, 108), "test100 case 17 failed\n");
	FAILED(!check_simd_lane_mov_zero(buf + 336, 16, 4, 4, 100), "test100 case 18 failed\n");
	FAILED(!check_simd_lane_mov_zero(buf + 352, 16, 8, 0, 108), "test100 case 19 failed\n");
	FAILED(!check_simd_lane_mov_zero(buf + 368, 16, 8, 0, 100), "test100 case 20 failed\n");
	FAILED(!check_simd_lane_mov_zero(buf + 384, 16, 8, 0, 108), "test100 case 21 failed\n");
	FAILED(!check_simd_lane_mov_zero(buf + 400, 16, 8, 8, 100), "test100 case 22 failed\n");

	if (supported[0]) {
		FAILED(!check_simd_lane_mov_zero(buf + 416, 32, 1, 0, 215), "test100 case 23 failed\n");
		FAILED(!check_simd_lane_mov_zero(buf + 448, 32, 1, 17, 78), "test100 case 24 failed\n");
		FAILED(!check_simd_lane_mov_zero(buf + 480, 32, 2, 8, 51), "test100 case 25 failed\n");
		FAILED(!check_simd_lane_mov_zero(buf + 512, 32, 4, 20, 160), "test100 case 26 failed\n");
#if IS_64BIT
		FAILED(!check_simd_lane_mov_zero(buf + 544, 32, 8, 24, 132), "test100 case 27 failed\n");
#endif /* IS_64BIT */
		FAILED(!check_simd_lane_mov_zero(buf + 576, 32, 4, 12, 148), "test100 case 28 failed\n");
		FAILED(!check_simd_lane_mov_zero(buf + 608, 32, 4, 24, 108), "test100 case 29 failed\n");
		FAILED(!check_simd_lane_mov_zero(buf + 640, 32, 8, 24, 140), "test100 case 30 failed\n");
	}

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void init_simd_extend(sljit_u8* buf, sljit_s32 length, sljit_s32 elem_size, sljit_s32 is_float, sljit_s32 data)
{
	sljit_u8* end = buf + length;

	do {
		if (elem_size == 0)
			*buf = (sljit_u8)data;
		else if (elem_size == 1)
			*(sljit_u16*)buf = (sljit_u16)data;
		else if (!is_float)
			*(sljit_u32*)buf = (sljit_u32)data;
		else
			*(sljit_f32*)buf = (sljit_f32)data;

		buf += 1 << elem_size;
		data++;
	} while (buf < end);
}

static sljit_s32 check_simd_extend_unsigned(sljit_u8* buf, sljit_s32 length, sljit_s32 elem_size, sljit_u32 mask)
{
	sljit_s32 data;
	sljit_u8* end = buf + length;

	if (elem_size == 1)
		data = -(length >> 2);
	else if (elem_size == 2)
		data = -(length >> 3);
	else
		data = -(length >> 4);

	do {
		if (elem_size == 1) {
			if (*(sljit_u16*)buf != ((sljit_u16)data & mask))
				return 0;
		} else {
			if (*(sljit_u32*)buf != ((sljit_u32)data & mask))
				return 0;
			if (elem_size == 3 && *(sljit_u32*)(buf + 4) != 0)
				return 0;
		}

		buf += 1 << elem_size;
		data++;
	} while (buf < end);

	return 1;
}

static sljit_s32 check_simd_extend_signed(sljit_u8* buf, sljit_s32 length, sljit_s32 elem_size, sljit_s32 is_float)
{
	sljit_s32 data;
	sljit_u8* end = buf + length;

	if (elem_size == 1)
		data = -(length >> 2);
	else if (elem_size == 2)
		data = -(length >> 3);
	else if (!is_float)
		data = -(length >> 4);
	else
		data = 1000;

	do {
		if (elem_size == 1) {
			if (*(sljit_s16*)buf != data)
				return 0;
		} else if (elem_size == 2) {
			if (*(sljit_s32*)buf != data)
				return 0;
		} else if (!is_float) {
			if (*(sljit_s32*)buf != data)
				return 0;
			if (*(sljit_s32*)(buf + 4) != (data >> 31))
				return 0;
		} else {
			if (*(sljit_f64*)buf != (sljit_f64)data)
				return 0;
		}

		buf += 1 << elem_size;
		data++;
	} while (buf < end);

	return 1;
}

static void test101(void)
{
	/* Test simd extension operation. */
	executable_code code;
	struct sljit_compiler* compiler;
	sljit_s32 i, type;
	sljit_u8 supported[1];
	sljit_u8* buf;
	sljit_u8 data[63 + 1088];
	sljit_s32 fs0 = SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS > 0 ? SLJIT_FS0 : SLJIT_FR5;

	if (verbose)
		printf("Run test101\n");

	if (!sljit_has_cpu_feature(SLJIT_HAS_SIMD)) {
		if (verbose)
			printf("no simd available, test101 skipped\n");
		successful_tests++;
		return;
	}

	/* Buffer is 64 byte aligned. */
	buf = (sljit_u8*)(((sljit_sw)data + (sljit_sw)63) & ~(sljit_sw)63);

	for (i = 0; i < 1088; i++)
		buf[i] = 0xaa;

	init_simd_extend(buf + 0, 16, 0, 0, -8);
	init_simd_extend(buf + 32, 16, 1, 0, -4);
	init_simd_extend(buf + 64, 16, 2, 0, -2);
	init_simd_extend(buf + 96, 16, 2, 1, 1000);
	init_simd_extend(buf + 128, 8, 0, 0, -4);
	init_simd_extend(buf + 160, 8, 1, 0, -2);
	init_simd_extend(buf + 192, 8, 2, 0, -1);
	init_simd_extend(buf + 224, 8, 2, 1, 1000);
	init_simd_extend(buf + 256, 4, 0, 0, -2);
	init_simd_extend(buf + 288, 4, 1, 0, -1);
	init_simd_extend(buf + 320, 2, 0, 0, -1);

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 4, 4, 6, SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS > 0 ? 1 : 0, 32);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_8 | SLJIT_SIMD_EXTEND_16;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 128);
	sljit_emit_simd_extend(compiler, type, SLJIT_FR2, SLJIT_FR0, 0);
	/* buf[352] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 352);

	sljit_emit_simd_extend(compiler, type | SLJIT_SIMD_EXTEND_SIGNED, SLJIT_FR1, SLJIT_FR0, 0);
	/* buf[368] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 368);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_S0, 0, SLJIT_IMM, 128);
	sljit_emit_simd_extend(compiler, type, SLJIT_FR0, SLJIT_MEM1(SLJIT_R1), 0);
	/* buf[384] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 384);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 128);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_SP), 0);
	sljit_emit_simd_extend(compiler, type | SLJIT_SIMD_EXTEND_SIGNED, fs0, SLJIT_MEM1(SLJIT_SP), 0);
	/* buf[400] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 400);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_16 | SLJIT_SIMD_EXTEND_32;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 160);
	sljit_emit_simd_extend(compiler, type, SLJIT_FR4, SLJIT_FR4, 0);
	/* buf[416] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 416);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 160);
	sljit_emit_simd_extend(compiler, type | SLJIT_SIMD_EXTEND_SIGNED, SLJIT_FR0, SLJIT_FR4, 0);
	/* buf[432] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 432);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 40);
	sljit_emit_simd_extend(compiler, type, SLJIT_FR1, SLJIT_MEM2(SLJIT_S0, SLJIT_R2), 2);
	/* buf[448] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 448);

	sljit_emit_simd_extend(compiler, type | SLJIT_SIMD_EXTEND_SIGNED, fs0, SLJIT_MEM0(), (sljit_sw)(buf + 160));
	/* buf[464] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 464);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_32 | SLJIT_SIMD_EXTEND_64;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 192);
	sljit_emit_simd_extend(compiler, type, SLJIT_FR0, SLJIT_FR2, 0);
	/* buf[480] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 480);

	sljit_emit_simd_extend(compiler, type | SLJIT_SIMD_EXTEND_SIGNED, SLJIT_FR3, SLJIT_FR2, 0);
	/* buf[496] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 496);

	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_IMM, 10000 - 192);
	sljit_emit_simd_extend(compiler, type, SLJIT_FR2, SLJIT_MEM1(SLJIT_R0), 10000);
	/* buf[512] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 512);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_IMM, 100000 + 192);
	sljit_emit_simd_extend(compiler, type | SLJIT_SIMD_EXTEND_SIGNED, fs0, SLJIT_MEM1(SLJIT_R0), -100000);
	/* buf[528] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 528);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_32 | SLJIT_SIMD_FLOAT | SLJIT_SIMD_EXTEND_64;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, SLJIT_MEM1(SLJIT_S0), 224);
	sljit_emit_simd_extend(compiler, type, fs0, fs0, 0);
	/* buf[544] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 544);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 224);
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_SP), 0);
	sljit_emit_simd_extend(compiler, type, SLJIT_FR3, SLJIT_MEM1(SLJIT_SP), 0);
	/* buf[560] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 560);

	sljit_emit_simd_extend(compiler, type, SLJIT_FR5, SLJIT_FR1, 0);
	/* buf[576] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR5, SLJIT_MEM1(SLJIT_S0), 576);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_8 | SLJIT_SIMD_EXTEND_32;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 256);
	sljit_emit_simd_extend(compiler, type, SLJIT_FR0, SLJIT_FR2, 0);
	/* buf[592] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 592);

	sljit_emit_simd_extend(compiler, type | SLJIT_SIMD_EXTEND_SIGNED, SLJIT_FR2, SLJIT_FR2, 0);
	/* buf[608] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 608);

	sljit_emit_simd_extend(compiler, type, fs0, SLJIT_MEM1(SLJIT_S0), 256);
	/* buf[624] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 624);

	sljit_emit_simd_extend(compiler, type | SLJIT_SIMD_EXTEND_SIGNED, SLJIT_FR4, SLJIT_MEM0(), (sljit_sw)(buf + 256));
	/* buf[640] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 640);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_8 | SLJIT_SIMD_EXTEND_64;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, SLJIT_MEM1(SLJIT_S0), 320);
	sljit_emit_simd_extend(compiler, type, SLJIT_FR0, fs0, 0);
	/* buf[656] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 656);

	sljit_emit_simd_extend(compiler, type | SLJIT_SIMD_EXTEND_SIGNED, SLJIT_FR0, fs0, 0);
	/* buf[672] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 672);

	sljit_emit_op1(compiler, SLJIT_MOV_U16, SLJIT_MEM1(SLJIT_SP), 0, SLJIT_MEM1(SLJIT_S0), 320);
	sljit_emit_simd_extend(compiler, type, SLJIT_FR3, SLJIT_MEM1(SLJIT_SP), 0);
	/* buf[688] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 688);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S2, 0, SLJIT_IMM, 320);
	sljit_emit_simd_extend(compiler, type | SLJIT_SIMD_EXTEND_SIGNED, SLJIT_FR3, SLJIT_MEM2(SLJIT_S0, SLJIT_S2), 0);
	/* buf[704] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 704);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_16 | SLJIT_SIMD_EXTEND_64;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 288);
	sljit_emit_simd_extend(compiler, type, SLJIT_FR2, SLJIT_FR0, 0);
	/* buf[720] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 720);

	sljit_emit_simd_extend(compiler, type | SLJIT_SIMD_EXTEND_SIGNED, SLJIT_FR0, SLJIT_FR0, 0);
	/* buf[736] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 736);

	sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_R2, 0, SLJIT_S0, 0, SLJIT_IMM, 100000 - 288);
	sljit_emit_simd_extend(compiler, type, fs0, SLJIT_MEM1(SLJIT_R2), 100000);
	/* buf[752] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 752);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_S2, 0, SLJIT_S0, 0, SLJIT_IMM, 10000 + 288);
	sljit_emit_simd_extend(compiler, type | SLJIT_SIMD_EXTEND_SIGNED, SLJIT_FR1, SLJIT_MEM1(SLJIT_S2), -10000);
	/* buf[768] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 768);

	type = SLJIT_SIMD_REG_64 | SLJIT_SIMD_ELEM_8 | SLJIT_SIMD_EXTEND_16;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 256);
	sljit_emit_simd_extend(compiler, type, fs0, SLJIT_FR1, 0);
	/* buf[784] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 784);

	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R2, 0, SLJIT_S0, 0, SLJIT_IMM, 256);
	sljit_emit_simd_extend(compiler, type | SLJIT_SIMD_EXTEND_SIGNED, SLJIT_FR2, SLJIT_MEM1(SLJIT_R2), 0);
	/* buf[792] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 792);

	type = SLJIT_SIMD_REG_64 | SLJIT_SIMD_ELEM_8 | SLJIT_SIMD_EXTEND_32;
	sljit_emit_simd_extend(compiler, type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 320);
	/* buf[800] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 800);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, SLJIT_MEM1(SLJIT_S0), 320);
	sljit_emit_simd_extend(compiler, type | SLJIT_SIMD_EXTEND_SIGNED, SLJIT_FR2, fs0, 0);
	/* buf[808] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 808);

	type = SLJIT_SIMD_REG_64 | SLJIT_SIMD_ELEM_16 | SLJIT_SIMD_EXTEND_32;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 288);
	sljit_emit_simd_extend(compiler, type, SLJIT_FR2, SLJIT_FR1, 0);
	/* buf[816] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 816);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, 288);
	sljit_emit_simd_extend(compiler, type | SLJIT_SIMD_EXTEND_SIGNED, fs0, SLJIT_MEM2(SLJIT_S1, SLJIT_S0), 0);
	/* buf[824] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 824);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_8 | SLJIT_SIMD_EXTEND_16;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 0);
	supported[0] = sljit_emit_simd_extend(compiler, type, SLJIT_FR4, SLJIT_FR1, 0) != SLJIT_ERR_UNSUPPORTED;
	/* buf[832] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 832);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_8 | SLJIT_SIMD_EXTEND_32;
	sljit_emit_simd_extend(compiler, type | SLJIT_SIMD_EXTEND_SIGNED, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 128);
	/* buf[864] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 864);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_8 | SLJIT_SIMD_EXTEND_64;
	sljit_emit_simd_extend(compiler, type, fs0, SLJIT_MEM0(), (sljit_sw)(buf + 256));
	/* buf[896] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S0), 896);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_16 | SLJIT_SIMD_EXTEND_32;
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_S0, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, 16);
	sljit_emit_simd_extend(compiler, type | SLJIT_SIMD_EXTEND_SIGNED, SLJIT_FR0, SLJIT_MEM2(SLJIT_R1, SLJIT_R2), 1);
	/* buf[928] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 928);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_16 | SLJIT_SIMD_EXTEND_64;
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_S1, 0, SLJIT_S0, 0, SLJIT_IMM, 100000 + 160);
	sljit_emit_simd_extend(compiler, type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S1), -100000);
	/* buf[960] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 960);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_32 | SLJIT_SIMD_EXTEND_64;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, SLJIT_MEM1(SLJIT_S0), 64);
	sljit_emit_simd_extend(compiler, type | SLJIT_SIMD_EXTEND_SIGNED, SLJIT_FR0, fs0, 0);
	/* buf[992] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 992);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_32 | SLJIT_SIMD_EXTEND_64 | SLJIT_SIMD_FLOAT;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 96);
	sljit_emit_simd_extend(compiler, type, SLJIT_FR2, SLJIT_FR2, 0);
	/* buf[1024] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 1024);

	sljit_emit_simd_extend(compiler, type, SLJIT_FR4, SLJIT_MEM0(), (sljit_sw)(buf + 96));
	/* buf[1056] */
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 1056);

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)buf);

	FAILED(!check_simd_extend_unsigned(buf + 352, 16, 1, 0xff), "test101 case 1 failed\n");
	FAILED(!check_simd_extend_signed(buf + 368, 16, 1, 0), "test101 case 2 failed\n");
	FAILED(!check_simd_extend_unsigned(buf + 384, 16, 1, 0xff), "test101 case 3 failed\n");
	FAILED(!check_simd_extend_signed(buf + 400, 16, 1, 0), "test101 case 4 failed\n");
	FAILED(!check_simd_extend_unsigned(buf + 416, 16, 2, 0xffff), "test101 case 5 failed\n");
	FAILED(!check_simd_extend_signed(buf + 432, 16, 2, 0), "test101 case 6 failed\n");
	FAILED(!check_simd_extend_unsigned(buf + 448, 16, 2, 0xffff), "test101 case 7 failed\n");
	FAILED(!check_simd_extend_signed(buf + 464, 16, 2, 0), "test101 case 8 failed\n");
	FAILED(!check_simd_extend_unsigned(buf + 480, 16, 3, 0xffffffff), "test101 case 9 failed\n");
	FAILED(!check_simd_extend_signed(buf + 496, 16, 3, 0), "test101 case 10 failed\n");
	FAILED(!check_simd_extend_unsigned(buf + 512, 16, 3, 0xffffffff), "test101 case 11 failed\n");
	FAILED(!check_simd_extend_signed(buf + 528, 16, 3, 0), "test101 case 12 failed\n");
	FAILED(!check_simd_extend_signed(buf + 544, 16, 3, 1), "test101 case 13 failed\n");
	FAILED(!check_simd_extend_signed(buf + 560, 16, 3, 1), "test101 case 14 failed\n");
	FAILED(!check_simd_extend_signed(buf + 576, 16, 3, 1), "test101 case 15 failed\n");
	FAILED(!check_simd_extend_unsigned(buf + 592, 16, 2, 0xff), "test101 case 16 failed\n");
	FAILED(!check_simd_extend_signed(buf + 608, 16, 2, 0), "test101 case 17 failed\n");
	FAILED(!check_simd_extend_unsigned(buf + 624, 16, 2, 0xff), "test101 case 18 failed\n");
	FAILED(!check_simd_extend_signed(buf + 640, 16, 2, 0), "test101 case 19 failed\n");
	FAILED(!check_simd_extend_unsigned(buf + 656, 16, 3, 0xff), "test101 case 20 failed\n");
	FAILED(!check_simd_extend_signed(buf + 672, 16, 3, 0), "test101 case 21 failed\n");
	FAILED(!check_simd_extend_unsigned(buf + 688, 16, 3, 0xff), "test101 case 22 failed\n");
	FAILED(!check_simd_extend_signed(buf + 704, 16, 3, 0), "test101 case 23 failed\n");
	FAILED(!check_simd_extend_unsigned(buf + 720, 16, 3, 0xffff), "test101 case 24 failed\n");
	FAILED(!check_simd_extend_signed(buf + 736, 16, 3, 0), "test101 case 25 failed\n");
	FAILED(!check_simd_extend_unsigned(buf + 752, 16, 3, 0xffff), "test101 case 26 failed\n");
	FAILED(!check_simd_extend_signed(buf + 768, 16, 3, 0), "test101 case 27 failed\n");

#if IS_ARM
	FAILED(!check_simd_extend_unsigned(buf + 784, 8, 1, 0xff), "test101 case 28 failed\n");
	FAILED(!check_simd_extend_signed(buf + 792, 8, 1, 0), "test101 case 29 failed\n");
	FAILED(!check_simd_extend_unsigned(buf + 800, 8, 2, 0xff), "test101 case 30 failed\n");
	FAILED(!check_simd_extend_signed(buf + 808, 8, 2, 0), "test101 case 31 failed\n");
	FAILED(!check_simd_extend_unsigned(buf + 816, 8, 2, 0xffff), "test101 case 32 failed\n");
	FAILED(!check_simd_extend_signed(buf + 824, 8, 2, 0), "test101 case 33 failed\n");
#endif /* IS_ARM */

	if (supported[0]) {
		FAILED(!check_simd_extend_unsigned(buf + 832, 32, 1, 0xff), "test101 case 34 failed\n");
		FAILED(!check_simd_extend_signed(buf + 864, 32, 2, 0), "test101 case 35 failed\n");
		FAILED(!check_simd_extend_unsigned(buf + 896, 32, 3, 0xff), "test101 case 36 failed\n");
		FAILED(!check_simd_extend_signed(buf + 928, 32, 2, 0), "test101 case 37 failed\n");
		FAILED(!check_simd_extend_unsigned(buf + 960, 32, 3, 0xffff), "test101 case 38 failed\n");
		FAILED(!check_simd_extend_signed(buf + 992, 32, 3, 0), "test101 case 39 failed\n");
		FAILED(!check_simd_extend_signed(buf + 1024, 32, 3, 1), "test101 case 40 failed\n");
		FAILED(!check_simd_extend_signed(buf + 1056, 32, 3, 1), "test101 case 41 failed\n");
	}

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void init_simd_sign(sljit_u8* buf, sljit_s32 length, sljit_s32 elem_size, sljit_u32 data)
{
	sljit_u8* end = buf + length;

	do {
		if (elem_size == 0)
			*buf = (sljit_u8)(((data & 0x1) << 7) + 0x7f);
		else if (elem_size == 1)
			*(sljit_u16*)buf = (sljit_u16)(((data & 0x1) << 15) + 0x7fff);
		else if (elem_size == 2)
			*(sljit_u32*)buf = (sljit_u32)(((data & 0x1) << 31) + 0x7fffffff);
		else {
			*(sljit_u32*)buf = 0xffffffff;
			*(sljit_u32*)(buf + 4) = (sljit_u32)(((data & 0x1) << 31) + 0x7fffffff);
		}

		data >>= 1;
		buf += 1 << elem_size;
	} while (buf < end);
}

static void test102(void)
{
	/* Test simd sign extraction operation. */
	executable_code code;
	struct sljit_compiler* compiler;
	sljit_s32 i, type;
	sljit_u8 supported[1];
	sljit_u8* buf;
	sljit_u8 data[63 + 288];
	sljit_s32 fs0 = SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS > 0 ? SLJIT_FS0 : SLJIT_FR5;
	sljit_uw resw[9];
	sljit_u32 res32[7];

	if (verbose)
		printf("Run test102\n");

	if (!sljit_has_cpu_feature(SLJIT_HAS_SIMD)) {
		if (verbose)
			printf("no simd available, test102 skipped\n");
		successful_tests++;
		return;
	}

	/* Buffer is 64 byte aligned. */
	buf = (sljit_u8*)(((sljit_sw)data + (sljit_sw)63) & ~(sljit_sw)63);

	for (i = 0; i < 9; i++)
		resw[i] = (sljit_uw)-1;
	for (i = 0; i < 7; i++)
		res32[i] = (sljit_u32)-1;

	init_simd_sign(buf + 0, 16, 0, 0x8fa3);
	init_simd_sign(buf + 16, 16, 0, 0x34d5);
	init_simd_sign(buf + 32, 16, 1, 0xa6);
	init_simd_sign(buf + 48, 16, 1, 0x5e);
	init_simd_sign(buf + 64, 16, 2, 0xd);
	init_simd_sign(buf + 80, 16, 2, 0x5);
	init_simd_sign(buf + 96, 16, 3, 0x2);
	init_simd_sign(buf + 112, 16, 3, 0x1);

	init_simd_sign(buf + 128, 8, 0, 0x45);
	init_simd_sign(buf + 136, 8, 1, 0x9);
	init_simd_sign(buf + 144, 8, 2, 0x1);

	init_simd_sign(buf + 160, 32, 0, 0x51e83b71);
	init_simd_sign(buf + 192, 32, 1, 0xc90d);
	init_simd_sign(buf + 224, 32, 2, 0xa5);
	init_simd_sign(buf + 256, 32, 3, 0x9);

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	sljit_emit_enter(compiler, 0, SLJIT_ARGS3(VOID, P, P, P), 4, 4, 6, SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS > 0 ? 1 : 0, 16);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_8;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_simd_sign(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR0, SLJIT_R0, 0);
	/* resw[0] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_R0, 0);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, SLJIT_MEM1(SLJIT_S0), 16);
	/* resw[1] */
	sljit_emit_simd_sign(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S1), sizeof(sljit_uw));

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_16;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 32);
	sljit_emit_simd_sign(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, SLJIT_R2, 0);
	/* resw[2] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 2 * sizeof(sljit_uw), SLJIT_R2, 0);

	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR4, SLJIT_MEM1(SLJIT_S0), 48);
	sljit_emit_simd_sign(compiler, SLJIT_SIMD_STORE | type | SLJIT_32, SLJIT_FR4, SLJIT_MEM1(SLJIT_SP), 4);
	/* res32[0] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S2), 0, SLJIT_MEM1(SLJIT_SP), 4);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_32;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, SLJIT_MEM1(SLJIT_S0), 64);
	sljit_emit_simd_sign(compiler, SLJIT_SIMD_STORE | type | SLJIT_32, fs0, SLJIT_R1, 0);
	/* res32[1] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S2), sizeof(sljit_u32), SLJIT_R1, 0);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_32 | SLJIT_SIMD_FLOAT;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR1, SLJIT_MEM1(SLJIT_S0), 80);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 4);
	/* res32[2] */
	sljit_emit_simd_sign(compiler, SLJIT_SIMD_STORE | type | SLJIT_32, SLJIT_FR1, SLJIT_MEM2(SLJIT_S2, SLJIT_R1), 1);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_64 | SLJIT_SIMD_FLOAT;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 96);
	sljit_emit_simd_sign(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_S3, 0);
	/* resw[3] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 3 * sizeof(sljit_uw), SLJIT_S3, 0);

	type = SLJIT_SIMD_REG_128 | SLJIT_SIMD_ELEM_64;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, SLJIT_MEM1(SLJIT_S0), 112);
	/* resw[4] */
	sljit_emit_simd_sign(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM0(), (sljit_sw)(resw + 4));

	type = SLJIT_SIMD_REG_64 | SLJIT_SIMD_ELEM_8;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 128);
	sljit_emit_simd_sign(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR2, SLJIT_R0, 0);
	/* resw[5] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S1), 5 * sizeof(sljit_uw), SLJIT_R0, 0);

	type = SLJIT_SIMD_REG_64 | SLJIT_SIMD_ELEM_16;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 136);
	/* res32[4] */
	sljit_emit_simd_sign(compiler, SLJIT_SIMD_STORE | type | SLJIT_32, SLJIT_FR0, SLJIT_MEM1(SLJIT_S2), 4 * sizeof(sljit_u32));

	type = SLJIT_SIMD_REG_64 | SLJIT_SIMD_ELEM_32;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, SLJIT_MEM1(SLJIT_S0), 144);
	/* resw[6] */
	sljit_emit_simd_sign(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S1), 6 * sizeof(sljit_uw));

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_8;
	supported[0] = sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR2, SLJIT_MEM1(SLJIT_S0), 160) != SLJIT_ERR_UNSUPPORTED;
	sljit_emit_simd_sign(compiler, SLJIT_SIMD_STORE | type | SLJIT_32, SLJIT_FR2, SLJIT_R2, 0);
	/* res32[5] */
	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S2), 5 * sizeof(sljit_u32), SLJIT_R2, 0);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_16;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, fs0, SLJIT_MEM1(SLJIT_S0), 192);
	/* resw[7] */
	sljit_emit_simd_sign(compiler, SLJIT_SIMD_STORE | type, fs0, SLJIT_MEM1(SLJIT_S1), 7 * sizeof(sljit_uw));

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_32;
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R2, 0, SLJIT_S1, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, sizeof(sljit_uw));
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR3, SLJIT_MEM1(SLJIT_S0), 224);
	/* resw[8] */
	sljit_emit_simd_sign(compiler, SLJIT_SIMD_STORE | type, SLJIT_FR3, SLJIT_MEM2(SLJIT_R2, SLJIT_R1), 3);

	type = SLJIT_SIMD_REG_256 | SLJIT_SIMD_ELEM_64 | SLJIT_SIMD_FLOAT;
	sljit_emit_simd_mov(compiler, SLJIT_SIMD_LOAD | type, SLJIT_FR0, SLJIT_MEM1(SLJIT_S0), 256);
	/* res32[6] */
	sljit_emit_simd_sign(compiler, SLJIT_SIMD_STORE | type | SLJIT_32, SLJIT_FR0, SLJIT_MEM0(), (sljit_sw)(res32 + 6));

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func3((sljit_sw)buf, (sljit_sw)resw, (sljit_sw)res32);

	FAILED(resw[0] != 0x8fa3, "test102 case 1 failed\n");
	FAILED(resw[1] != 0x34d5, "test102 case 2 failed\n");
	FAILED(resw[2] != 0xa6, "test102 case 3 failed\n");
	FAILED(res32[0] != 0x5e, "test102 case 4 failed\n");
	FAILED(res32[1] != 0xd, "test102 case 5 failed\n");
	FAILED(res32[2] != 0x5, "test102 case 6 failed\n");
	FAILED(res32[3] != (sljit_u32)-1, "test102 case 7 failed\n");
	FAILED(resw[3] != 0x2, "test102 case 8 failed\n");
	FAILED(resw[4] != 0x1, "test102 case 9 failed\n");
#if IS_ARM
	FAILED(resw[5] != 0x45, "test102 case 10 failed\n");
	FAILED(res32[4] != 0x9, "test102 case 11 failed\n");
	FAILED(resw[6] != 0x1, "test102 case 12 failed\n");
#endif /* IS_ARM */

	if (supported[0]) {
		FAILED(res32[5] != 0x51e83b71, "test102 case 13 failed\n");
		FAILED(resw[7] != 0xc90d, "test102 case 14 failed\n");
		FAILED(resw[8] != 0xa5, "test102 case 15 failed\n");
		FAILED(res32[6] != 0x9, "test102 case 16 failed\n");
	}

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

static void test103(void)
{
	/* Test f64 as f32 register pair access. */
	executable_code code;
	struct sljit_compiler* compiler;
	sljit_f32 buf[10];
	sljit_sw num;
	sljit_s32 i;

	if (verbose)
		printf("Run test103\n");

	if (!sljit_has_cpu_feature(SLJIT_HAS_F64_AS_F32_PAIR)) {
		if (verbose)
			printf("f32 register pairs are not available, test103 skipped\n");
		successful_tests++;
		return;
	}

	compiler = sljit_create_compiler(NULL, NULL);
	FAILED(!compiler, "cannot create compiler\n");

	buf[0] = -45.25;
	buf[1] = 33.5;
	buf[2] = -104.75;

	for (i = 3; i < 10; i++)
		buf[i] = -1.0;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1(VOID, P), 3, 2, 4, 2, 0);

	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_F64_SECOND(SLJIT_FR0), 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f32));

	sljit_emit_fop1(compiler, SLJIT_NEG_F32, SLJIT_F64_SECOND(SLJIT_FR0), 0, SLJIT_F64_SECOND(SLJIT_FR0), 0);
	/* buf[3] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S0), 3 * sizeof(sljit_f32), SLJIT_F64_SECOND(SLJIT_FR0), 0);

	sljit_emit_fop1(compiler, SLJIT_ABS_F32, SLJIT_F64_SECOND(SLJIT_FR0), 0, SLJIT_FR0, 0);
	/* buf[4] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S0), 4 * sizeof(sljit_f32), SLJIT_F64_SECOND(SLJIT_FR0), 0);

	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_f32));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 89);
	sljit_emit_fop1(compiler, SLJIT_CONV_F32_FROM_SW, SLJIT_F64_SECOND(SLJIT_FR1), 0, SLJIT_R0, 0);
	/* buf[5] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S0), 5 * sizeof(sljit_f32), SLJIT_F64_SECOND(SLJIT_FR1), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_F64_SECOND(SLJIT_FR1), 0, SLJIT_FR1, 0);
	/* num */
	sljit_emit_fop1(compiler, SLJIT_CONV_SW_FROM_F32, SLJIT_MEM0(), (sljit_sw)&num, SLJIT_F64_SECOND(SLJIT_FR1), 0);

	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FS1, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_F64_SECOND(SLJIT_FS1), 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f32));
	sljit_emit_fop2(compiler, SLJIT_ADD_F32, SLJIT_F64_SECOND(SLJIT_FS1), 0, SLJIT_FS1, 0, SLJIT_F64_SECOND(SLJIT_FS1), 0);
	/* buf[6] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S0), 6 * sizeof(sljit_f32), SLJIT_F64_SECOND(SLJIT_FS1), 0);

	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f32));
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_F64_SECOND(SLJIT_FR1), 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop2r(compiler, SLJIT_COPYSIGN_F32, SLJIT_F64_SECOND(SLJIT_FR1), SLJIT_FR1, 0, SLJIT_F64_SECOND(SLJIT_FR1), 0);
	/* buf[7] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S0), 7 * sizeof(sljit_f32), SLJIT_F64_SECOND(SLJIT_FR1), 0);

	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_F64_SECOND(SLJIT_FS0), 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fset32(compiler, SLJIT_F64_SECOND(SLJIT_FS0), -78.75);
	/* buf[8] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S0), 8 * sizeof(sljit_f32), SLJIT_F64_SECOND(SLJIT_FS0), 0);

	sljit_emit_op1(compiler, SLJIT_MOV32, SLJIT_S1, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_F64_SECOND(SLJIT_FR3), 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_f32));
	sljit_emit_fcopy(compiler, SLJIT_COPY32_TO_F32, SLJIT_F64_SECOND(SLJIT_FR3), SLJIT_S1);
	/* buf[9] */
	sljit_emit_fop1(compiler, SLJIT_MOV_F32, SLJIT_MEM1(SLJIT_S0), 9 * sizeof(sljit_f32), SLJIT_F64_SECOND(SLJIT_FR3), 0);

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);
	FAILED(buf[3] != -33.5, "test103 case 1 failed\n");
	FAILED(buf[4] != 45.25, "test103 case 2 failed\n");
	FAILED(buf[5] != 89.0, "test103 case 3 failed\n");
	FAILED(num != -104, "test103 case 4 failed\n");
	FAILED(buf[6] != -11.75, "test103 case 5 failed\n");
	FAILED(buf[7] != -33.5, "test103 case 6 failed\n");
	FAILED(buf[8] != -78.75, "test103 case 7 failed\n");
	FAILED(buf[9] != -45.25, "test103 case 8 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}

int sljit_test(int argc, char* argv[])
{
	sljit_s32 has_arg = (argc >= 2 && argv[1][0] == '-' && argv[1][2] == '\0');
	verbose = has_arg && argv[1][1] == 'v';
	silent = has_arg && argv[1][1] == 's';

	if (!verbose && !silent)
		printf("Pass -v to enable verbose, -s to disable this hint.\n\n");

#if !(defined SLJIT_CONFIG_UNSUPPORTED && SLJIT_CONFIG_UNSUPPORTED)
	test_exec_allocator();
#endif
	test_macros();
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
	test54();
	test55();
	test56();
	test57();
	test58();
	test59();
	test60();
	test61();
	test62();
	test63();
	test64();
	test65();
	test66();
	test67();
	test68();
	test69();
	test70();
	test71();
	test72();
	test73();
	test74();
	test75();
	test76();
	test77();
	test78();
	test79();
	test80();
	test81();
	test82();
	test83();
	test84();
	test85();
	test86();
	test87();
	test88();
	test89();
	test90();
	test91();
	test92();
	test93();
	test94();
	test95();
	test96();
	test97();
	test98();
	test99();
	test100();
	test101();
	test102();
	test103();

#if (defined SLJIT_EXECUTABLE_ALLOCATOR && SLJIT_EXECUTABLE_ALLOCATOR)
	sljit_free_unused_memory_exec();
#endif

#	define TEST_COUNT 103

	printf("SLJIT tests: ");
	if (successful_tests == TEST_COUNT)
		printf("all tests " COLOR_GREEN "PASSED" COLOR_DEFAULT " ");
	else
		printf(COLOR_RED "%d" COLOR_DEFAULT " (" COLOR_RED "%d%%" COLOR_DEFAULT ") tests " COLOR_RED "FAILED" COLOR_DEFAULT " ", TEST_COUNT - successful_tests, (TEST_COUNT - successful_tests) * 100 / TEST_COUNT);
	printf("on " COLOR_ARCH "%s" COLOR_DEFAULT " (%s)\n", sljit_get_platform_name(), sljit_has_cpu_feature(SLJIT_HAS_SIMD) ? "with simd" : (sljit_has_cpu_feature(SLJIT_HAS_FPU) ? "with fpu" : "basic"));

	return TEST_COUNT - successful_tests;

#	undef TEST_COUNT
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
