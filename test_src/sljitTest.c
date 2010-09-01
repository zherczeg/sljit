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

union executable_code {
	void* code;
	SLJIT_CALL sljit_w (*func0)(void);
	SLJIT_CALL sljit_w (*func1)(sljit_w a);
	SLJIT_CALL sljit_w (*func2)(sljit_w a, sljit_w b);
	SLJIT_CALL sljit_w (*func3)(sljit_w a, sljit_w b, sljit_w c);
};
typedef union executable_code executable_code;

static int successful_tests = 0;

#define FAILED(cond, text) \
	if (cond) { \
		printf(text); \
		return; \
	}

#define T(value) \
	if ((value) != SLJIT_SUCCESS) { \
		printf("Compiler error: %d\n", compiler->error); \
		sljit_free_compiler(compiler); \
		return; \
	}

#define TP(value) \
	if ((value) == NULL) { \
		printf("Compiler error: %d\n", compiler->error); \
		sljit_free_compiler(compiler); \
		return; \
	}

static void test1(void)
{
	// Enter and return from an sljit function
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();

	FAILED(!compiler, "cannot create compiler\n");

	// 3 arguments passed, 3 arguments used
	T(sljit_emit_enter(compiler, 3, 3, 3, 0));
	T(sljit_emit_return(compiler, SLJIT_GENERAL_REG2, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	SLJIT_ASSERT(compiler->error == SLJIT_ERR_COMPILED);
	sljit_free_compiler(compiler);

	FAILED(code.func3(3, -21, 86) != -21, "test1 case 1 failed\n");
	FAILED(code.func3(4789, 47890, 997) != 47890, "test1 case 2 failed\n");
	sljit_free_code(code.code);
	printf("test1 ok\n");
	successful_tests++;
}

static void test2(void)
{
	// Test mov
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_w buf[4];

	FAILED(!compiler, "cannot create compiler\n");

	buf[0] = 5678;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	T(sljit_emit_enter(compiler, 1, 3, 2, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_UNUSED, 0, SLJIT_MEM0(), (sljit_w)&buf));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 9999));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_GENERAL_REG2, 0, SLJIT_GENERAL_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG2), sizeof(sljit_w), SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, sizeof(sljit_w)));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG3, 0, SLJIT_MEM2(SLJIT_GENERAL_REG2, SLJIT_TEMPORARY_REG2), 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM2(SLJIT_GENERAL_REG2, SLJIT_TEMPORARY_REG2), sizeof(sljit_w), SLJIT_MEM2(SLJIT_GENERAL_REG2, SLJIT_TEMPORARY_REG2), 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM2(SLJIT_GENERAL_REG2, SLJIT_TEMPORARY_REG2), sizeof(sljit_w) * 2, SLJIT_MEM0(), (sljit_w)&buf));
	T(sljit_emit_return(compiler, SLJIT_TEMPORARY_REG3, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_w)&buf) != 9999, "test2 case 1 failed\n");
	FAILED(buf[1] != 9999, "test2 case 2 failed\n");
	FAILED(buf[2] != 9999, "test2 case 3 failed\n");
	FAILED(buf[3] != 5678, "test2 case 4 failed\n");
	sljit_free_code(code.code);
	printf("test2 ok\n");
	successful_tests++;
}

static void test3(void)
{
	// Test not
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_w buf[4];

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 1234;
	buf[1] = 0;
	buf[2] = 9876;
	buf[3] = 0;

	T(sljit_emit_enter(compiler, 1, 3, 2, 0));
	T(sljit_emit_op1(compiler, SLJIT_NOT, SLJIT_UNUSED, 0, SLJIT_MEM0(), (sljit_w)&buf));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w), SLJIT_MEM1(SLJIT_GENERAL_REG1), 0));
	T(sljit_emit_op1(compiler, SLJIT_NOT, SLJIT_MEM0(), (sljit_w)&buf[1], SLJIT_MEM0(), (sljit_w)&buf[1]));
	T(sljit_emit_op1(compiler, SLJIT_NOT, SLJIT_PREF_RET_REG, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0));
	T(sljit_emit_op1(compiler, SLJIT_NOT, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 3, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 2));
	T(sljit_emit_return(compiler, SLJIT_PREF_RET_REG, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_w)&buf) != ~1234, "test3 case 1 failed\n");
	FAILED(buf[1] != ~1234, "test3 case 2 failed\n");
	FAILED(buf[3] != ~9876, "test3 case 3 failed\n");

	sljit_free_code(code.code);
	printf("test3 ok\n");
	successful_tests++;
}

static void test4(void)
{
	// Test not
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_w buf[4];

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	buf[1] = 1234;
	buf[2] = 0;
	buf[3] = 0;

	T(sljit_emit_enter(compiler, 2, 3, 2, 0));
	T(sljit_emit_op1(compiler, SLJIT_NEG, SLJIT_UNUSED, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0));
	T(sljit_emit_op1(compiler, SLJIT_NEG, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 2, SLJIT_GENERAL_REG2, 0));
	T(sljit_emit_op1(compiler, SLJIT_NEG, SLJIT_MEM0(), (sljit_w)&buf[0], SLJIT_MEM0(), (sljit_w)&buf[1]));
	T(sljit_emit_op1(compiler, SLJIT_NEG, SLJIT_PREF_RET_REG, 0, SLJIT_GENERAL_REG2, 0));
	T(sljit_emit_op1(compiler, SLJIT_NEG, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 3, SLJIT_IMM, 299));
	T(sljit_emit_return(compiler, SLJIT_PREF_RET_REG, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	FAILED(code.func2((sljit_w)&buf, 4567) != -4567, "test4 case 1 failed\n");
	FAILED(buf[0] != -1234, "test4 case 2 failed\n");
	FAILED(buf[2] != -4567, "test4 case 3 failed\n");
	FAILED(buf[3] != -299, "test4 case 4 failed\n");

	sljit_free_code(code.code);
	printf("test4 ok\n");
	successful_tests++;
}

static void test5(void)
{
	// Test add
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_w buf[6];

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 100;
	buf[1] = 200;
	buf[2] = 300;
	buf[3] = 0;
	buf[4] = 0;
	buf[5] = 0;

	T(sljit_emit_enter(compiler, 1, 3, 2, 0));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_UNUSED, 0, SLJIT_IMM, 16, SLJIT_IMM, 16));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_UNUSED, 0, SLJIT_IMM, 255, SLJIT_IMM, 255));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_UNUSED, 0, SLJIT_GENERAL_REG1, 0, SLJIT_GENERAL_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, sizeof(sljit_w) * 2));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, 50));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM2(SLJIT_GENERAL_REG1, SLJIT_TEMPORARY_REG1), 0, SLJIT_MEM2(SLJIT_GENERAL_REG1, SLJIT_TEMPORARY_REG1), 0, SLJIT_MEM2(SLJIT_GENERAL_REG1, SLJIT_TEMPORARY_REG1), 0 - sizeof(sljit_w)));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 2));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_TEMPORARY_REG2, 0, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, 50));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 4, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, 50, SLJIT_TEMPORARY_REG2, 0));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_GENERAL_REG1), 5 * sizeof(sljit_w), SLJIT_IMM, 50, SLJIT_MEM1(SLJIT_GENERAL_REG1), 5 * sizeof(sljit_w)));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_TEMPORARY_REG2, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 5 * sizeof(sljit_w), SLJIT_TEMPORARY_REG2, 0));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_GENERAL_REG1), 4 * sizeof(sljit_w), SLJIT_TEMPORARY_REG2, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 4 * sizeof(sljit_w)));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_GENERAL_REG1), 5 * sizeof(sljit_w), SLJIT_MEM1(SLJIT_GENERAL_REG1), 4 * sizeof(sljit_w), SLJIT_MEM1(SLJIT_GENERAL_REG1), 5 * sizeof(sljit_w)));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_GENERAL_REG1), 3 * sizeof(sljit_w), SLJIT_MEM1(SLJIT_GENERAL_REG1), 4 * sizeof(sljit_w), SLJIT_TEMPORARY_REG2, 0));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 1000, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 1000));
	T(sljit_emit_return(compiler, SLJIT_TEMPORARY_REG1, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_w)&buf) != 2106 + 2 * sizeof(sljit_w), "test5 case 1 failed\n");
	FAILED(buf[0] != 202 + 2 * sizeof(sljit_w), "test5 case 2 failed\n");
	FAILED(buf[2] != 500, "test5 case 3 failed\n");
	FAILED(buf[3] != 400, "test5 case 4 failed\n");
	FAILED(buf[4] != 200, "test5 case 5 failed\n");
	FAILED(buf[5] != 250, "test5 case 6 failed\n");

	sljit_free_code(code.code);
	printf("test5 ok\n");
	successful_tests++;
}

static void test6(void)
{
	// Test addc, sub, subc
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_w buf[7];

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 0;
	buf[5] = 0;
	buf[6] = 0;

	T(sljit_emit_enter(compiler, 1, 3, 1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, -1));
	T(sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_SET_C, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, -1));
	T(sljit_emit_op2(compiler, SLJIT_ADDC, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_IMM, 0, SLJIT_IMM, 0));
	T(sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_SET_C, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op2(compiler, SLJIT_ADDC, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w), SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w), SLJIT_IMM, 4));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 100));
	T(sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 2, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 50));
	T(sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_C, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 6000));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 3, SLJIT_IMM, 10));
	T(sljit_emit_op2(compiler, SLJIT_SUBC, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 3, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 3, SLJIT_IMM, 5));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 100));
	T(sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 2));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 4, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 5000));
	T(sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_TEMPORARY_REG2, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 4, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_TEMPORARY_REG2, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG2, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 5, SLJIT_TEMPORARY_REG2, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 5000));
	T(sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 6000, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 6, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_PREF_RET_REG, 0, SLJIT_IMM, 10));
	T(sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_C, SLJIT_PREF_RET_REG, 0, SLJIT_PREF_RET_REG, 0, SLJIT_IMM, 5));
	T(sljit_emit_op2(compiler, SLJIT_SUBC, SLJIT_PREF_RET_REG, 0, SLJIT_PREF_RET_REG, 0, SLJIT_IMM, 2));
	T(sljit_emit_return(compiler, SLJIT_PREF_RET_REG, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_w)&buf) != 3, "test6 case 1 failed\n");
	FAILED(buf[0] != 1, "test6 case 2 failed\n");
	FAILED(buf[1] != 5, "test6 case 3 failed\n");
	FAILED(buf[2] != 50, "test6 case 4 failed\n");
	FAILED(buf[3] != 4, "test6 case 5 failed\n");
	FAILED(buf[4] != 50, "test6 case 6 failed\n");
	FAILED(buf[5] != 50, "test6 case 7 failed\n");
	FAILED(buf[6] != 1000, "test6 case 8 failed\n");

	sljit_free_code(code.code);
	printf("test6 ok\n");
	successful_tests++;
}

static void test7(void)
{
	// Test logical operators
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_w buf[6];

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0xff80;
	buf[1] = 0x0f808080;
	buf[2] = 0;
	buf[3] = 0xaaaaaa;
	buf[4] = 0;
	buf[5] = 0x4040;

	T(sljit_emit_enter(compiler, 1, 3, 1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0xf0C000));
	T(sljit_emit_op2(compiler, SLJIT_OR, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 2, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0x308f));
	T(sljit_emit_op2(compiler, SLJIT_XOR, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w)));
	T(sljit_emit_op2(compiler, SLJIT_AND, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 3, SLJIT_IMM, 0xf0f0f0, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 3));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0xC0F0));
	T(sljit_emit_op2(compiler, SLJIT_XOR, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 5));
	T(sljit_emit_op2(compiler, SLJIT_OR, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0xff0000));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 4, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG3, 0, SLJIT_IMM, 0xC0F0));
	T(sljit_emit_op2(compiler, SLJIT_AND, SLJIT_TEMPORARY_REG3, 0, SLJIT_TEMPORARY_REG3, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 5));
	T(sljit_emit_op2(compiler, SLJIT_OR, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 5, SLJIT_TEMPORARY_REG3, 0, SLJIT_IMM, 0xff0000));
	T(sljit_emit_op2(compiler, SLJIT_XOR, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w), SLJIT_IMM, 0xFFFFFF, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w)));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0xff00ff00));
	T(sljit_emit_op2(compiler, SLJIT_OR, SLJIT_TEMPORARY_REG2, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0x0f));
	T(sljit_emit_op2(compiler, SLJIT_AND, SLJIT_PREF_RET_REG, 0, SLJIT_IMM, 0x888888, SLJIT_TEMPORARY_REG2, 0));
	T(sljit_emit_return(compiler, SLJIT_PREF_RET_REG, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_w)&buf) != 0x8808, "test7 case 1 failed\n");
	FAILED(buf[0] != 0x0F807F00, "test7 case 2 failed\n");
	FAILED(buf[1] != 0x0F7F7F7F, "test7 case 3 failed\n");
	FAILED(buf[2] != 0x00F0F08F, "test7 case 4 failed\n");
	FAILED(buf[3] != 0x00A0A0A0, "test7 case 5 failed\n");
	FAILED(buf[4] != 0x00FF80B0, "test7 case 6 failed\n");
	FAILED(buf[5] != 0x00FF4040, "test7 case 7 failed\n");

	sljit_free_code(code.code);
	printf("test7 ok\n");
	successful_tests++;
}

