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

#include <sys/auxv.h>

#ifdef __ARCH__
#define ENABLE_STATIC_FACILITY_DETECTION 1
#else
#define ENABLE_STATIC_FACILITY_DETECTION 0
#endif
#define ENABLE_DYNAMIC_FACILITY_DETECTION 1

SLJIT_API_FUNC_ATTRIBUTE const char* sljit_get_platform_name(void)
{
	return "s390x" SLJIT_CPUINFO;
}

/* Instructions. */
typedef sljit_uw sljit_ins;

/* Instruction tags (most significant halfword). */
const sljit_ins sljit_ins_const = (sljit_ins)(1) << 48;

/* General Purpose Registers [0-15]. */
typedef sljit_uw sljit_gpr;
const sljit_gpr r0 = 0; /* 0 in address calculations; reserved */
const sljit_gpr r1 = 1; /* reserved */
const sljit_gpr r2 = 2; /* 1st argument */
const sljit_gpr r3 = 3; /* 2nd argument */
const sljit_gpr r4 = 4; /* 3rd argument */
const sljit_gpr r5 = 5; /* 4th argument */
const sljit_gpr r6 = 6; /* 5th argument; 1st saved register */
const sljit_gpr r7 = 7;
const sljit_gpr r8 = 8;
const sljit_gpr r9 = 9;
const sljit_gpr r10 = 10;
const sljit_gpr r11 = 11;
const sljit_gpr r12 = 12;
const sljit_gpr r13 = 13;
const sljit_gpr r14 = 14; /* return address and flag register */
const sljit_gpr r15 = 15; /* stack pointer */

const sljit_gpr tmp0 = 0; /* r0 */
const sljit_gpr tmp1 = 1; /* r1 */

/* Link registers. The normal link register is r14, but since
   we use that for flags we need to use r0 instead to do fast
   calls so that flags are preserved. */
const sljit_gpr link_r = 14;     /* r14 */
const sljit_gpr fast_link_r = 0; /* r0 */

/* Flag register layout:

   0               32  33  34      36      64
   +---------------+---+---+-------+-------+
   |      ZERO     | 0 | 0 |  C C  |///////|
   +---------------+---+---+-------+-------+
*/
const sljit_gpr flag_r = 14; /* r14 */

static const sljit_gpr reg_map[SLJIT_NUMBER_OF_REGISTERS + 1] = {
	2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15
};

struct sljit_s390x_const {
	struct sljit_const const_; /* must be first */
	sljit_sw init_value;       /* required to build literal pool */
};

/* Convert SLJIT register to hardware register. */
static sljit_gpr gpr(sljit_s32 r)
{
	SLJIT_ASSERT(r != SLJIT_UNUSED);
	r -= SLJIT_R0; /* normalize */
	SLJIT_ASSERT(r < (sizeof(reg_map) / sizeof(reg_map[0])));
	return reg_map[r];
}

/* Size of instruction in bytes. Tags must already be cleared. */
static sljit_uw sizeof_ins(sljit_ins ins)
{
	if (ins == 0) return 2; /* keep faulting instructions */
	if ((ins&0x00000000ffff) == ins) return 2;
	if ((ins&0x0000ffffffff) == ins) return 4;
	if ((ins&0xffffffffffff) == ins) return 6;
	abort();
}

static sljit_s32 push_inst(struct sljit_compiler *compiler, sljit_ins ins)
{
	sljit_ins *ibuf = (sljit_ins *)ensure_buf(compiler, sizeof(sljit_ins));
	FAIL_IF(!ibuf);
	*ibuf = ins;
	compiler->size++;
	return SLJIT_SUCCESS;
}

static sljit_s32 encode_inst(void **ptr, sljit_ins ins)
{
	sljit_u16 *ibuf = (sljit_u16 *)*ptr;
	sljit_uw size = sizeof_ins(ins);
	SLJIT_ASSERT((size&6) == size);
	switch (size) {
	case 6: *ibuf++ = (sljit_u16)(ins >> 32);
	case 4: *ibuf++ = (sljit_u16)(ins >> 16);
	case 2: *ibuf++ = (sljit_u16)(ins);
	}
	*ptr = (void*)ibuf;
	return SLJIT_SUCCESS;
}

/* Map the given type to a 4-bit condition code mask. */
static sljit_uw get_cc(sljit_s32 type) {
	const sljit_uw eq = 1 << 3; /* equal {,to zero} */
	const sljit_uw lt = 1 << 2; /* less than {,zero} */
	const sljit_uw gt = 1 << 1; /* greater than {,zero} */
	const sljit_uw ov = 1 << 0; /* {overflow,NaN} */
	const sljit_uw mask = 0xf;

	switch (type) {
	case SLJIT_EQUAL:
	case SLJIT_EQUAL_F64:
		return mask & eq;

	case SLJIT_NOT_EQUAL:
	case SLJIT_NOT_EQUAL_F64:
		return mask & ~eq;

	case SLJIT_LESS:
	case SLJIT_SIG_LESS:
	case SLJIT_LESS_F64:
		return mask & lt;

	case SLJIT_LESS_EQUAL:
	case SLJIT_SIG_LESS_EQUAL:
	case SLJIT_LESS_EQUAL_F64:
		return mask & (lt | eq);

	case SLJIT_GREATER:
	case SLJIT_SIG_GREATER:
	case SLJIT_GREATER_F64:
		return mask & gt;

	case SLJIT_GREATER_EQUAL:
	case SLJIT_SIG_GREATER_EQUAL:
	case SLJIT_GREATER_EQUAL_F64:
		return mask & (gt | eq);

	case SLJIT_OVERFLOW:
	case SLJIT_MUL_OVERFLOW:
	case SLJIT_UNORDERED_F64:
		return mask & ov;

	case SLJIT_NOT_OVERFLOW:
	case SLJIT_MUL_NOT_OVERFLOW:
	case SLJIT_ORDERED_F64:
		return mask & ~ov;
	}
	SLJIT_UNREACHABLE();
}

/* Facility to bit index mappings.
   Note: some facilities share the same bit index. */
typedef sljit_uw facility_bit;
#define STORE_FACILITY_LIST_EXTENDED_FACILITY 7
#define FAST_LONG_DISPLACEMENT_FACILITY 19
#define EXTENDED_IMMEDIATE_FACILITY 21
#define GENERAL_INSTRUCTION_EXTENSION_FACILITY 34
#define DISTINCT_OPERAND_FACILITY 45
#define HIGH_WORD_FACILITY 45
#define POPULATION_COUNT_FACILITY 45
#define LOAD_STORE_ON_CONDITION_1_FACILITY 45
#define MISCELLANEOUS_INSTRUCTION_EXTENSIONS_1_FACILITY 49
#define LOAD_STORE_ON_CONDITION_2_FACILITY 53
#define MISCELLANEOUS_INSTRUCTION_EXTENSIONS_2_FACILITY 58
#define VECTOR_FACILITY 129
#define VECTOR_ENHANCEMENTS_1_FACILITY 135

/* Report whether a facility is known to be present due to the compiler
   settings. This function should always be compiled to a constant
   value given a constant argument. */
static SLJIT_INLINE int have_facility_static(facility_bit x)
{
#if ENABLE_STATIC_FACILITY_DETECTION != 0
	switch (x) {
	case FAST_LONG_DISPLACEMENT_FACILITY:
		return (__ARCH__ >=  6 /* z990 */);
	case EXTENDED_IMMEDIATE_FACILITY:
	case STORE_FACILITY_LIST_EXTENDED_FACILITY:
		return (__ARCH__ >=  7 /* z9-109 */);
	case GENERAL_INSTRUCTION_EXTENSION_FACILITY:
		return (__ARCH__ >=  8 /* z10 */);
	case DISTINCT_OPERAND_FACILITY:
		return (__ARCH__ >=  9 /* z196 */);
	case MISCELLANEOUS_INSTRUCTION_EXTENSIONS_1_FACILITY:
		return (__ARCH__ >= 10 /* zEC12 */);
	case LOAD_STORE_ON_CONDITION_2_FACILITY:
	case VECTOR_FACILITY:
		return (__ARCH__ >= 11 /* z13 */);
	case MISCELLANEOUS_INSTRUCTION_EXTENSIONS_2_FACILITY:
	case VECTOR_ENHANCEMENTS_1_FACILITY:
		return (__ARCH__ >= 12 /* z14 */);
	}
#endif
	return 0;
}

static SLJIT_INLINE long get_hwcap()
{
	static long hwcap = 0;
	if (SLJIT_UNLIKELY(hwcap == 0)) {
		hwcap = getauxval(AT_HWCAP);
		SLJIT_ASSERT(hwcap != 0);
	}
	return hwcap;
}

static SLJIT_INLINE int have_stfle()
{
	if (have_facility_static(STORE_FACILITY_LIST_EXTENDED_FACILITY)) {
		return 1;
	}
	return SLJIT_LIKELY((HWCAP_S390_STFLE & get_hwcap()) != 0);
}

/* Report whether the given facility is available. This function always
   performs a runtime check. */
static int have_facility_dynamic(facility_bit x)
{
#if ENABLE_DYNAMIC_FACILITY_DETECTION
	static struct {
		sljit_uw bits[4];
	} cpu_features;
	size_t size = sizeof(cpu_features);
	SLJIT_ASSERT(sizeof(cpu_features) >= ((x+7)/8));
	if (SLJIT_UNLIKELY(!have_stfle())) {
		return 0;
	}
	if (SLJIT_UNLIKELY(cpu_features.bits[0] == 0)) {
		__asm__ __volatile__ (
			"lgr   %%r0, %0;"
			"stfle 0(%1);"
			/* outputs  */:
			/* inputs   */: "d" ((size / 8) - 1), "a" (&cpu_features)
			/* clobbers */: "r0", "cc", "memory"
		);
		SLJIT_ASSERT(cpu_features.bits[0] != 0);
	}
	const sljit_uw word_index = x / 64;
	const sljit_uw bit_index = ((1UL << 63) >> (x & 63));
	return (cpu_features.bits[word_index] & bit_index) != 0;
#else
	return 0;
#endif
}

#define HAVE_FACILITY(name, bit) \
static SLJIT_INLINE int name() \
{ \
	static int have = -1; \
	/* Static check first. May allow the function to be optimized away. */ \
	if (have_facility_static(bit)) { \
		return 1; \
	} \
	if (SLJIT_UNLIKELY(have < 0)) { \
		have = have_facility_dynamic(bit) ? 1 : 0; \
	} \
	return SLJIT_LIKELY(have); \
}
HAVE_FACILITY(have_eimm,    EXTENDED_IMMEDIATE_FACILITY)
HAVE_FACILITY(have_ldisp,   FAST_LONG_DISPLACEMENT_FACILITY)
HAVE_FACILITY(have_genext,  GENERAL_INSTRUCTION_EXTENSION_FACILITY)
HAVE_FACILITY(have_lscond1, LOAD_STORE_ON_CONDITION_1_FACILITY)
HAVE_FACILITY(have_lscond2, LOAD_STORE_ON_CONDITION_2_FACILITY)
HAVE_FACILITY(have_misc2,   MISCELLANEOUS_INSTRUCTION_EXTENSIONS_2_FACILITY)
#undef HAVE_FACILITY

#define CHECK_SIGNED(v, bitlen) \
	((v) == (((v) << (sizeof(v)*8 - bitlen)) >> (sizeof(v)*8 - bitlen)))
static int is_s16(sljit_sw d) { return CHECK_SIGNED(d, 16); }
static int is_s20(sljit_sw d) { return CHECK_SIGNED(d, 20); }
static int is_s32(sljit_sw d) { return CHECK_SIGNED(d, 32); }
#undef CHECK_SIGNED

static int is_u12(sljit_sw d) { return 0 <= d && d <= 0x00000fffL; }
static int is_u32(sljit_sw d) { return 0 <= d && d <= 0xffffffffL; }

static sljit_uw disp_s20(sljit_s32 d)
{
	SLJIT_ASSERT(is_s20(d));
	sljit_uw dh = (d >> 12) & 0xff;
	sljit_uw dl = (d << 8) & 0xfff00;
	return dh | dl;
}

#define SLJIT_S390X_INSTRUCTION(op, ...) \
static sljit_ins op(__VA_ARGS__)

/* RR form instructions. */
#define SLJIT_S390X_RR(name, pattern) \
SLJIT_S390X_INSTRUCTION(name, sljit_gpr dst, sljit_gpr src) \
{ \
	return pattern | ((dst&0xf)<<4) | (src&0xf); \
}

/* ADD */
SLJIT_S390X_RR(ar,   0x1a00)

/* ADD LOGICAL */
SLJIT_S390X_RR(alr,  0x1e00)

/* AND */
SLJIT_S390X_RR(nr,   0x1400)

/* BRANCH AND SAVE */
SLJIT_S390X_RR(basr, 0x0d00)

/* BRANCH ON CONDITION */
SLJIT_S390X_RR(bcr,  0x0700) /* TODO(mundaym): type for mask? */

/* COMPARE */
SLJIT_S390X_RR(cr,   0x1900)

/* COMPARE LOGICAL */
SLJIT_S390X_RR(clr,  0x1500)

/* DIVIDE */
SLJIT_S390X_RR(dr,   0x1d00)

/* EXCLUSIVE OR */
SLJIT_S390X_RR(xr,   0x1700)

