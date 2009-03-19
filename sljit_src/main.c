#include "sljitLir.h"

#include <stdio.h>

void sljit_test(void);

void error(char* str)
{
	printf("An error occured: %s\n", str);
	exit(-1);
}

union executable_code {
	void* code;
	SLJIT_CALL sljit_w (*func)(sljit_w* a);
};

int devel_dummy(void) { return rand(); }

void devel(void)
{
	union executable_code code;

	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_uw buf[4];

	if (!compiler)
		error("Not enough of memory");
	buf[0] = 5;
	buf[1] = 12;
	buf[2] = 0;
	buf[3] = 0;

#ifdef SLJIT_VERBOSE
	sljit_compiler_verbose(compiler, stdout);
#endif
	sljit_emit_enter(compiler, 1, 2, 4 * sizeof(sljit_uw));

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, sizeof(sljit_uw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM2(SLJIT_STACK_PTR_REG, SLJIT_TEMPORARY_REG1), 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM2(SLJIT_STACK_PTR_REG, SLJIT_TEMPORARY_REG1), -(int)sizeof(sljit_uw), SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_uw));
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 2 * sizeof(sljit_uw), SLJIT_MEM1(SLJIT_STACK_PTR_REG), sizeof(sljit_uw));
	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM1(SLJIT_GENERAL_REG1), 3 * sizeof(sljit_uw), SLJIT_MEM2(SLJIT_TEMPORARY_REG1, SLJIT_STACK_PTR_REG), 0, SLJIT_MEM1(SLJIT_STACK_PTR_REG), 0);

	sljit_emit_return(compiler, SLJIT_PREF_RET_REG);
	code.code = sljit_generate_code(compiler);
	sljit_free_compiler(compiler);

	printf("Code at: %p\n", code.code); devel_dummy();

	printf("Function returned with %ld\n", (long)code.func((sljit_uw*)buf));
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