static void test8(void)
{
	// Test flags (neg, cmp, test)
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_w buf[20];

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

	T(sljit_emit_enter(compiler, 1, 3, 2, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 20));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, 10));
	T(sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_IMM, 6, SLJIT_IMM, 5));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w), SLJIT_C_NOT_EQUAL));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 2, SLJIT_C_EQUAL));
	T(sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_U, SLJIT_UNUSED, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 3000));
	T(sljit_emit_cond_set(compiler, SLJIT_GENERAL_REG2, 0, SLJIT_C_GREATER));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 3, SLJIT_GENERAL_REG2, 0));
	T(sljit_emit_cond_set(compiler, SLJIT_TEMPORARY_REG3, 0, SLJIT_C_LESS));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 4, SLJIT_TEMPORARY_REG3, 0));
	T(sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_S, SLJIT_UNUSED, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, -15));
	T(sljit_emit_cond_set(compiler, SLJIT_TEMPORARY_REG3, 0, SLJIT_C_SIG_GREATER));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 5, SLJIT_TEMPORARY_REG3, 0));
	T(sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_E | SLJIT_SET_O, SLJIT_UNUSED, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG2, 0));
	T(sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_E | SLJIT_SET_O, SLJIT_UNUSED, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_NEG | SLJIT_SET_E | SLJIT_SET_O, SLJIT_UNUSED, 0, SLJIT_IMM, (sljit_w)1 << ((sizeof(sljit_w) << 3) - 1)));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 6, SLJIT_C_OVERFLOW));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, -1));
	T(sljit_emit_op1(compiler, SLJIT_NOT | SLJIT_SET_E, SLJIT_TEMPORARY_REG2, 0, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 7, SLJIT_C_ZERO));
	T(sljit_emit_op1(compiler, SLJIT_NOT | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_TEMPORARY_REG2, 0));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 8, SLJIT_C_ZERO));
	T(sljit_emit_op2(compiler, SLJIT_AND | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_IMM, 0xffff, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op2(compiler, SLJIT_AND | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0xffff));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 9, SLJIT_C_NOT_ZERO));
	T(sljit_emit_op2(compiler, SLJIT_AND | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_IMM, 0xffff, SLJIT_TEMPORARY_REG2, 0));
	T(sljit_emit_op2(compiler, SLJIT_AND | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, 0xffff));
	T(sljit_emit_op2(compiler, SLJIT_AND | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_TEMPORARY_REG2, 0));
	T(sljit_emit_op2(compiler, SLJIT_AND | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_TEMPORARY_REG2, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0));
	T(sljit_emit_op2(compiler, SLJIT_AND | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0));
	T(sljit_emit_op2(compiler, SLJIT_AND | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_IMM, 0x1));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 10, SLJIT_C_NOT_ZERO));
	T(sljit_emit_return(compiler, SLJIT_UNUSED, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	code.func1((sljit_w)&buf);
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

	sljit_free_code(code.code);
	printf("test8 ok\n");
	successful_tests++;
}

static void test9(void)
{
	// Test shift
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_w buf[10];

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

	T(sljit_emit_enter(compiler, 1, 3, 2, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0xf));
	T(sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 3));
	T(sljit_emit_op2(compiler, SLJIT_LSHR, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 1));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op2(compiler, SLJIT_ASHR, SLJIT_UNUSED, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 2));
	T(sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_TEMPORARY_REG2, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 1));
	T(sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w), SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, 1));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, -64));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_PREF_SHIFT_REG, 0, SLJIT_IMM, 2));
	T(sljit_emit_op2(compiler, SLJIT_ASHR, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 2, SLJIT_TEMPORARY_REG1, 0, SLJIT_PREF_SHIFT_REG, 0));

	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_PREF_SHIFT_REG, 0, SLJIT_IMM, 0xff));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 4));
	T(sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_PREF_SHIFT_REG, 0, SLJIT_PREF_SHIFT_REG, 0, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 3, SLJIT_PREF_SHIFT_REG, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_PREF_SHIFT_REG, 0, SLJIT_IMM, 0xff));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 8));
	T(sljit_emit_op2(compiler, SLJIT_LSHR, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 4, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 4, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 5, SLJIT_PREF_SHIFT_REG, 0, SLJIT_TEMPORARY_REG1, 0));

	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_GENERAL_REG2, 0, SLJIT_IMM, 0xf));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 2));
	T(sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_GENERAL_REG2, 0, SLJIT_GENERAL_REG2, 0, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 6, SLJIT_GENERAL_REG2, 0));
	T(sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_TEMPORARY_REG1, 0, SLJIT_GENERAL_REG2, 0, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 7, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG3, 0, SLJIT_IMM, 0xf00));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 4));
	T(sljit_emit_op2(compiler, SLJIT_LSHR, SLJIT_TEMPORARY_REG2, 0, SLJIT_TEMPORARY_REG3, 0, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 8, SLJIT_TEMPORARY_REG2, 0));

	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, sizeof(sljit_w) * 4));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, sizeof(sljit_w) * 5));
	T(sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_MEM2(SLJIT_TEMPORARY_REG1, SLJIT_TEMPORARY_REG2), (sljit_w)buf, SLJIT_MEM2(SLJIT_TEMPORARY_REG1, SLJIT_TEMPORARY_REG2), (sljit_w)buf, SLJIT_MEM2(SLJIT_TEMPORARY_REG1, SLJIT_TEMPORARY_REG2), (sljit_w)buf));

	T(sljit_emit_return(compiler, SLJIT_UNUSED, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	code.func1((sljit_w)&buf);
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

	sljit_free_code(code.code);
	printf("test9 ok\n");
	successful_tests++;
}

static void test10(void)
{
	// Test multiplications
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_w buf[6];

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 3;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 6;
	buf[4] = -10;
	buf[5] = 0;

	T(sljit_emit_enter(compiler, 1, 3, 1, 0));

	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 5));
	T(sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_TEMPORARY_REG1, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG3, 0, SLJIT_IMM, 7));
	T(sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG3, 0, SLJIT_IMM, 8));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w), SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, -3, SLJIT_IMM, -4));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 2, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, -2));
	T(sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 3, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 3, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, 0));
	T(sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_MEM2(SLJIT_TEMPORARY_REG2, SLJIT_TEMPORARY_REG1), (sljit_w)&buf[4], SLJIT_MEM2(SLJIT_TEMPORARY_REG2, SLJIT_TEMPORARY_REG1), (sljit_w)&buf[4], SLJIT_MEM2(SLJIT_TEMPORARY_REG2, SLJIT_TEMPORARY_REG1), (sljit_w)&buf[4]));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 9));
	T(sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 5, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_PREF_RET_REG, 0, SLJIT_IMM, 11, SLJIT_IMM, 10));
	T(sljit_emit_return(compiler, SLJIT_PREF_RET_REG, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_w)&buf) != 110, "test10 case 1 failed\n");
	FAILED(buf[0] != 15, "test10 case 2 failed\n");
	FAILED(buf[1] != 56, "test10 case 3 failed\n");
	FAILED(buf[2] != 12, "test10 case 4 failed\n");
	FAILED(buf[3] != -12, "test10 case 5 failed\n");
	FAILED(buf[4] != 100, "test10 case 6 failed\n");
	FAILED(buf[5] != 81, "test10 case 7 failed\n");

	sljit_free_code(code.code);
	printf("test10 ok\n");
	successful_tests++;
}

static void test11(void)
{
	// Test rewritable constants
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	struct sljit_const* const1;
	struct sljit_const* const2;
	struct sljit_const* const3;
	struct sljit_const* const4;
	sljit_uw const1_addr;
	sljit_uw const2_addr;
	sljit_uw const3_addr;
	sljit_uw const4_addr;
#ifdef SLJIT_64BIT_ARCHITECTURE
	sljit_w word_value1 = 0xaaaaaaaaaaaaaaaal;
	sljit_w word_value2 = 0xfee1deadfbadf00dl;
#else
	sljit_w word_value1 = 0xaaaaaaaal;
	sljit_w word_value2 = 0xfbadf00dl;
#endif
	sljit_w buf[3];

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;

	T(sljit_emit_enter(compiler, 1, 3, 1, 0));

	const1 = sljit_emit_const(compiler, SLJIT_MEM0(), (sljit_w)&buf[0], -0x81b9);
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 2 * sizeof(sljit_w)));
	const2 = sljit_emit_const(compiler, SLJIT_MEM2(SLJIT_GENERAL_REG1, SLJIT_TEMPORARY_REG1), -(int)sizeof(sljit_w), -65535);
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, (sljit_w)&buf[0]));
	const3 = sljit_emit_const(compiler, SLJIT_MEM1(SLJIT_TEMPORARY_REG1), 0, word_value1);
	const4 = sljit_emit_const(compiler, SLJIT_PREF_RET_REG, 0, 0xf7afcdb7);

	T(sljit_emit_return(compiler, SLJIT_PREF_RET_REG, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	const1_addr = sljit_get_const_addr(const1);
	const2_addr = sljit_get_const_addr(const2);
	const3_addr = sljit_get_const_addr(const3);
	const4_addr = sljit_get_const_addr(const4);
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_w)&buf) != 0xf7afcdb7, "test11 case 1 failed\n");
	FAILED(buf[0] != -0x81b9, "test11 case 2 failed\n");
	FAILED(buf[1] != -65535, "test11 case 3 failed\n");
	FAILED(buf[2] != word_value1, "test11 case 4 failed\n");

	sljit_set_const(const1_addr, -1);
	sljit_set_const(const2_addr, word_value2);
	sljit_set_const(const3_addr, 0xbab0fea1);
	sljit_set_const(const4_addr, -60089);

	FAILED(code.func1((sljit_w)&buf) != -60089, "test11 case 5 failed\n");
	FAILED(buf[0] != -1, "test11 case 6 failed\n");
	FAILED(buf[1] != word_value2, "test11 case 7 failed\n");
	FAILED(buf[2] != 0xbab0fea1, "test11 case 8 failed\n");

	sljit_free_code(code.code);
	printf("test11 ok\n");
	successful_tests++;
}