/* LOAD */
SLJIT_S390X_RR(lr,   0x1800)

/* LOAD COMPLEMENT */
SLJIT_S390X_RR(lcr,  0x1300)

/* OR */
SLJIT_S390X_RR(or,   0x1600)

/* SUBTRACT */
SLJIT_S390X_RR(sr,   0x1b00)

/* SUBTRACT LOGICAL */
SLJIT_S390X_RR(slr,  0x1f00)

#undef SLJIT_S390X_RR

/* RRE form instructions */
#define SLJIT_S390X_RRE(name, pattern) \
SLJIT_S390X_INSTRUCTION(name, sljit_gpr dst, sljit_gpr src) \
{ \
	return pattern | ((dst&0xf)<<4) | (src&0xf); \
}

/* ADD */
SLJIT_S390X_RRE(agr,   0xb9080000)

/* ADD LOGICAL */
SLJIT_S390X_RRE(algr,  0xb90a0000)

/* ADD LOGICAL WITH CARRY */
SLJIT_S390X_RRE(alcr,  0xb9980000)
SLJIT_S390X_RRE(alcgr, 0xb9880000)

/* AND */
SLJIT_S390X_RRE(ngr,   0xb9800000)

/* COMPARE */
SLJIT_S390X_RRE(cgr,   0xb9200000)

/* COMPARE LOGICAL */
SLJIT_S390X_RRE(clgr,  0xb9210000)

/* DIVIDE LOGICAL */
SLJIT_S390X_RRE(dlr,   0xb9970000)
SLJIT_S390X_RRE(dlgr,  0xb9870000)

/* DIVIDE SINGLE */
SLJIT_S390X_RRE(dsgr,  0xb90d0000)

/* EXCLUSIVE OR */
SLJIT_S390X_RRE(xgr,   0xb9820000)

/* LOAD */
SLJIT_S390X_RRE(lgr,   0xb9040000)
SLJIT_S390X_RRE(lgfr,  0xb9140000)

/* LOAD BYTE */
SLJIT_S390X_RRE(lbr,   0xb9260000)
SLJIT_S390X_RRE(lgbr,  0xb9060000)

/* LOAD COMPLEMENT */
SLJIT_S390X_RRE(lcgr,  0xb9030000)

/* LOAD HALFWORD */
SLJIT_S390X_RRE(lhr,   0xb9270000)
SLJIT_S390X_RRE(lghr,  0xb9070000)

/* LOAD LOGICAL */
SLJIT_S390X_RRE(llgfr, 0xb9160000)

/* LOAD LOGICAL CHARACTER */
SLJIT_S390X_RRE(llcr,  0xb9940000)
SLJIT_S390X_RRE(llgcr, 0xb9840000)

/* LOAD LOGICAL HALFWORD */
SLJIT_S390X_RRE(llhr,  0xb9950000)
SLJIT_S390X_RRE(llghr, 0xb9850000)

/* MULTIPLY LOGICAL */
SLJIT_S390X_RRE(mlgr,  0xb9860000)

/* MULTIPLY SINGLE */
SLJIT_S390X_RRE(msr,   0xb2520000)
SLJIT_S390X_RRE(msgr,  0xb90c0000)
SLJIT_S390X_RRE(msgfr, 0xb91c0000)

/* OR */
SLJIT_S390X_RRE(ogr,   0xb9810000)

/* SUBTRACT */
SLJIT_S390X_RRE(sgr,   0xb9090000)

/* SUBTRACT LOGICAL */
SLJIT_S390X_RRE(slgr,  0xb90b0000)

/* SUBTRACT LOGICAL WITH BORROW */
SLJIT_S390X_RRE(slbr,  0xb9990000)
SLJIT_S390X_RRE(slbgr, 0xb9890000)

#undef SLJIT_S390X_RRE

/* RI-a form instructions */
#define SLJIT_S390X_RIA(name, pattern, imm_type) \
SLJIT_S390X_INSTRUCTION(name, sljit_gpr reg, imm_type imm) \
{ \
	return pattern | ((reg&0xf) << 20) | (imm&0xffff); \
}

/* ADD HALFWORD IMMEDIATE */
SLJIT_S390X_RIA(ahi,   0xa70a0000, sljit_s16)
SLJIT_S390X_RIA(aghi,  0xa70b0000, sljit_s16)

/* COMPARE HALFWORD IMMEDIATE */
SLJIT_S390X_RIA(chi,   0xa70e0000, sljit_s16)
SLJIT_S390X_RIA(cghi,  0xa70f0000, sljit_s16)

/* LOAD HALFWORD IMMEDIATE */
SLJIT_S390X_RIA(lhi,   0xa7080000, sljit_s16)
SLJIT_S390X_RIA(lghi,  0xa7090000, sljit_s16)

/* LOAD LOGICAL IMMEDIATE */
SLJIT_S390X_RIA(llihh, 0xa50c0000, sljit_u16)
SLJIT_S390X_RIA(llihl, 0xa50d0000, sljit_u16)
SLJIT_S390X_RIA(llilh, 0xa50e0000, sljit_u16)
SLJIT_S390X_RIA(llill, 0xa50f0000, sljit_u16)

/* MULTIPLY HALFWORD IMMEDIATE */
SLJIT_S390X_RIA(mhi,   0xa70c0000, sljit_s16)
SLJIT_S390X_RIA(mghi,  0xa70d0000, sljit_s16)

/* OR IMMEDIATE */
SLJIT_S390X_RIA(oilh,  0xa50a0000, sljit_u16)

/* TEST UNDER MASK */
SLJIT_S390X_RIA(tmlh,  0xa7000000, sljit_u16)

#undef SLJIT_S390X_RIA

/* RIL-a form instructions (requires extended immediate facility) */
#define SLJIT_S390X_RILA(name, pattern, imm_type) \
SLJIT_S390X_INSTRUCTION(name, sljit_gpr reg, imm_type imm) \
{ \
	SLJIT_ASSERT(have_eimm()); \
	return pattern | ((sljit_ins)(reg&0xf) << 36) | (imm&0xffffffff); \
}

/* ADD IMMEDIATE */
SLJIT_S390X_RILA(afi,   0xc20900000000, sljit_s32)
SLJIT_S390X_RILA(agfi,  0xc20800000000, sljit_s32)

/* ADD IMMEDIATE HIGH */
SLJIT_S390X_RILA(aih,   0xcc0800000000, sljit_s32) /* TODO(mundaym): high-word facility? */

/* ADD LOGICAL IMMEDIATE */
SLJIT_S390X_RILA(alfi,  0xc20b00000000, sljit_u32)
SLJIT_S390X_RILA(algfi, 0xc20a00000000, sljit_u32)

/* AND IMMEDIATE */
SLJIT_S390X_RILA(nihf,  0xc00a00000000, sljit_u32)
SLJIT_S390X_RILA(nilf,  0xc00b00000000, sljit_u32)

/* COMPARE IMMEDIATE */
SLJIT_S390X_RILA(cfi,   0xc20d00000000, sljit_s32)
SLJIT_S390X_RILA(cgfi,  0xc20c00000000, sljit_s32)

/* COMPARE IMMEDIATE HIGH */
SLJIT_S390X_RILA(cih,   0xcc0d00000000, sljit_s32) /* TODO(mundaym): high-word facility? */

/* COMPARE LOGICAL IMMEDIATE */
SLJIT_S390X_RILA(clfi,  0xc20f00000000, sljit_u32)
SLJIT_S390X_RILA(clgfi, 0xc20e00000000, sljit_u32)

/* EXCLUSIVE OR IMMEDIATE */
SLJIT_S390X_RILA(xilf,  0xc00700000000, sljit_u32)

/* INSERT IMMEDIATE */
SLJIT_S390X_RILA(iihf,  0xc00800000000, sljit_u32)
SLJIT_S390X_RILA(iilf,  0xc00900000000, sljit_u32)

/* LOAD IMMEDIATE */
SLJIT_S390X_RILA(lgfi,  0xc00100000000, sljit_s32)

/* LOAD LOGICAL IMMEDIATE */
SLJIT_S390X_RILA(llihf, 0xc00e00000000, sljit_u32)
SLJIT_S390X_RILA(llilf, 0xc00f00000000, sljit_u32)

/* OR IMMEDIATE */
SLJIT_S390X_RILA(oilf,  0xc00d00000000, sljit_u32)

#undef SLJIT_S390X_RILA

/* RX-a form instructions */
#define SLJIT_S390X_RXA(name, pattern) \
SLJIT_S390X_INSTRUCTION(name, sljit_gpr r, sljit_u16 d, sljit_gpr x, sljit_gpr b) \
{ \
	SLJIT_ASSERT((d&0xfff) == d); \
	sljit_ins ri = (sljit_ins)(r&0xf) << 20; \
	sljit_ins xi = (sljit_ins)(x&0xf) << 16; \
	sljit_ins bi = (sljit_ins)(b&0xf) << 12; \
	sljit_ins di = (sljit_ins)(d&0xfff); \
	return pattern | ri | xi | bi | di; \
}

/* ADD */
SLJIT_S390X_RXA(a,   0x5a000000)

/* ADD LOGICAL */
SLJIT_S390X_RXA(al,  0x5e000000)

/* AND */
SLJIT_S390X_RXA(n,   0x54000000)

/* EXCLUSIVE OR */
SLJIT_S390X_RXA(x,   0x57000000)

/* LOAD */
SLJIT_S390X_RXA(l,   0x58000000)

/* LOAD ADDRESS */
SLJIT_S390X_RXA(la,  0x41000000)

/* LOAD HALFWORD */
SLJIT_S390X_RXA(lh,  0x48000000)

/* MULTIPLY SINGLE */
SLJIT_S390X_RXA(ms,  0x71000000)

/* OR */
SLJIT_S390X_RXA(o,   0x56000000)

/* STORE */
SLJIT_S390X_RXA(st,  0x50000000)

/* STORE CHARACTER */
SLJIT_S390X_RXA(stc, 0x42000000)

/* STORE HALFWORD */
SLJIT_S390X_RXA(sth, 0x40000000)

/* SUBTRACT */
SLJIT_S390X_RXA(s,   0x5b000000)

/* SUBTRACT LOGICAL */
SLJIT_S390X_RXA(sl,  0x5f000000)

#undef SLJIT_S390X_RXA

/* RXY-a instructions */
#define SLJIT_S390X_RXYA(name, pattern, cond) \
SLJIT_S390X_INSTRUCTION(name, sljit_gpr r, sljit_s32 d, sljit_gpr x, sljit_gpr b) \
{ \
	SLJIT_ASSERT(cond); \
	sljit_ins ri = (sljit_ins)(r&0xf) << 36; \
	sljit_ins xi = (sljit_ins)(x&0xf) << 32; \
	sljit_ins bi = (sljit_ins)(b&0xf) << 28; \
	sljit_ins di = (sljit_ins)disp_s20(d) << 8; \
	return pattern | ri | xi | bi | di; \
}

/* ADD */
SLJIT_S390X_RXYA(ay,    0xe3000000005a, have_ldisp())
SLJIT_S390X_RXYA(ag,    0xe30000000008, 1)

/* ADD LOGICAL */
SLJIT_S390X_RXYA(aly,   0xe3000000005e, have_ldisp())
SLJIT_S390X_RXYA(alg,   0xe3000000000a, 1)

/* ADD LOGICAL WITH CARRY */
SLJIT_S390X_RXYA(alc,   0xe30000000098, 1)
SLJIT_S390X_RXYA(alcg,  0xe30000000088, 1)

/* AND */
SLJIT_S390X_RXYA(ny,    0xe30000000054, have_ldisp())
SLJIT_S390X_RXYA(ng,    0xe30000000080, 1)

/* EXCLUSIVE OR */
SLJIT_S390X_RXYA(xy,    0xe30000000057, have_ldisp())
SLJIT_S390X_RXYA(xg,    0xe30000000082, 1)

/* LOAD */
SLJIT_S390X_RXYA(ly,    0xe30000000058, have_ldisp())
SLJIT_S390X_RXYA(lg,    0xe30000000004, 1)
SLJIT_S390X_RXYA(lgf,   0xe30000000014, 1)

/* LOAD BYTE */
SLJIT_S390X_RXYA(lb,    0xe30000000076, have_ldisp())
SLJIT_S390X_RXYA(lgb,   0xe30000000077, have_ldisp())

/* LOAD HALFWORD */
SLJIT_S390X_RXYA(lhy,   0xe30000000078, have_ldisp())
SLJIT_S390X_RXYA(lgh,   0xe30000000015, 1)

/* LOAD LOGICAL */
SLJIT_S390X_RXYA(llgf,  0xe30000000016, 1)

/* LOAD LOGICAL CHARACTER */
SLJIT_S390X_RXYA(llc,   0xe30000000094, have_eimm())
SLJIT_S390X_RXYA(llgc,  0xe30000000090, 1)

/* LOAD LOGICAL HALFWORD */
SLJIT_S390X_RXYA(llh,   0xe30000000095, have_eimm())
SLJIT_S390X_RXYA(llgh,  0xe30000000091, 1)

/* MULTIPLY SINGLE */
SLJIT_S390X_RXYA(msy,   0xe30000000051, have_ldisp())
SLJIT_S390X_RXYA(msg,   0xe3000000000c, 1)

/* OR */
SLJIT_S390X_RXYA(oy,    0xe30000000056, have_ldisp())
SLJIT_S390X_RXYA(og,    0xe30000000081, 1)

