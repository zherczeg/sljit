
// Length of an instruction word
// Both for ppc-32 and ppc-64
typedef unsigned int sljit_i;

#define TMP_REG1	(SLJIT_STACK_PTR_REG + 1)
#define TMP_REG2	(SLJIT_STACK_PTR_REG + 2)
#define TMP_REG3	(SLJIT_STACK_PTR_REG + 3)
#define REAL_STACK_PTR	(SLJIT_STACK_PTR_REG + 4)

// SLJIT_STACK_PTR_REG is not the real stack register, since it must
// point to the head of the stack chain
static sljit_ub reg_map[SLJIT_NO_REGISTERS + 5] = {
  0, 3, 4, 5, 28, 29, 30, 31, 6, 7, 8, 1
};

static int push_inst(struct sljit_compiler *compiler, sljit_i ins)
{
	sljit_i *ptr;

	ptr = (sljit_i*)ensure_buf(compiler, sizeof(sljit_i));
	if (!ptr) {
		compiler->error = SLJIT_MEMORY_ERROR;
		return SLJIT_MEMORY_ERROR;
	}
	*ptr = ins;
	compiler->size++;
}

void* sljit_generate_code(struct sljit_compiler *compiler)
{
	struct sljit_memory_fragment *buf;
	sljit_i *code;
	sljit_i *code_ptr;
	sljit_i *buf_ptr;
	sljit_i *buf_end;

	FUNCTION_ENTRY();

	SLJIT_ASSERT(compiler->size > 0);
	reverse_buf(compiler);

	code = SLJIT_MALLOC_EXEC(compiler->size * sizeof(sljit_uw));

	buf = compiler->buf;

	code_ptr = code;
	do {
		buf_ptr = (sljit_i*)buf->memory;
		buf_end = buf_ptr + (buf->used_size >> 2);
		do {
			*code_ptr++ = *buf_ptr++;
		} while (buf_ptr < buf_end);

		buf = buf->next;
	} while (buf);

	SLJIT_ASSERT(code_ptr - code <= compiler->size);

	__clear_cache((char *)code, (char *)code_ptr);

	compiler->error = SLJIT_CODE_GENERATED;
	return code;
}

void sljit_free_code(void* code)
{
	SLJIT_FREE_EXEC(code);
}

int sljit_emit_enter(struct sljit_compiler *compiler, int args, int general, int local_size)
{
	FUNCTION_ENTRY();
	SLJIT_ASSERT(args >= 0 && args <= SLJIT_NO_GEN_REGISTERS);
	SLJIT_ASSERT(general >= 0 && general <= SLJIT_NO_GEN_REGISTERS);
	SLJIT_ASSERT(args <= general);
	SLJIT_ASSERT(local_size >= 0 && local_size <= SLJIT_MAX_LOCAL_SIZE);
	SLJIT_ASSERT(compiler->general == -1);

	sljit_emit_enter_verbose();

	compiler->general = general;

	return SLJIT_NO_ERROR;
}

void sljit_fake_enter(struct sljit_compiler *compiler, int args, int general, int local_size)
{
	SLJIT_ASSERT(args >= 0 && args <= SLJIT_NO_GEN_REGISTERS);
	SLJIT_ASSERT(general >= 0 && general <= SLJIT_NO_GEN_REGISTERS);
	SLJIT_ASSERT(args <= general);
	SLJIT_ASSERT(local_size >= 0 && local_size <= SLJIT_MAX_LOCAL_SIZE);
	SLJIT_ASSERT(compiler->general == -1);

	sljit_fake_enter_verbose();

	compiler->general = general;

	return SLJIT_NO_ERROR;
}

int sljit_emit_return(struct sljit_compiler *compiler, int reg)
{
	FUNCTION_ENTRY();
	SLJIT_ASSERT(reg >= 0 && reg <= SLJIT_NO_REGISTERS);
	SLJIT_ASSERT(compiler->general >= 0);

	sljit_emit_return_verbose();

	TEST_FAIL(push_inst(compiler, 0x4e800020));

	return SLJIT_NO_ERROR;
}

// ---------------------------------------------------------------------
//  Operators
// ---------------------------------------------------------------------

int sljit_emit_op1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
	FUNCTION_ENTRY();

	SLJIT_ASSERT((op & ~SLJIT_32BIT_OPERATION) >= SLJIT_MOV && (op & ~SLJIT_32BIT_OPERATION) <= SLJIT_NEG);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_SRC(src, srcw);
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_op1_verbose();