static void test12(void)
{
	// Test rewriteable jumps
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	struct sljit_label *label1;
	struct sljit_label *label2;
	struct sljit_label *label3;
	struct sljit_jump *jump1;
	struct sljit_jump *jump2;
	struct sljit_jump *jump3;
	sljit_uw jump1_addr;
	sljit_uw label1_addr;
	sljit_uw label2_addr;
	sljit_w buf[1];

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;

	T(sljit_emit_enter(compiler, 2, 3, 2, 0));
	T(sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_S, SLJIT_UNUSED, 0, SLJIT_GENERAL_REG2, 0, SLJIT_IMM, 10));
	jump1 = sljit_emit_jump(compiler, SLJIT_REWRITABLE_JUMP | SLJIT_C_SIG_GREATER);
	T(compiler->error);
	// Default handler
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_IMM, 5));
	jump2 = sljit_emit_jump(compiler, SLJIT_JUMP);
	T(compiler->error);
	// Handler 1
	label1 = sljit_emit_label(compiler);
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_IMM, 6));
	jump3 = sljit_emit_jump(compiler, SLJIT_JUMP);
	T(compiler->error);
	// Handler 2
	label2 = sljit_emit_label(compiler);
	T(compiler->error);
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_IMM, 7));
	// Exit
	label3 = sljit_emit_label(compiler);
	T(compiler->error);
	sljit_set_label(jump2, label3);
	sljit_set_label(jump3, label3);
	// By default, set to handler 1
	sljit_set_label(jump1, label1);
	T(sljit_emit_return(compiler, SLJIT_UNUSED, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	jump1_addr = sljit_get_jump_addr(jump1);
	label1_addr = sljit_get_label_addr(label1);
	label2_addr = sljit_get_label_addr(label2);
	sljit_free_compiler(compiler);

	code.func2((sljit_w)&buf, 4);
	FAILED(buf[0] != 5, "test12 case 1 failed\n");

	code.func2((sljit_w)&buf, 11);
	FAILED(buf[0] != 6, "test12 case 2 failed\n");

	sljit_set_jump_addr(jump1_addr, label2_addr);
	code.func2((sljit_w)&buf, 12);
	FAILED(buf[0] != 7, "test12 case 3 failed\n");

	sljit_set_jump_addr(jump1_addr, label1_addr);
	code.func2((sljit_w)&buf, 13);
	FAILED(buf[0] != 6, "test12 case 4 failed\n");

	sljit_free_code(code.code);
	printf("test12 ok\n");
	successful_tests++;
}

static void test13(void)
{
	// Test fpu monadic functions
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	double buf[6];
	sljit_w buf2[6];

	if (!sljit_is_fpu_available()) {
		printf("no fpu available, test13 skipped\n");
		return;
	}

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 7.75;
	buf[1] = -4.5;
	buf[2] = 0.0;
	buf[3] = 0.0;
	buf[4] = 0.0;
	buf[5] = 0.0;

	buf2[0] = 10;
	buf2[1] = 10;
	buf2[2] = 10;
	buf2[3] = 10;
	buf2[4] = 10;
	buf2[5] = 10;

	T(sljit_emit_enter(compiler, 2, 3, 2, 0));
	T(sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_MEM0(), (sljit_w)&buf[2], SLJIT_MEM0(), (sljit_w)&buf[1]));
	T(sljit_emit_fop1(compiler, SLJIT_FABS, SLJIT_MEM1(SLJIT_GENERAL_REG1), 3 * sizeof(double), SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(double)));
	T(sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_FLOAT_REG1, 0, SLJIT_MEM0(), (sljit_w)&buf[0]));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 2 * sizeof(double)));
	T(sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_FLOAT_REG2, 0, SLJIT_MEM2(SLJIT_GENERAL_REG1, SLJIT_TEMPORARY_REG1), 0));
	T(sljit_emit_fop1(compiler, SLJIT_FNEG, SLJIT_FLOAT_REG3, 0, SLJIT_FLOAT_REG1, 0));
	T(sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_FLOAT_REG4, 0, SLJIT_FLOAT_REG3, 0));
	T(sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_MEM0(), (sljit_w)&buf[4], SLJIT_FLOAT_REG4, 0));
	T(sljit_emit_fop1(compiler, SLJIT_FABS, SLJIT_FLOAT_REG3, 0, SLJIT_FLOAT_REG2, 0));
	T(sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 5 * sizeof(double), SLJIT_FLOAT_REG3, 0));

	T(sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_FLOAT_REG2, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0));
	T(sljit_emit_fop1(compiler, SLJIT_FCMP | SLJIT_SET_S, SLJIT_FLOAT_REG2, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(double)));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG2), 0, SLJIT_C_FLOAT_GREATER));
	T(sljit_emit_fop1(compiler, SLJIT_FCMP | SLJIT_SET_S, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(double), SLJIT_FLOAT_REG2, 0));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG2), sizeof(sljit_w), SLJIT_C_FLOAT_GREATER));
	T(sljit_emit_fop1(compiler, SLJIT_FCMP | SLJIT_SET_E | SLJIT_SET_S, SLJIT_FLOAT_REG2, 0, SLJIT_FLOAT_REG2, 0));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG2), 2 * sizeof(sljit_w), SLJIT_C_FLOAT_EQUAL));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG2), 3 * sizeof(sljit_w), SLJIT_C_FLOAT_LESS));
	T(sljit_emit_fop1(compiler, SLJIT_FCMP | SLJIT_SET_E, SLJIT_FLOAT_REG2, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(double)));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG2), 4 * sizeof(sljit_w), SLJIT_C_FLOAT_EQUAL));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG2), 5 * sizeof(sljit_w), SLJIT_C_FLOAT_NOT_EQUAL));

	T(sljit_emit_return(compiler, SLJIT_UNUSED, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	code.func2((sljit_w)&buf, (sljit_w)&buf2);
	FAILED(buf[2] != -4.5, "test13 case 1 failed\n");
	FAILED(buf[3] != 4.5, "test13 case 2 failed\n");
	FAILED(buf[4] != -7.75, "test13 case 3 failed\n");
	FAILED(buf[5] != 4.5, "test13 case 4 failed\n");

	FAILED(buf2[0] != 1, "test13 case 5 failed\n");
	FAILED(buf2[1] != 0, "test13 case 6 failed\n");
	FAILED(buf2[2] != 1, "test13 case 7 failed\n");
	FAILED(buf2[3] != 0, "test13 case 8 failed\n");
	FAILED(buf2[4] != 0, "test13 case 9 failed\n");
	FAILED(buf2[5] != 1, "test13 case 10 failed\n");

	sljit_free_code(code.code);
	printf("test13 ok\n");
	successful_tests++;
}

static void test14(void)
{
	// Test fpu diadic functions
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	double buf[15];

	if (!sljit_is_fpu_available()) {
		printf("no fpu available, test14 skipped\n");
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
	T(sljit_emit_enter(compiler, 1, 3, 1, 0));

	// ADD
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, sizeof(double)));
	T(sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_FLOAT_REG1, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(double)));
	T(sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_FLOAT_REG2, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(double) * 2));
	T(sljit_emit_fop2(compiler, SLJIT_FADD, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(double) * 3, SLJIT_MEM2(SLJIT_GENERAL_REG1, SLJIT_TEMPORARY_REG1), 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0));
	T(sljit_emit_fop2(compiler, SLJIT_FADD, SLJIT_FLOAT_REG1, 0, SLJIT_FLOAT_REG1, 0, SLJIT_FLOAT_REG2, 0));
	T(sljit_emit_fop2(compiler, SLJIT_FADD, SLJIT_FLOAT_REG2, 0, SLJIT_FLOAT_REG1, 0, SLJIT_FLOAT_REG2, 0));
	T(sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(double) * 4, SLJIT_FLOAT_REG1, 0));
	T(sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(double) * 5, SLJIT_FLOAT_REG2, 0));

	// SUB
	T(sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_FLOAT_REG3, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0));
	T(sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_FLOAT_REG4, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(double) * 2));
	T(sljit_emit_fop2(compiler, SLJIT_FSUB, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(double) * 6, SLJIT_FLOAT_REG4, 0, SLJIT_MEM2(SLJIT_GENERAL_REG1, SLJIT_TEMPORARY_REG1), sizeof(double)));
	T(sljit_emit_fop2(compiler, SLJIT_FSUB, SLJIT_FLOAT_REG3, 0, SLJIT_FLOAT_REG3, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(double) * 2));
	T(sljit_emit_fop2(compiler, SLJIT_FSUB, SLJIT_FLOAT_REG4, 0, SLJIT_FLOAT_REG3, 0, SLJIT_FLOAT_REG4, 0));
	T(sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(double) * 7, SLJIT_FLOAT_REG3, 0));
	T(sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(double) * 8, SLJIT_FLOAT_REG4, 0));

	// MUL
	T(sljit_emit_fop2(compiler, SLJIT_FMUL, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(double) * 9, SLJIT_MEM2(SLJIT_GENERAL_REG1, SLJIT_TEMPORARY_REG1), 0, SLJIT_FLOAT_REG2, 0));
	T(sljit_emit_fop2(compiler, SLJIT_FMUL, SLJIT_FLOAT_REG2, 0, SLJIT_FLOAT_REG2, 0, SLJIT_FLOAT_REG3, 0));
	T(sljit_emit_fop2(compiler, SLJIT_FMUL, SLJIT_FLOAT_REG3, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(double) * 2, SLJIT_FLOAT_REG3, 0));
	T(sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(double) * 10, SLJIT_FLOAT_REG2, 0));
	T(sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(double) * 11, SLJIT_FLOAT_REG3, 0));

	// DIV
	T(sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_FLOAT_REG1, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(double) * 12));
	T(sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_FLOAT_REG2, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(double) * 13));
	T(sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_FLOAT_REG3, 0, SLJIT_FLOAT_REG1, 0));
	T(sljit_emit_fop2(compiler, SLJIT_FDIV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(double) * 12, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(double) * 12, SLJIT_FLOAT_REG2, 0));
	T(sljit_emit_fop2(compiler, SLJIT_FDIV, SLJIT_FLOAT_REG1, 0, SLJIT_FLOAT_REG1, 0, SLJIT_FLOAT_REG2, 0));
	T(sljit_emit_fop2(compiler, SLJIT_FDIV, SLJIT_FLOAT_REG3, 0, SLJIT_FLOAT_REG2, 0, SLJIT_FLOAT_REG3, 0));
	T(sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(double) * 13, SLJIT_FLOAT_REG1, 0));
	T(sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(double) * 14, SLJIT_FLOAT_REG3, 0));

	T(sljit_emit_return(compiler, SLJIT_UNUSED, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	code.func1((sljit_w)&buf);
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
	FAILED(buf[14] != 0.5, "test14 case 11 failed\n");

	sljit_free_code(code.code);
	printf("test14 ok\n");
	successful_tests++;
}

static sljit_w SLJIT_CALL func(sljit_w a, sljit_w b)
{
	return a + b + 5;
}

static void test15(void)
{
	// Test function call
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	struct sljit_jump* jump;
	sljit_w buf[3];

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;

	T(sljit_emit_enter(compiler, 1, 3, 1, 0));

	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 5));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, 7));
	T(sljit_emit_ijump(compiler, SLJIT_CALL2, SLJIT_IMM, SLJIT_FUNC_OFFSET(func)));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_PREF_RET_REG, 0));

	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, -5));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, -10));
	jump = sljit_emit_jump(compiler, SLJIT_CALL2 | SLJIT_REWRITABLE_JUMP); TP(jump);
	sljit_set_target(jump, (sljit_w)-1);
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w), SLJIT_PREF_RET_REG, 0));

	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 10));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, 16));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG3, 0, SLJIT_IMM, SLJIT_FUNC_OFFSET(func)));
	T(sljit_emit_ijump(compiler, SLJIT_CALL2, SLJIT_TEMPORARY_REG3, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 2 * sizeof(sljit_w), SLJIT_PREF_RET_REG, 0));

	T(sljit_emit_return(compiler, SLJIT_PREF_RET_REG, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_set_jump_addr(sljit_get_jump_addr(jump), SLJIT_FUNC_OFFSET(func));
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_w)&buf) != 31, "test15 case 1 failed\n");
	FAILED(buf[0] != 17, "test15 case 2 failed\n");
	FAILED(buf[1] != -10, "test15 case 3 failed\n");
	FAILED(buf[2] != 31, "test15 case 4 failed\n");

	sljit_free_code(code.code);
	printf("test15 ok\n");
	successful_tests++;
}

