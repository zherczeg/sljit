#include "sljitLir.h"

#include <stdio.h>

void sljit_test(void);

void error(char* str)
{
	printf("An error occured: %s\n", str);
	exit(-1);
}

#ifndef SLJIT_INDIRECT_CALL
union executable_code {
	void* code;
	SLJIT_CALL sljit_w (*func)(sljit_w* a);
};
typedef union executable_code executable_code;
#else
struct executable_code {
	void* code;
	union {
		SLJIT_CALL sljit_w (*func)(sljit_w* a);
		void** code_ptr;
	};
};
typedef struct executable_code executable_code;
#endif

/*
	TEST_FAIL(load_immediate(compiler, SLJIT_TEMPORARY_REG1, 0));
	TEST_FAIL(load_immediate(compiler, SLJIT_TEMPORARY_REG1, 0x7fff));
	TEST_FAIL(load_immediate(compiler, SLJIT_TEMPORARY_REG1, -0x8000));
	TEST_FAIL(load_immediate(compiler, SLJIT_TEMPORARY_REG1, 0x7fffffff));
	TEST_FAIL(load_immediate(compiler, SLJIT_TEMPORARY_REG1, -0x80000000l));
	TEST_FAIL(load_immediate(compiler, SLJIT_TEMPORARY_REG1, 0x1234567887654321));

	TEST_FAIL(load_immediate(compiler, SLJIT_TEMPORARY_REG1, 0xff80000000));
	TEST_FAIL(load_immediate(compiler, SLJIT_TEMPORARY_REG1, 0x3ff0000000));
	TEST_FAIL(load_immediate(compiler, SLJIT_TEMPORARY_REG1, 0xfffffff800100000));
	TEST_FAIL(load_immediate(compiler, SLJIT_TEMPORARY_REG1, 0xfffffff80010f000));

	TEST_FAIL(load_immediate(compiler, SLJIT_TEMPORARY_REG1, 0x07fff00000008001));
	TEST_FAIL(load_immediate(compiler, SLJIT_TEMPORARY_REG1, 0x07fff00080010000));
	TEST_FAIL(load_immediate(compiler, SLJIT_TEMPORARY_REG1, 0x07fff00080018001));

	TEST_FAIL(load_immediate(compiler, SLJIT_TEMPORARY_REG1, 0x07fff00ffff00000));
*/

int devel_dummy(void) { return rand(); }

