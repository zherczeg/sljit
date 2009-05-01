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