static void test16(void)
{
	// Ackermann benchmark
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	struct sljit_label *entry;
	struct sljit_label *label;
	struct sljit_jump *jump;
	struct sljit_jump *jump1;
	struct sljit_jump *jump2;

	FAILED(!compiler, "cannot create compiler\n");

	entry = sljit_emit_label(compiler);
	T(sljit_emit_enter(compiler, 2, 3, 2, 0));
	// if x == 0
	T(sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_GENERAL_REG1, 0, SLJIT_IMM, 0));
	jump1 = sljit_emit_jump(compiler, SLJIT_C_EQUAL); TP(jump1);
	// if y == 0
	T(sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_GENERAL_REG2, 0, SLJIT_IMM, 0));
	jump2 = sljit_emit_jump(compiler, SLJIT_C_EQUAL); TP(jump2);

	// Ack(x,y-1)
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_GENERAL_REG1, 0));
	T(sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_TEMPORARY_REG2, 0, SLJIT_GENERAL_REG2, 0, SLJIT_IMM, 1));
	jump = sljit_emit_jump(compiler, SLJIT_CALL2); TP(jump);
	sljit_set_label(jump, entry);

	// return Ack(x-1, Ack(x,y-1))
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_PREF_RET_REG, 0));
	T(sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_TEMPORARY_REG1, 0, SLJIT_GENERAL_REG1, 0, SLJIT_IMM, 1));
	jump = sljit_emit_jump(compiler, SLJIT_CALL2); TP(jump);
	sljit_set_label(jump, entry);
	T(sljit_emit_return(compiler, SLJIT_PREF_RET_REG, 0));

	// return y+1
	label = sljit_emit_label(compiler); TP(label);
	sljit_set_label(jump1, label);
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_PREF_RET_REG, 0, SLJIT_IMM, 1, SLJIT_GENERAL_REG2, 0));
	T(sljit_emit_return(compiler, SLJIT_PREF_RET_REG, 0));

	// return Ack(x-1,1)
	label = sljit_emit_label(compiler); TP(label);
	sljit_set_label(jump2, label);
	T(sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_TEMPORARY_REG1, 0, SLJIT_GENERAL_REG1, 0, SLJIT_IMM, 1));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, 1));
	jump = sljit_emit_jump(compiler, SLJIT_CALL2); TP(jump);
	sljit_set_label(jump, entry);
	T(sljit_emit_return(compiler, SLJIT_PREF_RET_REG, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	FAILED(code.func2(3, 3) != 61, "test16 case 1 failed\n");
	// For benchmarking
	//FAILED(code.func2(3, 11) != 16381, "test16 case 1 failed\n");

	sljit_free_code(code.code);
	printf("test16 ok\n");
	successful_tests++;
}

static void test17(void)
{
	// Test arm constant pool
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	int i;
	sljit_w buf[5];

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 0;

	T(sljit_emit_enter(compiler, 1, 3, 1, 0));
	for (i = 0; i <= 0xfff; i++) {
		T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0x81818000 | i));
		T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0x81818000 | i));
		if ((i & 0x3ff) == 0)
			T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), (i >> 10) * sizeof(sljit_w), SLJIT_TEMPORARY_REG1, 0));
	}
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 4 * sizeof(sljit_w), SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_return(compiler, SLJIT_UNUSED, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	code.func1((sljit_w)&buf);
	FAILED((sljit_uw)buf[0] != 0x81818000, "test17 case 1 failed\n");
	FAILED((sljit_uw)buf[1] != 0x81818400, "test17 case 2 failed\n");
	FAILED((sljit_uw)buf[2] != 0x81818800, "test17 case 3 failed\n");
	FAILED((sljit_uw)buf[3] != 0x81818c00, "test17 case 4 failed\n");
	FAILED((sljit_uw)buf[4] != 0x81818fff, "test17 case 5 failed\n");

	sljit_free_code(code.code);
	printf("test17 ok\n");
	successful_tests++;
}

static void test18(void)
{
	// Test 64 bit
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_w buf[11];

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
#if defined(SLJIT_64BIT_ARCHITECTURE) && defined(SLJIT_BIG_ENDIAN)
	buf[10] = 1l << 32;
#else
	buf[10] = 1;
#endif

	T(sljit_emit_enter(compiler, 1, 3, 2, 0));

#ifdef SLJIT_64BIT_ARCHITECTURE
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_IMM, 0x1122334455667788));
	T(sljit_emit_op1(compiler, SLJIT_MOV_UI, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w), SLJIT_IMM, 0x1122334455667788));

	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 1000000000000));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 2, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 1000000000000));
	T(sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 3, SLJIT_IMM, 5000000000000, SLJIT_TEMPORARY_REG1, 0));

	T(sljit_emit_op1(compiler, SLJIT_MOV_UI, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, 0x1108080808));
	T(sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_INT_OP, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 4, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, 0x1120202020));

	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0x1108080808));
	T(sljit_emit_op2(compiler, SLJIT_AND | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0x1120202020));
	T(sljit_emit_cond_set(compiler, SLJIT_GENERAL_REG2, 0, SLJIT_C_ZERO));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 5, SLJIT_GENERAL_REG2, 0));
	T(sljit_emit_op2(compiler, SLJIT_AND | SLJIT_INT_OP | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0x1120202020));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 6, SLJIT_C_ZERO));

	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0x1108080808));
	T(sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_U, SLJIT_UNUSED, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0x2208080808));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 7, SLJIT_C_LESS));
	T(sljit_emit_op2(compiler, SLJIT_AND | SLJIT_INT_OP | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0x1104040404));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 8, SLJIT_C_NOT_ZERO));

	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 4));
	T(sljit_emit_op2(compiler, SLJIT_SHL | SLJIT_INT_OP, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 9, SLJIT_IMM, 0xffff0000, SLJIT_TEMPORARY_REG1, 0));

	T(sljit_emit_op2(compiler, SLJIT_MUL | SLJIT_INT_OP, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 10, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 10, SLJIT_IMM, -1));
#else
	// 32 bit operations

	T(sljit_emit_op1(compiler, SLJIT_MOV_UI, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_IMM, 0x11223344));
	T(sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_INT_OP, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w), SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w), SLJIT_IMM, 0x44332211));

#endif

	T(sljit_emit_return(compiler, SLJIT_UNUSED, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	code.func1((sljit_w)&buf);
#ifdef SLJIT_64BIT_ARCHITECTURE
	FAILED(buf[0] != 0x1122334455667788, "test18 case 1 failed\n");
#ifdef SLJIT_LITTLE_ENDIAN
	FAILED(buf[1] != 0x55667788, "test18 case 2 failed\n");
#else
	FAILED(buf[1] != 0x5566778800000000, "test18 case 2 failed\n");
#endif
	FAILED(buf[2] != 2000000000000, "test18 case 3 failed\n");
	FAILED(buf[3] != 4000000000000, "test18 case 4 failed\n");
#ifdef SLJIT_LITTLE_ENDIAN
	FAILED(buf[4] != 0x28282828, "test18 case 5 failed\n");
#else
	FAILED(buf[4] != 0x2828282800000000, "test18 case 5 failed\n");
#endif
	FAILED(buf[5] != 0, "test18 case 6 failed\n");
	FAILED(buf[6] != 1, "test18 case 7 failed\n");
	FAILED(buf[7] != 1, "test18 case 8 failed\n");
	FAILED(buf[8] != 0, "test18 case 9 failed\n");
#ifdef SLJIT_LITTLE_ENDIAN
	FAILED(buf[9] != 0xfff00000, "test18 case 10 failed\n");
	FAILED(buf[10] != 0xffffffff, "test18 case 11 failed\n");
#else
	FAILED(buf[9] != 0xfff0000000000000, "test18 case 10 failed\n");
	FAILED(buf[10] != 0xffffffff00000000, "test18 case 11 failed\n");
#endif
#else
	FAILED(buf[0] != 0x11223344, "test18 case 1 failed\n");
	FAILED(buf[1] != 0x44332211, "test18 case 2 failed\n");
#endif

	sljit_free_code(code.code);
	printf("test18 ok\n");
	successful_tests++;
}

static void test19(void)
{
	// Test arm partial instruction caching
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_w buf[10];

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 6;
	buf[1] = 4;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 0;
	buf[5] = 0;
	buf[6] = 2;
	buf[7] = 0;

	T(sljit_emit_enter(compiler, 1, 3, 1, 0));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w)));
#ifdef SLJIT_CONFIG_ARM
	SLJIT_ASSERT(compiler->cache_arg == 0);
#endif
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM0(), (sljit_w)&buf[2], SLJIT_MEM0(), (sljit_w)&buf[1], SLJIT_MEM0(), (sljit_w)&buf[0]));
#ifdef SLJIT_CONFIG_ARM
	SLJIT_ASSERT(compiler->cache_arg > 0);
#endif
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, sizeof(sljit_w)));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 3, SLJIT_MEM1(SLJIT_TEMPORARY_REG1), (sljit_w)&buf[0], SLJIT_MEM1(SLJIT_TEMPORARY_REG2), (sljit_w)&buf[0]));
#ifdef SLJIT_CONFIG_ARM
	SLJIT_ASSERT(compiler->cache_arg > 0);
#endif
	T(sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_MEM2(SLJIT_GENERAL_REG1, SLJIT_TEMPORARY_REG2), sizeof(sljit_w) * 3, SLJIT_MEM2(SLJIT_GENERAL_REG1, SLJIT_TEMPORARY_REG2), -(sljit_w)sizeof(sljit_w), SLJIT_IMM, 2));
#ifdef SLJIT_CONFIG_ARM
	SLJIT_ASSERT(compiler->cache_arg > 0);
#endif
	T(sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_MEM2(SLJIT_GENERAL_REG1, SLJIT_TEMPORARY_REG2), sizeof(sljit_w) * 4, SLJIT_MEM2(SLJIT_GENERAL_REG1, SLJIT_TEMPORARY_REG2), sizeof(sljit_w), SLJIT_MEM2(SLJIT_GENERAL_REG1, SLJIT_TEMPORARY_REG1), 4 * sizeof(sljit_w)));
#ifdef SLJIT_CONFIG_ARM
	SLJIT_ASSERT(compiler->cache_arg > 0);
#endif
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 7, SLJIT_IMM, 10));
	// The last SLJIT_MEM2 is intentionally reversed
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM2(SLJIT_TEMPORARY_REG2, SLJIT_TEMPORARY_REG1), (sljit_w)&buf[5], SLJIT_MEM2(SLJIT_TEMPORARY_REG2, SLJIT_TEMPORARY_REG1), (sljit_w)&buf[6], SLJIT_MEM2(SLJIT_TEMPORARY_REG1, SLJIT_TEMPORARY_REG2), (sljit_w)&buf[5]));
#ifdef SLJIT_CONFIG_ARM
	SLJIT_ASSERT(compiler->cache_arg > 0);
#endif

	T(sljit_emit_return(compiler, SLJIT_UNUSED, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	code.func1((sljit_w)&buf);
	FAILED(buf[0] != 10, "test19 case 1 failed\n");
	FAILED(buf[1] != 4, "test19 case 2 failed\n");
	FAILED(buf[2] != 14, "test19 case 3 failed\n");
	FAILED(buf[3] != 14, "test19 case 4 failed\n");
	FAILED(buf[4] != 8, "test19 case 5 failed\n");
	FAILED(buf[5] != 6, "test19 case 6 failed\n");
	FAILED(buf[6] != 12, "test19 case 7 failed\n");
	FAILED(buf[7] != 10, "test19 case 8 failed\n");

	sljit_free_code(code.code);
	printf("test19 ok\n");
	successful_tests++;
}

static void test20(void)
{
	// Test stack
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_w buf[5];

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 5;
	buf[1] = 12;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = -12345;

	T(sljit_emit_enter(compiler, 1, 3, 2, 4 * sizeof(sljit_w)));

	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, sizeof(sljit_uw)));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM2(SLJIT_LOCALS_REG, SLJIT_TEMPORARY_REG1), 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM2(SLJIT_LOCALS_REG, SLJIT_TEMPORARY_REG1), -(int)sizeof(sljit_uw), SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_uw)));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 2 * sizeof(sljit_uw), SLJIT_MEM1(SLJIT_LOCALS_REG), sizeof(sljit_uw)));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_GENERAL_REG1), 3 * sizeof(sljit_uw), SLJIT_MEM2(SLJIT_TEMPORARY_REG1, SLJIT_LOCALS_REG), 0, SLJIT_MEM1(SLJIT_LOCALS_REG), 0));

	T(sljit_emit_return(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), 4 * sizeof(sljit_uw)));
	// Dummy codes
	TP(sljit_emit_const(compiler, SLJIT_TEMPORARY_REG1, 0, -9));
	TP(sljit_emit_label(compiler));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_w)&buf) != -12345, "test20 case 1 failed\n")

	FAILED(buf[2] != 5, "test20 case 2 failed\n");
	FAILED(buf[3] != 17, "test20 case 3 failed\n");

	sljit_free_code(code.code);
	printf("test20 ok\n");
	successful_tests++;
}

