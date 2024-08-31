#include "sljitLir.h"

#include <stdio.h>
#include <stdlib.h>

typedef sljit_sw (SLJIT_FUNC *func_arr_t)(sljit_sw *arr, sljit_sw narr);

static void SLJIT_FUNC print_num(sljit_sw a)
{
	printf("num = %ld\n", (long)a);
}

/*
  This example, we generate a function like this:

sljit_sw func(sljit_sw *array, sljit_sw narray)
{
	sljit_sw i;
	for (i = 0; i < narray; ++i)
		print_num(array[i]);
	return narray;
}

*/

static int array_access(sljit_sw *arr, sljit_sw narr)
{
	void *code;
	sljit_uw len;
	func_arr_t func;
	struct sljit_label *loopstart;
	struct sljit_jump *out;

	/* Create a SLJIT compiler */
	struct sljit_compiler *C = sljit_create_compiler(NULL);

	sljit_emit_enter(C, 0, SLJIT_ARGS2(W, P, W), 1, 3, 0);
	/*                  opt arg                  R  S  local_size */

	/* S2 = 0 */
	sljit_emit_op2(C, SLJIT_XOR, SLJIT_S2, 0, SLJIT_S2, 0, SLJIT_S2, 0);

	/* S1 = narr */
	sljit_emit_op1(C, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, narr);

	/* loopstart:              */
	loopstart = sljit_emit_label(C);

	/* S2 >= narr --> jumo out */
	out = sljit_emit_cmp(C, SLJIT_GREATER_EQUAL, SLJIT_S2, 0, SLJIT_S1, 0);

	/* R0 = (sljit_sw *)S0[S2];    */
	sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM2(SLJIT_S0, SLJIT_S2), SLJIT_WORD_SHIFT);

	/* print_num(R0)           */
	sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS1V(W), SLJIT_IMM, SLJIT_FUNC_ADDR(print_num));

	/* S2 += 1                 */
	sljit_emit_op2(C, SLJIT_ADD, SLJIT_S2, 0, SLJIT_S2, 0, SLJIT_IMM, 1);

	/* jump loopstart          */
	sljit_set_label(sljit_emit_jump(C, SLJIT_JUMP), loopstart);

	/* out:                    */
	sljit_set_label(out, sljit_emit_label(C));

	/* return S1               */
	sljit_emit_return(C, SLJIT_MOV, SLJIT_S1, 0);

	/* Generate machine code */
	code = sljit_generate_code(C, 0, NULL);
	len = sljit_get_generated_code_size(C);

	/* Execute code */
	func = (func_arr_t)code;
	printf("func return %ld\n", (long)func(arr, narr));

	/* dump_code(code, len); */

	/* Clean up */
	sljit_free_compiler(C);
	sljit_free_code(code, NULL);
	return 0;
}

int main(void)
{
	sljit_sw arr[8] = { 3, -10, 4, 6, 8, 12, 2000, 0 };
	return array_access(arr, 8);
}
