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

union executable_code {
	void* code;
	SLJIT_CDECL sljit_w (*func0)(void);
	SLJIT_CDECL sljit_w (*func1)(sljit_w a);
	SLJIT_CDECL sljit_w (*func2)(sljit_w a, sljit_w b);
	SLJIT_CDECL sljit_w (*func3)(sljit_w a, sljit_w b, sljit_w c);
};

#define FAILED(cond, text) \
	if (cond) { \
		printf(text); \
		return; \
	}

#define T(value) \
	if ((value) != SLJIT_NO_ERROR) { \
		printf("Compiler error: %d\n", compiler->error); \
		sljit_free_compiler(compiler); \
		return; \
	}

static void test1(void)
{
	// Enter and return from an sljit function
	union executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();

	FAILED(!compiler, "cannot create compiler\n");

	// 3 arguments passed, 3 arguments used
	T(sljit_emit_enter(compiler, CALL_TYPE_CDECL, 3, 3));
	T(sljit_emit_return(compiler, SLJIT_GENERAL_REG2));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	FAILED(code.func3(3, -21, 86) != -21, "test1 case 1 failed\n");
	FAILED(code.func3(4789, 47890, 997) != 47890, "test1 case 1 failed\n");
	sljit_free_code(code.code);
	printf("test1 ok\n");
}

static void test2(void)
{
	// Test mov
	union executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_w buf[4];

	FAILED(!compiler, "cannot create compiler\n");

	buf[0] = 5678;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	T(sljit_emit_enter(compiler, CALL_TYPE_CDECL, 1, 2));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_NO_REG, 0, SLJIT_MEM0(), (sljit_w)&buf));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 9999));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_GENERAL_REG2, 0, SLJIT_GENERAL_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG2), sizeof(sljit_w), SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, sizeof(sljit_w)));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG3, 0, SLJIT_MEM2(SLJIT_GENERAL_REG2, SLJIT_TEMPORARY_REG2), 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM2(SLJIT_GENERAL_REG2, SLJIT_TEMPORARY_REG2), sizeof(sljit_w), SLJIT_MEM2(SLJIT_GENERAL_REG2, SLJIT_TEMPORARY_REG2), 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM2(SLJIT_GENERAL_REG2, SLJIT_TEMPORARY_REG2), sizeof(sljit_w) * 2, SLJIT_MEM0(), (sljit_w)&buf));
	T(sljit_emit_return(compiler, SLJIT_TEMPORARY_REG3));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_w)&buf) != 9999, "test2 case 1 failed\n");
	FAILED(buf[1] != 9999, "test2 case 2 failed\n");
	FAILED(buf[2] != 9999, "test2 case 3 failed\n");
	FAILED(buf[3] != 5678, "test2 case 4 failed\n");
	sljit_free_code(code.code);
	printf("test2 ok\n");
}

static void test3(void)
{
	// Test not
	union executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_w buf[4];

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 1234;
	buf[1] = 0;
	buf[2] = 9876;
	buf[3] = 0;

	T(sljit_emit_enter(compiler, CALL_TYPE_CDECL, 1, 2));
	T(sljit_emit_op1(compiler, SLJIT_NOT, SLJIT_NO_REG, 0, SLJIT_MEM0(), (sljit_w)&buf));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w), SLJIT_MEM1(SLJIT_GENERAL_REG1), 0));
	T(sljit_emit_op1(compiler, SLJIT_NOT, SLJIT_MEM0(), (sljit_w)&buf[1], SLJIT_MEM0(), (sljit_w)&buf[1]));
	T(sljit_emit_op1(compiler, SLJIT_NOT, SLJIT_PREF_RET_REG, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0));
	T(sljit_emit_op1(compiler, SLJIT_NOT, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 3, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 2));
	T(sljit_emit_return(compiler, SLJIT_PREF_RET_REG));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_w)&buf) != ~1234, "test3 case 1 failed\n");
	FAILED(buf[1] != ~1234, "test3 case 2 failed\n");
	FAILED(buf[3] != ~9876, "test3 case 3 failed\n");

	sljit_free_code(code.code);
	printf("test3 ok\n");
}

