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
	SLJIT_CDECL sljit_w (*func)(sljit_w a, sljit_w b, sljit_w c);
};

void development(void)
{
	union executable_code code;
	struct sljit_label *label;
	struct sljit_jump *jump;
	sljit_w ret;

	struct sljit_compiler* compiler = sljit_create_compiler();

	if (!compiler)
		error("Not enough of memory");

#ifdef SLJIT_VERBOSE
	sljit_compiler_verbose(compiler, stdout);
#endif
	sljit_emit_enter(compiler, CALL_TYPE_CDECL, 3, 3);
	jump = sljit_emit_jump(compiler, SLJIT_JUMP /*| SLJIT_LONG_JUMP*/);

	label = sljit_emit_label(compiler);
	sljit_set_label(jump, label);

	sljit_emit_return(compiler, SLJIT_GENERAL_REG3);

	code.code = sljit_generate_code(compiler);
	printf("Label at 0x%x\n", sljit_get_label_addr(label));
	sljit_free_compiler(compiler);

	ret = code.func(3, 21, 86);
	sljit_free_code(code.code);

	printf("Function returned with %d\n", ret);
}

int main(int argc, char* argv[])
{
	sljit_test();

	return 0;
}