void devel(void)
{
	executable_code code;

	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_w buf[4];

	if (!compiler)
		error("Not enough of memory");
	buf[0] = 5;
	buf[1] = 12;
	buf[2] = 0;
	buf[3] = 0;

#ifdef SLJIT_VERBOSE
	sljit_compiler_verbose(compiler, stdout);
#endif
	sljit_emit_enter(compiler, 1, 3, 4 * sizeof(sljit_w));

/*	sljit_emit_op1(compiler, SLJIT_MOV_SB, SLJIT_GENERAL_REG1, 0, SLJIT_MEM1(SLJIT_TEMPORARY_REG1), 17);
	sljit_emit_op1(compiler, SLJIT_MOV_UB, SLJIT_GENERAL_REG1, 0, SLJIT_MEM1(SLJIT_TEMPORARY_REG1), 17);
	sljit_emit_op1(compiler, SLJIT_MOVU_UI, SLJIT_MEM0(), 100, SLJIT_MEM0(), 100);

	sljit_emit_op1(compiler, SLJIT_MOV_SB, SLJIT_MEM1(SLJIT_GENERAL_REG1), 20, SLJIT_MEM1(SLJIT_TEMPORARY_REG1), 32);
	sljit_emit_op1(compiler, SLJIT_MOV_UB, SLJIT_MEM1(SLJIT_GENERAL_REG1), 20, SLJIT_MEM1(SLJIT_TEMPORARY_REG1), 32);
	sljit_emit_op1(compiler, SLJIT_MOV_SH, SLJIT_MEM1(SLJIT_GENERAL_REG1), 20, SLJIT_MEM1(SLJIT_TEMPORARY_REG1), 32);
	sljit_emit_op1(compiler, SLJIT_MOV_UH, SLJIT_MEM1(SLJIT_GENERAL_REG1), 20, SLJIT_MEM1(SLJIT_TEMPORARY_REG1), 32);
	sljit_emit_op1(compiler, SLJIT_MOV_SI, SLJIT_MEM1(SLJIT_GENERAL_REG1), 20, SLJIT_MEM1(SLJIT_TEMPORARY_REG1), 32);
	sljit_emit_op1(compiler, SLJIT_MOV_UI, SLJIT_MEM1(SLJIT_GENERAL_REG1), 20, SLJIT_MEM1(SLJIT_TEMPORARY_REG1), 32);

	sljit_emit_op1(compiler, SLJIT_MOV_SB, SLJIT_MEM2(SLJIT_GENERAL_REG1, SLJIT_GENERAL_REG2), 0, SLJIT_MEM2(SLJIT_TEMPORARY_REG1, SLJIT_TEMPORARY_REG2), 0);
	sljit_emit_op1(compiler, SLJIT_MOV_UB, SLJIT_MEM2(SLJIT_GENERAL_REG1, SLJIT_GENERAL_REG2), 0, SLJIT_MEM2(SLJIT_TEMPORARY_REG1, SLJIT_TEMPORARY_REG2), 0);
	sljit_emit_op1(compiler, SLJIT_MOV_SH, SLJIT_MEM2(SLJIT_GENERAL_REG1, SLJIT_GENERAL_REG2), 0, SLJIT_MEM2(SLJIT_TEMPORARY_REG1, SLJIT_TEMPORARY_REG2), 0);
	sljit_emit_op1(compiler, SLJIT_MOV_UH, SLJIT_MEM2(SLJIT_GENERAL_REG1, SLJIT_GENERAL_REG2), 0, SLJIT_MEM2(SLJIT_TEMPORARY_REG1, SLJIT_TEMPORARY_REG2), 0);
	sljit_emit_op1(compiler, SLJIT_MOV_SI, SLJIT_MEM2(SLJIT_GENERAL_REG1, SLJIT_GENERAL_REG2), 0, SLJIT_MEM2(SLJIT_TEMPORARY_REG1, SLJIT_TEMPORARY_REG2), 0);
	sljit_emit_op1(compiler, SLJIT_MOV_UI, SLJIT_MEM2(SLJIT_GENERAL_REG1, SLJIT_GENERAL_REG2), 0, SLJIT_MEM2(SLJIT_TEMPORARY_REG1, SLJIT_TEMPORARY_REG2), 0);*/

	/*sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM0(), &buf[0], SLJIT_MEM0(), &buf[1], SLJIT_IMM, -16);
	sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_SET_FLAGS, SLJIT_MEM0(), &buf[0], SLJIT_MEM0(), &buf[1], SLJIT_IMM, -16);
	sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_INT_OPERATION, SLJIT_MEM0(), &buf[0], SLJIT_IMM, -16, SLJIT_MEM0(), &buf[1]);
	sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_INT_OPERATION | SLJIT_SET_FLAGS, SLJIT_MEM0(), &buf[1], SLJIT_MEM0(), &buf[0], SLJIT_IMM, -16);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_TEMPORARY_REG1, 0, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, 0xff00000);
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0xff00000, SLJIT_TEMPORARY_REG2, 0);
	sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_SET_FLAGS, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0xff00000, SLJIT_TEMPORARY_REG2, 0);
	sljit_emit_op2(compiler, SLJIT_ADD | SLJIT_INT_OPERATION | SLJIT_SET_FLAGS, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, 0xff00000, SLJIT_TEMPORARY_REG2, 0);*/

	sljit_emit_op1(compiler, SLJIT_MOVU, SLJIT_MEM2(SLJIT_GENERAL_REG1, SLJIT_TEMPORARY_REG1), 0x777778, SLJIT_GENERAL_REG1, 0);

	sljit_emit_return(compiler, SLJIT_PREF_RET_REG);
	code.code = sljit_generate_code(compiler);
#ifdef SLJIT_INDIRECT_CALL
	code.code_ptr = &code.code;
#endif
	sljit_free_compiler(compiler);

	printf("Code at: %p\n", code.code); devel_dummy();

	printf("Function returned with %ld\n", (long)code.func((sljit_w*)buf));
	printf("buf[0] = %ld\n", (long)buf[0]);
	printf("buf[1] = %ld\n", (long)buf[1]);
	printf("buf[2] = %ld\n", (long)buf[2]);
	printf("buf[3] = %ld\n", (long)buf[3]);
	sljit_free_code(code.code);
}

int main(int argc, char* argv[])
{
	//devel();
	sljit_test();

	return 0;
}
