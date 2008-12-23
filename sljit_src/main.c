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
	SLJIT_CALL sljit_w (*func)(sljit_w* a, sljit_w b, sljit_w c);
};

int devel_dummy(void) { return rand(); }

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
	sljit_emit_enter(compiler, 3, 3);

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

