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
	double buf[7];

	if (!compiler)
		error("Not enough of memory");
	buf[0] = 10.1;
	buf[1] = 22.1;
	buf[2] = 0.0;
	buf[3] = 0.0;
	buf[4] = 0.0;
	buf[5] = 0.0;
	buf[6] = 0.0;

#ifdef SLJIT_VERBOSE
	sljit_compiler_verbose(compiler, stdout);
#endif
	sljit_emit_enter(compiler, 1, 3);

	sljit_emit_fop2(compiler, SLJIT_FADD, SLJIT_MEM0(), (sljit_uw)&buf[2], SLJIT_MEM0(), (sljit_uw)&buf[0], SLJIT_MEM0(), (sljit_uw)&buf[1]);
	sljit_emit_fop2(compiler, SLJIT_FSUB, SLJIT_FLOAT_REG2, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(double));
	sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(double) * 3, SLJIT_FLOAT_REG2, 0);
	sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_FLOAT_REG1, 0, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(double) * 2);
	sljit_emit_fop2(compiler, SLJIT_FADD, SLJIT_FLOAT_REG1, 0, SLJIT_FLOAT_REG1, 0, SLJIT_FLOAT_REG2, 0);
	sljit_emit_fop1(compiler, SLJIT_FMOV, SLJIT_MEM1(SLJIT_GENERAL_REG1), sizeof(double) * 4, SLJIT_FLOAT_REG1, 0);

	sljit_emit_return(compiler, SLJIT_PREF_RET_REG);
	code.code = sljit_generate_code(compiler);
	sljit_free_compiler(compiler);

	printf("Code at: %p\n", code.code); devel_dummy();

	printf("Function returned with %ld\n", (long)code.func((sljit_uw*)buf));
	printf("buf[0] = %lf\n", buf[0]);
	printf("buf[1] = %lf\n", buf[1]);
	printf("buf[2] = %lf\n", buf[2]);
	printf("buf[3] = %lf\n", buf[3]);
	printf("buf[4] = %lf\n", buf[4]);
	printf("buf[5] = %lf\n", buf[5]);
	printf("buf[6] = %lf\n", buf[6]);
	sljit_free_code(code.code);
}

int main(int argc, char* argv[])
{
	//devel();
	sljit_test();

	return 0;
}