static void test4(void)
{
	// Test not
	union executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_w buf[4];

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	buf[1] = 1234;
	buf[2] = 0;
	buf[3] = 0;

	T(sljit_emit_enter(compiler, CALL_TYPE_CDECL, 2, 2));
	T(sljit_emit_op1(compiler, SLJIT_NEG, SLJIT_NO_REG, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0));
	T(sljit_emit_op1(compiler, SLJIT_NEG, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 2, SLJIT_GENERAL_REG2, 0));
	T(sljit_emit_op1(compiler, SLJIT_NEG, SLJIT_MEM0(), (sljit_w)&buf[0], SLJIT_MEM0(), (sljit_w)&buf[1]));
	T(sljit_emit_op1(compiler, SLJIT_NEG, SLJIT_PREF_RET_REG, 0, SLJIT_GENERAL_REG2, 0));
	T(sljit_emit_op1(compiler, SLJIT_NEG, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 3, SLJIT_IMM, 299));
	T(sljit_emit_return(compiler, SLJIT_PREF_RET_REG));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	FAILED(code.func2((sljit_w)&buf, 4567) != -4567, "test4 case 1 failed\n");
	FAILED(buf[0] != -1234, "test4 case 2 failed\n");
	FAILED(buf[2] != -4567, "test4 case 3 failed\n");
	FAILED(buf[3] != -299, "test4 case 4 failed\n");

	sljit_free_code(code.code);
	printf("test4 ok\n");
}

static void test5(void)
{
	// Test add
	union executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_w buf[6];

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 100;
	buf[1] = 200;
	buf[2] = 300;
	buf[3] = 0;
	buf[4] = 0;
	buf[5] = 0;

	T(sljit_emit_enter(compiler, CALL_TYPE_CDECL, 1, 2));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_NO_REG, 0, SLJIT_IMM, 16, SLJIT_IMM, 16));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_NO_REG, 0, SLJIT_IMM, 255, SLJIT_IMM, 255));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_NO_REG, 0, SLJIT_GENERAL_REG1, 0, SLJIT_GENERAL_REG1, 0));
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
	T(sljit_emit_return(compiler, SLJIT_TEMPORARY_REG1));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_w)&buf) != 2114, "test5 case 1 failed\n");
	FAILED(buf[0] != 210, "test5 case 2 failed\n");
	FAILED(buf[2] != 500, "test5 case 3 failed\n");
	FAILED(buf[3] != 400, "test5 case 4 failed\n");
	FAILED(buf[4] != 200, "test5 case 5 failed\n");
	FAILED(buf[5] != 250, "test5 case 6 failed\n");

	sljit_free_code(code.code);
	printf("test5 ok\n");
}

static void test6(void)
{
	// Test addc, sub, subc
	union executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_w buf[6];

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 0;
	buf[5] = 0;

	T(sljit_emit_enter(compiler, CALL_TYPE_CDECL, 1, 1));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, -1));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, -1));
	T(sljit_emit_op2(compiler, SLJIT_ADDC, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_IMM, 0, SLJIT_IMM, 0));
	T(sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op2(compiler, SLJIT_ADDC, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w), SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w), SLJIT_IMM, 4));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 100));
	T(sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 2, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 50));
	T(sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 6000));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 3, SLJIT_IMM, 10));
	T(sljit_emit_op2(compiler, SLJIT_SUBC, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 3, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 3, SLJIT_IMM, 5));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 100));
	T(sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 2));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 4, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 5000));
	T(sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_TEMPORARY_REG2, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 4, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op2(compiler, SLJIT_ADDC, SLJIT_TEMPORARY_REG2, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG2, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 5, SLJIT_TEMPORARY_REG2, 0));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_PREF_RET_REG, 0, SLJIT_IMM, 10));
	T(sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_PREF_RET_REG, 0, SLJIT_PREF_RET_REG, 0, SLJIT_IMM, 5));
	T(sljit_emit_op2(compiler, SLJIT_SUBC, SLJIT_PREF_RET_REG, 0, SLJIT_PREF_RET_REG, 0, SLJIT_IMM, 2));
	T(sljit_emit_op2(compiler, SLJIT_ADDC, SLJIT_PREF_RET_REG, 0, SLJIT_PREF_RET_REG, 0, SLJIT_IMM, 6));
	T(sljit_emit_return(compiler, SLJIT_PREF_RET_REG));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	FAILED(code.func1((sljit_w)&buf) != 9, "test6 case 1 failed\n");
	FAILED(buf[0] != 1, "test6 case 2 failed\n");
	FAILED(buf[1] != 5, "test6 case 3 failed\n");
	FAILED(buf[2] != 50, "test6 case 4 failed\n");
	FAILED(buf[3] != 4, "test6 case 5 failed\n");
	FAILED(buf[4] != 50, "test6 case 6 failed\n");
	FAILED(buf[5] != 51, "test6 case 7 failed\n");

	sljit_free_code(code.code);
	printf("test6 ok\n");
}