static void test21(void)
{
	// Test fake enter. The parts of the jit code can
	// be separated in the memory
	executable_code code1;
	executable_code code2;
	struct sljit_compiler* compiler = sljit_create_compiler();
	struct sljit_jump* jump;
	sljit_uw addr;
	sljit_w buf[4];

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 9;
	buf[1] = -6;
	buf[2] = 0;
	buf[3] = 0;

	T(sljit_emit_enter(compiler, 1, 3, 2, 2 * sizeof(sljit_w)));

	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_LOCALS_REG), 0, SLJIT_IMM, 10));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_LOCALS_REG), sizeof(sljit_w), SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_MEM1(SLJIT_LOCALS_REG), 0));
	jump = sljit_emit_jump(compiler, SLJIT_JUMP | SLJIT_REWRITABLE_JUMP);
	T(compiler->error);
	sljit_set_target(jump, 0);

	code1.code = sljit_generate_code(compiler);
	FAILED(!code1.code, "code generation error\n");
	addr = sljit_get_jump_addr(jump);
	sljit_free_compiler(compiler);

	compiler = sljit_create_compiler();
	FAILED(!compiler, "cannot create compiler\n");

	// Other part of the jit code
	sljit_fake_enter(compiler, 1, 3, 2, 2 * sizeof(sljit_w));

	T(sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 2, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w), SLJIT_MEM1(SLJIT_LOCALS_REG), 0));
	T(sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 3, SLJIT_MEM1(SLJIT_LOCALS_REG), 0, SLJIT_MEM1(SLJIT_LOCALS_REG), 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_PREF_RET_REG, 0, SLJIT_MEM1(SLJIT_LOCALS_REG), sizeof(sljit_w)));

	T(sljit_emit_return(compiler, SLJIT_PREF_RET_REG, 0));

	code2.code = sljit_generate_code(compiler);
	FAILED(!code2.code, "code generation error\n");
	sljit_free_compiler(compiler);

	sljit_set_jump_addr(addr, SLJIT_FUNC_OFFSET(code2.code));

	FAILED(code1.func1((sljit_w)&buf) != 19, "test21 case 1 failed\n");
	FAILED(buf[2] != -16, "test21 case 2 failed\n");
	FAILED(buf[3] != 100, "test21 case 3 failed\n");

	sljit_free_code(code1.code);
	sljit_free_code(code2.code);
	printf("test21 ok\n");
	successful_tests++;
}

static void test22(void)
{
	// Test simple byte and half-int data transfers
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_w buf[7];
	short sbuf[5];
	signed char bbuf[5];

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 5;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 0;
	buf[5] = 0;
	buf[6] = 0;

	sbuf[0] = 0;
	sbuf[1] = 0;
	sbuf[2] = -9;
	sbuf[3] = 0;
	sbuf[4] = 0;

	bbuf[0] = 0;
	bbuf[1] = 0;
	bbuf[2] = -56;
	bbuf[3] = 0;
	bbuf[4] = 0;

	T(sljit_emit_enter(compiler, 3, 3, 3, 0));

	T(sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_TEMPORARY_REG1, 0, SLJIT_GENERAL_REG1, 0, SLJIT_IMM, sizeof(sljit_w)));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_GENERAL_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_TEMPORARY_REG2), sizeof(sljit_w), SLJIT_IMM, -13));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_TEMPORARY_REG3, 0, SLJIT_MEM1(SLJIT_TEMPORARY_REG1), sizeof(sljit_w)));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_TEMPORARY_REG2), sizeof(sljit_w), SLJIT_TEMPORARY_REG3, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_TEMPORARY_REG2), sizeof(sljit_w), SLJIT_MEM1(SLJIT_TEMPORARY_REG1), sizeof(sljit_w)));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 2 * sizeof(sljit_w)));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM2(SLJIT_TEMPORARY_REG2, SLJIT_TEMPORARY_REG1), -(sljit_w)sizeof(sljit_w), SLJIT_TEMPORARY_REG2, 0));

	T(sljit_emit_op1(compiler, SLJIT_MOV_SH, SLJIT_MEM1(SLJIT_GENERAL_REG2), 0, SLJIT_IMM, -13));
	T(sljit_emit_op1(compiler, SLJIT_MOVU_UH, SLJIT_MEM1(SLJIT_GENERAL_REG2), sizeof(short), SLJIT_IMM, 0x1234));
	T(sljit_emit_op1(compiler, SLJIT_MOVU_SH, SLJIT_TEMPORARY_REG1, 0, SLJIT_MEM1(SLJIT_GENERAL_REG2), sizeof(short)));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 5 * sizeof(sljit_w), SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV_UH, SLJIT_MEM1(SLJIT_GENERAL_REG2), sizeof(short), SLJIT_MEM1(SLJIT_GENERAL_REG2), -(int)sizeof(short)));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 8000));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, 2 * sizeof(short)));
	T(sljit_emit_op1(compiler, SLJIT_MOV_SH, SLJIT_MEM2(SLJIT_GENERAL_REG2, SLJIT_TEMPORARY_REG2), 0, SLJIT_TEMPORARY_REG1, 0));

	T(sljit_emit_op1(compiler, SLJIT_MOV_SB, SLJIT_MEM1(SLJIT_GENERAL_REG3), 0, SLJIT_IMM, -45));
	T(sljit_emit_op1(compiler, SLJIT_MOVU_UB, SLJIT_MEM1(SLJIT_GENERAL_REG3), sizeof(char), SLJIT_IMM, 0x12));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 2 * sizeof(char)));
	T(sljit_emit_op1(compiler, SLJIT_MOVU_SB, SLJIT_TEMPORARY_REG2, 0, SLJIT_MEM1(SLJIT_GENERAL_REG3), sizeof(char)));
	T(sljit_emit_op1(compiler, SLJIT_MOV_SB, SLJIT_GENERAL_REG2, 0, SLJIT_TEMPORARY_REG2, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV_UB, SLJIT_GENERAL_REG2, 0, SLJIT_GENERAL_REG2, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV_SB, SLJIT_TEMPORARY_REG3, 0, SLJIT_GENERAL_REG2, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 6 * sizeof(sljit_w), SLJIT_TEMPORARY_REG3, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV_UB, SLJIT_MEM1(SLJIT_GENERAL_REG3), sizeof(char), SLJIT_GENERAL_REG2, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV_UB, SLJIT_MEM2(SLJIT_GENERAL_REG3, SLJIT_TEMPORARY_REG1), 0, SLJIT_TEMPORARY_REG1, 0));

	T(sljit_emit_return(compiler, SLJIT_UNUSED, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	code.func3((sljit_w)&buf, (sljit_w)&sbuf, (sljit_w)&bbuf);
	FAILED(buf[1] != -13, "test22 case 1 failed\n");
	FAILED(buf[2] != 5, "test22 case 2 failed\n");
	FAILED(buf[3] != -13, "test22 case 3 failed\n");
	FAILED(buf[4] != (sljit_w)&buf[3], "test22 case 4 failed\n");
	FAILED(buf[5] != -9, "test22 case 5 failed\n");
	FAILED(buf[6] != -56, "test22 case 6 failed\n");

	FAILED(sbuf[0] != -13, "test22 case 7 failed\n");
	FAILED(sbuf[1] != 0x1234, "test22 case 8 failed\n");
	FAILED(sbuf[3] != 0x1234, "test22 case 9 failed\n");
	FAILED(sbuf[4] != 8000, "test22 case 10 failed\n");

	FAILED(bbuf[0] != -45, "test22 case 11 failed\n");
	FAILED(bbuf[1] != 0x12, "test22 case 12 failed\n");
	FAILED(bbuf[3] != -56, "test22 case 13 failed\n");
	FAILED(bbuf[4] != 2, "test22 case 14 failed\n");

	sljit_free_code(code.code);
	printf("test22 ok\n");
	successful_tests++;
}

static void test23(void)
{
	// Test 32 bit / 64 bit signed / unsigned int transfer and conversion
	// This test has on real meaning on 64 bit systems, but works on 32 bit systems as well
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_w buf[7];
	int ibuf[5];
#ifdef SLJIT_64BIT_ARCHITECTURE
	sljit_w garbage = 0x1234567812345678;
#else
	sljit_w garbage = 0x12345678;
#endif

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 0;
	buf[5] = 0;
	buf[6] = 0;

	ibuf[0] = 0;
	ibuf[1] = 0;
	ibuf[2] = -5791;
	ibuf[3] = 43579;
	ibuf[4] = 658923;

	T(sljit_emit_enter(compiler, 2, 3, 3, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV_UI, SLJIT_MEM1(SLJIT_GENERAL_REG2), 0, SLJIT_IMM, 34567));
	T(sljit_emit_op1(compiler, SLJIT_MOVU_SI, SLJIT_MEM1(SLJIT_GENERAL_REG2), sizeof(int), SLJIT_IMM, -7654));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, garbage));
	T(sljit_emit_op1(compiler, SLJIT_MOVU_SI, SLJIT_TEMPORARY_REG1, 0, SLJIT_MEM1(SLJIT_GENERAL_REG2), sizeof(int)));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, garbage));
	T(sljit_emit_op1(compiler, SLJIT_MOVU_UI, SLJIT_TEMPORARY_REG1, 0, SLJIT_MEM1(SLJIT_GENERAL_REG2), sizeof(int)));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w), SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, garbage));
	T(sljit_emit_op1(compiler, SLJIT_MOV_SI, SLJIT_TEMPORARY_REG1, 0, SLJIT_MEM1(SLJIT_GENERAL_REG2), sizeof(int)));
	T(sljit_emit_op1(compiler, SLJIT_MOV_UI, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w), SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0x0f00f00));
	T(sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_GENERAL_REG1, 0, SLJIT_GENERAL_REG1, 0, SLJIT_IMM, 0x7777));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0x7777 + 2 * sizeof(sljit_w), SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_GENERAL_REG1, 0, SLJIT_GENERAL_REG1, 0, SLJIT_IMM, 0x7777));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_GENERAL_REG1), -0x7777 + (int)sizeof(sljit_w), SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0x0abcdef1));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM2(SLJIT_TEMPORARY_REG1, SLJIT_GENERAL_REG1), -0x0abcdef1 + (int)sizeof(sljit_w), SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_TEMPORARY_REG1), (sljit_w)&buf[6], SLJIT_MEM0(), (sljit_w)&buf[6]));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_TEMPORARY_REG1), 0, SLJIT_IMM, 0x123456));

	T(sljit_emit_return(compiler, SLJIT_UNUSED, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	code.func2((sljit_w)&buf, (sljit_w)&ibuf);
	FAILED(buf[0] != -5791, "test23 case 1 failed\n");
	FAILED(buf[1] != 43579, "test23 case 2 failed\n");
	FAILED(buf[2] != 658923, "test23 case 3 failed\n");
	FAILED(buf[3] != 0x0f00f00, "test23 case 4 failed\n");
	FAILED(buf[4] != 0x0f00f00, "test23 case 5 failed\n");
	FAILED(buf[5] != 0x0abcdef1, "test23 case 6 failed\n");
	FAILED(buf[6] != 0x123456, "test23 case 7 failed\n");

	FAILED(ibuf[0] != 34567, "test23 case 8 failed\n");
	FAILED(ibuf[1] != -7654, "test23 case 9 failed\n");

	sljit_free_code(code.code);
	printf("test23 ok\n");
	successful_tests++;
}

static void test24(void)
{
	// Some complicated addressing modes
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_w buf[3];
	short sbuf[5];
	signed char bbuf[4];

	FAILED(!compiler, "cannot create compiler\n");

	buf[0] = 100567;
	buf[1] = 75799;
	buf[2] = 0;

	sbuf[0] = 30000;
	sbuf[1] = 0;
	sbuf[2] = 0;
	sbuf[3] = -12345;
	sbuf[4] = 0;

	bbuf[0] = -128;
	bbuf[1] = 0;
	bbuf[2] = 0;
	bbuf[3] = 99;

	T(sljit_emit_enter(compiler, 3, 3, 3, 0));

	// Nothing should be updated
	T(sljit_emit_op1(compiler, SLJIT_MOVU_SH, SLJIT_MEM0(), (sljit_w)&sbuf[1], SLJIT_MEM0(), (sljit_w)&sbuf[0]));
#ifdef SLJIT_CONFIG_ARM
	SLJIT_ASSERT(compiler->cache_arg > 0);
#endif
	T(sljit_emit_op1(compiler, SLJIT_MOVU_SB, SLJIT_MEM0(), (sljit_w)&bbuf[1], SLJIT_MEM0(), (sljit_w)&bbuf[0]));
#ifdef SLJIT_CONFIG_ARM
	SLJIT_ASSERT(compiler->cache_arg > 0);
#endif
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, -(int)sizeof(short)));
	T(sljit_emit_op1(compiler, SLJIT_MOV_UH, SLJIT_MEM2(SLJIT_GENERAL_REG2, SLJIT_TEMPORARY_REG1), 3 * sizeof(short), SLJIT_MEM2(SLJIT_GENERAL_REG2, SLJIT_TEMPORARY_REG1), 4 * sizeof(short)));