/* STORE */
SLJIT_S390X_RXYA(sty,   0xe30000000050, have_ldisp())
SLJIT_S390X_RXYA(stg,   0xe30000000024, 1)

/* STORE CHARACTER */
SLJIT_S390X_RXYA(stcy,  0xe30000000072, have_ldisp())

/* STORE HALFWORD */
SLJIT_S390X_RXYA(sthy,  0xe30000000070, have_ldisp())

/* SUBTRACT */
SLJIT_S390X_RXYA(sy,    0xe3000000005b, have_ldisp())
SLJIT_S390X_RXYA(sg,    0xe30000000009, 1)

/* SUBTRACT LOGICAL */
SLJIT_S390X_RXYA(sly,   0xe3000000005f, have_ldisp())
SLJIT_S390X_RXYA(slg,   0xe3000000000b, 1)

/* SUBTRACT LOGICAL WITH BORROW */
SLJIT_S390X_RXYA(slb,   0xe30000000099, 1)
SLJIT_S390X_RXYA(slbg,  0xe30000000089, 1)

#undef SLJIT_S390X_RXYA

/* RS-a instructions */
#define SLJIT_S390X_RSA(name, pattern) \
SLJIT_S390X_INSTRUCTION(name, sljit_gpr reg, sljit_sw d, sljit_gpr b) \
{ \
	sljit_ins r1 = (sljit_ins)(reg&0xf) << 20; \
	sljit_ins b2 = (sljit_ins)(b&0xf) << 12; \
	sljit_ins d2 = (sljit_ins)(d&0xfff); \
	return pattern | r1 | b2 | d2; \
}

/* SHIFT LEFT SINGLE LOGICAL */
SLJIT_S390X_RSA(sll, 0x89000000)

/* SHIFT RIGHT SINGLE */
SLJIT_S390X_RSA(sra, 0x8a000000)

/* SHIFT RIGHT SINGLE LOGICAL */
SLJIT_S390X_RSA(srl, 0x88000000)

#undef SLJIT_S390X_RSA

/* RSY-a instructions */
#define SLJIT_S390X_RSYA(name, pattern, cond) \
SLJIT_S390X_INSTRUCTION(name, sljit_gpr dst, sljit_gpr src, sljit_sw d, sljit_gpr b) \
{ \
	SLJIT_ASSERT(cond); \
	sljit_ins r1 = (sljit_ins)(dst&0xf) << 36; \
	sljit_ins r3 = (sljit_ins)(src&0xf) << 32; \
	sljit_ins b2 = (sljit_ins)(b&0xf) << 28; \
	sljit_ins d2 = (sljit_ins)disp_s20(d) << 8; \
	return pattern | r1 | r3 | b2 | d2; \
}

/* LOAD MULTIPLE */
SLJIT_S390X_RSYA(lmg,   0xeb0000000004, 1)

/* SHIFT LEFT LOGICAL */
SLJIT_S390X_RSYA(sllg,  0xeb000000000d, 1)

/* SHIFT RIGHT SINGLE */
SLJIT_S390X_RSYA(srag,  0xeb000000000a, 1)

/* SHIFT RIGHT SINGLE LOGICAL */
SLJIT_S390X_RSYA(srlg,  0xeb000000000c, 1)

/* STORE MULTIPLE */
SLJIT_S390X_RSYA(stmg,  0xeb0000000024, 1)

#undef SLJIT_S390X_RSYA

/* RIE-f instructions (require general-instructions-extension facility) */
#define SLJIT_S390X_RIEF(name, pattern) \
SLJIT_S390X_INSTRUCTION(name, sljit_gpr dst, sljit_gpr src, sljit_u8 start, sljit_u8 end, sljit_u8 rot) \
{ \
	SLJIT_ASSERT(have_genext()); \
	sljit_ins r1 = (sljit_ins)(dst&0xf) << 36; \
	sljit_ins r2 = (sljit_ins)(src&0xf) << 32; \
	sljit_ins i3 = (sljit_ins)(start) << 24; \
	sljit_ins i4 = (sljit_ins)(end) << 16; \
	sljit_ins i5 = (sljit_ins)(rot) << 8; \
	return pattern | r1 | r2 | i3 | i4 | i5; \
}

/* ROTATE THEN AND SELECTED BITS */
/* SLJIT_S390X_RIEF(rnsbg,  0xec0000000054) */

/* ROTATE THEN EXCLUSIVE OR SELECTED BITS */
/* SLJIT_S390X_RIEF(rxsbg,  0xec0000000057) */

/* ROTATE THEN OR SELECTED BITS */
SLJIT_S390X_RIEF(rosbg,  0xec0000000056)

/* ROTATE THEN INSERT SELECTED BITS */
/* SLJIT_S390X_RIEF(risbg,  0xec0000000055) */
/* SLJIT_S390X_RIEF(risbgn, 0xec0000000059) */

/* ROTATE THEN INSERT SELECTED BITS HIGH */
SLJIT_S390X_RIEF(risbhg, 0xec000000005d)

/* ROTATE THEN INSERT SELECTED BITS LOW */
/* SLJIT_S390X_RIEF(risblg, 0xec0000000051) */

#undef SLJIT_S390X_RIEF

/* RRF-a instructions */
#define SLJIT_S390X_RRFA(name, pattern, cond) \
SLJIT_S390X_INSTRUCTION(name, sljit_gpr dst, sljit_gpr src1, sljit_gpr src2) \
{ \
	SLJIT_ASSERT(cond); \
	sljit_ins r1 = (sljit_ins)(dst&0xf) << 4; \
	sljit_ins r2 = (sljit_ins)(src1&0xf); \
	sljit_ins r3 = (sljit_ins)(src2&0xf) << 12; \
	return pattern | r3 | r1 | r2; \
}

/* MULTIPLY */
SLJIT_S390X_RRFA(msrkc,  0xb9fd0000, have_misc2())
SLJIT_S390X_RRFA(msgrkc, 0xb9ed0000, have_misc2())

#undef SLJIT_S390X_RRFA

/* RRF-c instructions (require load/store-on-condition 1 facility) */
#define SLJIT_S390X_RRFC(name, pattern) \
SLJIT_S390X_INSTRUCTION(name, sljit_gpr dst, sljit_gpr src, sljit_uw mask) \
{ \
	SLJIT_ASSERT(have_lscond1()); \
	sljit_ins r1 = (sljit_ins)(dst&0xf) << 4; \
	sljit_ins r2 = (sljit_ins)(src&0xf); \
	sljit_ins m3 = (sljit_ins)(mask&0xf) << 12; \
	return pattern | m3 | r1 | r2; \
}

/* LOAD HALFWORD IMMEDIATE ON CONDITION */
SLJIT_S390X_RRFC(locr,  0xb9f20000)
SLJIT_S390X_RRFC(locgr, 0xb9e20000)

#undef SLJIT_S390X_RRFC

/* RIE-g instructions (require load/store-on-condition 2 facility) */
#define SLJIT_S390X_RIEG(name, pattern) \
SLJIT_S390X_INSTRUCTION(name, sljit_gpr reg, sljit_sw imm, sljit_uw mask) \
{ \
	SLJIT_ASSERT(have_lscond2()); \
	sljit_ins r1 = (sljit_ins)(reg&0xf) << 36; \
	sljit_ins m3 = (sljit_ins)(mask&0xf) << 32; \
	sljit_ins i2 = (sljit_ins)(imm&0xffffL) << 16; \
	return pattern | r1 | m3 | i2; \
}

/* LOAD HALFWORD IMMEDIATE ON CONDITION */
SLJIT_S390X_RIEG(lochi,  0xec0000000042)
SLJIT_S390X_RIEG(locghi, 0xec0000000046)

#undef SLJIT_S390X_RIEG

#define SLJIT_S390X_RILB(name, pattern, cond) \
SLJIT_S390X_INSTRUCTION(name, sljit_gpr reg, sljit_sw ri) \
{ \
	SLJIT_ASSERT(cond); \
	sljit_ins r1 = (sljit_ins)(reg&0xf) << 36; \
	sljit_ins ri2 = (sljit_ins)(ri&0xffffffff); \
	return pattern | r1 | ri2; \
}

/* BRANCH RELATIVE AND SAVE LONG */
SLJIT_S390X_RILB(brasl, 0xc00500000000, 1);

/* LOAD ADDRESS RELATIVE LONG */
SLJIT_S390X_RILB(larl,  0xc00000000000, 1);

/* LOAD RELATIVE LONG */
SLJIT_S390X_RILB(lgrl,  0xc40800000000, have_genext());

#undef SLJIT_S390X_RILB

SLJIT_S390X_INSTRUCTION(br, sljit_gpr target)
{
	return 0x07f0 | target;
}

SLJIT_S390X_INSTRUCTION(brcl, sljit_uw mask, sljit_sw target)
{
	sljit_ins m1 = (sljit_ins)(mask&0xf) << 36;
	sljit_ins ri2 = (sljit_ins)target&0xffffffff;
	return 0xc00400000000 | m1 | ri2;
}

SLJIT_S390X_INSTRUCTION(flogr, sljit_gpr dst, sljit_gpr src)
{
	SLJIT_ASSERT(have_eimm());
	sljit_ins r1 = ((sljit_ins)(dst)&0xf) << 8;
	sljit_ins r2 = ((sljit_ins)(src)&0xf);
	return 0xb9830000 | r1 | r2;
}

/* INSERT PROGRAM MASK */
SLJIT_S390X_INSTRUCTION(ipm, sljit_gpr dst)
{
	return 0xb2220000 | ((sljit_ins)(dst&0xf) << 4);
}

/* ROTATE THEN INSERT SELECTED BITS HIGH (ZERO) */
SLJIT_S390X_INSTRUCTION(risbhgz, sljit_gpr dst, sljit_gpr src, sljit_u8 start, sljit_u8 end, sljit_u8 rot)
{
	return risbhg(dst, src, start, 0x8 | end, rot);
}

#undef SLJIT_S390X_INSTRUCTION

/* load condition code as needed to match type */
static sljit_s32 push_load_cc(struct sljit_compiler *compiler, sljit_s32 type)
{
	type &= ~SLJIT_I32_OP;
	switch (type) {
	case SLJIT_ZERO:
	case SLJIT_NOT_ZERO:
		FAIL_IF(push_inst(compiler, cih(flag_r, 0)));
		break;
	default:
		FAIL_IF(push_inst(compiler, tmlh(flag_r, 0x3000)));
		break;
	}
	return SLJIT_SUCCESS;
}

static sljit_s32 push_store_zero_flag(struct sljit_compiler *compiler, sljit_s32 op, sljit_gpr source)
{
	/* insert low 32-bits into high 32-bits of flag register */
	FAIL_IF(push_inst(compiler, risbhgz(flag_r, source, 0, 31, 32)));
	if (!(op & SLJIT_I32_OP)) {
		/* OR high 32-bits with high 32-bits of flag register */
		FAIL_IF(push_inst(compiler, rosbg(flag_r, source, 0, 31, 0)));
	}
	return SLJIT_SUCCESS;
}

/* load 64-bit immediate into register without clobbering flags */
static sljit_s32 push_load_imm_inst(struct sljit_compiler *compiler, sljit_gpr target, sljit_sw v)
{
	/* 4 byte instructions */
	if (v == ((v << 48)>>48)) {
		return push_inst(compiler, lghi(target, (sljit_s16)v));
	}
	if (v == (v & 0x000000000000ffff)) {
		return push_inst(compiler, llill(target, (sljit_u16)(v)));
	}
	if (v == (v & 0x00000000ffff0000)) {
		return push_inst(compiler, llilh(target, (sljit_u16)(v>>16)));
	}
	if (v == (v & 0x0000ffff00000000)) {
		return push_inst(compiler, llihl(target, (sljit_u16)(v>>32)));
	}
	if (v == (v & 0xffff000000000000)) {
		return push_inst(compiler, llihh(target, (sljit_u16)(v>>48)));
	}

	/* 6 byte instructions (requires extended immediate facility) */
	if (have_eimm()) {
		if (v == ((v << 32)>>32)) {
			return push_inst(compiler, lgfi(target, (sljit_s32)v));
		}
		if (v == (v & 0x00000000ffffffff)) {
			return push_inst(compiler, llilf(target, (sljit_u32)(v)));
		}
		if (v == (v & 0xffffffff00000000)) {
			return push_inst(compiler, llihf(target, (sljit_u32)(v>>32)));
		}
		FAIL_IF(push_inst(compiler, llilf(target, (sljit_u32)(v))));
		return push_inst(compiler, iihf(target, (sljit_u32)(v>>32)));
	}
	/* TODO(mundaym): instruction sequences that don't use extended immediates */
	abort();
}

struct addr {
	sljit_gpr base;
	sljit_gpr index;
	sljit_sw  offset;
};