#ifdef SLJIT_CONFIG_PPC_64
	compiler->mode32 = op & SLJIT_32BIT_OPERATION;
#endif

	return SLJIT_NO_ERROR;
}

int sljit_emit_op2(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	FUNCTION_ENTRY();

	SLJIT_ASSERT((op & ~SLJIT_32BIT_OPERATION) >= SLJIT_ADD && (op & ~SLJIT_32BIT_OPERATION) <= SLJIT_ASHR);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_SRC(src1, src1w);
	FUNCTION_CHECK_SRC(src2, src2w);
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_op2_verbose();

#ifdef SLJIT_CONFIG_PPC_64
	compiler->mode32 = op & SLJIT_32BIT_OPERATION;
#endif

	return SLJIT_NO_ERROR;
}

// ---------------------------------------------------------------------
//  Floating point operators
// ---------------------------------------------------------------------

int sljit_is_fpu_available(void)
{
	// Always available
	return 1;
}

int sljit_emit_fop1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw)
{
	int dst_freg;

	FUNCTION_ENTRY();

	SLJIT_ASSERT(op >= SLJIT_FCMP && op <= SLJIT_FABS);
#ifdef SLJIT_DEBUG
	FUNCTION_FCHECK(src, srcw);
	FUNCTION_FCHECK(dst, dstw);
#endif
	sljit_emit_fop1_verbose();

	return SLJIT_NO_ERROR;
}

int sljit_emit_fop2(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w)
{
	int dst_freg;

	FUNCTION_ENTRY();

	SLJIT_ASSERT(op >= SLJIT_FADD && op <= SLJIT_FDIV);
#ifdef SLJIT_DEBUG
	FUNCTION_FCHECK(src1, src1w);
	FUNCTION_FCHECK(src2, src2w);
	FUNCTION_FCHECK(dst, dstw);
#endif
	sljit_emit_fop2_verbose();

	return SLJIT_NO_ERROR;
}

// ---------------------------------------------------------------------
//  Conditional instructions
// ---------------------------------------------------------------------

struct sljit_label* sljit_emit_label(struct sljit_compiler *compiler)
{
	struct sljit_label *label;

	FUNCTION_ENTRY();

	sljit_emit_label_verbose();

	label = NULL;

	return label;
}

struct sljit_jump* sljit_emit_jump(struct sljit_compiler *compiler, int type)
{
	struct sljit_jump *jump;

	FUNCTION_ENTRY();
	SLJIT_ASSERT((type & ~0x1ff) == 0);
	SLJIT_ASSERT((type & 0xff) >= SLJIT_C_EQUAL && (type & 0xff) <= SLJIT_CALL3);

	sljit_emit_jump_verbose();

	jump = NULL;

	return jump;
}

int sljit_emit_ijump(struct sljit_compiler *compiler, int type, int src, sljit_w srcw)
{
	struct sljit_jump *jump;

	FUNCTION_ENTRY();
	SLJIT_ASSERT(type >= SLJIT_JUMP && type <= SLJIT_CALL3);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_SRC(src, srcw);
#endif
	sljit_emit_ijump_verbose();

	return SLJIT_NO_ERROR;
}

int sljit_emit_cond_set(struct sljit_compiler *compiler, int dst, sljit_w dstw, int type)
{
	int reg;

	FUNCTION_ENTRY();
	SLJIT_ASSERT(type >= SLJIT_C_EQUAL && type <= SLJIT_C_NOT_OVERFLOW);
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_set_cond_verbose();

	if (dst == SLJIT_NO_REG)
		return SLJIT_NO_ERROR;

	return SLJIT_NO_ERROR;
}

struct sljit_const* sljit_emit_const(struct sljit_compiler *compiler, int dst, sljit_w dstw, sljit_w initval)
{
	struct sljit_const *const_;
	int reg;

	FUNCTION_ENTRY();
#ifdef SLJIT_DEBUG
	FUNCTION_CHECK_DST(dst, dstw);
#endif
	sljit_emit_const_verbose();

	const_ = NULL;

	return const_;
}

void sljit_set_jump_addr(sljit_uw addr, sljit_uw new_addr)
{

}

void sljit_set_const(sljit_uw addr, sljit_w constant)
{

}