#ifdef SLJIT_CONFIG_ARM
	SLJIT_ASSERT(compiler->cache_arg > 0);
#endif
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, sizeof(sljit_w)));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG3, 0, SLJIT_IMM, 2 * sizeof(sljit_w)));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM2(SLJIT_TEMPORARY_REG3, SLJIT_TEMPORARY_REG1), (sljit_w)&buf[0], SLJIT_MEM2(SLJIT_TEMPORARY_REG1, SLJIT_TEMPORARY_REG1), (sljit_w)&buf[0], SLJIT_MEM2(SLJIT_TEMPORARY_REG2, SLJIT_TEMPORARY_REG1), (sljit_w)&buf[0]));
#ifdef SLJIT_CONFIG_ARM
	SLJIT_ASSERT(compiler->cache_arg > 0);
#endif
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, sizeof(signed char)));
	T(sljit_emit_op1(compiler, SLJIT_MOV_UB, SLJIT_MEM2(SLJIT_TEMPORARY_REG1, SLJIT_TEMPORARY_REG2), (sljit_w)&bbuf[1], SLJIT_MEM2(SLJIT_TEMPORARY_REG1, SLJIT_TEMPORARY_REG2), (sljit_w)&bbuf[2]));
#ifdef SLJIT_CONFIG_ARM
	SLJIT_ASSERT(compiler->cache_arg > 0);
#endif
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, sizeof(short)));
	T(sljit_emit_op1(compiler, SLJIT_MOV_SH, SLJIT_MEM2(SLJIT_TEMPORARY_REG1, SLJIT_TEMPORARY_REG2), (sljit_w)&sbuf[3], SLJIT_TEMPORARY_REG2, 0));
#ifdef SLJIT_CONFIG_ARM
	SLJIT_ASSERT(compiler->cache_arg == 0);
#endif

	T(sljit_emit_return(compiler, SLJIT_UNUSED, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	code.func3((sljit_w)&buf, (sljit_w)&sbuf, (sljit_w)&bbuf);
	FAILED(buf[2] != 176366, "test24 case 1 failed\n");

	FAILED(sbuf[1] != 30000, "test24 case 2 failed\n");
	FAILED(sbuf[2] != -12345, "test24 case 3 failed\n");
	FAILED(sbuf[4] != sizeof(short), "test24 case 4 failed\n");

	FAILED(bbuf[1] != -128, "test24 case 5 failed\n");
	FAILED(bbuf[2] != 99, "test24 case 6 failed\n");

	sljit_free_code(code.code);
	printf("test24 ok\n");
	successful_tests++;
}

static void test25(void)
{
#ifdef SLJIT_64BIT_ARCHITECTURE
	// 64 bit loads
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_w buf[14];

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

	T(sljit_emit_enter(compiler, 1, 3, 1, 0));

	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_IMM, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 1 * sizeof(sljit_w), SLJIT_IMM, 0x7fff));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 2 * sizeof(sljit_w), SLJIT_IMM, -0x8000));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 3 * sizeof(sljit_w), SLJIT_IMM, 0x7fffffff));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 4 * sizeof(sljit_w), SLJIT_IMM, -0x80000000l));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 5 * sizeof(sljit_w), SLJIT_IMM, 0x1234567887654321));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 6 * sizeof(sljit_w), SLJIT_IMM, 0xff80000000));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 7 * sizeof(sljit_w), SLJIT_IMM, 0x3ff0000000));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 8 * sizeof(sljit_w), SLJIT_IMM, 0xfffffff800100000));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 9 * sizeof(sljit_w), SLJIT_IMM, 0xfffffff80010f000));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 10 * sizeof(sljit_w), SLJIT_IMM, 0x07fff00000008001));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 11 * sizeof(sljit_w), SLJIT_IMM, 0x07fff00080010000));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 12 * sizeof(sljit_w), SLJIT_IMM, 0x07fff00080018001));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 13 * sizeof(sljit_w), SLJIT_IMM, 0x07fff00ffff00000));

	T(sljit_emit_return(compiler, SLJIT_UNUSED, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	code.func1((sljit_w)&buf);
	FAILED(buf[0] != 0, "test25 case 1 failed\n");
	FAILED(buf[1] != 0x7fff, "test25 case 2 failed\n");
	FAILED(buf[2] != -0x8000, "test25 case 3 failed\n");
	FAILED(buf[3] != 0x7fffffff, "test25 case 4 failed\n");
	FAILED(buf[4] != -0x80000000l, "test25 case 5 failed\n");
	FAILED(buf[5] != 0x1234567887654321, "test25 case 6 failed\n");
	FAILED(buf[6] != 0xff80000000, "test25 case 7 failed\n");
	FAILED(buf[7] != 0x3ff0000000, "test25 case 8 failed\n");
	FAILED((sljit_uw)buf[8] != 0xfffffff800100000, "test25 case 9 failed\n");
	FAILED((sljit_uw)buf[9] != 0xfffffff80010f000, "test25 case 10 failed\n");
	FAILED(buf[10] != 0x07fff00000008001, "test25 case 11 failed\n");
	FAILED(buf[11] != 0x07fff00080010000, "test25 case 12 failed\n");
	FAILED(buf[12] != 0x07fff00080018001, "test25 case 13 failed\n");
	FAILED(buf[13] != 0x07fff00ffff00000, "test25 case 14 failed\n");

	sljit_free_code(code.code);
#endif
	printf("test25 ok\n");
	successful_tests++;
}

static void test26(void)
{
	// Aligned access without aligned offsets
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_w buf[4];
	int ibuf[4];
	double dbuf[4];

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

	T(sljit_emit_enter(compiler, 3, 3, 3, 0));

	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_GENERAL_REG1, 0, SLJIT_GENERAL_REG1, 0, SLJIT_IMM, 3));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_GENERAL_REG2, 0, SLJIT_GENERAL_REG2, 0, SLJIT_IMM, 1));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), -3));
	T(sljit_emit_op1(compiler, SLJIT_MOV_SI, SLJIT_MEM1(SLJIT_GENERAL_REG2), sizeof(int) - 1, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV_SI, SLJIT_TEMPORARY_REG1, 0, SLJIT_MEM1(SLJIT_GENERAL_REG2), -1));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) - 3, SLJIT_TEMPORARY_REG1, 0));

	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_TEMPORARY_REG1, 0, SLJIT_GENERAL_REG1, 0, SLJIT_IMM, 100));
	T(sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_MEM1(SLJIT_TEMPORARY_REG1), sizeof(sljit_w) * 2 - 103, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 2 - 3, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 3 - 3));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_TEMPORARY_REG1, 0, SLJIT_GENERAL_REG2, 0, SLJIT_IMM, 100));
	T(sljit_emit_op2(compiler, SLJIT_MUL | SLJIT_INT_OP, SLJIT_MEM1(SLJIT_TEMPORARY_REG1), sizeof(int) * 2 - 101, SLJIT_MEM1(SLJIT_GENERAL_REG2), sizeof(int) * 2 - 1, SLJIT_MEM1(SLJIT_GENERAL_REG2), sizeof(int) * 3 - 1));

	if (sljit_is_fpu_available()) {
		T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_GENERAL_REG3, 0, SLJIT_GENERAL_REG3, 0, SLJIT_IMM, 3));
		T(sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_MEM1(SLJIT_GENERAL_REG3), sizeof(double) - 3, SLJIT_MEM1(SLJIT_GENERAL_REG3), -3));
		T(sljit_emit_fop2(compiler, SLJIT_FADD, SLJIT_MEM1(SLJIT_GENERAL_REG3), sizeof(double) * 2 - 3, SLJIT_MEM1(SLJIT_GENERAL_REG3), -3, SLJIT_MEM1(SLJIT_GENERAL_REG3), sizeof(double) - 3));
		T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_TEMPORARY_REG1, 0, SLJIT_GENERAL_REG3, 0, SLJIT_IMM, 2));
		T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, 3));
		T(sljit_emit_fop2(compiler, SLJIT_FDIV, SLJIT_MEM1(SLJIT_TEMPORARY_REG1), sizeof(double) * 3 - 5, SLJIT_MEM1(SLJIT_GENERAL_REG3), sizeof(double) * 2 - 3, SLJIT_MEM2(SLJIT_GENERAL_REG3, SLJIT_TEMPORARY_REG2), sizeof(double) * 3 - 6));
	}

	T(sljit_emit_return(compiler, SLJIT_UNUSED, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	code.func3((sljit_w)&buf, (sljit_w)&ibuf, (sljit_w)&dbuf);

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
	printf("test26 ok\n");
	successful_tests++;
}

static void test27(void)
{
#define SET_NEXT_BYTE(type) \
		T(sljit_emit_cond_set(compiler, SLJIT_TEMPORARY_REG3, 0, type)); \
		T(sljit_emit_op1(compiler, SLJIT_MOVU_UB, SLJIT_MEM1(SLJIT_GENERAL_REG1), 1, SLJIT_TEMPORARY_REG3, 0));
#ifdef SLJIT_64BIT_ARCHITECTURE
#define RESULT(i) i
#else
#define RESULT(i) (1 - i)
#endif


	// Playing with conditional flags
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_b buf[25];
	int i;

	for (i = 0; i < 25; ++i)
		buf[i] = 10;

	FAILED(!compiler, "cannot create compiler\n");

	// 3 arguments passed, 3 arguments used
	T(sljit_emit_enter(compiler, 1, 3, 3, 0));

	T(sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_GENERAL_REG1, 0, SLJIT_GENERAL_REG1, 0, SLJIT_IMM, 1));

	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0x1001));
	T(sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 20));
	// 0x100100000 on 64 bit machines, 0x100000 on 32 bit machines
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, 0x800000));
	T(sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_U, SLJIT_UNUSED, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG2, 0));
	SET_NEXT_BYTE(SLJIT_C_GREATER);
	SET_NEXT_BYTE(SLJIT_C_LESS);
	T(sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_INT_OP | SLJIT_SET_U, SLJIT_UNUSED, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG2, 0));
	SET_NEXT_BYTE(SLJIT_C_GREATER);
	SET_NEXT_BYTE(SLJIT_C_LESS);

	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0x1000));
	T(sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 20));
	T(sljit_emit_op2(compiler, SLJIT_OR, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0x10));
	// 0x100000010 on 64 bit machines, 0x10 on 32 bit machines
	T(sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_U, SLJIT_UNUSED, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0x80));
	SET_NEXT_BYTE(SLJIT_C_GREATER);
	SET_NEXT_BYTE(SLJIT_C_LESS);
	T(sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_INT_OP | SLJIT_SET_U, SLJIT_UNUSED, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0x80));
	SET_NEXT_BYTE(SLJIT_C_GREATER);
	SET_NEXT_BYTE(SLJIT_C_LESS);

	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0));
	T(sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 1));
	// 0xff..ff on all machines
	T(sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_U, SLJIT_TEMPORARY_REG2, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 1));
	SET_NEXT_BYTE(SLJIT_C_NOT_GREATER);
	SET_NEXT_BYTE(SLJIT_C_NOT_LESS);
	T(sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_S, SLJIT_TEMPORARY_REG2, 0, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, -1));
	SET_NEXT_BYTE(SLJIT_C_SIG_GREATER);
	SET_NEXT_BYTE(SLJIT_C_SIG_LESS);
	T(sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_E, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG2, 0, SLJIT_TEMPORARY_REG1, 0));
	SET_NEXT_BYTE(SLJIT_C_EQUAL);
	SET_NEXT_BYTE(SLJIT_C_NOT_EQUAL);
	T(sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_O | SLJIT_SET_U, SLJIT_TEMPORARY_REG2, 0, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, -2));
	SET_NEXT_BYTE(SLJIT_C_OVERFLOW);
	SET_NEXT_BYTE(SLJIT_C_NOT_OVERFLOW);
	SET_NEXT_BYTE(SLJIT_C_NOT_LESS);
	SET_NEXT_BYTE(SLJIT_C_NOT_GREATER);

	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0x80000000));
	T(sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 16));
	T(sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 16));
	// 0x80..0 on 64 bit machines, 0 on 32 bit machines
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, 0xffffffff));
	T(sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_O, SLJIT_UNUSED, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG2, 0));
	SET_NEXT_BYTE(SLJIT_C_OVERFLOW);
	SET_NEXT_BYTE(SLJIT_C_NOT_OVERFLOW);
	T(sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_INT_OP | SLJIT_SET_O, SLJIT_UNUSED, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG2, 0));
	SET_NEXT_BYTE(SLJIT_C_OVERFLOW);
	SET_NEXT_BYTE(SLJIT_C_NOT_OVERFLOW);

	T(sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_C, SLJIT_UNUSED, 0, SLJIT_IMM, 0, SLJIT_IMM, 1));
	T(sljit_emit_op2(compiler, SLJIT_SUBC | SLJIT_SET_U, SLJIT_UNUSED, 0, SLJIT_IMM, 1, SLJIT_IMM, 0));
	SET_NEXT_BYTE(SLJIT_C_GREATER);
	SET_NEXT_BYTE(SLJIT_C_LESS);

	T(sljit_emit_return(compiler, SLJIT_UNUSED, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	code.func1((sljit_w)&buf);
	sljit_free_code(code.code);

	FAILED(buf[0] != RESULT(1), "test27 case 1 failed\n");
	FAILED(buf[1] != RESULT(0), "test27 case 2 failed\n");
	FAILED(buf[2] != 0, "test27 case 3 failed\n");
	FAILED(buf[3] != 1, "test27 case 4 failed\n");
	FAILED(buf[4] != RESULT(1), "test27 case 5 failed\n");
	FAILED(buf[5] != RESULT(0), "test27 case 6 failed\n");
	FAILED(buf[6] != 0, "test27 case 7 failed\n");
	FAILED(buf[7] != 1, "test27 case 8 failed\n");

	FAILED(buf[8] != 0, "test27 case 9 failed\n");
	FAILED(buf[9] != 1, "test27 case 10 failed\n");
	FAILED(buf[10] != 0, "test27 case 11 failed\n");
	FAILED(buf[11] != 1, "test27 case 12 failed\n");
	FAILED(buf[12] != 1, "test27 case 13 failed\n");
	FAILED(buf[13] != 0, "test27 case 14 failed\n");
	FAILED(buf[14] != 0, "test27 case 15 failed\n");
	FAILED(buf[15] != 1, "test27 case 16 failed\n");
	FAILED(buf[16] != 1, "test27 case 17 failed\n");
	FAILED(buf[17] != 0, "test27 case 18 failed\n");

	FAILED(buf[18] != RESULT(1), "test27 case 19 failed\n");
	FAILED(buf[19] != RESULT(0), "test27 case 20 failed\n");
	FAILED(buf[20] != 0, "test27 case 21 failed\n");
	FAILED(buf[21] != 1, "test27 case 22 failed\n");

	FAILED(buf[22] != 0, "test27 case 23 failed\n");
	FAILED(buf[23] != 0, "test27 case 24 failed\n");

	FAILED(buf[24] != 10, "test27 case 25 failed\n");
	printf("test27 ok\n");
	successful_tests++;
#undef SET_NEXT_BYTE
#undef RESULT
}

static void test28(void)
{
	// Test mov
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	struct sljit_const* const1;
	struct sljit_label* label;
	sljit_uw label_addr;
	sljit_w buf[5];

	FAILED(!compiler, "cannot create compiler\n");

	buf[0] = -36;
	buf[1] = 8;
	buf[2] = 0;
	buf[3] = 10;
	buf[4] = 0;

	T(sljit_emit_enter(compiler, 1, 5, 5, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_EREG1, 0, SLJIT_IMM, -234));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_EREG2, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w)));
	T(sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_GENERAL_EREG1, 0, SLJIT_TEMPORARY_EREG1, 0, SLJIT_TEMPORARY_EREG2, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w), SLJIT_GENERAL_EREG1, 0));
	T(sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_GENERAL_EREG1, 0, SLJIT_IMM, 0));
	T(sljit_emit_cond_set(compiler, SLJIT_GENERAL_EREG1, 0, SLJIT_C_NOT_ZERO));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 2 * sizeof(sljit_w), SLJIT_GENERAL_EREG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_GENERAL_EREG2, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 3 * sizeof(sljit_w)));
	T(sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_GENERAL_EREG2, 0, SLJIT_GENERAL_EREG2, 0, SLJIT_TEMPORARY_EREG2, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 3 * sizeof(sljit_w), SLJIT_GENERAL_EREG2, 0));
	const1 = sljit_emit_const(compiler, SLJIT_GENERAL_EREG1, 0, 0);
	T(sljit_emit_ijump(compiler, SLJIT_JUMP, SLJIT_GENERAL_EREG1, 0));
	T(sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_GENERAL_EREG1, 0, SLJIT_GENERAL_EREG1, 0, SLJIT_IMM, 100));
	label = sljit_emit_label(compiler);
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 4 * sizeof(sljit_w), SLJIT_GENERAL_EREG1, 0));
	T(sljit_emit_return(compiler, SLJIT_TEMPORARY_EREG2, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	label_addr = sljit_get_label_addr(label);
	sljit_set_const(sljit_get_const_addr(const1), label_addr);
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_w)&buf) != 8, "test28 case 1 failed\n");
	FAILED(buf[1] != -1872, "test28 case 2 failed\n");
	FAILED(buf[2] != 1, "test28 case 3 failed\n");
	FAILED(buf[3] != 2, "test28 case 4 failed\n");
	FAILED(buf[4] != label_addr, "test28 case 5 failed\n");
	sljit_free_code(code.code);
	printf("test28 ok\n");
	successful_tests++;
}