static void test7(void)
{
	// Test addc, sub, subc
	union executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_w buf[6];

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0xff80;
	buf[1] = 0x0f808080;
	buf[2] = 0;
	buf[3] = 0xaaaaaa;
	buf[4] = 0;
	buf[5] = 0x4040;

	T(sljit_emit_enter(compiler, CALL_TYPE_CDECL, 1, 1));
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
	T(sljit_emit_return(compiler, SLJIT_PREF_RET_REG));

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
}

static void test8(void)
{
	// Test flags (neg, cmp, test)
	union executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_w buf[7];

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 100;
	buf[1] = 3;
	buf[2] = 3;
	buf[3] = 3;
	buf[4] = 3;
	buf[5] = 3;
	buf[6] = 3;

	T(sljit_emit_enter(compiler, CALL_TYPE_CDECL, 1, 2));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 20));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, 10));
	T(sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_NO_REG, 0, SLJIT_IMM, 6, SLJIT_IMM, 5));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w), SLJIT_C_NOT_EQUAL));
	T(sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_NO_REG, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 3000));
	T(sljit_emit_cond_set(compiler, SLJIT_GENERAL_REG2, 0, SLJIT_C_GREATER));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 2, SLJIT_GENERAL_REG2, 0));
	T(sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_NO_REG, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, -15));
	T(sljit_emit_cond_set(compiler, SLJIT_TEMPORARY_REG3, 0, SLJIT_C_SIG_GREATER));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 3, SLJIT_TEMPORARY_REG3, 0));
	T(sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_NO_REG, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG2, 0));
	T(sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_NO_REG, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op1(compiler, SLJIT_NEG, SLJIT_NO_REG, 0, SLJIT_IMM, 1 << ((sizeof(sljit_w) << 3) - 1)));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 4, SLJIT_C_OVERFLOW));
	T(sljit_emit_op2(compiler, SLJIT_AND, SLJIT_NO_REG, 0, SLJIT_IMM, 0xffff, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op2(compiler, SLJIT_AND, SLJIT_NO_REG, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0xffff));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 5, SLJIT_C_NOT_ZERO));
	T(sljit_emit_op2(compiler, SLJIT_AND, SLJIT_NO_REG, 0, SLJIT_IMM, 0xffff, SLJIT_TEMPORARY_REG2, 0));
	T(sljit_emit_op2(compiler, SLJIT_AND, SLJIT_NO_REG, 0, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, 0xffff));
	T(sljit_emit_op2(compiler, SLJIT_AND, SLJIT_NO_REG, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_TEMPORARY_REG2, 0));
	T(sljit_emit_op2(compiler, SLJIT_AND, SLJIT_NO_REG, 0, SLJIT_TEMPORARY_REG2, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0));
	T(sljit_emit_op2(compiler, SLJIT_AND, SLJIT_NO_REG, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0));
	T(sljit_emit_op2(compiler, SLJIT_AND, SLJIT_NO_REG, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_IMM, 0x1));
	T(sljit_emit_cond_set(compiler, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w) * 6, SLJIT_C_NOT_ZERO));
	T(sljit_emit_return(compiler, SLJIT_NO_REG));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	code.func1((sljit_w)&buf);
	FAILED(buf[1] != 1, "test8 case 1 failed\n");
	FAILED(buf[2] != 0, "test8 case 2 failed\n");
	FAILED(buf[3] != 1, "test8 case 3 failed\n");
	FAILED(buf[4] != 1, "test8 case 4 failed\n");
	FAILED(buf[5] != 1, "test8 case 5 failed\n");
	FAILED(buf[6] != 0, "test8 case 6 failed\n");

	sljit_free_code(code.code);
	printf("test8 ok\n");
}