/* transform memory operand into D(X,B) form with a signed 20-bit offset */
static sljit_s32 make_addr_bxy(
	struct sljit_compiler *compiler,
	struct addr *addr,
	sljit_s32 mem, sljit_sw off,
	sljit_gpr tmp /* clobbered, must not be R0 */)
{
	SLJIT_ASSERT(tmp != r0);
	sljit_gpr base = r0;
	if (mem & REG_MASK) {
		base = gpr(mem & REG_MASK);
	}
	sljit_gpr index = r0;
	if (mem & OFFS_REG_MASK) {
		index = gpr(OFFS_REG(mem));
		if (off != 0) {
			/* shift and put the result into tmp */
			SLJIT_ASSERT(0 <= off && off < 64);
			FAIL_IF(push_inst(compiler, sllg(tmp, index, off, 0)));
			index = tmp;
			off = 0; /* clear offset */
		}
	} else if (!is_s20(off)) {
		FAIL_IF(push_load_imm_inst(compiler, tmp, off));
		index = tmp;
		off = 0; /* clear offset */
	}
	*addr = (struct addr) {
		.base = base,
		.index = index,
		.offset = off
	};
	return SLJIT_SUCCESS;
}

/* transform memory operand into D(X,B) form with an unsigned 12-bit offset */
static sljit_s32 make_addr_bx(
	struct sljit_compiler *compiler,
	struct addr *addr,
	sljit_s32 mem, sljit_sw off,
	sljit_gpr tmp /* clobbered, must not be R0 */)
{
	SLJIT_ASSERT(tmp != r0);
	sljit_gpr base = r0;
	if (mem & REG_MASK) {
		base = gpr(mem & REG_MASK);
	}
	sljit_gpr index = r0;
	if (mem & OFFS_REG_MASK) {
		index = gpr(OFFS_REG(mem));
		if (off != 0) {
			/* shift and put the result into tmp */
			SLJIT_ASSERT(0 <= off && off < 64);
			FAIL_IF(push_inst(compiler, sllg(tmp, index, off, 0)));
			index = tmp;
			off = 0; /* clear offset */
		}
	} else if (!is_u12(off)) {
		FAIL_IF(push_load_imm_inst(compiler, tmp, off));
		index = tmp;
		off = 0; /* clear offset */
	}
	*addr = (struct addr) {
		.base = base,
		.index = index,
		.offset = off
	};
	return SLJIT_SUCCESS;
}

static sljit_s32 load_word(struct sljit_compiler *compiler, sljit_gpr dst,
		sljit_s32 src, sljit_sw srcw,
		sljit_gpr tmp /* clobbered */, sljit_s32 is_32bit)
{
	SLJIT_ASSERT(src & SLJIT_MEM);
	struct addr addr;
	if (have_ldisp() || !is_32bit) {
		FAIL_IF(make_addr_bxy(compiler, &addr, src, srcw, tmp));
	} else {
		FAIL_IF(make_addr_bx(compiler, &addr, src, srcw, tmp));
	}
	sljit_ins ins = 0;
	if (is_32bit) {
		ins = is_u12(addr.offset) ?
			l(dst, addr.offset, addr.index, addr.base) :
			ly(dst, addr.offset, addr.index, addr.base);
	} else {
		ins = lg(dst, addr.offset, addr.index, addr.base);
	}
	return push_inst(compiler, ins);
}

static sljit_s32 store_word(struct sljit_compiler *compiler, sljit_gpr src,
		sljit_s32 dst, sljit_sw dstw,
		sljit_gpr tmp /* clobbered */, sljit_s32 is_32bit)
{
	SLJIT_ASSERT(dst & SLJIT_MEM);
	struct addr addr;
	if (have_ldisp() || !is_32bit) {
		FAIL_IF(make_addr_bxy(compiler, &addr, dst, dstw, tmp));
	} else {
		FAIL_IF(make_addr_bx(compiler, &addr, dst, dstw, tmp));
	}
	sljit_ins ins = 0;
	if (is_32bit) {
		ins = is_u12(addr.offset) ?
			st(src, addr.offset, addr.index, addr.base) :
			sty(src, addr.offset, addr.index, addr.base);
	} else {
		ins = stg(src, addr.offset, addr.index, addr.base);
	}
	return push_inst(compiler, ins);
}

SLJIT_API_FUNC_ATTRIBUTE void* sljit_generate_code(struct sljit_compiler *compiler)
{
	CHECK_ERROR_PTR();
	CHECK_PTR(check_sljit_generate_code(compiler));
	reverse_buf(compiler);

	/* branch handling */
	struct sljit_label *label = compiler->labels;
	struct sljit_jump *jump = compiler->jumps;

	/* calculate the size of the code */
	sljit_uw ins_size = 0;  /* instructions */
	sljit_uw pool_size = 0; /* literal pool */
	sljit_uw i, j = 0;
	struct sljit_memory_fragment *buf = NULL;
	for (buf = compiler->buf; buf != NULL; buf = buf->next) {
		sljit_uw len = buf->used_size / sizeof(sljit_ins);
		sljit_ins *ibuf = (sljit_ins *)buf->memory;
		for (i = 0; i < len; ++i, ++j) {
			/* TODO(mundaym): labels, jumps, constants... */
			sljit_ins ins = ibuf[i];
			if (ins & sljit_ins_const) {
				pool_size += 8;
				ins &= ~sljit_ins_const;
			}
			if (label && label->size == j) {
				label->size = ins_size;
				label = label->next;
			}
			if (jump && jump->addr == j) {
				if ((jump->flags & SLJIT_REWRITABLE_JUMP) || (jump->flags & JUMP_ADDR)) {
					/* encoded: */
					/*   brasl %r14, <rel_addr> (or brcl <mask>, <rel_addr>) */
					/* replace with: */
					/*   lgrl %r1, <pool_addr> */
					/*   bras %r14, %r1 (or bcr <mask>, %r1) */
					pool_size += 8;
					ins_size += 2;
				}
				jump = jump->next;
			}
			ins_size += sizeof_ins(ins);
		}
		/* emit trailing label */
		if (label && label->size == j) {
			label->size = ins_size;
			label = label->next;
		}
	}
	SLJIT_ASSERT(label == NULL);
	SLJIT_ASSERT(jump == NULL);

	/* pad code size to 8 bytes */
	/* the literal pool needs to be doubleword aligned */
	sljit_uw pad_size = (((ins_size)+7UL)&~7UL) - ins_size;
	SLJIT_ASSERT(pad_size < 8UL);

	/* allocate target buffer */
	void *code = SLJIT_MALLOC_EXEC(ins_size + pad_size + pool_size);
	PTR_FAIL_WITH_EXEC_IF(code);
	void *code_ptr = code;
	sljit_uw *pool = (sljit_uw *)((sljit_uw)(code) + ins_size + pad_size);
	sljit_uw *pool_ptr = pool;
	struct sljit_s390x_const *const_ = (struct sljit_s390x_const *)compiler->consts;

	/* update label addresses */
	label = compiler->labels;
	while (label) {
		label->addr = (sljit_uw)code_ptr + label->size;
		label = label->next;
	}

	/* reset jumps */
	jump = compiler->jumps;

	/* emit the code */
	j = 0;
	for (buf = compiler->buf; buf != NULL; buf = buf->next) {
		sljit_uw len = buf->used_size / sizeof(sljit_ins);
		sljit_ins *ibuf = (sljit_ins *)buf->memory;
		for (i = 0; i < len; ++i, ++j) {
			/* TODO(mundaym): labels, jumps, constants... */
			sljit_ins ins = ibuf[i];
			if (ins & sljit_ins_const) {
				/* clear the const tag */
				ins &= ~sljit_ins_const;

				/* update instruction with relative address of constant */
				sljit_uw pos = (sljit_uw)(code_ptr);
				sljit_uw off = (sljit_uw)(pool_ptr) - pos;
				SLJIT_ASSERT((off&1) == 0);
				off >>= 1; /* halfword (not byte) offset */
				SLJIT_ASSERT(is_s32(off));
				ins |= (sljit_ins)(off&0xffffffff);

				/* update address */
				const_->const_.addr = (sljit_uw)pool_ptr;

				/* store initial value into pool and update pool address */
				*(pool_ptr++) = const_->init_value;

				/* move to next constant */
				const_ = (struct sljit_s390x_const *)const_->const_.next;
			}
			if (jump && jump->addr == j) {
				sljit_sw target = (jump->flags & JUMP_LABEL) ? jump->u.label->addr : jump->u.target;
				if ((jump->flags & SLJIT_REWRITABLE_JUMP) || (jump->flags & JUMP_ADDR)) {
					jump->addr = (sljit_uw)pool_ptr;

					/* load address into tmp1 */
					sljit_uw pos = (sljit_uw)(code_ptr);
					sljit_uw off = (sljit_uw)(pool_ptr) - pos;
					SLJIT_ASSERT((off&1) == 0);
					off >>= 1; /* halfword (not byte) offset */
					SLJIT_ASSERT(is_s32(off));
					encode_inst(&code_ptr, lgrl(tmp1, off&0xffffffff));

					/* store jump target into pool and update pool address */
					*(pool_ptr++) = target;

					/* branch to tmp1 */
					sljit_ins op = (ins>>32)&0xf;
					sljit_ins arg = (ins>>36)&0xf;
					switch (op) {
					case 4: /* brcl -> bcr */
						ins = bcr(arg, tmp1);
						break;
					case 5: /* brasl -> basr */
						ins = basr(arg, tmp1);
						break;
					default:
						abort();
					}
				} else {
					jump->addr = (sljit_uw)(code_ptr) + 2;
					/* TODO(mundaym): handle executable offset? */
					sljit_sw source = (sljit_sw)(code_ptr);
					sljit_sw offset = target - source;

					/* offset must be halfword aligned */
					SLJIT_ASSERT(!(offset & 1));
					offset >>= 1;
					SLJIT_ASSERT(is_s32(offset)); /* TODO(mundaym): handle arbitrary offsets */

					/* patch jump target */
					ins |= (sljit_ins)(offset)&0xffffffff;
				}
				jump = jump->next;
			}
			encode_inst(&code_ptr, ins);
		}
	}
	SLJIT_ASSERT(code + ins_size == code_ptr);
	SLJIT_ASSERT(((void *)(pool) + pool_size) == (void *)pool_ptr);

	compiler->error = SLJIT_ERR_COMPILED;
	compiler->executable_offset = SLJIT_EXEC_OFFSET(code);
	compiler->executable_size = ins_size;
	code = SLJIT_ADD_EXEC_OFFSET(code, executable_offset);
	code_ptr = SLJIT_ADD_EXEC_OFFSET(code_ptr, executable_offset);
	SLJIT_CACHE_FLUSH(code, code_ptr);
	return code;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_has_cpu_feature(sljit_s32 feature_type)
{
	/* TODO(mundaym): implement all */
	switch (feature_type) {
	case SLJIT_HAS_CLZ:
		return have_eimm() ? 1 : 0; /* FLOGR instruction */
	case SLJIT_HAS_CMOV:
		return have_lscond1() ? 1 : 0;
	case SLJIT_HAS_FPU:
		return 0;
	}
	return 0;
}

/* --------------------------------------------------------------------- */
/*  Entry, exit                                                          */
/* --------------------------------------------------------------------- */

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_enter(struct sljit_compiler *compiler,
	sljit_s32 options, sljit_s32 arg_types, sljit_s32 scratches, sljit_s32 saveds,
	sljit_s32 fscratches, sljit_s32 fsaveds, sljit_s32 local_size)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_enter(compiler, options, arg_types, scratches, saveds, fscratches, fsaveds, local_size));
	set_emit_enter(compiler, options, arg_types, scratches, saveds, fscratches, fsaveds, local_size);

	/* saved registers go in callee allocated save area */
	compiler->local_size = (local_size+0xf)&~0xf;
	sljit_sw frame_size = compiler->local_size + 160;

	FAIL_IF(push_inst(compiler, stmg(r6, r15, 48, r15))); /* save registers TODO(MGM): optimize */
	if (frame_size != 0) {
		if (is_s16(-frame_size)) {
			FAIL_IF(push_inst(compiler, aghi(r15, -((sljit_s16)frame_size))));
		} else if (is_s32(-frame_size)) {
			FAIL_IF(push_inst(compiler, agfi(r15, -((sljit_s32)frame_size))));
		} else {
			FAIL_IF(push_load_imm_inst(compiler, tmp1, -frame_size));
			FAIL_IF(push_inst(compiler, la(r15, 0, tmp1, r15)));
		}
	}

	sljit_s32 args = get_arg_count(arg_types);
	if (args >= 1)
		FAIL_IF(push_inst(compiler, lgr(gpr(SLJIT_S0), gpr(SLJIT_R0))));
	if (args >= 2)
		FAIL_IF(push_inst(compiler, lgr(gpr(SLJIT_S1), gpr(SLJIT_R1))));
	if (args >= 3)
		FAIL_IF(push_inst(compiler, lgr(gpr(SLJIT_S2), gpr(SLJIT_R2))));
	SLJIT_ASSERT(args < 4);

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_set_context(struct sljit_compiler *compiler,
	sljit_s32 options, sljit_s32 arg_types, sljit_s32 scratches, sljit_s32 saveds,
	sljit_s32 fscratches, sljit_s32 fsaveds, sljit_s32 local_size)
{
	CHECK_ERROR();
	CHECK(check_sljit_set_context(compiler, options, arg_types, scratches, saveds, fscratches, fsaveds, local_size));
	set_set_context(compiler, options, arg_types, scratches, saveds, fscratches, fsaveds, local_size);

	/* TODO(mundaym): stack space for saved floating point registers */
	compiler->local_size = (local_size + 0xf) & ~0xf;
	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_return(struct sljit_compiler *compiler, sljit_s32 op, sljit_s32 src, sljit_sw srcw)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_return(compiler, op, src, srcw));

	FAIL_IF(emit_mov_before_return(compiler, op, src, srcw));

	sljit_sw size = compiler->local_size + 160 + 48;
	sljit_gpr end = r15;
	if (!is_s20(size)) {
		FAIL_IF(push_load_imm_inst(compiler, tmp1, compiler->local_size + 160));
		FAIL_IF(push_inst(compiler, la(r15, 0, tmp1, r15)));
		size = 48;
		end = r14; /* r15 has been restored already */
	}
	FAIL_IF(push_inst(compiler, lmg(r6, end, size, r15))); /* restore registers TODO(MGM): optimize */
	FAIL_IF(push_inst(compiler, br(r14))); /* return */

	return SLJIT_SUCCESS;
}

