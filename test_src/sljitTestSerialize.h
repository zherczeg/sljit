/*
 *    Stack-less Just-In-Time compiler
 *
 *    Copyright Zoltan Herczeg (hzmester@freemail.hu). All rights reserved.
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

static void test_serialize1(void)
{
	/* Test serializing interface. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler(NULL, NULL);
	struct sljit_label *label;
	struct sljit_jump *jump1;
	struct sljit_jump *jump2;
	struct sljit_put_label *put_label;
	sljit_sw executable_offset;
	sljit_uw const_addr;
	sljit_uw jump_addr;
	sljit_uw label_addr;
	sljit_sw buf[3];
	sljit_uw* serialized_buffer;
	sljit_uw serialized_size;
	sljit_s32 i;

	if (verbose)
		printf("Run test_serialize1\n");

	FAILED(!compiler, "cannot create compiler\n");
	buf[0] = 0;
	buf[1] = 0;

	sljit_emit_enter(compiler, 0, SLJIT_ARGS1V(P), 3, 2, 0, 0, 0);

	jump1 = sljit_emit_jump(compiler, SLJIT_JUMP);
	label = sljit_emit_label(compiler);
	jump2 = sljit_emit_jump(compiler, SLJIT_JUMP);
	sljit_set_label(jump2, label);
	label = sljit_emit_label(compiler);
	sljit_set_label(jump1, label);

	put_label = sljit_emit_put_label(compiler, SLJIT_R2, 0);
	/* buf[0] */
	sljit_emit_const(compiler, SLJIT_MEM1(SLJIT_S0), 0, -1234);

	sljit_emit_ijump(compiler, SLJIT_JUMP, SLJIT_R2, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 0, SLJIT_IMM, -1234);

	label = sljit_emit_label(compiler);
	sljit_set_put_label(put_label, label);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 7);
	for (i = 0; i < 16000; i++)
		sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 3);

	/* buf[1] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_R1, 0);

	/* buf[2] */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_IMM, -56789);
	jump1 = sljit_emit_jump(compiler, SLJIT_JUMP | SLJIT_REWRITABLE_JUMP);
	label = sljit_emit_label(compiler);
	sljit_set_label(jump1, label);

	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_sw), SLJIT_IMM, 0);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), 2 * sizeof(sljit_sw), SLJIT_IMM, 0);
	label = sljit_emit_label(compiler);

	serialized_buffer = sljit_serialize_compiler(compiler, &serialized_size);
	FAILED(!serialized_buffer, "cannot serialize compiler\n");
	sljit_free_compiler(compiler);

	/* Continue code generation. */
	compiler = sljit_deserialize_compiler(serialized_buffer, serialized_size, NULL, NULL);
	SLJIT_FREE(serialized_buffer, NULL);

	jump1 = sljit_emit_jump(compiler, SLJIT_JUMP);
	label = sljit_emit_label(compiler);
	jump2 = sljit_emit_jump(compiler, SLJIT_JUMP);
	sljit_set_label(jump2, label);
	label = sljit_emit_label(compiler);
	sljit_set_label(jump1, label);

	sljit_emit_return_void(compiler);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	executable_offset = sljit_get_executable_offset(compiler);
	const_addr = sljit_get_const_addr(sljit_get_first_const(compiler));
	jump1 = sljit_get_next_jump(sljit_get_next_jump(sljit_get_first_jump(compiler)));
	jump_addr = sljit_get_jump_addr(jump1);
	label = sljit_get_next_label(sljit_get_next_label(sljit_get_next_label(sljit_get_next_label(sljit_get_first_label(compiler)))));
	label_addr = sljit_get_label_addr(label);
	sljit_free_compiler(compiler);

	sljit_set_const(const_addr, 87654, executable_offset);
	sljit_set_jump_addr(jump_addr, label_addr, executable_offset);

	code.func1((sljit_sw)&buf);
	FAILED(buf[0] != 87654, "test_serialize1 case 1 failed\n");
	FAILED(buf[1] != 7 + 16000 * 3, "test_serialize1 case 2 failed\n");
	FAILED(buf[2] != -56789, "test_serialize1 case 3 failed\n");

	sljit_free_code(code.code, NULL);
	successful_tests++;
}
