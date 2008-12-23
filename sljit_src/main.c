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
	SLJIT_CDECL sljit_w (*func)(sljit_w* a, sljit_w b, sljit_w c);
};

int devel_dummy() { return rand(); }

void devel(void)
{
	union executable_code code;

	struct sljit_compiler* compiler = sljit_create_compiler();
	struct sljit_const *c;
	sljit_w buf[6];

	if (!compiler)
		error("Not enough of memory");
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;

#ifdef SLJIT_VERBOSE
	sljit_compiler_verbose(compiler, stdout);
#endif
	sljit_emit_enter(compiler, CALL_TYPE_CDECL, 3, 3);
//	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_GENERAL_REG3, 0, SLJIT_GENERAL_REG2, 0);
//	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, 0xC0000000);
//	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG2, 0, SLJIT_IMM, 0xfff44fff);
//	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w));
//	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG2), -sizeof(sljit_w) * 2, SLJIT_MEM1(SLJIT_GENERAL_REG2), -sizeof(sljit_w));
//	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_TEMPORARY_REG1, 0, SLJIT_IMM, sizeof(sljit_w));
//	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_MEM2(SLJIT_GENERAL_REG1, SLJIT_TEMPORARY_REG1), 0);
//	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_IMM, 0xfffffff0);
//	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(sljit_w), SLJIT_IMM, 0xc002);
//	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 2 * sizeof(sljit_w), SLJIT_IMM, 0xffcfffcf);
//	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), 3 * sizeof(sljit_w), SLJIT_IMM, 0x12345678);
//	sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_MEM0(), &buf[0], SLJIT_MEM0(), &buf[0], SLJIT_MEM2(SLJIT_GENERAL_REG1, SLJIT_GENERAL_REG2), sizeof(sljit_w));

	sljit_emit_return(compiler, SLJIT_PREF_RET_REG);
	code.code = sljit_generate_code(compiler);
	sljit_free_compiler(compiler);

	printf("Code at: %x\n", (unsigned int)code.code);
	devel_dummy();

	printf("Function returned with %d\n", code.func(buf, 11, 33));
	printf("buf[0] = 0x%x\n", buf[0]);
	printf("buf[1] = 0x%x\n", buf[1]);
	printf("buf[2] = 0x%x\n", buf[2]);
	printf("buf[3] = 0x%x\n", buf[3]);
	sljit_free_code(code.code);
}

int main(int argc, char* argv[])
{
//	devel();
	sljit_test();

	return 0;
}