/* --------------------------------------------------------------------- */
/*  Operators                                                            */
/* --------------------------------------------------------------------- */

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_op0(struct sljit_compiler *compiler, sljit_s32 op)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_op0(compiler, op));

	sljit_gpr arg0 = gpr(SLJIT_R0);
	sljit_gpr arg1 = gpr(SLJIT_R1);

	op = GET_OPCODE(op) | (op & SLJIT_I32_OP);
	switch (op) {
	case SLJIT_BREAKPOINT:
		/* TODO(mundaym): insert real breakpoint? */
	case SLJIT_NOP:
		return push_inst(compiler, 0x0700 /* 2-byte nop */);
	case SLJIT_LMUL_UW:
		FAIL_IF(push_inst(compiler, mlgr(arg0, arg0)));
		break;
	case SLJIT_LMUL_SW:
		/* signed multiplication from: */
		/* Hacker's Delight, Second Edition: Chapter 8-3. */
		FAIL_IF(push_inst(compiler, srag(tmp0, arg0, 63, 0)));
		FAIL_IF(push_inst(compiler, srag(tmp1, arg1, 63, 0)));
		FAIL_IF(push_inst(compiler, ngr(tmp0, arg1)));
		FAIL_IF(push_inst(compiler, ngr(tmp1, arg0)));

		/* unsigned multiplication */
		FAIL_IF(push_inst(compiler, mlgr(arg0, arg0)));

		FAIL_IF(push_inst(compiler, sgr(arg0, tmp0)));
		FAIL_IF(push_inst(compiler, sgr(arg0, tmp1)));
		break;
	case SLJIT_DIV_U32:
	case SLJIT_DIVMOD_U32:
		FAIL_IF(push_inst(compiler, lhi(tmp0, 0)));
		FAIL_IF(push_inst(compiler, lr(tmp1, arg0)));
		FAIL_IF(push_inst(compiler, dlr(tmp0, arg1)));
		FAIL_IF(push_inst(compiler, lr(arg0, tmp1))); /* quotient */
		if (op == SLJIT_DIVMOD_U32) {
			FAIL_IF(push_inst(compiler, lr(arg1, tmp0))); /* remainder */
		}
		return SLJIT_SUCCESS;
	case SLJIT_DIV_S32:
	case SLJIT_DIVMOD_S32:
		FAIL_IF(push_inst(compiler, lhi(tmp0, 0)));
		FAIL_IF(push_inst(compiler, lr(tmp1, arg0)));
		FAIL_IF(push_inst(compiler, dr(tmp0, arg1)));
		FAIL_IF(push_inst(compiler, lr(arg0, tmp1))); /* quotient */
		if (op == SLJIT_DIVMOD_S32) {
			FAIL_IF(push_inst(compiler, lr(arg1, tmp0))); /* remainder */
		}
		return SLJIT_SUCCESS;
	case SLJIT_DIV_UW:
	case SLJIT_DIVMOD_UW:
		FAIL_IF(push_inst(compiler, lghi(tmp0, 0)));
		FAIL_IF(push_inst(compiler, lgr(tmp1, arg0)));
		FAIL_IF(push_inst(compiler, dlgr(tmp0, arg1)));
		FAIL_IF(push_inst(compiler, lgr(arg0, tmp1))); /* quotient */
		if (op == SLJIT_DIVMOD_UW) {
			FAIL_IF(push_inst(compiler, lgr(arg1, tmp0))); /* remainder */
		}
		return SLJIT_SUCCESS;
	case SLJIT_DIV_SW:
	case SLJIT_DIVMOD_SW:
		FAIL_IF(push_inst(compiler, lgr(tmp1, arg0)));
		FAIL_IF(push_inst(compiler, dsgr(tmp0, arg1)));
		FAIL_IF(push_inst(compiler, lgr(arg0, tmp1))); /* quotient */
		if (op == SLJIT_DIVMOD_SW) {
			FAIL_IF(push_inst(compiler, lgr(arg1, tmp0))); /* remainder */
		}
		return SLJIT_SUCCESS;
	default:
		SLJIT_UNREACHABLE();
	}
	/* swap result registers */
	FAIL_IF(push_inst(compiler, lgr(tmp0, arg0)));
	FAIL_IF(push_inst(compiler, lgr(arg0, arg1)));
	return push_inst(compiler, lgr(arg1, tmp0));
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_op1(struct sljit_compiler *compiler, sljit_s32 op,
        sljit_s32 dst, sljit_sw dstw,
        sljit_s32 src, sljit_sw srcw)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_op1(compiler, op, dst, dstw, src, srcw));
	ADJUST_LOCAL_OFFSET(dst, dstw);
	ADJUST_LOCAL_OFFSET(src, srcw);

	sljit_s32 opcode = GET_OPCODE(op);
	if ((dst == SLJIT_UNUSED) && !HAS_FLAGS(op)) {
		/* TODO(mundaym): implement prefetch? */
		return SLJIT_SUCCESS;
	}
	if (opcode >= SLJIT_MOV && opcode <= SLJIT_MOV_P) {
		/* LOAD REGISTER */
		if (FAST_IS_REG(dst) && FAST_IS_REG(src)) {
			sljit_ins ins = 0;
			sljit_gpr dst_r = gpr(dst);
			sljit_gpr src_r = gpr(src);
			switch (opcode | (op & SLJIT_I32_OP)) {
			/* 32-bit */
			case SLJIT_MOV32_U8:  ins = llcr(dst_r, src_r); break;
			case SLJIT_MOV32_S8:  ins =  lbr(dst_r, src_r); break;
			case SLJIT_MOV32_U16: ins = llhr(dst_r, src_r); break;
			case SLJIT_MOV32_S16: ins =  lhr(dst_r, src_r); break;
			case SLJIT_MOV32:     ins =   lr(dst_r, src_r); break;
			/* 64-bit */
			case SLJIT_MOV_U8:  ins = llgcr(dst_r, src_r); break;
			case SLJIT_MOV_S8:  ins =  lgbr(dst_r, src_r); break;
			case SLJIT_MOV_U16: ins = llghr(dst_r, src_r); break;
			case SLJIT_MOV_S16: ins =  lghr(dst_r, src_r); break;
			case SLJIT_MOV_U32: ins = llgfr(dst_r, src_r); break;
			case SLJIT_MOV_S32: ins =  lgfr(dst_r, src_r); break;
			case SLJIT_MOV:
			case SLJIT_MOV_P:
				ins = lgr(dst_r, src_r);
				break;
			default:
				SLJIT_UNREACHABLE();
			}
			FAIL_IF(push_inst(compiler, ins));
			if (HAS_FLAGS(op)) {
				/* only handle zero flag */
				SLJIT_ASSERT(!(op & VARIABLE_FLAG_MASK));
				FAIL_IF(push_store_zero_flag(compiler, op, dst_r));
			}
			return SLJIT_SUCCESS;
		}
		/* LOAD IMMEDIATE */
		if (FAST_IS_REG(dst) && (src & SLJIT_IMM)) {
			switch (opcode) {
			case SLJIT_MOV_U8:  srcw = (sljit_sw)((sljit_u8)(srcw));  break;
			case SLJIT_MOV_S8:  srcw = (sljit_sw)((sljit_s8)(srcw));  break;
			case SLJIT_MOV_U16: srcw = (sljit_sw)((sljit_u16)(srcw)); break;
			case SLJIT_MOV_S16: srcw = (sljit_sw)((sljit_s16)(srcw)); break;
			case SLJIT_MOV_U32: srcw = (sljit_sw)((sljit_u32)(srcw)); break;
			case SLJIT_MOV_S32: srcw = (sljit_sw)((sljit_s32)(srcw)); break;
			}
			return push_load_imm_inst(compiler, gpr(dst), srcw);
		}
		/* LOAD */
		if (FAST_IS_REG(dst) && (src & SLJIT_MEM)) {
			sljit_gpr reg = gpr(dst);
			struct addr mem;
			FAIL_IF(make_addr_bxy(compiler, &mem, src, srcw, tmp1));
			sljit_ins ins;
			switch (opcode | (op & SLJIT_I32_OP)) {
			case SLJIT_MOV32_U8:  ins = llc(reg, mem.offset, mem.index, mem.base); break;
			case SLJIT_MOV32_S8:  ins =  lb(reg, mem.offset, mem.index, mem.base); break;
			case SLJIT_MOV32_U16: ins = llh(reg, mem.offset, mem.index, mem.base); break;
			case SLJIT_MOV32_S16:
				ins = is_u12(mem.offset) ?
					lh(reg, mem.offset, mem.index, mem.base) :
					lhy(reg, mem.offset, mem.index, mem.base);
				break;
			case SLJIT_MOV32:
				ins = is_u12(mem.offset) ?
					l(reg, mem.offset, mem.index, mem.base) :
					ly(reg, mem.offset, mem.index, mem.base);
				break;
			case SLJIT_MOV_U8:  ins = llgc(reg, mem.offset, mem.index, mem.base); break;
			case SLJIT_MOV_S8:  ins =  lgb(reg, mem.offset, mem.index, mem.base); break;
			case SLJIT_MOV_U16: ins = llgh(reg, mem.offset, mem.index, mem.base); break;
			case SLJIT_MOV_S16: ins =  lgh(reg, mem.offset, mem.index, mem.base); break;
			case SLJIT_MOV_U32: ins = llgf(reg, mem.offset, mem.index, mem.base); break;
			case SLJIT_MOV_S32: ins =  lgf(reg, mem.offset, mem.index, mem.base); break;
			case SLJIT_MOV_P:
			case SLJIT_MOV:
				ins = lg(reg, mem.offset, mem.index, mem.base);
				break;
			default:
				SLJIT_UNREACHABLE();
			}
			FAIL_IF(push_inst(compiler, ins));
			if (HAS_FLAGS(op)) {
				/* only handle zero flag */
				SLJIT_ASSERT(!(op & VARIABLE_FLAG_MASK));
				FAIL_IF(push_store_zero_flag(compiler, op, reg));
			}
			return SLJIT_SUCCESS;
		}
		/* STORE and STORE IMMEDIATE */
		if ((dst & SLJIT_MEM) &&
			(FAST_IS_REG(src) || (src & SLJIT_IMM))) {
			sljit_gpr reg = FAST_IS_REG(src) ? gpr(src) : tmp0;
			if (src & SLJIT_IMM) {
				/* TODO(mundaym): MOVE IMMEDIATE? */
				FAIL_IF(push_load_imm_inst(compiler, reg, srcw));
			}
			struct addr mem;
			FAIL_IF(make_addr_bxy(compiler, &mem, dst, dstw, tmp1));
			switch (opcode) {
			case SLJIT_MOV_U8:
			case SLJIT_MOV_S8:
				return push_inst(compiler, is_u12(mem.offset) ?
					stc(reg, mem.offset, mem.index, mem.base) :
					stcy(reg, mem.offset, mem.index, mem.base));
			case SLJIT_MOV_U16:
			case SLJIT_MOV_S16:
				return push_inst(compiler, is_u12(mem.offset) ?
					sth(reg, mem.offset, mem.index, mem.base) :
					sthy(reg, mem.offset, mem.index, mem.base));
			case SLJIT_MOV_U32:
			case SLJIT_MOV_S32:
				return push_inst(compiler, is_u12(mem.offset) ?
					st(reg, mem.offset, mem.index, mem.base) :
					sty(reg, mem.offset, mem.index, mem.base));
			case SLJIT_MOV_P:
			case SLJIT_MOV:
				FAIL_IF(push_inst(compiler,
					stg(reg, mem.offset, mem.index, mem.base)));
				if (HAS_FLAGS(op)) {
					/* only handle zero flag */
					SLJIT_ASSERT(!(op & VARIABLE_FLAG_MASK));
					FAIL_IF(push_store_zero_flag(compiler, op, reg));
				}
				return SLJIT_SUCCESS;
			default:
				SLJIT_UNREACHABLE();
			}
		}
		/* MOVE CHARACTERS */
		if ((dst & SLJIT_MEM) && (src & SLJIT_MEM)) {
			struct addr mem;
			FAIL_IF(make_addr_bxy(compiler, &mem, src, srcw, tmp1));
			switch (opcode) {
			case SLJIT_MOV_U8:
			case SLJIT_MOV_S8:
				FAIL_IF(push_inst(compiler,
					llgc(tmp0, mem.offset, mem.index, mem.base)));
				FAIL_IF(make_addr_bxy(compiler, &mem, dst, dstw, tmp1));
				return push_inst(compiler,
					stcy(tmp0, mem.offset, mem.index, mem.base));
			case SLJIT_MOV_U16:
			case SLJIT_MOV_S16:
				FAIL_IF(push_inst(compiler,
					llgh(tmp0, mem.offset, mem.index, mem.base)));
				FAIL_IF(make_addr_bxy(compiler, &mem, dst, dstw, tmp1));
				return push_inst(compiler,
					sthy(tmp0, mem.offset, mem.index, mem.base));
			case SLJIT_MOV_U32:
			case SLJIT_MOV_S32:
				FAIL_IF(push_inst(compiler,
					ly(tmp0, mem.offset, mem.index, mem.base)));
				FAIL_IF(make_addr_bxy(compiler, &mem, dst, dstw, tmp1));
				return push_inst(compiler,
					sty(tmp0, mem.offset, mem.index, mem.base));
			case SLJIT_MOV_P:
			case SLJIT_MOV:
				FAIL_IF(push_inst(compiler,
					lg(tmp0, mem.offset, mem.index, mem.base)));
				FAIL_IF(make_addr_bxy(compiler, &mem, dst, dstw, tmp1));
				FAIL_IF(push_inst(compiler,
					stg(tmp0, mem.offset, mem.index, mem.base)));
				if (HAS_FLAGS(op)) {
					/* only handle zero flag */
					SLJIT_ASSERT(!(op & VARIABLE_FLAG_MASK));
					FAIL_IF(push_store_zero_flag(compiler, op, tmp0));
				}
				return SLJIT_SUCCESS;
			default:
				SLJIT_UNREACHABLE();
			}
		}
		abort();
	}

	SLJIT_ASSERT((src & SLJIT_IMM) == 0); /* no immediates */

	sljit_gpr dst_r = SLOW_IS_REG(dst) ? gpr(REG_MASK & dst) : tmp0;
	sljit_gpr src_r = FAST_IS_REG(src) ? gpr(REG_MASK & src) : tmp0;
	if (src & SLJIT_MEM) {
		FAIL_IF(load_word(compiler, src_r, src, srcw, tmp1, src & SLJIT_I32_OP));
	}

	/* TODO(mundaym): optimize loads and stores */
	switch (opcode | (op & SLJIT_I32_OP)) {
	case SLJIT_NOT:
		/* emulate ~x with x^-1 */
		FAIL_IF(push_load_imm_inst(compiler, tmp1, -1));
		if (src_r != dst_r) {
			FAIL_IF(push_inst(compiler, lgr(dst_r, src_r)));
		}
		FAIL_IF(push_inst(compiler, xgr(dst_r, tmp1)));
		break;
	case SLJIT_NOT32:
		/* emulate ~x with x^-1 */
		if (have_eimm()) {
			FAIL_IF(push_inst(compiler, xilf(dst_r, -1)));
		} else {
			FAIL_IF(push_load_imm_inst(compiler, tmp1, -1));
			if (src_r != dst_r) {
				FAIL_IF(push_inst(compiler, lr(dst_r, src_r)));
			}
			FAIL_IF(push_inst(compiler, xr(dst_r, tmp1)));
		}
		break;
	case SLJIT_NEG:
		FAIL_IF(push_inst(compiler, lcgr(dst_r, src_r)));
		break;
	case SLJIT_NEG32:
		FAIL_IF(push_inst(compiler, lcr(dst_r, src_r)));
		break;
	case SLJIT_CLZ:
		if (have_eimm()) {
			FAIL_IF(push_inst(compiler, flogr(tmp0, src_r))); /* clobbers tmp1 */
			if (dst_r != tmp0) {
				FAIL_IF(push_inst(compiler, lgr(dst_r, tmp0)));
			}
		} else {
			abort(); /* TODO(mundaym): no eimm (?) */
		}
		break;
	case SLJIT_CLZ32:
		if (have_eimm()) {
			FAIL_IF(push_inst(compiler, sllg(tmp1, src_r, 32, 0)));
			FAIL_IF(push_inst(compiler, iilf(tmp1, 0xffffffff)));
			FAIL_IF(push_inst(compiler, flogr(tmp0, tmp1))); /* clobbers tmp1 */
			if (dst_r != tmp0) {
				FAIL_IF(push_inst(compiler, lr(dst_r, tmp0)));
			}
		} else {
			abort(); /* TODO(mundaym): no eimm (?) */
		}
		break;
	default:
		SLJIT_UNREACHABLE();
	}

	/* write condition code to emulated flag register */
	if (op & VARIABLE_FLAG_MASK) {
		FAIL_IF(push_inst(compiler, ipm(flag_r)));
	}

	/* write zero flag to emulated flag register */
	if (op & SLJIT_SET_Z) {
		FAIL_IF(push_store_zero_flag(compiler, op, dst_r));
	}

	if ((dst != SLJIT_UNUSED) && (dst & SLJIT_MEM)) {
		FAIL_IF(store_word(compiler, dst_r, dst, dstw, tmp1, op & SLJIT_I32_OP));
	}

	return SLJIT_SUCCESS;
}