static void test29(void)
{
	// Test signed/unsigned bytes and halfs
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();

	sljit_w buf[24];
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

	T(sljit_emit_enter(compiler, 1, 5, 5, 0));

	T(sljit_emit_op1(compiler, SLJIT_MOV_SB, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, -187));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOVU_SB, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, -605));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_uw), SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV_UB, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, -56));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_uw), SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOVU_UB, SLJIT_TEMPORARY_EREG2, 0, SLJIT_IMM, 0xcde5));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_uw), SLJIT_TEMPORARY_EREG2, 0));

	T(sljit_emit_op1(compiler, SLJIT_MOV_SH, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, -45896));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_uw), SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOVU_SH, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, -1472797));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_uw), SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV_UH, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, -12890));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_uw), SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOVU_UH, SLJIT_TEMPORARY_EREG2, 0, SLJIT_IMM, 0x9cb0a6));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_uw), SLJIT_TEMPORARY_EREG2, 0));

#ifdef SLJIT_64BIT_ARCHITECTURE
	T(sljit_emit_op1(compiler, SLJIT_MOV_SI, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, -3580429715l));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_uw), SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOVU_SI, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, -100722768662l));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_uw), SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV_UI, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, -1457052677972l));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_uw), SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOVU_UI, SLJIT_TEMPORARY_EREG2, 0, SLJIT_IMM, 0xcef97a70b5l));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_uw), SLJIT_TEMPORARY_EREG2, 0));
#else
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_GENERAL_REG1, 0, SLJIT_GENERAL_REG1, 0, SLJIT_IMM, 4 * sizeof(sljit_uw)));
#endif

	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, -187));
	T(sljit_emit_op1(compiler, SLJIT_MOV_SB, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG2, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_uw), SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_GENERAL_REG3, 0, SLJIT_IMM, -605));
	T(sljit_emit_op1(compiler, SLJIT_MOVU_SB, SLJIT_TEMPORARY_REG1, 0, SLJIT_GENERAL_REG3, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_uw), SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG3, 0, SLJIT_IMM, -56));
	T(sljit_emit_op1(compiler, SLJIT_MOV_UB, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG3, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_uw), SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_EREG1, 0, SLJIT_IMM, 0xcde5));
	T(sljit_emit_op1(compiler, SLJIT_MOVU_UB, SLJIT_TEMPORARY_EREG2, 0, SLJIT_TEMPORARY_EREG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_uw), SLJIT_TEMPORARY_EREG2, 0));

	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, -45896));
	T(sljit_emit_op1(compiler, SLJIT_MOV_SH, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG2, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_uw), SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_GENERAL_REG3, 0, SLJIT_IMM, -1472797));
	T(sljit_emit_op1(compiler, SLJIT_MOVU_SH, SLJIT_TEMPORARY_REG1, 0, SLJIT_GENERAL_REG3, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_uw), SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG3, 0, SLJIT_IMM, -12890));
	T(sljit_emit_op1(compiler, SLJIT_MOV_UH, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG3, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_uw), SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_EREG1, 0, SLJIT_IMM, 0x9cb0a6));
	T(sljit_emit_op1(compiler, SLJIT_MOVU_UH, SLJIT_TEMPORARY_EREG2, 0, SLJIT_TEMPORARY_EREG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_uw), SLJIT_TEMPORARY_EREG2, 0));

#ifdef SLJIT_64BIT_ARCHITECTURE
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, -3580429715l));
	T(sljit_emit_op1(compiler, SLJIT_MOV_SI, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG2, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_uw), SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_GENERAL_REG3, 0, SLJIT_IMM, -100722768662l));
	T(sljit_emit_op1(compiler, SLJIT_MOVU_SI, SLJIT_TEMPORARY_REG1, 0, SLJIT_GENERAL_REG3, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_uw), SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG3, 0, SLJIT_IMM, -1457052677972l));
	T(sljit_emit_op1(compiler, SLJIT_MOV_UI, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG3, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_uw), SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_EREG1, 0, SLJIT_IMM, 0xcef97a70b5l));
	T(sljit_emit_op1(compiler, SLJIT_MOVU_UI, SLJIT_TEMPORARY_EREG2, 0, SLJIT_TEMPORARY_EREG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_uw), SLJIT_TEMPORARY_EREG2, 0));