static void test9(void)
{
	// Test shift
	union executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_w buf[6];

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 1 << 10;
	buf[5] = 0;

	T(sljit_emit_enter(compiler, CALL_TYPE_CDECL, 1, 2));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0xf));
	T(sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 3));
	T(sljit_emit_op2(compiler, SLJIT_LSHR, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 1));
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_op2(compiler, SLJIT_ASHR, SLJIT_NO_REG, 0, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 2));
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
	T(sljit_emit_return(compiler, SLJIT_NO_REG));

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

	sljit_free_code(code.code);
	printf("test9 ok\n");
}

static void test10(void)
{
	// Test rewriteable constants and jumps
	union executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	struct sljit_const* const1;
	struct sljit_label *label1;
	struct sljit_label *label2;
	struct sljit_label *label3;
	struct sljit_jump *jump1;
	struct sljit_jump *jump2;
	struct sljit_jump *jump3;
	sljit_uw const1_addr;
	sljit_uw jump1_addr;
	sljit_uw label1_addr;
	sljit_uw label2_addr;
	sljit_w buf[1];

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;

	T(sljit_emit_enter(compiler, CALL_TYPE_CDECL, 2, 2));
	T(sljit_emit_op2(compiler, SLJIT_SUB, SLJIT_NO_REG, 0, SLJIT_GENERAL_REG2, 0, SLJIT_IMM, 10));
	jump1 = sljit_emit_jump(compiler, SLJIT_LONG_JUMP | SLJIT_C_SIG_GREATER);
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
	const1 = sljit_emit_const(compiler, SLJIT_PREF_RET_REG, 0, 1234);
	T(compiler->error);
	T(sljit_emit_return(compiler, SLJIT_PREF_RET_REG));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	jump1_addr = sljit_get_jump_addr(jump1);
	label1_addr = sljit_get_label_addr(label1);
	label2_addr = sljit_get_label_addr(label2);
	const1_addr = sljit_get_const_addr(const1);
	sljit_free_compiler(compiler);

	FAILED(code.func2((sljit_w)&buf, 4) != 1234, "test10 case 1 failed\n")
	FAILED(buf[0] != 5, "test10 case 2 failed\n");

	sljit_set_const(const1_addr, 2345);
	FAILED(code.func2((sljit_w)&buf, 11) != 2345, "test10 case 3 failed\n")
	FAILED(buf[0] != 6, "test10 case 4 failed\n");

	sljit_set_const(const1_addr, 4567);
	sljit_set_jump_addr(jump1_addr, label2_addr);
	FAILED(code.func2((sljit_w)&buf, 12) != 4567, "test10 case 5 failed\n")
	FAILED(buf[0] != 7, "test10 case 6 failed\n");

	sljit_set_const(const1_addr, 6789);
	sljit_set_jump_addr(jump1_addr, label1_addr);
	FAILED(code.func2((sljit_w)&buf, 13) != 6789, "test10 case 7 failed\n")
	FAILED(buf[0] != 6, "test10 case 8 failed\n");

	sljit_free_code(code.code);
	printf("test10 ok\n");
}

static void test11(void)
{
	// Test arm constant pool
	union executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	int i;
	sljit_w buf[5];

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 0;

	T(sljit_emit_enter(compiler, CALL_TYPE_CDECL, 1, 1));
	for (i = 0; i <= 0xfff; i++) {
		T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0x81818000 | i));
		T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0x81818000 | i));
		if ((i & 0x3ff) == 0)
			T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), (i >> 10) * sizeof(sljit_w), SLJIT_TEMPORARY_REG1, 0));
	}
	T(sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 4 * sizeof(sljit_w), SLJIT_TEMPORARY_REG1, 0));
	T(sljit_emit_return(compiler, SLJIT_NO_REG));

	code.code = sljit_generate_code(compiler);
	FAILED(!code.code, "code generation error\n");
	sljit_free_compiler(compiler);

	code.func1((sljit_w)&buf);
	FAILED(buf[0] != 0x81818000, "test11 case 1 failed\n");
	FAILED(buf[1] != 0x81818400, "test11 case 2 failed\n");
	FAILED(buf[2] != 0x81818800, "test11 case 3 failed\n");
	FAILED(buf[3] != 0x81818c00, "test11 case 4 failed\n");
	FAILED(buf[4] != 0x81818fff, "test11 case 5 failed\n");

	sljit_free_code(code.code);
	printf("test11 ok\n");
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
}