static int is_commutative(sljit_s32 op)
{
	switch (GET_OPCODE(op)) {
	case SLJIT_ADD:
	case SLJIT_ADDC:
	case SLJIT_MUL:
	case SLJIT_AND:
	case SLJIT_OR:
	case SLJIT_XOR:
		return 1;
	}
	return 0;
}

static int is_shift(sljit_s32 op) {
	sljit_s32 v = GET_OPCODE(op);
	return v == SLJIT_SHL ||
		v == SLJIT_ASHR ||
		v == SLJIT_LSHR
		? 1 : 0;
}

static int sets_signed_flag(sljit_s32 op)
{
	switch (GET_FLAG_TYPE(op)) {
	case SLJIT_OVERFLOW:
	case SLJIT_NOT_OVERFLOW:
	case SLJIT_SIG_LESS:
	case SLJIT_SIG_LESS_EQUAL:
	case SLJIT_SIG_GREATER:
	case SLJIT_SIG_GREATER_EQUAL:
		return 1;
	}
	return 0;
}

/* Report whether we have an instruction for:
     op dst src imm
   where dst and src are separate registers. */
static int have_op_3_imm(sljit_s32 op, sljit_sw imm) {
	return 0; /* TODO(mundaym): implement */
}

/* Report whether we have an instruction for:
     op reg imm
  where reg is both a source and the destination. */