#endif

	T(sljit_emit_return(compiler, SLJIT_UNUSED, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	code.func1((sljit_w)&buf);
	FAILED(buf[0] != 69, "test29 case 1 failed\n");
	FAILED(buf[1] != -93, "test29 case 2 failed\n");
	FAILED(buf[2] != 200, "test29 case 3 failed\n");
	FAILED(buf[3] != 0xe5, "test29 case 4 failed\n");
	FAILED(buf[4] != 19640, "test29 case 5 failed\n");
	FAILED(buf[5] != -31005, "test29 case 6 failed\n");
	FAILED(buf[6] != 52646, "test29 case 7 failed\n");
	FAILED(buf[7] != 0xb0a6, "test29 case 8 failed\n");

#ifdef SLJIT_64BIT_ARCHITECTURE
	FAILED(buf[8] != 714537581l, "test29 case 9 failed\n");
	FAILED(buf[9] != -1938520854l, "test29 case 10 failed\n");
	FAILED(buf[10] != 3236202668l, "test29 case 11 failed\n");
	FAILED(buf[11] != 0xf97a70b5l, "test29 case 12 failed\n");
#endif

	FAILED(buf[12] != 69, "test29 case 13 failed\n");
	FAILED(buf[13] != -93, "test29 case 14 failed\n");
	FAILED(buf[14] != 200, "test29 case 15 failed\n");
	FAILED(buf[15] != 0xe5, "test29 case 16 failed\n");
	FAILED(buf[16] != 19640, "test29 case 17 failed\n");
	FAILED(buf[17] != -31005, "test29 case 18 failed\n");
	FAILED(buf[18] != 52646, "test29 case 19 failed\n");
	FAILED(buf[19] != 0xb0a6, "test29 case 20 failed\n");

#ifdef SLJIT_64BIT_ARCHITECTURE
	FAILED(buf[20] != 714537581l, "test29 case 21 failed\n");
	FAILED(buf[21] != -1938520854l, "test29 case 22 failed\n");
	FAILED(buf[22] != 3236202668l, "test29 case 23 failed\n");
	FAILED(buf[23] != 0xf97a70b5l, "test29 case 24 failed\n");
#endif

	sljit_free_code(code.code);
	printf("test29 ok\n");
	successful_tests++;
}

static void test30(void)
{
	// Test unused results
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();

	sljit_w buf[1];
	buf[0] = 0;

	T(sljit_emit_enter(compiler, 1, 5, 5, 0));

	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 1));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, 1));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG3, 0, SLJIT_IMM, 1));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_EREG1, 0, SLJIT_IMM, 1));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_EREG2, 0, SLJIT_IMM, 1));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_GENERAL_REG2, 0, SLJIT_IMM, 1));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_GENERAL_REG3, 0, SLJIT_IMM, 1));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_GENERAL_EREG1, 0, SLJIT_IMM, 1));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_GENERAL_EREG2, 0, SLJIT_IMM, 1));

	// some calculations with unused results
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_UNUSED, 0, SLJIT_TEMPORARY_REG3, 0));
	T(sljit_emit_op1(compiler, SLJIT_NOT, SLJIT_UNUSED, 0, SLJIT_TEMPORARY_REG2, 0));
	T(sljit_emit_op1(compiler, SLJIT_NEG, SLJIT_UNUSED, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0));
	T(sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0));
	T(sljit_emit_op2(compiler, SLJIT_SUB | SLJIT_INT_OP | SLJIT_SET_U, SLJIT_UNUSED, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0));
	T(sljit_emit_op2(compiler, SLJIT_MUL, SLJIT_UNUSED, 0, SLJIT_GENERAL_REG1, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0));
	T(sljit_emit_op2(compiler, SLJIT_SHL | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_GENERAL_EREG1, 0, SLJIT_TEMPORARY_REG3, 0));
	T(sljit_emit_op2(compiler, SLJIT_LSHR | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_IMM, 5));
	T(sljit_emit_op2(compiler, SLJIT_AND, SLJIT_UNUSED, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_IMM, 0xff));
	T(sljit_emit_op1(compiler, SLJIT_NOT | SLJIT_INT_OP | SLJIT_SET_E, SLJIT_UNUSED, 0, SLJIT_GENERAL_REG2, 0));

	// Testing that any change happens
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG2, 0));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG3, 0));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_EREG1, 0));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_EREG2, 0));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_GENERAL_REG2, 0));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_GENERAL_REG3, 0));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_GENERAL_EREG1, 0));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_GENERAL_EREG2, 0));

	T(sljit_emit_return(compiler, SLJIT_UNUSED, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	code.func1((sljit_w)&buf);
	FAILED(buf[0] != 9, "test30 case 1 failed\n");
	sljit_free_code(code.code);
	printf("test30 ok\n");
	successful_tests++;
}

static void test31(void)
{
	// Integer mul and set flags
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();

#ifdef SLJIT_64BIT_ARCHITECTURE
	sljit_w big_word = 0x7fffffff00000000l;
	sljit_w big_word2 = 0x7fffffff00000012l;
#else
	sljit_w big_word = 0x7fffffff;
	sljit_w big_word2 = 0x00000012;
#endif

	sljit_w buf[12];
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

	T(sljit_emit_enter(compiler, 1, 3, 5, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, 0));
	T(sljit_emit_op2(compiler, SLJIT_MUL | SLJIT_SET_O, SLJIT_UNUSED, 0, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, -45));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_C_MUL_NOT_OVERFLOW));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w), SLJIT_C_MUL_OVERFLOW));

	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_GENERAL_REG3, 0, SLJIT_IMM, big_word));
	T(sljit_emit_op2(compiler, SLJIT_MUL | SLJIT_SET_O, SLJIT_TEMPORARY_REG3, 0, SLJIT_GENERAL_REG3, 0, SLJIT_IMM, -2));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, 33)); // Should not change flags
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, 0)); // Should not change flags
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), 2 * sizeof(sljit_w), SLJIT_C_MUL_OVERFLOW));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), 3 * sizeof(sljit_w), SLJIT_C_MUL_NOT_OVERFLOW));

	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_GENERAL_EREG1, 0, SLJIT_IMM, 0x3f6b0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_GENERAL_EREG2, 0, SLJIT_IMM, 0x2a783));
	T(sljit_emit_op2(compiler, SLJIT_MUL | SLJIT_INT_OP | SLJIT_SET_O, SLJIT_TEMPORARY_REG2, 0, SLJIT_GENERAL_EREG1, 0, SLJIT_GENERAL_EREG2, 0));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), 4 * sizeof(sljit_w), SLJIT_C_MUL_OVERFLOW));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 5 * sizeof(sljit_w), SLJIT_TEMPORARY_REG2, 0));

	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, big_word2));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG3, 0, SLJIT_TEMPORARY_REG2, 0));
	T(sljit_emit_op2(compiler, SLJIT_MUL | SLJIT_INT_OP | SLJIT_SET_O, SLJIT_TEMPORARY_REG2, 0, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, 23));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), 6 * sizeof(sljit_w), SLJIT_C_MUL_OVERFLOW));

	T(sljit_emit_op2(compiler, SLJIT_MUL | SLJIT_INT_OP | SLJIT_SET_O, SLJIT_UNUSED, 0, SLJIT_TEMPORARY_REG3, 0, SLJIT_IMM, -23));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), 7 * sizeof(sljit_w), SLJIT_C_MUL_NOT_OVERFLOW));
	T(sljit_emit_op2(compiler, SLJIT_MUL | SLJIT_SET_O, SLJIT_UNUSED, 0, SLJIT_TEMPORARY_REG3, 0, SLJIT_IMM, -23));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), 8 * sizeof(sljit_w), SLJIT_C_MUL_NOT_OVERFLOW));

	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, 67));
	T(sljit_emit_op2(compiler, SLJIT_MUL | SLJIT_SET_O, SLJIT_TEMPORARY_REG2, 0, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, -23));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 9 * sizeof(sljit_w), SLJIT_TEMPORARY_REG2, 0));

	T(sljit_emit_return(compiler, SLJIT_UNUSED, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	code.func1((sljit_w)&buf);

	FAILED(buf[0] != 1, "test31 case 1 failed\n");
	FAILED(buf[1] != 0, "test31 case 2 failed\n");
// Qemu issues for 64 bit muls
#ifndef SLJIT_64BIT_ARCHITECTURE
	FAILED(buf[2] != 1, "test31 case 3 failed\n");
	FAILED(buf[3] != 0, "test31 case 4 failed\n");
#endif
	FAILED(buf[4] != 1, "test31 case 5 failed\n");
	FAILED((buf[5] & 0xffffffff) != 0x85540c10, "test31 case 6 failed\n");
	FAILED(buf[6] != 0, "test31 case 7 failed\n");
	FAILED(buf[7] != 1, "test31 case 8 failed\n");
#ifndef SLJIT_64BIT_ARCHITECTURE
	FAILED(buf[8] != 1, "test31 case 9 failed\n");
#endif
	FAILED(buf[9] != -1541, "test31 case 10 failed\n");
	sljit_free_code(code.code);
	printf("test31 ok\n");
	successful_tests++;
}

static void test32(void)
{
	// Integer mul and set flags
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();

	sljit_w buf[16];
	union {
		double value;
		struct {
			int value1;
			int value2;
		};
	} dbuf[4];

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

	dbuf[0].value1 = 0x7fffffff;
	dbuf[0].value2 = 0x7fffffff;
	dbuf[1].value1 = 0x7fffffff;
	dbuf[1].value2 = 0x7fffffff;
	dbuf[2].value = -13.0;
	dbuf[3].value = 27.0;

	SLJIT_ASSERT(sizeof(double) == 8 && sizeof(int) == 4 && sizeof(dbuf[0]) == 8);

	T(sljit_emit_enter(compiler, 2, 1, 2, 0));

	T(sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_FLOAT_REG1, 0, SLJIT_MEM1(SLJIT_GENERAL_REG2), 0));
	T(sljit_emit_fop1(compiler, SLJIT_FCMP | SLJIT_SET_E, SLJIT_MEM1(SLJIT_GENERAL_REG2), 3 * sizeof(double), SLJIT_FLOAT_REG1, 0));
	T(sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_FLOAT_REG2, 0, SLJIT_MEM1(SLJIT_GENERAL_REG2), 2 * sizeof(double)));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_C_FLOAT_NAN));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w), SLJIT_C_FLOAT_NOT_NAN));

	T(sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_FLOAT_REG3, 0, SLJIT_MEM1(SLJIT_GENERAL_REG2), 3 * sizeof(double)));
	T(sljit_emit_fop1(compiler, SLJIT_FCMP | SLJIT_SET_E | SLJIT_SET_S, SLJIT_FLOAT_REG2, 0, SLJIT_FLOAT_REG3, 0));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), 2 * sizeof(sljit_w), SLJIT_C_FLOAT_NAN));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), 3 * sizeof(sljit_w), SLJIT_C_FLOAT_NOT_NAN));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), 4 * sizeof(sljit_w), SLJIT_C_FLOAT_LESS));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), 5 * sizeof(sljit_w), SLJIT_C_FLOAT_NOT_LESS));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), 6 * sizeof(sljit_w), SLJIT_C_FLOAT_GREATER));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), 7 * sizeof(sljit_w), SLJIT_C_FLOAT_NOT_GREATER));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), 8 * sizeof(sljit_w), SLJIT_C_FLOAT_EQUAL));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), 9 * sizeof(sljit_w), SLJIT_C_FLOAT_NOT_EQUAL));

	T(sljit_emit_fop1(compiler, SLJIT_FCMP | SLJIT_SET_E, SLJIT_FLOAT_REG3, 0, SLJIT_MEM1(SLJIT_GENERAL_REG2), 3 * sizeof(double)));
	T(sljit_emit_fop2(compiler, SLJIT_FADD, SLJIT_FLOAT_REG4, 0, SLJIT_FLOAT_REG2, 0, SLJIT_MEM1(SLJIT_GENERAL_REG2), sizeof(double)));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), 10 * sizeof(sljit_w), SLJIT_C_FLOAT_NAN));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), 11 * sizeof(sljit_w), SLJIT_C_FLOAT_EQUAL));

	T(sljit_emit_fop1(compiler, SLJIT_FCMP | SLJIT_SET_S, SLJIT_MEM1(SLJIT_GENERAL_REG2), sizeof(double), SLJIT_FLOAT_REG1, 0));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), 12 * sizeof(sljit_w), SLJIT_C_FLOAT_NOT_NAN));

	T(sljit_emit_fop1(compiler, SLJIT_FCMP | SLJIT_SET_S, SLJIT_FLOAT_REG4, 0, SLJIT_FLOAT_REG3, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV_UB, SLJIT_TEMPORARY_REG1, 0, SLJIT_MEM1(SLJIT_GENERAL_REG2), 0));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), 13 * sizeof(sljit_w), SLJIT_C_FLOAT_NAN));

	T(sljit_emit_return(compiler, SLJIT_UNUSED, 0));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	code.func2((sljit_w)&buf, (sljit_w)&dbuf);

	FAILED(buf[0] != 1, "test32 case 1 failed\n");
	FAILED(buf[1] != 0, "test32 case 2 failed\n");
	FAILED(buf[2] != 0, "test32 case 3 failed\n");
	FAILED(buf[3] != 1, "test32 case 4 failed\n");
	FAILED(buf[4] != 1, "test32 case 5 failed\n");
	FAILED(buf[5] != 0, "test32 case 6 failed\n");
	FAILED(buf[6] != 0, "test32 case 7 failed\n");
	FAILED(buf[7] != 1, "test32 case 8 failed\n");
	FAILED(buf[8] != 0, "test32 case 9 failed\n");
	FAILED(buf[9] != 1, "test32 case 10 failed\n");
	FAILED(buf[10] != 0, "test32 case 11 failed\n");
	FAILED(buf[11] != 1, "test32 case 12 failed\n");
	FAILED(buf[12] != 0, "test32 case 13 failed\n");
	FAILED(buf[13] != 1, "test32 case 14 failed\n");

	sljit_free_code(code.code);
	printf("test32 ok\n");
	successful_tests++;
}

void sljit_test(void)
{
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
	if (successful_tests == 32)
		printf("All tests are passed.\n");
	else
		printf("Successful test ratio: %d%%.\n", successful_tests * 100 / 32);
}