static int have_op_2_imm(sljit_s32 op, sljit_sw imm) {
	switch (GET_OPCODE(op) | (op & SLJIT_I32_OP)) {
	case SLJIT_ADD32:
	case SLJIT_ADD:
		if (!HAS_FLAGS(op) || sets_signed_flag(op)) {
			return have_eimm() ? is_s32(imm) : is_s16(imm);
		}
		return have_eimm() && is_u32(imm);
	case SLJIT_MUL32:
	case SLJIT_MUL:
		/* TODO(mundaym): general extension check */
		/* for ms{,g}fi */
		if (op & VARIABLE_FLAG_MASK) {
			return 0;
		}
		return have_genext() && is_s16(imm);
	case SLJIT_OR32:
	case SLJIT_XOR32:
	case SLJIT_AND32:
		/* only use if have extended immediate facility */
		/* this ensures flags are set correctly */
		return have_eimm();
	case SLJIT_AND:
	case SLJIT_OR:
	case SLJIT_XOR:
		/* TODO(mundaym): make this more flexible */
		/* avoid using immediate variations, flags */
		/* won't be set correctly */
		return 0;
	case SLJIT_ADDC32:
	case SLJIT_ADDC:
		/* no ADD LOGICAL WITH CARRY IMMEDIATE */
		return 0;
	case SLJIT_SUB:
	case SLJIT_SUB32:
	case SLJIT_SUBC:
	case SLJIT_SUBC32:
		/* no SUBTRACT IMMEDIATE */
		/* TODO(mundaym): SUBTRACT LOGICAL IMMEDIATE */
		return 0;
	}
	return 0;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_op2(struct sljit_compiler *compiler, sljit_s32 op,
	sljit_s32 dst, sljit_sw dstw,
	sljit_s32 src1, sljit_sw src1w,
	sljit_s32 src2, sljit_sw src2w)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_op2(compiler, op, dst, dstw, src1, src1w, src2, src2w));
	ADJUST_LOCAL_OFFSET(dst, dstw);
	ADJUST_LOCAL_OFFSET(src1, src1w);
	ADJUST_LOCAL_OFFSET(src2, src2w);

	if (dst == SLJIT_UNUSED && !HAS_FLAGS(op))
		return SLJIT_SUCCESS;

	sljit_gpr dst_r = SLOW_IS_REG(dst) ? gpr(dst & REG_MASK) : tmp0;

	if (is_commutative(op)) {
		#define SWAP_ARGS \
		do {                         \
			sljit_s32 t = src1;  \
			sljit_sw tw = src1w; \
			src1 = src2;         \
			src1w = src2w;       \
			src2 = t;            \
			src2w = tw;          \
		} while(0);

		/* prefer immediate in src2 */
		if (src1 & SLJIT_IMM) {
			SWAP_ARGS
		}

		/* prefer to have src1 use same register as dst */
		if (FAST_IS_REG(src2) && gpr(src2 & REG_MASK) == dst_r) {
			SWAP_ARGS
		}

		/* prefer memory argument in src2 */
		if (FAST_IS_REG(src2) && (src1 & SLJIT_MEM)) {
			SWAP_ARGS
		}
	}

	/* src1 must be in a register */
	sljit_gpr src1_r = FAST_IS_REG(src1) ? gpr(src1 & REG_MASK) : tmp0;
	if (src1 & SLJIT_IMM) {
		FAIL_IF(push_load_imm_inst(compiler, src1_r, src1w));
	}
	if (src1 & SLJIT_MEM) {
		FAIL_IF(load_word(compiler, src1_r, src1, src1w, tmp1, op & SLJIT_I32_OP));
	}

	/* emit comparison before subtract */
	if (GET_OPCODE(op) == SLJIT_SUB && (op & VARIABLE_FLAG_MASK)) {
		sljit_sw cmp = 0;
		switch (GET_FLAG_TYPE(op)) {
		case SLJIT_LESS:
		case SLJIT_LESS_EQUAL:
		case SLJIT_GREATER:
		case SLJIT_GREATER_EQUAL:
			cmp = 1; /* unsigned */
			break;
		case SLJIT_EQUAL:
		case SLJIT_SIG_LESS:
		case SLJIT_SIG_LESS_EQUAL:
		case SLJIT_SIG_GREATER:
		case SLJIT_SIG_GREATER_EQUAL:
			cmp = -1; /* signed */
			break;
		}
		if (cmp != 0) {
			/* clear flags - no need to generate now */
			op &= ~VARIABLE_FLAG_MASK;
			sljit_gpr src2_r = FAST_IS_REG(src2) ? gpr(src2 & REG_MASK) : tmp1;
			if (src2 & SLJIT_IMM) {
				if (cmp > 0 && is_u32(src2w)) {
					/* unsigned */
					FAIL_IF(push_inst(compiler, (op & SLJIT_I32_OP) ?
						clfi(src1_r, src2w) :
						clgfi(src1_r, src2w)));
				} else if (cmp < 0 && is_s16(src2w)) {
					/* signed */
					FAIL_IF(push_inst(compiler, (op & SLJIT_I32_OP) ?
						chi(src1_r, src2w) :
						cghi(src1_r, src2w)));
				} else if (cmp < 0 && is_s32(src2w)) {
					/* signed */
					FAIL_IF(push_inst(compiler, (op & SLJIT_I32_OP) ?
						cfi(src1_r, src2w) :
						cgfi(src1_r, src2w)));
				} else {
					FAIL_IF(push_load_imm_inst(compiler, src2_r, src2w));
					if (cmp > 0) {
						/* unsigned */
						FAIL_IF(push_inst(compiler, (op & SLJIT_I32_OP) ?
							clr(src1_r, src2_r) :
							clgr(src1_r, src2_r)));
					}
					if (cmp < 0) {
						/* signed */
						FAIL_IF(push_inst(compiler, (op & SLJIT_I32_OP) ?
							cr(src1_r, src2_r) :
							cgr(src1_r, src2_r)));
					}
				}
			} else {
				if (src2 & SLJIT_MEM) {
					/* TODO(mundaym): comparisons with memory */
					/* load src2 into register */
					FAIL_IF(load_word(compiler, src2_r, src2, src2w, tmp1, op & SLJIT_I32_OP));
				}
				if (cmp > 0) {
					/* unsigned */
					FAIL_IF(push_inst(compiler, (op & SLJIT_I32_OP) ?
						clr(src1_r, src2_r) :
						clgr(src1_r, src2_r)));
				}
				if (cmp < 0) {
					/* signed */
					FAIL_IF(push_inst(compiler, (op & SLJIT_I32_OP) ?
						cr(src1_r, src2_r) :
						cgr(src1_r, src2_r)));
				}
			}
			FAIL_IF(push_inst(compiler, ipm(flag_r)));
		}
	}

	if (!HAS_FLAGS(op) && dst == SLJIT_UNUSED) {
		return SLJIT_SUCCESS;
	}

	/* need to specify signed or logical operation */
	int signed_flags = sets_signed_flag(op);

	if (is_shift(op)) {
		/* handle shifts first, they have more constraints than other operations */
		sljit_sw d = 0;
		sljit_gpr b = FAST_IS_REG(src2) ? gpr(src2 & REG_MASK) : r0;
		if (src2 & SLJIT_IMM) {
			d = src2w & ((op & SLJIT_I32_OP) ? 31 : 63);
		}
		if (src2 & SLJIT_MEM) {
			/* shift amount (b) cannot be in r0 (i.e. tmp0) */
			FAIL_IF(load_word(compiler, tmp1, src2, src2w, tmp1, op & SLJIT_I32_OP));
			b = tmp1;
		}
		/* src1 and dst share the same register in the base 32-bit ISA */
		/* TODO(mundaym): not needed when distinct-operand facility is available */
		int workaround_alias = op & SLJIT_I32_OP && src1_r != dst_r;
		if (workaround_alias) {
			/* put src1 into tmp0 so we can overwrite it */
			FAIL_IF(push_inst(compiler, lr(tmp0, src1_r)));
			src1_r = tmp0;
		}
		switch (GET_OPCODE(op) | (op & SLJIT_I32_OP)) {
		case SLJIT_SHL:
			FAIL_IF(push_inst(compiler, sllg(dst_r, src1_r, d, b)));
			break;
		case SLJIT_SHL32:
			FAIL_IF(push_inst(compiler, sll(src1_r, d, b)));
			break;
		case SLJIT_LSHR:
			FAIL_IF(push_inst(compiler, srlg(dst_r, src1_r, d, b)));
			break;
		case SLJIT_LSHR32:
			FAIL_IF(push_inst(compiler, srl(src1_r, d, b)));
			break;
		case SLJIT_ASHR:
			FAIL_IF(push_inst(compiler, srag(dst_r, src1_r, d, b)));
			break;
		case SLJIT_ASHR32:
			FAIL_IF(push_inst(compiler, sra(src1_r, d, b)));
			break;
		default:
			SLJIT_UNREACHABLE();
		}
		if (workaround_alias && dst_r != src1_r) {
			FAIL_IF(push_inst(compiler, lr(dst_r, src1_r)));
		}
	} else if ((GET_OPCODE(op) == SLJIT_MUL) && HAS_FLAGS(op)) {
		/* multiply instructions do not generally set flags so we need to manually */
		/* detect overflow conditions */
		/* TODO(mundaym): 64-bit overflow */
		SLJIT_ASSERT(GET_FLAG_TYPE(op) == SLJIT_MUL_OVERFLOW ||
		             GET_FLAG_TYPE(op) == SLJIT_MUL_NOT_OVERFLOW);
		sljit_gpr src2_r = FAST_IS_REG(src2) ? gpr(src2 & REG_MASK) : tmp1;
		if (src2 & SLJIT_IMM) {
			/* load src2 into register */
			FAIL_IF(push_load_imm_inst(compiler, src2_r, src2w));
		}
		if (src2 & SLJIT_MEM) {
			/* load src2 into register */
			FAIL_IF(load_word(compiler, src2_r, src2, src2w, tmp1, op & SLJIT_I32_OP));
		}
		if (have_misc2()) {
			FAIL_IF(push_inst(compiler, op & SLJIT_I32_OP ?
			        msrkc(dst_r, src1_r, src2_r) :
				msgrkc(dst_r, src1_r, src2_r)));
		} else if (op & SLJIT_I32_OP) {
			op &= ~VARIABLE_FLAG_MASK;
			FAIL_IF(push_inst(compiler, lgfr(tmp0, src1_r)));
			FAIL_IF(push_inst(compiler, msgfr(tmp0, src2_r)));
			if (dst_r != tmp0) {
				FAIL_IF(push_inst(compiler, lr(dst_r, tmp0)));
			}
			FAIL_IF(push_inst(compiler, aih(tmp0, 1)));
			FAIL_IF(push_inst(compiler, nihf(tmp0, ~1U)));
			FAIL_IF(push_inst(compiler, ipm(flag_r)));
			FAIL_IF(push_inst(compiler, oilh(flag_r, 0x2000)));
		} else {
			return SLJIT_ERR_UNSUPPORTED;
		}
	} else if ((GET_OPCODE(op) == SLJIT_SUB) && (op & SLJIT_SET_Z) && !signed_flags) {
		/* subtract logical instructions do not set the right flags unfortunately */
		/* instead, negate src2 and issue an add logical */
		/* TODO(mundaym): distinct operand facility where needed */
		if (src1_r != dst_r && src1_r != tmp0) {
			FAIL_IF(push_inst(compiler, (op & SLJIT_I32_OP) ?
				lr(tmp0, src1_r) : lgr(tmp0, src1_r)));
			src1_r = tmp0;
		}
		sljit_gpr src2_r = FAST_IS_REG(src2) ? gpr(src2 & REG_MASK) : tmp1;
		if (src2 & SLJIT_IMM) {
			/* load src2 into register */
			FAIL_IF(push_load_imm_inst(compiler, src2_r, src2w));
		}
		if (src2 & SLJIT_MEM) {
			/* load src2 into register */
			FAIL_IF(load_word(compiler, src2_r, src2, src2w, tmp1, op & SLJIT_I32_OP));
		}
		FAIL_IF(push_inst(compiler, (op & SLJIT_I32_OP) ?
			lcr(tmp1, src2_r) :
			lcgr(tmp1, src2_r)));
		FAIL_IF(push_inst(compiler, (op & SLJIT_I32_OP) ?
			alr(src1_r, tmp1) :
			algr(src1_r, tmp1)));
		if (src1_r != dst_r) {
			FAIL_IF(push_inst(compiler, (op & SLJIT_I32_OP) ?
				lr(dst_r, src1_r) : lgr(dst_r, src1_r)));
		}
	} else if ((src2 & SLJIT_IMM) && (src1_r == dst_r) && have_op_2_imm(op, src2w)) {
		switch (GET_OPCODE(op) | (op & SLJIT_I32_OP)) {
		case SLJIT_ADD:
			if (!HAS_FLAGS(op) || signed_flags) {
				FAIL_IF(push_inst(compiler, is_s16(src2w) ?
					aghi(dst_r, src2w) :
					agfi(dst_r, src2w)
				));
			} else {
				FAIL_IF(push_inst(compiler, algfi(dst_r, src2w)));
			}
			break;
		case SLJIT_ADD32:
			if (!HAS_FLAGS(op) || signed_flags) {
				FAIL_IF(push_inst(compiler, is_s16(src2w) ?
					ahi(dst_r, src2w) :
					afi(dst_r, src2w)
				));
			} else {
				FAIL_IF(push_inst(compiler, alfi(dst_r, src2w)));
			}
			break;
		case SLJIT_MUL:
			FAIL_IF(push_inst(compiler, mhi(dst_r, src2w)));
			break;
		case SLJIT_MUL32:
			FAIL_IF(push_inst(compiler, mghi(dst_r, src2w)));
			break;
		case SLJIT_OR32:
			FAIL_IF(push_inst(compiler, oilf(dst_r, src2w)));
			break;
		case SLJIT_XOR32:
			FAIL_IF(push_inst(compiler, xilf(dst_r, src2w)));
			break;
		case SLJIT_AND32:
			FAIL_IF(push_inst(compiler, nilf(dst_r, src2w)));
			break;
		default:
			SLJIT_UNREACHABLE();
		}
	} else if ((src2 & SLJIT_IMM) && have_op_3_imm(op, src2w)) {
		abort(); /* TODO(mundaym): implement */
	} else if ((src2 & SLJIT_MEM) && (dst_r == src1_r)) {
		/* most 32-bit instructions can only handle 12-bit immediate offsets */
		int need_u12 = !have_ldisp() &&
			(op & SLJIT_I32_OP) &&
			(GET_OPCODE(op) != SLJIT_ADDC) &&
			(GET_OPCODE(op) != SLJIT_SUBC);
		struct addr mem;
		if (need_u12) {
			FAIL_IF(make_addr_bx(compiler, &mem, src2, src2w, tmp1));
		} else {
			FAIL_IF(make_addr_bxy(compiler, &mem, src2, src2w, tmp1));
		}

		int can_u12 = is_u12(mem.offset) ? 1 : 0;
		sljit_ins ins = 0;
		switch (GET_OPCODE(op) | (op & SLJIT_I32_OP)) {
		/* 64-bit ops */
#define EVAL(op) op(dst_r, mem.offset, mem.index, mem.base)
		case SLJIT_ADD:  ins = signed_flags ? EVAL(ag) : EVAL(alg); break;
		case SLJIT_SUB:  ins = signed_flags ? EVAL(sg) : EVAL(slg); break;
		case SLJIT_ADDC: ins = EVAL(alcg); break;
		case SLJIT_SUBC: ins = EVAL(slbg); break;
		case SLJIT_MUL:  ins = EVAL( msg); break;
		case SLJIT_OR:   ins = EVAL(  og); break;
		case SLJIT_XOR:  ins = EVAL(  xg); break;
		case SLJIT_AND:  ins = EVAL(  ng); break;
		/* 32-bit ops */
		case SLJIT_ADD32: ins = signed_flags ?
			(can_u12 ? EVAL( a) : EVAL( ay)) :
			(can_u12 ? EVAL(al) : EVAL(aly));
			break;
		case SLJIT_SUB32: ins = signed_flags ?
			(can_u12 ? EVAL( s) : EVAL( sy)) :
			(can_u12 ? EVAL(sl) : EVAL(sly));
			break;
		case SLJIT_ADDC32: ins = EVAL(alc); break;
		case SLJIT_SUBC32: ins = EVAL(slb); break;
		case SLJIT_MUL32:  ins = can_u12 ? EVAL(ms) : EVAL(msy); break;
		case SLJIT_OR32:   ins = can_u12 ? EVAL( o) : EVAL( oy); break;
		case SLJIT_XOR32:  ins = can_u12 ? EVAL( x) : EVAL( xy); break;
		case SLJIT_AND32:  ins = can_u12 ? EVAL( n) : EVAL( ny); break;
#undef EVAL
		default:
			SLJIT_UNREACHABLE();
		}
		FAIL_IF(push_inst(compiler, ins));
	} else {
		sljit_gpr src2_r = FAST_IS_REG(src2) ? gpr(src2 & REG_MASK) : tmp1;
		if (src2 & SLJIT_IMM) {
			/* load src2 into register */
			FAIL_IF(push_load_imm_inst(compiler, src2_r, src2w));
		}
		if (src2 & SLJIT_MEM) {
			/* load src2 into register */
			FAIL_IF(load_word(compiler, src2_r, src2, src2w, tmp1, op & SLJIT_I32_OP));
		}
		/* TODO(mundaym): distinct operand facility where needed */
		if (src1_r != dst_r && src1_r != tmp0) {
			FAIL_IF(push_inst(compiler, (op & SLJIT_I32_OP) ?
				lr(tmp0, src1_r) : lgr(tmp0, src1_r)));
			src1_r = tmp0;
		}
		sljit_ins ins = 0;
		switch (GET_OPCODE(op) | (op & SLJIT_I32_OP)) {
		/* 64-bit ops */
		case SLJIT_ADD: ins = signed_flags ?
			agr(src1_r, src2_r) :
			algr(src1_r, src2_r);
			break;
		case SLJIT_SUB: ins = signed_flags ?
			sgr(src1_r, src2_r) :
			slgr(src1_r, src2_r);
			break;
		case SLJIT_ADDC: ins = alcgr(src1_r, src2_r); break;
		case SLJIT_SUBC: ins = slbgr(src1_r, src2_r); break;
		case SLJIT_MUL:  ins =  msgr(src1_r, src2_r); break;
		case SLJIT_AND:  ins =   ngr(src1_r, src2_r); break;
		case SLJIT_OR:   ins =   ogr(src1_r, src2_r); break;
		case SLJIT_XOR:  ins =   xgr(src1_r, src2_r); break;
		/* 32-bit ops */
		case SLJIT_ADD32: ins = signed_flags ?
			ar(src1_r, src2_r) :
			alr(src1_r, src2_r);
			break;
		case SLJIT_SUB32: ins = signed_flags ?
			sr(src1_r, src2_r) :
			slr(src1_r, src2_r);
			break;
		case SLJIT_ADDC32: ins = alcr(src1_r, src2_r); break;
		case SLJIT_SUBC32: ins = slbr(src1_r, src2_r); break;
		case SLJIT_MUL32:  ins =  msr(src1_r, src2_r); break;
		case SLJIT_AND32:  ins =   nr(src1_r, src2_r); break;
		case SLJIT_OR32:   ins =   or(src1_r, src2_r); break;
		case SLJIT_XOR32:  ins =   xr(src1_r, src2_r); break;
		default:
			SLJIT_UNREACHABLE();
		}
		FAIL_IF(push_inst(compiler, ins));
		if (src1_r != dst_r) {
			FAIL_IF(push_inst(compiler, (op & SLJIT_I32_OP) ?
				lr(dst_r, src1_r) : lgr(dst_r, src1_r)));
		}
	}

	/* write condition code to emulated flag register */
	if (op & VARIABLE_FLAG_MASK) {
		FAIL_IF(push_inst(compiler, ipm(flag_r)));
	}

	/* write zero flag to emulated flag register */
	if (op & SLJIT_SET_Z) {
		FAIL_IF(push_store_zero_flag(compiler, op, dst_r));
	}

	/* finally write the result to memory if required */
	if (dst & SLJIT_MEM) {
		SLJIT_ASSERT(dst_r != tmp1);
		FAIL_IF(store_word(compiler, dst_r, dst, dstw, tmp1, op & SLJIT_I32_OP));
	}

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_get_register_index(sljit_s32 reg)
{
	CHECK_REG_INDEX(check_sljit_get_register_index(reg));
	return gpr(reg);
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_get_float_register_index(sljit_s32 reg)
{
	abort();
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_op_custom(struct sljit_compiler *compiler,
	void *instruction, sljit_s32 size)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_op_custom(compiler, instruction, size));

	sljit_u8 *src = (sljit_u8*)instruction;
	sljit_ins ins = 0;
	switch (size) {
	case 2:
		ins |= (sljit_ins)(src[0]) << 8;
		ins |= (sljit_ins)(src[1]);
		break;
	case 4:
		ins |= (sljit_ins)(src[0]) << 24;
		ins |= (sljit_ins)(src[1]) << 16;
		ins |= (sljit_ins)(src[2]) << 8;
		ins |= (sljit_ins)(src[3]);
		break;
	case 6:
		ins |= (sljit_ins)(src[0]) << 40;
		ins |= (sljit_ins)(src[1]) << 32;
		ins |= (sljit_ins)(src[2]) << 24;
		ins |= (sljit_ins)(src[3]) << 16;
		ins |= (sljit_ins)(src[4]) << 8;
		ins |= (sljit_ins)(src[5]);
		break;
	default:
		return SLJIT_ERR_UNSUPPORTED;
	}
	return push_inst(compiler, ins);
}

/* --------------------------------------------------------------------- */
/*  Floating point operators                                             */
/* --------------------------------------------------------------------- */

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_fop1(struct sljit_compiler *compiler, sljit_s32 op,
	sljit_s32 dst, sljit_sw dstw,
	sljit_s32 src, sljit_sw srcw)
{
	CHECK_ERROR();
	abort();
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_fop2(struct sljit_compiler *compiler, sljit_s32 op,
	sljit_s32 dst, sljit_sw dstw,
	sljit_s32 src1, sljit_sw src1w,
	sljit_s32 src2, sljit_sw src2w)
{
	CHECK_ERROR();
	abort();
}

/* --------------------------------------------------------------------- */
/*  Other instructions                                                   */
/* --------------------------------------------------------------------- */

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_fast_enter(struct sljit_compiler *compiler, sljit_s32 dst, sljit_sw dstw)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_fast_enter(compiler, dst, dstw));
	ADJUST_LOCAL_OFFSET(dst, dstw);

	if (FAST_IS_REG(dst))
		return push_inst(compiler, lgr(gpr(dst), fast_link_r));

	/* memory */
	return store_word(compiler, fast_link_r, dst, dstw, tmp1, 0);
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_fast_return(struct sljit_compiler *compiler, sljit_s32 src, sljit_sw srcw)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_fast_return(compiler, src, srcw));
	ADJUST_LOCAL_OFFSET(src, srcw);

	sljit_gpr src_r = FAST_IS_REG(src) ? gpr(src) : tmp1;
	if (src & SLJIT_MEM) {
	       FAIL_IF(load_word(compiler, tmp1, src, srcw, tmp1, 0));
	}

	return push_inst(compiler, br(src_r));
}

/* --------------------------------------------------------------------- */
/*  Conditional instructions                                             */
/* --------------------------------------------------------------------- */

SLJIT_API_FUNC_ATTRIBUTE struct sljit_label* sljit_emit_label(struct sljit_compiler *compiler)
{
	struct sljit_label *label;

	CHECK_ERROR_PTR();
	CHECK_PTR(check_sljit_emit_label(compiler));

	if (compiler->last_label && compiler->last_label->size == compiler->size)
		return compiler->last_label;

	label = (struct sljit_label*)ensure_abuf(compiler, sizeof(struct sljit_label));
	PTR_FAIL_IF(!label);
	set_label(label, compiler);
	return label;
}

SLJIT_API_FUNC_ATTRIBUTE struct sljit_jump* sljit_emit_jump(struct sljit_compiler *compiler, sljit_s32 type)
{
	CHECK_ERROR_PTR();
	CHECK_PTR(check_sljit_emit_jump(compiler, type));

	/* reload condition code */
	sljit_uw mask = ((type&0xff) < SLJIT_JUMP) ? get_cc(type&0xff) : 0xf;
	if (mask != 0xf) {
		PTR_FAIL_IF(push_load_cc(compiler, type&0xff));
	}

	/* record jump */
	struct sljit_jump *jump = (struct sljit_jump *)
		ensure_abuf(compiler, sizeof(struct sljit_jump));
	PTR_FAIL_IF(!jump);
	set_jump(jump, compiler, type & SLJIT_REWRITABLE_JUMP);
	jump->addr = compiler->size;

	/* emit jump instruction */
	type &= 0xff;
	if (type >= SLJIT_FAST_CALL) {
		PTR_FAIL_IF(push_inst(compiler, brasl(type == SLJIT_FAST_CALL ? fast_link_r : link_r, 0)));
	} else {
		PTR_FAIL_IF(push_inst(compiler, brcl(mask, 0)));
	}

	return jump;
}

SLJIT_API_FUNC_ATTRIBUTE struct sljit_jump* sljit_emit_call(struct sljit_compiler *compiler, sljit_s32 type,
	sljit_s32 arg_types)
{
	CHECK_ERROR_PTR();
	CHECK_PTR(check_sljit_emit_call(compiler, type, arg_types));

#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE) \
		|| (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	compiler->skip_checks = 1;
#endif

	return sljit_emit_jump(compiler, type);
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_ijump(struct sljit_compiler *compiler, sljit_s32 type, sljit_s32 src, sljit_sw srcw)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_ijump(compiler, type, src, srcw));
	ADJUST_LOCAL_OFFSET(src, srcw);

	sljit_gpr src_r = FAST_IS_REG(src) ? gpr(src) : tmp1;
	if (src & SLJIT_IMM) {
		SLJIT_ASSERT(!(srcw & 1)); /* target address must be even */
		FAIL_IF(push_load_imm_inst(compiler, src_r, srcw));
	} else if (src & SLJIT_MEM) {
		FAIL_IF(load_word(compiler, src_r, src, srcw, tmp1, 0 /* 64-bit */));
	}

	/* emit jump instruction */
	if (type >= SLJIT_FAST_CALL) {
		return push_inst(compiler, basr(type == SLJIT_FAST_CALL ? fast_link_r : link_r, src_r));
	}
	return push_inst(compiler, br(src_r));
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_icall(struct sljit_compiler *compiler, sljit_s32 type,
	sljit_s32 arg_types,
	sljit_s32 src, sljit_sw srcw)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_icall(compiler, type, arg_types, src, srcw));

#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE) \
		|| (defined SLJIT_ARGUMENT_CHECKS && SLJIT_ARGUMENT_CHECKS)
	compiler->skip_checks = 1;
#endif

	return sljit_emit_ijump(compiler, type, src, srcw);
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_op_flags(struct sljit_compiler *compiler, sljit_s32 op,
	sljit_s32 dst, sljit_sw dstw,
	sljit_s32 type)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_op_flags(compiler, op, dst, dstw, type));

	sljit_gpr dst_r = FAST_IS_REG(dst) ? gpr(dst & REG_MASK) : tmp0;
	sljit_gpr loc_r = tmp1;
	switch (GET_OPCODE(op)) {
	case SLJIT_AND:
	case SLJIT_OR:
	case SLJIT_XOR:
		/* dst is also source operand */
		if (dst & SLJIT_MEM) {
			FAIL_IF(load_word(compiler, dst_r, dst, dstw, tmp1, op & SLJIT_I32_OP));
		}
		break;
	case SLJIT_MOV:
	case (SLJIT_MOV32 & ~SLJIT_I32_OP):
		/* can write straight into destination */
		loc_r = dst_r;
		break;
	default:
		SLJIT_UNREACHABLE();
	}

	sljit_uw mask = get_cc(type & 0xff);
	if (mask != 0xf) {
		FAIL_IF(push_load_cc(compiler, type & 0xff));
	}
	/* TODO(mundaym): fold into cmov helper function? */
	if (have_lscond2()) {
		FAIL_IF(push_load_imm_inst(compiler, loc_r, 0));
		FAIL_IF(push_inst(compiler, (op & SLJIT_I32_OP) ?
			lochi(loc_r, 1, mask) :
			locghi(loc_r, 1, mask)));
	} else {
		/* TODO(mundaym): no load/store-on-condition 2 facility (ipm? branch-and-set?) */
		abort();
	}

	/* apply bitwise op and set condition codes */
	switch (GET_OPCODE(op)) {
	case SLJIT_AND:
		FAIL_IF(push_inst(compiler, (op & SLJIT_I32_OP) ?
			nr(dst_r, loc_r) :
			ngr(dst_r, loc_r)));
		break;
	case SLJIT_OR:
		FAIL_IF(push_inst(compiler, (op & SLJIT_I32_OP) ?
			or(dst_r, loc_r) :
			ogr(dst_r, loc_r)));
		break;
	case SLJIT_XOR:
		FAIL_IF(push_inst(compiler, (op & SLJIT_I32_OP) ?
			xr(dst_r, loc_r) :
			xgr(dst_r, loc_r)));
		break;
	}

	/* set zero flag if needed */
	if (op & SLJIT_SET_Z) {
		FAIL_IF(push_store_zero_flag(compiler, op, dst_r));
	}

	/* store result to memory if required */
	if (dst & SLJIT_MEM) {
		FAIL_IF(store_word(compiler, dst_r, dst, dstw, tmp1, op & SLJIT_I32_OP));
	}

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_cmov(struct sljit_compiler *compiler, sljit_s32 type,
	sljit_s32 dst_reg,
	sljit_s32 src, sljit_sw srcw)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_cmov(compiler, type, dst_reg, src, srcw));

	sljit_uw mask = get_cc(type & 0xff);
	if (mask != 0xf) {
		FAIL_IF(push_load_cc(compiler, type & 0xff));
	}

	sljit_gpr dst_r = gpr(dst_reg & ~SLJIT_I32_OP);
	sljit_gpr src_r = FAST_IS_REG(src) ? gpr(src) : tmp0;
	if (src & SLJIT_IMM) {
		/* TODO(mundaym): fast path with lscond2 */
		FAIL_IF(push_load_imm_inst(compiler, src_r, srcw));
	}

	if (have_lscond1()) {
		return push_inst(compiler, dst_reg & SLJIT_I32_OP ?
			locr(dst_r, src_r, mask) :
			locgr(dst_r, src_r, mask));
	}

	/* TODO(mundaym): implement */
	return SLJIT_ERR_UNSUPPORTED;
}

/* --------------------------------------------------------------------- */
/*  Other instructions                                                   */
/* --------------------------------------------------------------------- */

/* On s390x we build a literal pool to hold constants. This has two main
   advantages:

     1. we only need one instruction in the instruction stream (LGRL)
     2. we don't need to worry about flushing the instruction cache
        when updating constants

   To retrofit the extra information needed to build the literal pool we
   add a new sljit_s390x_const struct that contains the initial value but
   can still be cast to a sljit_const. */

SLJIT_API_FUNC_ATTRIBUTE struct sljit_const* sljit_emit_const(struct sljit_compiler *compiler, sljit_s32 dst, sljit_sw dstw, sljit_sw init_value)
{
	CHECK_ERROR_PTR();
	CHECK_PTR(check_sljit_emit_const(compiler, dst, dstw, init_value));

	struct sljit_s390x_const *const_ =
		(struct sljit_s390x_const*)ensure_abuf(compiler, sizeof(struct sljit_s390x_const));
	PTR_FAIL_IF(!const_);
	set_const((struct sljit_const*)const_, compiler);
	const_->init_value = init_value;

	sljit_gpr dst_r = FAST_IS_REG(dst) ? gpr(dst & REG_MASK) : tmp0;
	if (have_genext()) {
		PTR_FAIL_IF(push_inst(compiler, sljit_ins_const | lgrl(dst_r, 0)));
	} else {
		PTR_FAIL_IF(push_inst(compiler, sljit_ins_const | larl(tmp1, 0)));
		PTR_FAIL_IF(push_inst(compiler, lg(dst_r, 0, r0, tmp1)));
	}
	if (dst & SLJIT_MEM) {
		PTR_FAIL_IF(store_word(compiler, dst_r, dst, dstw, tmp1, 0 /* always 64-bit */));
	}
	return (struct sljit_const*)const_;
}

SLJIT_API_FUNC_ATTRIBUTE void sljit_set_const(sljit_uw addr, sljit_sw new_constant, sljit_sw executable_offset)
{
	/* Update the constant pool. */
	sljit_uw *ptr = (sljit_uw *)(SLJIT_ADD_EXEC_OFFSET((sljit_uw *)(addr), executable_offset));
	*ptr = new_constant;
	/* No need to flush the instruction cache as we do not modify
	   any executable code (the constant is fetched from the data
	   data cache). */
}

SLJIT_API_FUNC_ATTRIBUTE void sljit_set_jump_addr(sljit_uw addr, sljit_uw new_target, sljit_sw executable_offset)
{
	/* Update the constant pool. */
	sljit_uw *ptr = (sljit_uw *)(SLJIT_ADD_EXEC_OFFSET((sljit_uw *)(addr), executable_offset));
	*ptr = new_target;
	/* No need to flush the instruction cache as we do not modify
	   any executable code (the constant is fetched from the data
	   data cache). */
}
