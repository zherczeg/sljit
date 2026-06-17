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

/*
    1. All emit_<FORMAT> should accept bundle ptr as argument. These functions
       may be used for wide command filling.
    1.1 Function emit_op is subset of emit_<FORMAT> functions.
    2. All emit_<...> must be complete commands. These functions can not
       fill wide command. Functions do not accept bundle pointer.
    2.1 Function emit_op_full is subset of emit_<...> functions.
*/

/*
----------------------
| E2K backend scheme |
----------------------

- sljit interface function
  SLJIT API.
  SLJIT layer.
          |
         \ /
          v
- [optionally] signature_switcher | emit_load | emit_store
  Helper to choose correct signature for operation.
  High level.
  [High level stepler check.]
          |
         \ /
          v
- emit_[full_]op | emit_<special_OP>
  Generate human readable op.
  Middle level.
  [Bundle single instructions via stepler.]
          |
         \ /
          v
- emit_<FORMAT>
  Low level.
  Generate op of e2k format.
          |
         \ /
          v
- emit_src | codegen
  Codegen level.
  - Choose reg from reg map.
  - Encode operation.
*/

/* TODO:
   1. SIMD *
   2. Support short jumps **
   3. Atomic *
   4. Barrier *
   5. Support scheduler **
   6. Check for nops in div and mul operations **

   *  func extension.
   ** perf extension. */

#include <sys/syscall.h>
#include <asm/e2k_syswork.h>
#include <unistd.h>

SLJIT_API_FUNC_ATTRIBUTE const char* sljit_get_platform_name(void)
{
    return "e2k" SLJIT_CPUINFO;
}

/* E2K specific macro */
#define E2K_CHANNEL_NUMBER 6
#define E2K_CHANNEL_STORE_NUMBER 2
#define E2K_CHANNEL_LOAD_NUMBER 4

/* In some cases we should wait for the latency to prevent L1 lock */
#define E2K_STORE_LATENCY 0
#if (__iset__ >= 6)
#define E2K_LOAD_LATENCY 4
#else
#define E2K_LOAD_LATENCY 2
#endif

/* SLJIT_SP */
#define TMP_REG1 (SLJIT_SP + 1)             /* signature_switcher and lower */
#define TMP_REG2 (SLJIT_SP + 2)             /* all layers */
#define TMP_REG3 (SLJIT_SP + 3)             /* upper than signature_switcher */
#define FLAG_REG (SLJIT_SP + 4)
#define EXTRA_NREGS_NUM 5

/* signature_switcher and lower */
#define TMP_FREG1 (SLJIT_NUMBER_OF_FLOAT_REGISTERS + 1)
#define TMP_FREG2 TMP_FREG1 + 1           /* all layers */
#define TMP_FREG3 TMP_FREG2 + 1           /* upper than signature_switcher */
#define EXTRA_FREGS_NUM 3

#define FLAG_PREG 16
#define TMP_PREG1 17
#define TMP_PREG2 18

/*  All E2K registers ids.
    Number registers.
    Predicates.
    Control. */

#define E2K_NREG_NUMBER (SLJIT_NUMBER_OF_REGISTERS + EXTRA_NREGS_NUM)
#define E2K_FREG_NUMBER (SLJIT_NUMBER_OF_FLOAT_REGISTERS + EXTRA_FREGS_NUM)
#define E2K_REG_NUMBER (E2K_NREG_NUMBER + E2K_FREG_NUMBER)

/* Use the one reg_map */
#define E2K_FREG(fr) (fr + E2K_NREG_NUMBER)
#define E2K_PRED 0x400000
#define E2K_CTPR 0x800000

/*
    64 regs only cause of ENTER_GET_REGS has mask 0xff
*/
#if (E2K_REG_NUMBER != 64)
#error "E2K_REG_NUMBER must be equal 64"
#endif

/* QP terms */
#define E2K_PREG_WINDOW_SIZE 24
#define E2K_ROTATED_REG_BASE ((E2K_REG_NUMBER >> 1) - 4)
#define E2K_ROTATED_REG_SIZE 4
#define E2K_REG_WINDOW_SIZE  ((E2K_REG_NUMBER >> 1))

/*
    VA_MSB  47
    VA_SIZE 48
*/
#define VA_MASK 0x0000FFFFFFFFFFFF

/* Load and store management macro */
/* ------------------------------- */
/* LOAD */
#define LOAD_SHIFT      0x0
#define LOAD_BYTE       0x1
#define LOAD_HALFWORD   0x2
#define LOAD_WORD       0x4
#define LOAD_DOUBLEWORD 0x8
#define LOAD_MASK       0xF
/* STORE */
#define STORE_SHIFT         0x4
#define STORE_BYTE          0x10
#define STORE_HALFWORD      0x20
#define STORE_WORD          0x40
#define STORE_DOUBLEWORD    0x80
#define STORE_MASK          0xF0
/* LOAD and STORE */
#define BYTE        (LOAD_BYTE | STORE_BYTE)
#define HALFWORD    (LOAD_HALFWORD | STORE_HALFWORD)
#define WORD        (LOAD_WORD | STORE_WORD)
#define DOUBLEWORD  (LOAD_DOUBLEWORD | STORE_DOUBLEWORD)
/* LTS management */
#define FORCE_LTS  0x100
/* Numbers in additional code, so we should provide correct sign extension.
   This is useful when we want restrict memory transfer but use more size LTS. */
#define EXTEND_LTS 0x200
/* Prevent nops in LOAD and STORE */
#define NO_LATENCY 0x400
/* ------------------------------- */

#define NOT_SRC1(x) ((x < 0) || (x > 0xF))
/* In case of zero operand these ops does not affect and may be optimized.
   Useful for MOV operations.
*/
#define NON_AFFECTING_OP(op, srcw) \
    (((op == E2K_ADDd) || (op == E2K_ADDs)) && (srcw == 0))

/* Types */
typedef enum {
    E2K_CONST =     0x0,
    E2K_CONST4 =    0x4,
    E2K_CONST5 =    0x5,
    E2K_CONST16 =   0x10,
    E2K_CONST32 =   0x20,
    E2K_CONST64 =   0x40,
    E2K_REG =       0x80,
    E2K_REG_R =     0x100,
    E2K_REG_G =     0x200,
    E2K_REG_PRED =  0x400,
    E2K_REG_CTPR =  0x8000,
} E2kOpT;

/* Reg coding */
#define R(reg) reg | 0x80
#define B(reg) reg
#define R0_OFFSET 8     /* r0 start index in reg_map */
#define nip 0x82
#define cr0_hi 0x51

/* Imm coding */
#define CONST4(i)  i | 0xc0
#define CONST16(i) i | 0xd0
#define CONST32(i) i | 0xd8
#define CONST64(i) i | 0xdc
#define SXT_EXTEND_8  0x0
#define SXT_EXTEND_16 0x1
#define SXT_EXTEND_32 0x2


/* + zero */
static const sljit_u8 reg_map[E2K_REG_NUMBER + 1] = {
    0,
    /* scratch */
    B(0), B(1), B(2), B(3), B(4), B(5), B(6), B(7),     /* ABI param regs */
    R(0), R(1), R(2), R(3), R(4), R(5), R(6), R(7),     /* ABI input regs */
    R(8), R(9), R(10), R(11),
    R(12), R(13), R(14), R(15), R(16), R(17), R(18), R(19),
    R(20), R(21), R(22), R(23), R(24), R(25),
    /* saved */
    R(26), R(27), R(28), R(29), R(30), R(31),
    R(32),  /* SLJIT_SP */
    R(33),  /* TMP_REG1 */
    R(34),  /* TMP_REG2 */
    R(35),  /* TMP_REG3 */
    R(36),  /* FLAG_REG */

    /* float scratch */
    R(37), R(38), R(39), R(40), R(41), R(42), R(43), R(44),
    R(45), R(46), R(47), R(48), R(49), R(50),
    /* float saved */
    R(51), R(52),
    R(53),  /* TMP_FREG1 */
    R(54),  /* TMP_FREG2 */
    R(55),  /* TMP_FREG3 */
};

/* label for ctpr */
static const sljit_u8 ctpr_map[4] = {
    0x0, 0x11, 0x12, 0x13
};

/* pdst, direct mapping */
static const sljit_u16 pred_map[E2K_PREG_WINDOW_SIZE] = {
    /* base */
    0,  1,  2,  3,  4,  5,  6,  7,
    8,  9,  10, 11, 12, 13, 14, 15,
    /* special */
    16, 17, 18, 19, 20, 21, 22, 23
};

/* -- e2k bundle ---------------------------------------------------------- */

typedef enum E2kResourceMask {
    /* HS */
    RES_ALS0    = 0x0001,
    RES_ALS1    = 0x0002,
    RES_ALS2    = 0x0004,
    RES_ALS3    = 0x0008,
    RES_ALS4    = 0x0010,
    RES_ALS5    = 0x0020,
    RES_CS0     = 0x0040,
    RES_CS1     = 0x0080,
    RES_AAS0    = 0x0100,
    RES_AAS1    = 0x0200,
    RES_AAS2    = 0x0400,
    RES_AAS3    = 0x0800,
    RES_AAS4    = 0x1000,
    RES_AAS5    = 0x2000,
    RES_LTS0    = 0x4000,
    RES_LTS1    = 0x8000,
    RES_LTS2    = 0x10000,
    RES_LTS3    = 0x20000,
    RES_PLS0    = 0x40000,
    RES_PLS1    = 0x80000,
    RES_PLS2    = 0x100000,
    RES_ALES0   = 0x200000,
    RES_ALES1   = 0x400000,
    RES_ALES2   = 0x800000,
    RES_ALES3   = 0x1000000,
    RES_ALES4   = 0x2000000,
    RES_ALES5   = 0x4000000,
    RES_SS      = 0x8000000,
    RES_CDS00   = 0x10000000,
    RES_CDS01   = 0x20000000,
    RES_CDS10   = 0x40000000,
    RES_CDS11   = 0x80000000,
    RES_CDS20   = 0x100000000,
    RES_CDS21   = 0x200000000,
    RES_MASK    = 0x3ffffffff,
    RES_INIT    = RES_MASK,
    RES_NONE    = 0,
    RES_ALS_012345 = RES_ALS0 | RES_ALS1 | RES_ALS2 | RES_ALS3 | RES_ALS4 | RES_ALS5,
    RES_ALS_0235   = RES_ALS0 | RES_ALS2 | RES_ALS3 | RES_ALS5,
    RES_ALS_0134   = RES_ALS0 | RES_ALS1 | RES_ALS3 | RES_ALS4,
    RES_ALS_03     = RES_ALS0 | RES_ALS3,
    RES_ALS_14     = RES_ALS1 | RES_ALS4,
    RES_ALS_25     = RES_ALS2 | RES_ALS5,
    RES_ALS_ALL    = RES_ALS_012345,
    RES_ALES_25    = RES_ALES2 | RES_ALES5,
    RES_ALES_ALL   = RES_ALES0 | RES_ALES1 | RES_ALES2 | RES_ALES3 | RES_ALES4 | RES_ALES5,
    RES_CS_ALL     = RES_CS0 | RES_CS1,
    RES_LTS_ALL    = RES_LTS0 | RES_LTS1 | RES_LTS2| RES_LTS3,
    RES_CDS_ALL    = RES_CDS00 | RES_CDS01 | RES_CDS10 | RES_CDS11 | RES_CDS20 | RES_CDS21,
    RES_CDS0       = RES_CDS00 | RES_CDS01,
    RES_CDS1       = RES_CDS10 | RES_CDS11,
    RES_CDS2       = RES_CDS20 | RES_CDS21,
    RES_ALS_SHIFT  = 0,
    RES_CS_SHIFT   = 6,
    RES_LTS_SHIFT  = 14,
    RES_ALES_SHIFT = 21,
    RES_CDS_SHIFT  = 28,
} E2kResourceMask;

typedef struct E2kBundle {
    sljit_u8 f1;
    sljit_u8 f2;
    sljit_u8 f3;
    sljit_u8 f4;
    sljit_u8 hs_cds;
    sljit_u32 nop;
    sljit_uw res;
    sljit_u32 ss;
    sljit_u32 als[6];
    sljit_u32 cs[2];
    sljit_u16 ales[6];
    sljit_u32 lts[4];
    sljit_u16 cds[6];
} E2kBundle;

/* -- Bundling helpers ---------------------------------------------------- */
static SLJIT_INLINE sljit_uw check_resource(struct sljit_compiler *compiler,
                                            E2kBundle *bundle,
                                            sljit_uw mask)
{
    sljit_uw res = bundle->res & mask;
    SLJIT_UNUSED_ARG(compiler);
    return res;
}

static SLJIT_INLINE sljit_uw get_resource(struct sljit_compiler *compiler,
                                          E2kBundle *bundle,
                                          sljit_uw mask)
{
    sljit_uw res = check_resource(compiler, bundle, mask);
    SLJIT_ASSERT(res);
    res = res & (-res);     /* get the last bit of avaiable resource */
    bundle->res &= ~res;    /* take resource */
    return res;
}

static SLJIT_INLINE int get_sylidx(struct sljit_compiler *compiler,
                                   E2kBundle *bundle,
                                   sljit_uw mask,
                                   sljit_uw shift)
{
    int idx = 0;
    sljit_uw syl = get_resource(compiler, bundle, mask);
    syl = syl >> shift;
    while(syl >>= 1)
        idx++;
    return idx;
}

#define checku4(x)  ((x) == (int32_t)(sljit_u8)(x & 0xf))
#define checku5(x)  ((x) == (int32_t)(sljit_u8)(x & 0x1f))
#define checki16(x) ((x) == (int32_t)(sljit_s16)(x))
#define checku16(x) ((x) == (int32_t)(sljit_u16)(x))
#define checki32(x) ((x) == (int32_t)(x))
#define checku32(x) ((x) == (sljit_u32)(x))

static E2kOpT get_reg_type(struct sljit_compiler *compiler,
                           E2kBundle *bundle,
                           sljit_uw val)
{
    SLJIT_UNUSED_ARG(compiler);
    SLJIT_UNUSED_ARG(bundle);

    /* keep zero reg in mind */
    if (!(val & (E2K_CTPR | E2K_PRED)))
        return E2K_REG_R;
    else if ((val & E2K_CTPR) == E2K_CTPR)
        return E2K_REG_CTPR;
    else if ((val & E2K_PRED) == E2K_PRED)
        return E2K_REG_PRED;
    else
        SLJIT_ASSERT(0);
    return 0;
}

static E2kOpT get_const_type(struct sljit_compiler *compiler,
                             E2kBundle *bundle,
                             sljit_uw val)
{
    SLJIT_UNUSED_ARG(compiler);
    SLJIT_UNUSED_ARG(bundle);

    if (checku4(val))
        return E2K_CONST4;
    else if(checku5(val))
        return E2K_CONST5;
    else if (checki16(val))
        return E2K_CONST16;
    else if (checki32(val))
        return E2K_CONST32;
    else
        return E2K_CONST64;
}

static SLJIT_INLINE sljit_u32 bundle_size_in_words(E2kBundle *bundle)
{
    sljit_u32 hs_cds =  (bundle->hs_cds + 1) & (sljit_u32)-2;

    sljit_u8 f1 =       bundle->f1;
    sljit_u8 f2 =       bundle->f2;
    sljit_u8 half_pad = (sljit_u8)(bundle->f3 & 0x1);
    sljit_u8 f3 =       (sljit_u8)((bundle->f3 >> 1) + half_pad);
    sljit_u8 f4 =       (sljit_u8)(bundle->f4 + (hs_cds >> 1));
    sljit_u8 lng =      (sljit_u8)(f1 + f2 + f3 + f4);
    sljit_u8 hs_lng =   (sljit_u8)((lng + 1) & (sljit_u32)-2);

    return hs_lng;
}

/* -- Instructions formats ------------------------------------------------ */

/* E2K Format description */
typedef enum SrcDstMask {
    FMT_ZERO = 0x0,
    FMT_SRC1 = 0x1,
    FMT_SRC2 = 0x2,
    FMT_SRC3 = 0x4,
    FMT_DST  = 0x8,
    FMT_DST2 = 0x10,
} SrcDstMask;

/* Correct input src type, sync with SrcDstMask.
   Imm in case of 0, Reg in case of 1.
   Also add special control flags. */
#define EMIT_FLAGS_NO_REGIMM_MASK 0xFFF0
typedef enum EmitFlags {
    REG_ZERO = 0x0,
    REG_SRC1 = 0x1,
    REG_SRC2 = 0x2,
    REG_SRC3 = 0x4,
    DST_CTPR = 0x8,
    STEPLER_CHECK_RESOURCE_ONLY = 0x10,
} EmitFlags;

typedef enum IFLMask {
    IFL_ZERO = 0x00000000,
    IFL_CF = 0x1,
    IFL_PF = 0x4,
    IFL_AF = 0x10,
    IFL_ZF = 0x40,
    IFL_SF = 0x80,
    IFL_OF = 0x800,

    IFL_EQ_F = IFL_ZF,
    IFL_LESS_F = IFL_CF,
    IFL_MORE_F = IFL_ZERO,
    IFL_UNORDERED = IFL_ZF | IFL_PF | IFL_CF
} IFLMask;

typedef struct E2kFormat {
    char *name;

    /*         src1 src2 src3
       ALOPF1: 1    1    0
       ...
    */
    sljit_u32 src_dst_mask;
} E2kFormat;

typedef union {
    sljit_u32 i;
    struct {
      sljit_u32 mdl     : 4;
      sljit_u32 lng     : 3;
      sljit_u32 nop     : 3;
      sljit_u32 lm      : 1;
      sljit_u32 x_s_sw  : 3;
      sljit_u32 c       : 2;
      sljit_u32 cds     : 2;
      sljit_u32 pls     : 2;
      sljit_u32 ales    : 6;
      sljit_u32 als     : 6;
    } fields;
} E2kHS;

typedef union {
    sljit_u32 i;
    struct {
      sljit_u32 ctcond  : 9;
      sljit_u32 x       : 1;
      sljit_u32 ctop    : 2;
      sljit_u32 aa      : 4;
      sljit_u32 alc     : 2;
      sljit_u32 abp     : 2;
      sljit_u32 type    : 1;
      sljit_u32 abn     : 2;
      sljit_u32 abg     : 2;
      sljit_u32 rp_lo   : 1;
      sljit_u32 vfdi    : 1;
      sljit_u32 rp_hi   : 1;
      sljit_u32 bap     : 1;
      sljit_u32 eap     : 1;
      sljit_u32 ipd     : 2;
    } fields;
} E2kSS;

typedef union {
    sljit_u32 i;
    struct {
      sljit_u32 rbs   : 6;
      sljit_u32 rsz   : 6;
      sljit_u32 rcur  : 6;
      sljit_u32 psz   : 5;
      sljit_u32 x     : 3;
      sljit_u32 setbn : 1;
      sljit_u32 setbp : 1;
      sljit_u32 opc   : 4;
    } fields;
} E2kSETR;

typedef union {
    sljit_u32 i;
    struct {
      sljit_u32 x    : 3;
      sljit_u32 dbl  : 1;
      sljit_u32 nfx  : 1;
      sljit_u32 wsz  : 7;
      sljit_u32 rpsz : 5;
      sljit_u32 x1   : 15;
    } fields;
} E2kSETR_LTS0;

typedef union {
    sljit_u32 i;
    struct {
      sljit_u32 dst     : 8;
      sljit_u32 src2    : 8;
      sljit_u32 src1    : 8;
      sljit_u32 cop     : 7;
      sljit_u32 spec    : 1;
    } fields;
} E2kAlopf1;

typedef union {
    sljit_u32 i;
    struct {
      sljit_u32 dst     : 8;
      sljit_u32 src2    : 8;
      sljit_u32 opce    : 8;
      sljit_u32 cop     : 7;
      sljit_u32 spec    : 1;
    } fields;
} E2kAlopf2;

typedef union {
  sljit_u32 i;
  struct {
    sljit_u32 src3    : 8;
    sljit_u32 src2    : 8;
    sljit_u32 src1    : 8;
    sljit_u32 cop     : 7;
    sljit_u32 spec    : 1;
  } fields;
} E2kAlopf3;

typedef union {
    sljit_u32 i;
    struct {
      sljit_u32 pdst    : 5;
      sljit_u32 cmpopce : 3;
      sljit_u32 src2    : 8;
      sljit_u32 src1    : 8;
      sljit_u32 cop     : 7;
      sljit_u32 spec    : 1;
    } fields;
} E2kAlopf7;

typedef union {
    sljit_u32 i;
    struct {
      sljit_u32 pdst    : 5;
      sljit_u32 cmpopce : 3;
      sljit_u32 src2    : 8;
      sljit_u32 opce    : 8;
      sljit_u32 cop     : 7;
      sljit_u32 spec    : 1;
    } fields;
} E2kAlopf8;

typedef union {
    sljit_u32 i;
    struct {
      sljit_u32 param   : 28;
      sljit_u32 opc     : 2;
      sljit_u32 ctpr    : 2;
    } fields;
} E2kCopf1;

typedef union {
    sljit_u32 i;
    struct {
      sljit_s32 disp    : 28;
      sljit_u32 opc     : 2;
      sljit_u32 ctpr    : 2;
    } fields;
} E2kCopf2;

typedef union {
    sljit_u32 i;
    struct {
      sljit_u32 params  : 28;
      sljit_u32 opc     : 4;
    } fields;
} E2kC1f1;

typedef union {
    sljit_u16 i;
    struct {
      sljit_u16 pred    : 7;
      sljit_u16 neg     : 3;
      sljit_u16 mask    : 4;
      sljit_u16 opc     : 2;
    } fields;
} E2kCDS;

typedef union {
    sljit_u16 i;
    struct {
      sljit_u16 opce2   : 8;
      sljit_u16 opc2    : 8;
    } fields;
} E2kAlef2;

typedef union {
    sljit_u16 i;
    struct {
      sljit_u16 src3    : 8;
      sljit_u16 opc2    : 8;
    } fields;
} E2kAlef1;

typedef struct E2kOp {
    char *name;
    E2kResourceMask mask;
    sljit_u32 opc;
    sljit_u32 opce;
    sljit_u16 opc2;
    sljit_u16 opce2;
    sljit_u32 format;
} E2kOp;

enum {
    /* short encoding */
    E2K_ALOPF1,
    E2K_ALOPF2,
    E2K_ALOPF3,
    E2K_ALOPF7,
    E2K_ALOPF8,
    /* long encoding */
    E2K_ALOPF10,
    E2K_ALOPF11,
    E2K_ALOPF12,
    E2K_ALOPF13,
    E2K_ALOPF15,
    E2K_ALOPF16,
    E2K_ALOPF17,
    E2K_ALOPF19,
    E2K_ALOPF21,
    E2K_ALOPF22,
    /* CS */
    E2K_COPF1,
    E2K_COPF2,
    E2K_COPF4,
    E2K_COPF4_ext,
    /* CDS */
    E2K_CDOPF1,
    /* PLS */
    E2K_PLOPF1,
    E2K_PLOPF2,
    E2K_PLOPF3,
    /* AAS */
    E2K_AAOPF1,
    /* SS */
    E2K_SSOPF1,
    E2K_SSOPF2
};

static const E2kFormat e2kfmt[] = {
    /* Only most usable ops have full fmt description.
       Maybe extended in the next versions. */
/*    name          src_dst_mask                                             */
    /* short encoding */
    { "ALOPF1",     FMT_SRC1 | FMT_SRC2 | FMT_DST },
    { "ALOPF2",     FMT_SRC2 | FMT_DST },
    { "ALOPF3",     FMT_SRC1 | FMT_SRC2 | FMT_SRC3 },
    { "ALOPF7",     FMT_SRC1 | FMT_SRC2 | FMT_DST2},
    { "ALOPF8",     FMT_SRC2 | FMT_DST2 },
    /* long encoding, not full support */
    { "ALOPF10",    FMT_SRC3 },
    { "ALOPF11",    FMT_SRC1 | FMT_SRC2 },
    { "ALOPF12",    FMT_SRC2 | FMT_DST },
    { "ALOPF13",    FMT_SRC1 | FMT_SRC2 | FMT_SRC3 },
    { "ALOPF15",    FMT_SRC2 },
    { "ALOPF16",    FMT_ZERO },
    { "ALOPF17",    FMT_SRC1 | FMT_SRC2 | FMT_DST },
    { "ALOPF19",    FMT_DST },
    { "ALOPF21",    FMT_SRC1 | FMT_SRC2 | FMT_SRC3 },
    { "ALOPF22",    FMT_SRC2 },
    /* CS */
    { "COPF1",      FMT_ZERO },
    { "COPF2",      FMT_ZERO },
    { "COPF4",      FMT_ZERO },
    { "COPF4_ext",  FMT_ZERO },
    /* CDS */
    { "CDOPF1",     FMT_ZERO },
    /* PLS */
    { "PLOPF1",     FMT_ZERO },
    { "PLOPF2",     FMT_ZERO },
    { "PLOPF3",     FMT_ZERO },
    /* AAS */
    { "AAOPF1",     FMT_ZERO },
    /* SS */
    { "SSOPF1",     FMT_ZERO },
    { "SSOPF2",     FMT_ZERO },
};

/* -- Opcodes ------------------------------------------------------------- */

#define E2K_DISP    0x0
#define E2K_SDISP   0x2
#define E2K_IBRANCH 0x0
#define E2K_CALL    0x5
#define E2K_RETURN  0x3
#define E2K_NOP     0x0
#define SXT_BS      0x0
#define SXT_HS      0x1
#define SXT_WS      0x2
#define SXT_BZ      0x4
#define SXT_HZ      0x5
#define SXT_WZ      0x6
#define E2K_SETR0   0x0
#define E2K_SETR1   0x1


enum {
    /* ------------------------------------------------- */
    E2K_ANDs,     E2K_ANDd,     E2K_ANDNs,    E2K_ANDNd,
    E2K_ANDs_fd,  E2K_ANDd_fd,  E2K_ANDNs_fd, E2K_ANDNd_fd,
    E2K_ORs,      E2K_ORd,      E2K_ORNs,     E2K_ORNd,
    E2K_ORs_fd,   E2K_ORd_fd,   E2K_ORNs_fd,  E2K_ORNd_fd,
    /* ------------------------------------------------- */
    E2K_XORs,     E2K_XORd,     E2K_XORNs,    E2K_XORNd,
    E2K_XORs_fd,  E2K_XORd_fd,  E2K_XORNs_fd, E2K_XORNd_fd,
    E2K_SXT,      E2K_MERGEs,   E2K_MERGEd,
    /* ------------------------------------------------- */
    E2K_ADDs,     E2K_ADDd,     E2K_SUBs,     E2K_SUBd,
    E2K_ADDs_fd,  E2K_ADDd_fd,  E2K_SUBs_fd,  E2K_SUBd_fd,
    E2K_SCLs,     E2K_SCLd,     E2K_SCRs,     E2K_SCRd,
    E2K_SCLs_fd,  E2K_SCLd_fd,  E2K_SCRs_fd,  E2K_SCRd_fd,
    /* ------------------------------------------------- */
    E2K_SHLs,     E2K_SHLd,     E2K_SHRs,     E2K_SHRd,
    E2K_SHLs_fd,  E2K_SHLd_fd,  E2K_SHRs_fd,  E2K_SHRd_fd,
    E2K_SARs,     E2K_SARd,     E2K_GETFs,    E2K_GETFd,
    E2K_SARs_fd,  E2K_SARd_fd,
    /* ------------------------------------------------- */
    E2K_CMPOsb,   E2K_CMPBsb,   E2K_CMPEsb,   E2K_CMPBEsb,
    E2K_CMPSsb,   E2K_CMPPsb,   E2K_CMPLsb,   E2K_CMPLEsb,
    E2K_CMPOdb,   E2K_CMPBdb,   E2K_CMPEdb,   E2K_CMPBEdb,
    E2K_CMPSdb,   E2K_CMPPdb,   E2K_CMPLdb,   E2K_CMPLEdb,
    E2K_CMPANDEsb,E2K_CMPANDSsb,E2K_CMPANDPsb,E2K_CMPANDLEsb,
    E2K_CMPANDEdb,E2K_CMPANDSdb,E2K_CMPANDPdb,E2K_CMPANDLEdb,
    E2K_CCTOPO,   E2K_CCTOPB,   E2K_CCTOPE,   E2K_CCTOPBE,
    E2K_CCTOPS,   E2K_CCTOPP,   E2K_CCTOPL,   E2K_CCTOPLE,
    E2K_STb,      E2K_STh,      E2K_BITREVs,  E2K_STw,
    E2K_BITREVd,  E2K_STd,
    /* ------------------------------------------------- */
    E2K_FCMPEQsb, E2K_FCMPLTsb, E2K_FCMPLEsb, E2K_FCMPUODsb,
    E2K_FCMPNEQsb,E2K_FCMPNLTsb,E2K_FCMPNLEsb,
    E2K_FCMPEQdb, E2K_FCMPLTdb, E2K_FCMPLEdb, E2K_FCMPUODdb,
    E2K_FCMPNEQdb,E2K_FCMPNLTdb,E2K_FCMPNLEdb,
    E2K_FCMPODsf, E2K_FCMPODdf, E2K_FCMPUDsf, E2K_FCMPUDdf,
    /* ------------------------------------------------- */
    E2K_FADDs,    E2K_FADDd,    E2K_FSUBs,    E2K_FSUBd,
    E2K_FMINs,    E2K_FMINd,    E2K_FMAXs,    E2K_FMAXd,
    /* ------------------------------------------------- */
    E2K_FMULs,    E2K_FMULd,
    E2K_FSTOIS,   E2K_FSTOIStr, E2K_ISTOFS,
    E2K_FDTOID,   E2K_FDTOIDtr, E2K_IDTOFD,
    E2K_FSTOID,   E2K_FSTOIDtr, E2K_ISTOFD,
    E2K_FSTOFD,   E2K_FDTOIS,   E2K_FDTOIStr,
    E2K_IDTOFS,   E2K_FDTOFS,
    /* ------------------------------------------------- */
    E2K_RRs,      E2K_RRd,
    E2K_PSHUFB,
    E2K_UDIVs,    E2K_UDIVd,    E2K_SDIVs,    E2K_SDIVd,
    E2K_UDIVX,    E2K_UMODX,    E2K_SDIVX,    E2K_SMODX,
    /* ------------------------------------------------- */
    E2K_MOVTs,    E2K_MOVTd,    E2K_GETPL,
    E2K_LDb,      E2K_LZCNTs,   E2K_LDh,      E2K_LZCNTd,
    E2K_LDw,      E2K_POPCNTs,  E2K_LDd,      E2K_POPCNTd,
    /* ------------------------------------------------- */
    E2K_MULs,     E2K_MULd,     E2K_UMULX,    E2K_SMULX,
    E2K_UMULX_fd, E2K_SMULX_fd,
    /* ------------------------------------------------- */
    E2K_FDIVs,    E2K_FDIVd,
    /* ------------------------------------------------- */
    E2K_FSQRTs,   E2K_FSQRTId, E2K_FSQRTTd,
    /* ------------------------------------------------- */
    E2K_GETSP,
    /* ------------------------------------------------- */
    E2K_FDTOIFs,  E2K_FDTOIFd,
    /* ------------------------------------------------- */
    E2K_UMULHd,   E2K_SMULHd
    /* ------------------------------------------------- */
};

/* B.1.1 */
static const E2kOp e2kop[] = {
/* --- Small encoding ------------------------------------------------------- */
/*    name         resources       opc   opce  opc2 opce2  format */
    { "ANDs",      RES_ALS_012345, 0x00, 0,    0,   0,     E2K_ALOPF1},
    { "ANDd",      RES_ALS_012345, 0x01, 0,    0,   0,     E2K_ALOPF1},
    { "ANDNs",     RES_ALS_012345, 0x02, 0,    0,   0,     E2K_ALOPF1},
    { "ANDNd",     RES_ALS_012345, 0x03, 0,    0,   0,     E2K_ALOPF1},
    { "ANDs_fd",   RES_ALS_03,     0x00, 0xc0, 0x6, 0xc0,  E2K_ALOPF11},
    { "ANDd_fd",   RES_ALS_03,     0x01, 0xc0, 0x7, 0xc0,  E2K_ALOPF11},
    { "ANDNs_fd",  RES_ALS_03,     0x02, 0xc0, 0x6, 0xc0,  E2K_ALOPF11},
    { "ANDNd_fd",  RES_ALS_03,     0x03, 0xc0, 0x7, 0xc0,  E2K_ALOPF11},
    { "ORs",       RES_ALS_012345, 0x04, 0,    0,   0,     E2K_ALOPF1},
    { "ORd",       RES_ALS_012345, 0x05, 0,    0,   0,     E2K_ALOPF1},
    { "ORNs",      RES_ALS_012345, 0x06, 0,    0,   0,     E2K_ALOPF1},
    { "ORNd",      RES_ALS_012345, 0x07, 0,    0,   0,     E2K_ALOPF1},
    { "ORs_fd",    RES_ALS_03,     0x04, 0xc0, 0x6, 0xc0,  E2K_ALOPF11},
    { "ORd_fd",    RES_ALS_03,     0x05, 0xc0, 0x7, 0xc0,  E2K_ALOPF11},
    { "ORNs_fd",   RES_ALS_03,     0x06, 0xc0, 0x6, 0xc0,  E2K_ALOPF11},
    { "ORNd_fd",   RES_ALS_03,     0x07, 0xc0, 0x7, 0xc0,  E2K_ALOPF11},
/* --------------------------------------------------- */
    { "XORs",      RES_ALS_012345, 0x08, 0,    0,   0,     E2K_ALOPF1},
    { "XORd",      RES_ALS_012345, 0x09, 0,    0,   0,     E2K_ALOPF1},
    { "XORNs",     RES_ALS_012345, 0x0a, 0,    0,   0,     E2K_ALOPF1},
    { "XORNd",     RES_ALS_012345, 0x0b, 0,    0,   0,     E2K_ALOPF1},
    { "XORs_fd",   RES_ALS_03,     0x08, 0xc0, 0x6, 0xc0,  E2K_ALOPF11},
    { "XORd_fd",   RES_ALS_03,     0x09, 0xc0, 0x7, 0xc0,  E2K_ALOPF11},
    { "XORNs_fd",  RES_ALS_03,     0x0a, 0xc0, 0x6, 0xc0,  E2K_ALOPF11},
    { "XORNd_fd",  RES_ALS_03,     0x0b, 0xc0, 0x7, 0xc0,  E2K_ALOPF11},
    { "SXT",       RES_ALS_012345, 0x0c, 0,    0,   0,     E2K_ALOPF1},
    { "MERGEs",    RES_ALS_012345, 0x0e, 0,    0,   0,     E2K_ALOPF1},
    { "MERGEd",    RES_ALS_012345, 0x0f, 0,    0,   0,     E2K_ALOPF1},
/* --------------------------------------------------- */
    { "ADDs",      RES_ALS_012345, 0x10, 0,    0,   0,     E2K_ALOPF1},
    { "ADDd",      RES_ALS_012345, 0x11, 0,    0,   0,     E2K_ALOPF1},
    { "SUBs",      RES_ALS_012345, 0x12, 0,    0,   0,     E2K_ALOPF1},
    { "SUBd",      RES_ALS_012345, 0x13, 0,    0,   0,     E2K_ALOPF1},
    { "ADDs_fd",   RES_ALS_03,     0x10, 0xc0, 0x6, 0xc0,  E2K_ALOPF11},
    { "ADDd_fd",   RES_ALS_03,     0x11, 0xc0, 0x7, 0xc0,  E2K_ALOPF11},
    { "SUBs_fd",   RES_ALS_03,     0x12, 0xc0, 0x6, 0xc0,  E2K_ALOPF11},
    { "SUBd_fd",   RES_ALS_03,     0x13, 0xc0, 0x7, 0xc0,  E2K_ALOPF11},
    { "SCLs",      RES_ALS_012345, 0x14, 0,    0,   0,     E2K_ALOPF1},
    { "SCLd",      RES_ALS_012345, 0x15, 0,    0,   0,     E2K_ALOPF1},
    { "SCRs",      RES_ALS_012345, 0x16, 0,    0,   0,     E2K_ALOPF1},
    { "SCRd",      RES_ALS_012345, 0x17, 0,    0,   0,     E2K_ALOPF1},
    { "SCLs_fd",   RES_ALS_03,     0x14, 0xc0, 0x6, 0xc0,  E2K_ALOPF21},
    { "SCLd_fd",   RES_ALS_03,     0x15, 0xc0, 0x7, 0xc0,  E2K_ALOPF21},
    { "SCRs_fd",   RES_ALS_03,     0x16, 0xc0, 0x6, 0xc0,  E2K_ALOPF21},
    { "SCRd_fd",   RES_ALS_03,     0x17, 0xc0, 0x7, 0xc0,  E2K_ALOPF21},
/* --------------------------------------------------- */
    { "SHLs",      RES_ALS_012345, 0x18, 0,    0,   0,     E2K_ALOPF1},
    { "SHLd",      RES_ALS_012345, 0x19, 0,    0,   0,     E2K_ALOPF1},
    { "SHRs",      RES_ALS_012345, 0x1a, 0,    0,   0,     E2K_ALOPF1},
    { "SHRd",      RES_ALS_012345, 0x1b, 0,    0,   0,     E2K_ALOPF1},
    { "SHLs_fd",   RES_ALS_03,     0x18, 0xc0, 0x6, 0xc0,  E2K_ALOPF21},
    { "SHLd_fd",   RES_ALS_03,     0x19, 0xc0, 0x7, 0xc0,  E2K_ALOPF21},
    { "SHRs_fd",   RES_ALS_03,     0x1a, 0xc0, 0x6, 0xc0,  E2K_ALOPF21},
    { "SHRd_fd",   RES_ALS_03,     0x1b, 0xc0, 0x7, 0xc0,  E2K_ALOPF21},
    { "SARs",      RES_ALS_012345, 0x1c, 0,    0,   0,     E2K_ALOPF1},
    { "SARd",      RES_ALS_012345, 0x1d, 0,    0,   0,     E2K_ALOPF1},
    { "GETFs",     RES_ALS_012345, 0x1e, 0,    0,   0,     E2K_ALOPF1},
    { "GETFd",     RES_ALS_012345, 0x1f, 0,    0,   0,     E2K_ALOPF1},
    { "SARs_fd",   RES_ALS_03,     0x1c, 0xc0, 0x6, 0xc0,  E2K_ALOPF21},
    { "SARd_fd",   RES_ALS_03,     0x1d, 0xc0, 0x7, 0xc0,  E2K_ALOPF21},
/* --------------------------------------------------- */
    { "CMPOsb",    RES_ALS_0134,   0x20, 0x0,  0,   0,     E2K_ALOPF7},
    { "CMPBsb",    RES_ALS_0134,   0x20, 0x1,  0,   0,     E2K_ALOPF7},
    { "CMPEsb",    RES_ALS_0134,   0x20, 0x2,  0,   0,     E2K_ALOPF7},
    { "CMPBEsb",   RES_ALS_0134,   0x20, 0x3,  0,   0,     E2K_ALOPF7},
    { "CMPSsb",    RES_ALS_0134,   0x20, 0x4,  0,   0,     E2K_ALOPF7},
    { "CMPPsb",    RES_ALS_0134,   0x20, 0x5,  0,   0,     E2K_ALOPF7},
    { "CMPLsb",    RES_ALS_0134,   0x20, 0x6,  0,   0,     E2K_ALOPF7},
    { "CMPLEsb",   RES_ALS_0134,   0x20, 0x7,  0,   0,     E2K_ALOPF7},
    { "CMPOdb",    RES_ALS_0134,   0x21, 0x0,  0,   0,     E2K_ALOPF7},
    { "CMPBdb",    RES_ALS_0134,   0x21, 0x1,  0,   0,     E2K_ALOPF7},
    { "CMPEdb",    RES_ALS_0134,   0x21, 0x2,  0,   0,     E2K_ALOPF7},
    { "CMPBEdb",   RES_ALS_0134,   0x21, 0x3,  0,   0,     E2K_ALOPF7},
    { "CMPSdb",    RES_ALS_0134,   0x21, 0x4,  0,   0,     E2K_ALOPF7},
    { "CMPPdb",    RES_ALS_0134,   0x21, 0x5,  0,   0,     E2K_ALOPF7},
    { "CMPLdb",    RES_ALS_0134,   0x21, 0x6,  0,   0,     E2K_ALOPF7},
    { "CMPLEdb",   RES_ALS_0134,   0x21, 0x7,  0,   0,     E2K_ALOPF7},
    { "CMPANDEsb", RES_ALS_0134,   0x22, 0x2,  0,   0,     E2K_ALOPF7},
    { "CMPANDSsb", RES_ALS_0134,   0x22, 0x4,  0,   0,     E2K_ALOPF7},
    { "CMPANDPsb", RES_ALS_0134,   0x22, 0x5,  0,   0,     E2K_ALOPF7},
    { "CMPANDLEsb",RES_ALS_0134,   0x22, 0x7,  0,   0,     E2K_ALOPF7},
    { "CMPANDEdb", RES_ALS_0134,   0x23, 0x2,  0,   0,     E2K_ALOPF7},
    { "CMPANDSdb", RES_ALS_0134,   0x23, 0x4,  0,   0,     E2K_ALOPF7},
    { "CMPANDPdb", RES_ALS_0134,   0x23, 0x5,  0,   0,     E2K_ALOPF7},
    { "CMPANDLEdb",RES_ALS_0134,   0x23, 0x7,  0,   0,     E2K_ALOPF7},
    { "CCTOPO",    RES_ALS_03,     0x24, 0x0,  0,   0,     E2K_ALOPF8},
    { "CCTOPB",    RES_ALS_03,     0x24, 0x1,  0,   0,     E2K_ALOPF8},
    { "CCTOPE",    RES_ALS_03,     0x24, 0x2,  0,   0,     E2K_ALOPF8},
    { "CCTOPBE",   RES_ALS_03,     0x24, 0x3,  0,   0,     E2K_ALOPF8},
    { "CCTOPS",    RES_ALS_03,     0x24, 0x4,  0,   0,     E2K_ALOPF8},
    { "CCTOPP",    RES_ALS_03,     0x24, 0x5,  0,   0,     E2K_ALOPF8},
    { "CCTOPL",    RES_ALS_03,     0x24, 0x6,  0,   0,     E2K_ALOPF8},
    { "CCTOPLE",   RES_ALS_03,     0x24, 0x7,  0,   0,     E2K_ALOPF8},
    { "STb",       RES_ALS_25,     0x24, 0,    0,   0,     E2K_ALOPF3},
    { "STh",       RES_ALS_25,     0x25, 0,    0,   0,     E2K_ALOPF3},
    { "BITREVs",   RES_ALS_0134,   0x26, 0xc0, 0,   0,     E2K_ALOPF2},
    { "STw",       RES_ALS_25,     0x26, 0,    0,   0,     E2K_ALOPF3},
    { "BITREVd",   RES_ALS_0134,   0x27, 0xc0, 0,   0,     E2K_ALOPF2},
    { "STd",       RES_ALS_25,     0x27, 0,    0,   0,     E2K_ALOPF3},
/* --------------------------------------------------- */
    { "FCMPEQsb",  RES_ALS_0134,   0x2e, 0x0,  0x01,0xc0,  E2K_ALOPF11},
    { "FCMPLTsb",  RES_ALS_0134,   0x2e, 0x1,  0x01,0xc0,  E2K_ALOPF11},
    { "FCMPLEsb",  RES_ALS_0134,   0x2e, 0x2,  0x01,0xc0,  E2K_ALOPF11},
    { "FCMPUODsb", RES_ALS_0134,   0x2e, 0x3,  0x01,0xc0,  E2K_ALOPF11},
    { "FCMPNEQsb", RES_ALS_0134,   0x2e, 0x4,  0x01,0xc0,  E2K_ALOPF11},
    { "FCMPNLTsb", RES_ALS_0134,   0x2e, 0x5,  0x01,0xc0,  E2K_ALOPF11},
    { "FCMPNLEsb", RES_ALS_0134,   0x2e, 0x6,  0x01,0xc0,  E2K_ALOPF11},
    { "FCMPEQdb",  RES_ALS_0134,   0x2f, 0x0,  0x01,0xc0,  E2K_ALOPF11},
    { "FCMPLTdb",  RES_ALS_0134,   0x2f, 0x1,  0x01,0xc0,  E2K_ALOPF11},
    { "FCMPLEdb",  RES_ALS_0134,   0x2f, 0x2,  0x01,0xc0,  E2K_ALOPF11},
    { "FCMPUODdb", RES_ALS_0134,   0x2f, 0x3,  0x01,0xc0,  E2K_ALOPF11},
    { "FCMPNEQdb", RES_ALS_0134,   0x2f, 0x4,  0x01,0xc0,  E2K_ALOPF11},
    { "FCMPNLTdb", RES_ALS_0134,   0x2f, 0x5,  0x01,0xc0,  E2K_ALOPF11},
    { "FCMPNLEdb", RES_ALS_0134,   0x2f, 0x6,  0x01,0xc0,  E2K_ALOPF11},
    { "FCMPODsf" , RES_ALS_0134,   0x2e, 0xc7, 0x01,0xc7,  E2K_ALOPF11},
    { "FCMPODdf" , RES_ALS_0134,   0x2f, 0xc7, 0x01,0xc7,  E2K_ALOPF11},
    { "FCMPUDsf" , RES_ALS_0134,   0x2e, 0xc3, 0x01,0xc3,  E2K_ALOPF11},
    { "FCMPUDdf" , RES_ALS_0134,   0x2f, 0xc3, 0x01,0xc3,  E2K_ALOPF11},
/* --------------------------------------------------- */
    { "FADDs",    RES_ALS_0134,    0x30, 0,    0,   0,     E2K_ALOPF1},
    { "FADDd",    RES_ALS_0134,    0x31, 0,    0,   0,     E2K_ALOPF1},
    { "FSUBs",    RES_ALS_0134,    0x32, 0,    0,   0,     E2K_ALOPF1},
    { "FSUBd",    RES_ALS_0134,    0x33, 0,    0,   0,     E2K_ALOPF1},
    { "FMINs",    RES_ALS_0134,    0x34, 0,    0,   0,     E2K_ALOPF1},
    { "FMINd",    RES_ALS_0134,    0x35, 0,    0,   0,     E2K_ALOPF1},
    { "FMAXs",    RES_ALS_0134,    0x36, 0,    0,   0,     E2K_ALOPF1},
    { "FMAXd",    RES_ALS_0134,    0x37, 0,    0,   0,     E2K_ALOPF1},
/* --------------------------------------------------- */
    { "FMULs",    RES_ALS_0134,    0x38, 0,    0,   0,     E2K_ALOPF1},
    { "FMULd",    RES_ALS_0134,    0x39, 0,    0,   0,     E2K_ALOPF1},
    { "FSTOIS",   RES_ALS_0134,    0x3c, 0xc0, 0,   0,     E2K_ALOPF2},
    { "FSTOIStr", RES_ALS_0134,    0x3c, 0xc2, 0,   0,     E2K_ALOPF2},
    { "ISTOFS",   RES_ALS_0134,    0x3c, 0xc4, 0,   0,     E2K_ALOPF2},
    { "FDTOID",   RES_ALS_0134,    0x3d, 0xc0, 0,   0,     E2K_ALOPF2},
    { "FDTOIDtr", RES_ALS_0134,    0x3d, 0xc2, 0,   0,     E2K_ALOPF2},
    { "IDTOFD",   RES_ALS_0134,    0x3d, 0xc4, 0,   0,     E2K_ALOPF2},
    { "FSTOID",   RES_ALS_0134,    0x3e, 0xc0, 0,   0,     E2K_ALOPF2},
    { "FSTOIDtr", RES_ALS_0134,    0x3e, 0xc2, 0,   0,     E2K_ALOPF2},
    { "ISTOFD",   RES_ALS_0134,    0x3e, 0xc4, 0,   0,     E2K_ALOPF2},
    { "FSTOFD",   RES_ALS_0134,    0x3e, 0xc6, 0,   0,     E2K_ALOPF2},
    { "FDTOIS",   RES_ALS_0134,    0x3f, 0xc0, 0,   0,     E2K_ALOPF2},
    { "FDTOIStr", RES_ALS_0134,    0x3f, 0xc2, 0,   0,     E2K_ALOPF2},
    { "IDTOFS",   RES_ALS_0134,    0x3f, 0xc4, 0,   0,     E2K_ALOPF2},
    { "FDTOFS",   RES_ALS_0134,    0x3f, 0xc6, 0,   0,     E2K_ALOPF2},
    /* Skip FX subops */
/* --------------------------------------------------- */
    { "RRs",      RES_ALS0,        0x3e, 0,    0x01, 0xc0, E2K_ALOPF16},
    { "RRd",      RES_ALS0,        0x3f, 0,    0x01, 0xc0, E2K_ALOPF16},
    /* PSHUFB: opce is src3 here */
    { "PSHUFB",   RES_ALS_0134,    0x4d, 0,    0x0f, 0x00, E2K_ALOPF21},
    /* Skip FX ops */
    { "UDIVs",    RES_ALS5,        0x40, 0,    0,    0, E2K_ALOPF1},
    { "UDIVd",    RES_ALS5,        0x41, 0,    0,    0, E2K_ALOPF1},
    { "SDIVs",    RES_ALS5,        0x42, 0,    0,    0, E2K_ALOPF1},
    { "SDIVd",    RES_ALS5,        0x43, 0,    0,    0, E2K_ALOPF1},
    { "UDIVX",    RES_ALS5,        0x44, 0,    0,    0, E2K_ALOPF1},
    { "UMODX",    RES_ALS5,        0x45, 0,    0,    0, E2K_ALOPF1},
    { "SDIVX",    RES_ALS5,        0x46, 0,    0,    0, E2K_ALOPF1},
    { "SMODX",    RES_ALS5,        0x47, 0,    0,    0, E2K_ALOPF1},
/* --------------------------------------------------- */
    { "MOVTs",    RES_ALS0,        0x60, 0xc0, 0,    0,    E2K_ALOPF2},
    { "MOVTd",    RES_ALS0,        0x61, 0xc0, 0,    0,    E2K_ALOPF2},
    /* Skip DRTOAP */
    { "GETPL",    RES_ALS0,        0x63, 0xf0, 0,    0,    E2K_ALOPF2},
    { "LDb",      RES_ALS_0235,    0x64, 0,    0,    0,    E2K_ALOPF1},
    { "LZCNTs",   RES_ALS_14,      0x64, 0xc0, 0,    0,    E2K_ALOPF2},
    { "LDh",      RES_ALS_0235,    0x65, 0,    0,    0,    E2K_ALOPF1},
    { "LZCNTd",   RES_ALS_14,      0x65, 0xc0, 0,    0,    E2K_ALOPF2},
    { "LDw",      RES_ALS_0235,    0x66, 0,    0,    0,    E2K_ALOPF1},
    { "POPCNTs",  RES_ALS_14,      0x66, 0xc0, 0,    0,    E2K_ALOPF2},
    { "LDd",      RES_ALS_0235,    0x67, 0,    0,    0,    E2K_ALOPF1},
    { "POPCNTd",  RES_ALS_14,      0x67, 0xc0, 0,    0,    E2K_ALOPF2},
/* --------------------------------------------------- */
/* --- Long encoding -------------------------------------------------------- */
/* --------------------------------------------------- */
    /* Skip vector ops */
/* --------------------------------------------------- */
    /* Skip array ops */
/* --------------------------------------------------- */
    { "MULs",     RES_ALS_0134,    0x20, 0,    0x01, 0xc0, E2K_ALOPF11},
    { "MULd",     RES_ALS_0134,    0x21, 0,    0x01, 0xc0, E2K_ALOPF11},
    { "UMULX",    RES_ALS_0134,    0x22, 0,    0x01, 0xc0, E2K_ALOPF11},
    { "SMULX",    RES_ALS_0134,    0x23, 0,    0x01, 0xc0, E2K_ALOPF11},
    { "UMULX_fd", RES_ALS_03,      0x22, 0xc0, 0x06, 0xc0, E2K_ALOPF11},
    { "SMULX_fd", RES_ALS_03,      0x23, 0xc0, 0x06, 0xc0, E2K_ALOPF11},
/* --------------------------------------------------- */
    { "FDIVs",    RES_ALS5,        0x48, 0,    0x01, 0xc0, E2K_ALOPF11},
    { "FDIVd",    RES_ALS5,        0x49, 0,    0x01, 0xc0, E2K_ALOPF11},
/* --------------------------------------------------- */
    { "FSQRTs",   RES_ALS5,        0x4c, 0,    0x01, 0xc0, E2K_ALOPF12},
    { "FSQRTId",  RES_ALS5,        0x4d, 0,    0x01, 0xc0, E2K_ALOPF12},
    { "FSQRTTd",  RES_ALS5,        0x51, 0,    0x01, 0xc0, E2K_ALOPF12},
/* --------------------------------------------------- */
    { "GETSP",    RES_ALS0,        0x58, 0xec, 0x01, 0xc0, E2K_ALOPF12},
/* --------------------------------------------------- */
    { "FSTOIFs",  RES_ALS_0134,    0x6c, 0,    0x01, 0xc0, E2K_ALOPF11},
    { "FDTOIFd",  RES_ALS_0134,    0x6d, 0,    0x01, 0xc0, E2K_ALOPF11},
/* --------------------------------------------------- */
    { "UMULHd",   RES_ALS_0134,    0x70, 0,    0x01, 0xc0, E2K_ALOPF11},
    { "SMULHd",   RES_ALS_0134,    0x71, 0,    0x01, 0xc0, E2K_ALOPF11}
/* --------------------------------------------------- */
};


/* -- Emit basic instructions --------------------------------------------- */

static SLJIT_INLINE sljit_u32 emit_lts(struct sljit_compiler *compiler,
                                       E2kBundle *bundle,
                                       E2kOpT type,
                                       sljit_uw val)
{
    /* 16-bit halfsyls not managed yet, using lo part only */
    sljit_uw mask = 0, lts = 0;
    int lts_idx = 0;
    if (type == E2K_CONST16) {
        mask = RES_LTS1|RES_LTS0;
    } else if (type == E2K_CONST32) {
        mask = RES_LTS3|RES_LTS2|RES_LTS1|RES_LTS0;
    } else {
        mask = RES_LTS2|RES_LTS1|RES_LTS0;
    }

    lts = get_resource(compiler, bundle, mask);
    if (type == E2K_CONST64)    /* check 2 syls for 64-bit literal */
        get_resource(compiler, bundle, lts << 1);
    lts = lts >> RES_LTS_SHIFT;
    while (lts >>= 1)
        lts_idx++;

    switch (type) {
    case E2K_CONST16:
        bundle->lts[lts_idx] = (sljit_u16)(val); break;
    case E2K_CONST32:
        bundle->lts[lts_idx] = (sljit_u32)(val); break;
    case E2K_CONST64:
        bundle->lts[lts_idx] = (sljit_u32)(val);
        bundle->lts[lts_idx+1] = (sljit_u32)(val >> 32);
        bundle->f4++;
        break;
    }
    bundle->f4++;
    return lts_idx;
}

static sljit_u32 emit_src1(struct sljit_compiler *compiler,
                           E2kBundle *bundle,
                           E2kOpT type,
                           sljit_u32 src1)
{
    if (type == E2K_REG) {
        switch (get_reg_type(compiler, bundle, src1)) {
        case E2K_REG_R:
            return reg_map[src1];
        default:
            SLJIT_ASSERT(0);
            return 0;
        }
    } else {
        switch (get_const_type(compiler, bundle, src1)) {
        case E2K_CONST4: case E2K_CONST5:
            return CONST4(src1);
        default:
            SLJIT_ASSERT(0);
            return 0;
        }
    }
}

static sljit_u32 emit_src2(struct sljit_compiler *compiler,
                           E2kBundle *bundle,
                           E2kOpT type,
                           sljit_sw src2,
                           sljit_s32 mode)
{
    if (type == E2K_REG) {
        switch (get_reg_type(compiler, bundle, src2)) {
        case E2K_REG_R:
            return reg_map[src2];
        default:
            SLJIT_ASSERT(0);
            return 0;
        }
    } else {
        if ((mode & FORCE_LTS) == 0) {
            switch (get_const_type(compiler, bundle, src2)) {
            case E2K_CONST4:
                return (sljit_u32)CONST4(src2);
            case E2K_CONST5:
            /* deny CONST16 due to may be coded only in LTS1 and LTS0 */
            case E2K_CONST16:
            case E2K_CONST32:
                return (emit_lts(compiler, bundle, E2K_CONST32, src2) | 0xd8);
            case E2K_CONST64:
                return (emit_lts(compiler, bundle, E2K_CONST64, src2) | 0xdc);
            default:
                SLJIT_ASSERT(0);
                return 0;
            }
        }

        switch (mode & (STORE_MASK | EXTEND_LTS)) {
            case STORE_BYTE:
            case STORE_BYTE | EXTEND_LTS:
            case STORE_HALFWORD:
                return (emit_lts(compiler, bundle, E2K_CONST16, src2) | 0xd0);
            case STORE_WORD:
            case STORE_HALFWORD | EXTEND_LTS:
                return (emit_lts(compiler, bundle, E2K_CONST32, src2) | 0xd8);
            case STORE_WORD | EXTEND_LTS:
            case STORE_DOUBLEWORD:
            case STORE_DOUBLEWORD | EXTEND_LTS:
                return (emit_lts(compiler, bundle, E2K_CONST64, src2) | 0xdc);
            default:
                SLJIT_ASSERT(0);
                return 0;
        }
    }
}

static sljit_u32 emit_src3(struct sljit_compiler *compiler,
                           E2kBundle *bundle,
                           E2kOpT type,
                           sljit_uw src3)
{
    if (type == E2K_REG) {
        switch (get_reg_type(compiler, bundle, src3)) {
        case E2K_REG_R:
            return reg_map[src3];
        default:
            SLJIT_ASSERT(0);
        }
    } else {
        SLJIT_ASSERT(0);
    }
    return 0;
}

static sljit_u32 emit_dst(struct sljit_compiler *compiler,
                          E2kBundle *bundle,
                          E2kOpT type,
                          sljit_uw dst)
{
    if (type == E2K_REG) {
        switch (get_reg_type(compiler, bundle, dst)) {
        case E2K_REG_R:
            return reg_map[dst & 0xFF];
        case E2K_REG_CTPR:
            return ctpr_map[dst & 0xFF] | 0xd0;
        default:
            SLJIT_ASSERT(0);
        }
    } else {
        SLJIT_ASSERT(0);
    }
    return 0;
}

static sljit_u32 emit_pdst(struct sljit_compiler *compiler,
                           E2kBundle *bundle,
                           E2kOpT type,
                           sljit_uw pred)
{
    SLJIT_UNUSED_ARG(compiler);
    SLJIT_UNUSED_ARG(bundle);

    SLJIT_ASSERT(pred & E2K_PRED);
    pred &= 0xFF;

    if (type == E2K_REG_PRED) {
        return pred_map[pred];
    } else {
        SLJIT_ASSERT(0);
        return 0;
    }
}

static sljit_s32 emit_alopf1(struct sljit_compiler *compiler,
                             E2kBundle *bundle,
                             sljit_u32 spec,
                             sljit_u32 op,
                             sljit_u32 src1,
                             sljit_u32 src2,
                             sljit_u32 dst)
{
    sljit_uw mask = e2kop[op].mask;
    sljit_s32 als_idx = get_sylidx(compiler, bundle, mask, RES_ALS_SHIFT);
    E2kAlopf1 syl;
    syl.i = 0;
    syl.fields.dst  = dst;
    syl.fields.src2 = src2;
    syl.fields.src1 = src1;
    syl.fields.cop = e2kop[op].opc;
    syl.fields.spec = spec;

    bundle->als[als_idx] = syl.i;
    bundle->f1++;
    return als_idx;
}

static sljit_s32 emit_alopf2(struct sljit_compiler *compiler,
                             E2kBundle *bundle,
                             sljit_u32 spec,
                             sljit_u32 op,
                             sljit_u32 src2,
                             sljit_u32 dst)
{
    sljit_uw mask = e2kop[op].mask;
    sljit_s32 als_idx = get_sylidx(compiler, bundle, mask, RES_ALS_SHIFT);
    E2kAlopf2 syl;
    syl.i = 0;
    syl.fields.dst  = dst;
    syl.fields.src2 = src2;
    syl.fields.opce = e2kop[op].opce;
    syl.fields.cop = e2kop[op].opc;
    syl.fields.spec = spec;

    bundle->als[als_idx] = syl.i;
    bundle->f1++;
    return als_idx;
}

static sljit_s32 emit_alopf3(struct sljit_compiler *compiler,
                             E2kBundle *bundle,
                             sljit_u32 spec,
                             sljit_u32 op,
                             sljit_u32 src1,
                             sljit_u32 src2,
                             sljit_u32 src3)
{
    sljit_uw mask = e2kop[op].mask;
    sljit_s32 als_idx = get_sylidx(compiler, bundle, mask, RES_ALS_SHIFT);
    E2kAlopf3 syl;
    syl.i = 0;
    syl.fields.src3  = src3;
    syl.fields.src2 = src2;
    syl.fields.src1 = src1;
    syl.fields.cop = e2kop[op].opc;
    syl.fields.spec = spec;

    bundle->als[als_idx] = syl.i;
    bundle->f1++;
    return als_idx;
}

static sljit_s32 emit_alopf7(struct sljit_compiler *compiler,
                             E2kBundle *bundle,
                             sljit_u32 spec,
                             sljit_u32 op,
                             sljit_u32 src1,
                             sljit_u32 src2,
                             sljit_u32 pred)
{
    sljit_uw mask = e2kop[op].mask;
    sljit_s32 als_idx = get_sylidx(compiler, bundle, mask, RES_ALS_SHIFT);
    E2kAlopf7 syl;
    syl.i = 0;
    syl.fields.pdst = pred;
    syl.fields.cmpopce = e2kop[op].opce;
    syl.fields.src2 = src2;
    syl.fields.src1 = src1;
    syl.fields.cop = e2kop[op].opc;
    syl.fields.spec = spec;

    bundle->als[als_idx] = syl.i;
    bundle->f1++;
    return als_idx;
}

static sljit_s32 emit_alopf8(struct sljit_compiler *compiler,
                             E2kBundle *bundle,
                             sljit_u32 spec,
                             sljit_u32 op,
                             sljit_u32 src2,
                             sljit_u32 pred)
{
    sljit_uw mask = e2kop[op].mask;
    sljit_s32 als_idx = get_sylidx(compiler, bundle, mask, RES_ALS_SHIFT);
    E2kAlopf8 syl;
    syl.i = 0;
    syl.fields.pdst = pred;
    syl.fields.cmpopce = e2kop[op].opce;
    syl.fields.src2 = src2;
    syl.fields.opce = 0xc0;     /* Now supported only none */
    syl.fields.cop = e2kop[op].opc;
    syl.fields.spec = spec;

    bundle->als[als_idx] = syl.i;
    bundle->f1++;
    return als_idx;
}

static void emit_alef1(struct sljit_compiler *compiler,
                       E2kBundle *bundle,
                       sljit_u32 op,
                       sljit_u32 src3,
                       sljit_s32 als_idx)
{
    /* No combined operations on 2 and 5 channels */
    sljit_s32 res = 1 << (RES_ALES_SHIFT + als_idx);
    get_resource(compiler, bundle, res);
    if (res) {
        E2kAlef1 syl;
        syl.i = 0;
        syl.fields.src3 = (sljit_u16)src3;
        syl.fields.opc2 = e2kop[op].opc2;
        bundle->ales[als_idx] = syl.i;
        /* This is wrong in case of multiple alef,
           should be revised in the new version. */
        bundle->f3++;
    } else {
        SLJIT_ASSERT(0);
    }
}

static void emit_alef2(struct sljit_compiler *compiler,
                       E2kBundle *bundle,
                       sljit_u32 op,
                       sljit_s32 als_idx)
{
    /* No combined operations on 2 and 5 channels */
    sljit_s32 res = 1 << (RES_ALES_SHIFT + als_idx);
    get_resource(compiler, bundle, res);
    if (res) {
        E2kAlef2 syl;
        syl.i = 0;
        syl.fields.opce2 = e2kop[op].opce2;
        syl.fields.opc2 = e2kop[op].opc2;
        bundle->ales[als_idx] = syl.i;
        bundle->f3++;
    } else {
        SLJIT_ASSERT(0);
    }
}

static sljit_s32 emit_alopf12(struct sljit_compiler *compiler,
                              E2kBundle *bundle,
                              sljit_u32 spec,
                              sljit_u32 op,
                              sljit_u32 src2,
                              sljit_u32 dst)
{
    sljit_s32 als_idx = emit_alopf2(compiler, bundle, spec, op, src2, dst);
    emit_alef2(compiler, bundle, op, als_idx);
    return als_idx;
}

static sljit_s32 emit_alopf21(struct sljit_compiler *compiler,
                              E2kBundle *bundle,
                              sljit_u32 spec,
                              sljit_u32 op,
                              sljit_u32 src1,
                              sljit_u32 src2,
                              sljit_u32 src3,
                              sljit_u32 dst)
{
    sljit_s32 als_idx = emit_alopf1(compiler, bundle, spec, op, src1, src2, dst);
    emit_alef1(compiler, bundle, op, src3, als_idx);
    return als_idx;
}

static sljit_s32 emit_alopf11(struct sljit_compiler* compiler,
                              E2kBundle *bundle,
                              sljit_u32 spec,
                              sljit_u32 op,
                              sljit_u32 src1,
                              sljit_u32 src2,
                              sljit_u32 dst)
{
    sljit_s32 als_idx = emit_alopf1(compiler, bundle, spec, op, src1, src2, dst);
    emit_alef2(compiler, bundle, op, als_idx);
    return als_idx;
}

static sljit_s32 emit_alopf16(struct sljit_compiler *compiler,
                              E2kBundle *bundle,
                              sljit_u32 spec,
                              sljit_u32 op,
                              sljit_u32 reg,
                              sljit_u32 dst)
{
    sljit_s32 als_idx = emit_alopf1(compiler, bundle, spec, op, reg, CONST4(0), dst);
    emit_alef2(compiler, bundle, op, als_idx);
    return als_idx;
}

/* Not used
static sljit_s32 emit_alopf22(struct sljit_compiler *compiler,
                              E2kBundle *bundle,
                              sljit_u32 spec,
                              sljit_u32 op,
                              sljit_u32 src2,
                              sljit_u32 dst)
{
    sljit_s32 als_idx = emit_alopf2(compiler, bundle, spec, op, src2, dst);
    emit_alef2(compiler, bundle, op, als_idx);
    return als_idx;
}
*/

#define pcnt 0x40
#define psrc 0x60

static void emit_cds(struct sljit_compiler *compiler,
                     E2kBundle *bundle,
                     sljit_u16 opc,
                     sljit_u32 als,
                     sljit_u32 pred,
                     sljit_u32 inverted)
{
    sljit_s32 cds_idx;
    E2kCDS syl;

    SLJIT_ASSERT(pred & E2K_PRED);
    pred &= 0xFF;

    cds_idx = get_sylidx(compiler, bundle, RES_CDS_ALL, RES_CDS_SHIFT);
    syl.i = 0;
    syl.fields.pred = (sljit_u16)(pred_map[pred] | psrc);
    switch (1 << als) {
    case 0x1: case 0x8:
        if (inverted) syl.fields.neg = 1;
        syl.fields.mask = 1;
        break;
    case 0x2: case 0x10:
        if (inverted) syl.fields.neg = 2;
        syl.fields.mask = 2;
        break;
    case 0x4: case 0x20:
        if (inverted) syl.fields.neg = 4;
        syl.fields.mask = 4;
        break;
    }
    if (als >= 3) opc++;
    syl.fields.opc = opc;
    bundle->cds[cds_idx] = syl.i;
    bundle->hs_cds++;
}

static void emit_copf1(struct sljit_compiler *compiler,
                       E2kBundle *bundle,
                       sljit_u32 opc,
                       sljit_u32 ctpr,
                       sljit_u32 param)
{
    E2kCopf1 syl;

    SLJIT_ASSERT(ctpr & E2K_CTPR);
    ctpr &= 0xFF;

    get_resource(compiler, bundle, RES_CS0);
    syl.i = 0;
    syl.fields.param = param;
    syl.fields.opc = opc;
    if (ctpr)       /* this is not label for ctpr */
        syl.fields.ctpr = ctpr_map[ctpr];

    bundle->cs[0] = syl.i;
    bundle->f1++;
}

static void emit_copf2(struct sljit_compiler *compiler,
                       E2kBundle *bundle,
                       sljit_u32 opc,
                       sljit_u32 ctpr,
                       sljit_s32 disp)
{
    E2kCopf2 syl;

    SLJIT_ASSERT(ctpr & E2K_CTPR);
    ctpr &= 0xFF;

    get_resource(compiler, bundle, RES_CS0);
    syl.i = 0;
    if (opc == E2K_SDISP) {
        syl.fields.disp = disp;
    } else {
        syl.fields.disp = disp >> 3;
    }
    syl.fields.opc = opc;
    if (ctpr)
      syl.fields.ctpr = ctpr_map[ctpr];

    bundle->cs[0] = syl.i;
    bundle->f1++;
}

#define emit_rlp(compiler, bundle, als, pred, inv) \
  emit_cds(compiler, bundle, 0, als, pred, inv)
#define emit_mrgc(compiler, bundle, als, pred, inv) \
  emit_cds(compiler, bundle, 2, als, pred, inv)

#define ISRC1(src1) emit_src1(compiler, bundle, E2K_CONST,    src1)
#define RSRC1(src1) emit_src1(compiler, bundle, E2K_REG,      src1)
#define ISRC2(src2, mode) emit_src2(compiler, bundle, E2K_CONST,    src2, mode)
#define RSRC2(src2) emit_src2(compiler, bundle, E2K_REG,      src2, DOUBLEWORD)
#define RSRC3(src3) emit_src3(compiler, bundle, E2K_REG,      src3)
#define RDST(dst)   emit_dst (compiler, bundle, E2K_REG,      dst)
#define PDST(pred)  emit_pdst(compiler, bundle, E2K_REG_PRED, pred)

#define emit_alopf1_rr(compiler, bundle, spec, op, src1, src2, dst, mode) \
  emit_alopf1(compiler, bundle, spec, op, RSRC1(src1), RSRC2(src2), RDST(dst))
#define emit_alopf1_ir(compiler, bundle, spec, op, src1, src2, dst, mode) \
  emit_alopf1(compiler, bundle, spec, op, ISRC1(src1), RSRC2(src2), RDST(dst))
#define emit_alopf1_ri(compiler, bundle, spec, op, src1, src2, dst, mode) \
  emit_alopf1(compiler, bundle, spec, op, RSRC1(src1), ISRC2(src2, mode), RDST(dst))
#define emit_alopf1_ii(compiler, bundle, spec, op, src1, src2, dst, mode) \
  emit_alopf1(compiler, bundle, spec, op, ISRC1(src1), ISRC2(src2, mode), RDST(dst))

#define emit_alopf2_i(compiler, bundle, spec, op, src2, dst, mode) \
  emit_alopf2(compiler, bundle, spec, op, ISRC2(src2, mode), RDST(dst))
#define emit_alopf2_r(compiler, bundle, spec, op, src2, dst, mode) \
  emit_alopf2(compiler, bundle, spec, op, RSRC2(src2), RDST(dst))

#define emit_alopf3_rr(compiler, bundle, spec, op, src1, src2, src3, mode) \
  emit_alopf3(compiler, bundle, spec, op, RSRC1(src1), RSRC2(src2), RSRC3(src3))
#define emit_alopf3_ir(compiler, bundle, spec, op, src1, src2, src3, mode) \
  emit_alopf3(compiler, bundle, spec, op, ISRC1(src1), RSRC2(src2), RSRC3(src3))
#define emit_alopf3_ri(compiler, bundle, spec, op, src1, src2, src3, mode) \
  emit_alopf3(compiler, bundle, spec, op, RSRC1(src1), ISRC2(src2, mode), RSRC3(src3))
#define emit_alopf3_ii(compiler, bundle, spec, op, src1, src2, src3, mode) \
  emit_alopf3(compiler, bundle, spec, op, ISRC1(src1), ISRC2(src2, mode), RSRC3(src3))

#define emit_alopf7_rr(compiler, bundle, spec, op, src1, src2, pred, mode) \
  emit_alopf7(compiler, bundle, spec, op, RSRC1(src1), RSRC2(src2), PDST(pred))
#define emit_alopf7_ri(compiler, bundle, spec, op, src1, src2, pred, mode) \
  emit_alopf7(compiler, bundle, spec, op, RSRC1(src1), ISRC2(src2, mode), PDST(pred))
#define emit_alopf7_ir(compiler, bundle, spec, op, src1, src2, pred, mode) \
  emit_alopf7(compiler, bundle, spec, op, ISRC1(src1), RSRC2(src2), PDST(pred))

#define emit_alopf8_r(compiler, bundle, spec, op, src2, pred, mode) \
  emit_alopf8(compiler, bundle, spec, op, RSRC2(src2), PDST(pred))
#define emit_alopf8_i(compiler, bundle, spec, op, src2, pred, mode) \
  emit_alopf8(compiler, bundle, spec, op, ISRC2(src2, mode), PDST(pred))

#define emit_alopf12_i(compiler, bundle, spec, op, src2, dst, mode) \
  emit_alopf12(compiler, bundle, spec, op, ISRC2(src2, mode), RDST(dst))
#define emit_alopf12_r(compiler, bundle, spec, op, src2, dst, mode) \
  emit_alopf12(compiler, bundle, spec, op, RSRC2(src2), RDST(dst))

#define emit_alopf11_rr(compiler, bundle, spec, op, src1, src2, dst, mode) \
  emit_alopf11(compiler, bundle, spec, op, RSRC1(src1), RSRC2(src2), RDST(dst))
#define emit_alopf11_ir(compiler, bundle, spec, op, src1, src2, dst, mode) \
  emit_alopf11(compiler, bundle, spec, op, ISRC1(src1), RSRC2(src2), RDST(dst))
#define emit_alopf11_ri(compiler, bundle, spec, op, src1, src2, dst, mode) \
  emit_alopf11(compiler, bundle, spec, op, RSRC1(src1), ISRC2(src2, mode), RDST(dst))
#define emit_alopf11_ii(compiler, bundle, spec, op, src1, src2, dst, mode) \
  emit_alopf11(compiler, bundle, spec, op, ISRC1(src1), ISRC2(src2, mode), RDST(dst))

#define emit_alopf21_rrr(compiler, bundle, spec, op, src1, src2, src3, dst, mode) \
  emit_alopf21(compiler, bundle, spec, op, RSRC1(src1), RSRC2(src2), RSRC3(src3), RDST(dst))
#define emit_alopf21_rir(compiler, bundle, spec, op, src1, src2, src3, dst, mode) \
  emit_alopf21(compiler, bundle, spec, op, RSRC1(src1), ISRC2(src2, mode), RSRC3(src3), RDST(dst))

#define emit_nop(compiler, bundle, nops) \
  bundle->nop = nops


/* -- Stepler ------------------------------------------------------------- */
static void emit_bundle_setup(struct sljit_compiler *compiler,
                              E2kBundle *bundle);
static void emit_bundle_finalize(struct sljit_compiler *compiler,
                                 E2kBundle *bundle,
                                 sljit_u32 *target_addr);
static sljit_s32 try_lift_disp(struct sljit_compiler *compiler,
                               E2kBundle *bundle);
static void emit_bundle_optimize_and_finalize(struct sljit_compiler *compiler,
                                              E2kBundle *bundle);
static void push_bundle_track(struct sljit_compiler *compiler,
                              E2kBundle *bundle,
                              sljit_u32 *start_addr,
                              sljit_sw offset);
static sljit_u32 bundle_size_in_words(E2kBundle *b);
static SLJIT_INLINE sljit_uw check_resource(struct sljit_compiler *compiler,
                                            E2kBundle *bundle,
                                            sljit_uw mask);

#define BUNDLE_CLOSE() \
        do { \
            stepler_setup(); \
            if (_BUNDLE.f1 > 1) { \
                emit_bundle_finalize(compiler, &_BUNDLE, (void*)0); \
            } \
        } while(0)

#define BUNDLE_DECLARATION() \
        E2kBundle *bundle = &_BUNDLE

#define BUNDLE_SETUP() \
        E2kBundle *bundle = &_BUNDLE; \
        BUNDLE_CLOSE(); \
        emit_bundle_setup(compiler, bundle)

static E2kBundle _BUNDLE;

/* Allowed stepler op */
static sljit_s32 STEPLER_OPS[] = {
    SLJIT_MOV, SLJIT_MOV_P,
    SLJIT_MOV32_U8, SLJIT_MOV_U8,
    SLJIT_MOV32_S8, SLJIT_MOV_S8,
    SLJIT_MOV32_U16, SLJIT_MOV_U16,
    SLJIT_MOV32_S16, SLJIT_MOV_S16,
    SLJIT_MOV_U32, SLJIT_MOV_S32,
    SLJIT_MOV32,
    SLJIT_ADD, SLJIT_SUB,
    SLJIT_AND, SLJIT_OR, SLJIT_XOR,
    SLJIT_MSHL, SLJIT_SHL, SLJIT_MLSHR, SLJIT_LSHR,
    SLJIT_MASHR, SLJIT_ASHR, SLJIT_ROTL, SLJIT_ROTR};

#define STEPLER_OPS_LEN (sljit_s32)(sizeof(STEPLER_OPS) / sizeof(STEPLER_OPS[0]))
#define STEPLER_WIDTH E2K_CHANNEL_NUMBER

typedef struct Stepler {
    /* generic data analysis */
    sljit_sw dst_list[STEPLER_WIDTH];
    sljit_sw src1_list[STEPLER_WIDTH];
    sljit_sw src2_list[STEPLER_WIDTH];
    sljit_sw src3_list[STEPLER_WIDTH];
    /* specify src type: reg or imm */
    sljit_sw src1_is_reg_list[STEPLER_WIDTH];
    sljit_sw src2_is_reg_list[STEPLER_WIDTH];
    sljit_sw src3_is_reg_list[STEPLER_WIDTH];
    /* current data real len */
    sljit_s32 len;
    /* mem ops management */
    sljit_sw is_ld_list[STEPLER_WIDTH];
    sljit_sw is_st_list[STEPLER_WIDTH];
    sljit_s32 ld_len;
    sljit_s32 st_len;
} Stepler;

static Stepler STEPLER;

static void stepler_setup(void)
{
    STEPLER.len  = 0;
    STEPLER.ld_len = 0;
    STEPLER.st_len = 0;
}

/* Check for sljit IR restrictions for bundling */
static void stepler_high_level_check(struct sljit_compiler *compiler,
                                     sljit_s32 op,
                                     sljit_s32 dst,
                                     sljit_uw dstw,
                                     sljit_s32 src1,
                                     sljit_uw src1w,
                                     sljit_s32 src2,
                                     sljit_uw src2w)
{
    sljit_s32 len = STEPLER.len;
    sljit_s32 allowed = 0;

    SLJIT_UNUSED_ARG(src1w);
    SLJIT_UNUSED_ARG(src2w);
    SLJIT_UNUSED_ARG(dstw);

    /* Allowed op */
    for (sljit_s32 i = 0; i < STEPLER_OPS_LEN; i++) {
        if (op == STEPLER_OPS[i]) {
            allowed = 1;
            break;
        }
    }

    /* only IMM and REG */
    SLJIT_UNUSED_ARG(src1);
    SLJIT_UNUSED_ARG(src2);

    /* no dst conflict */
    if (STEPLER.len && (STEPLER.dst_list[len-1] == dst)) {
        allowed = 0;
    }

    if (!allowed) {
        BUNDLE_CLOSE();
    }

    return;
}

/* low level op */
static sljit_s32 stepler_check_op(struct sljit_compiler *compiler,
                                  sljit_s32 emit_flags,
                                  sljit_s32 op,
                                  sljit_sw src1,
                                  sljit_sw src2,
                                  sljit_sw src3,
                                  sljit_sw dst)
{
    sljit_s32 allowed = 1;
    sljit_s32 is_ld = 0;
    sljit_s32 is_st = 0;
    sljit_s32 src1_is_reg = ((emit_flags & REG_SRC1) > 0);
    sljit_s32 src2_is_reg = ((emit_flags & REG_SRC2) > 0);
    sljit_s32 src3_is_reg = ((emit_flags & REG_SRC3) > 0);

    SLJIT_UNUSED_ARG(src3);
    SLJIT_UNUSED_ARG(dst);
    SLJIT_UNUSED_ARG(emit_flags);

    if (!check_resource(compiler, &_BUNDLE, e2kop[op].mask)) {
        allowed = 0;
    }

    if (!(emit_flags & STEPLER_CHECK_RESOURCE_ONLY)) {
        if ((op >= E2K_LDb) && (op <= E2K_LDd)) {
            is_ld = 1;
        }
        if ((op >= E2K_STb) && (op <= E2K_STd)) {
            is_st = 1;
        }

        /* RAW check */
        for (sljit_s32 d = 0; d < STEPLER.len; d++) {
            /* Prevent the imm from mimicking a reg */
            if (((src1_is_reg) && (STEPLER.dst_list[d] == src1))
                ||
                ((src2_is_reg) && (STEPLER.dst_list[d] == src2))) {
                allowed = 0;
                break;
            }
            if (is_st) {
                if ((src3_is_reg) && (STEPLER.dst_list[d] == src3)) {
                    allowed = 0;
                    break;
                }
            }
        }

        /* LTS resource */
        if (_BUNDLE.f4 > 2) {
            allowed = 0;
        }

        /* B.1.1 ST/LD restrictions */
        if (is_st && STEPLER.ld_len && !(_BUNDLE.res & RES_ALS3)) {
            allowed = 0;
        }
        if (is_st && STEPLER.ld_len && !(_BUNDLE.res & RES_ALS0)) {
            allowed = 0;
        }
        if (is_ld && STEPLER.st_len && !(_BUNDLE.res & RES_ALS2)) {
            allowed = 0;
        }
        if (is_ld && STEPLER.st_len && !(_BUNDLE.res & RES_ALS5)) {
            allowed = 0;
        }
    }

    return allowed;
}

/* low level op */
static sljit_s32 stepler_add_op(struct sljit_compiler *compiler,
                                sljit_s32 emit_flags,
                                sljit_s32 op,
                                sljit_sw src1,
                                sljit_sw src2,
                                sljit_sw src3,
                                sljit_sw dst)
{
    sljit_s32 len  = STEPLER.len;

    sljit_s32 allowed = 1;
    sljit_s32 is_ld = 0;
    sljit_s32 is_st = 0;
    SLJIT_UNUSED_ARG(compiler);

    if ((op >= E2K_LDb) && (op <= E2K_LDd)) {
        is_ld = 1;
    }
    if ((op >= E2K_STb) && (op <= E2K_STd)) {
        is_st = 1;
    }

    STEPLER.dst_list[len]   = dst;
    STEPLER.src1_list[len]  = src1;
    STEPLER.src2_list[len]  = src2;
    STEPLER.src3_list[len]  = src3;

    /* mem ops management */
    STEPLER.is_ld_list[len] = 0;
    STEPLER.is_st_list[len] = 0;
    if (is_ld) {
        STEPLER.is_ld_list[len] = 1;
        STEPLER.ld_len++;
    }
    if (is_st) {
        STEPLER.is_st_list[len] = 1;
        STEPLER.st_len++;
    }

    /* src type */
    STEPLER.src1_is_reg_list[len] = ((emit_flags & REG_SRC1) > 0);
    STEPLER.src2_is_reg_list[len] = ((emit_flags & REG_SRC2) > 0);
    STEPLER.src3_is_reg_list[len] = ((emit_flags & REG_SRC3) > 0);

    STEPLER.len++;
    return allowed;
}

/* low level op */
static sljit_s32 stepler_check_add_op(struct sljit_compiler *compiler,
                                      sljit_s32 emit_flags,
                                      sljit_s32 op,
                                      sljit_sw src1,
                                      sljit_sw src2,
                                      sljit_sw src3,
                                      sljit_sw dst)
{
    sljit_s32 allowed = 1;
    allowed = stepler_check_op(compiler, emit_flags, op, src1, src2, src3, dst);

    if (!allowed) {
        if (_BUNDLE.f1 > 1) {
            BUNDLE_CLOSE();
        }
    }

    stepler_add_op(compiler, emit_flags, op, src1, src2, src3, dst);

    return allowed;
}

/* -- Bundling ------------------------------------------------------------ */
/* Backstep optimizing area for disp lifting */
#define DISP_LIFT_MAX_TRACK 3

typedef struct {
    sljit_u32   *start_addr;    /* real addr in buf */
    sljit_uw    offset;         /* frag offset */
    E2kBundle   bundle;
} BundleTrackEntry;

typedef struct {
    BundleTrackEntry entries[DISP_LIFT_MAX_TRACK];
    sljit_s32 count;
    struct sljit_memory_fragment *tracker_frag;
} BundleTracker;

/* init tracker */
static BundleTracker bundle_tracker = {
    .entries = {0},
    .count = 0,
    .tracker_frag = NULL
};

static SLJIT_INLINE void emit_bundle_setup(struct sljit_compiler *compiler,
                                           E2kBundle *bundle)
{
    SLJIT_UNUSED_ARG(compiler);
    SLJIT_ZEROMEM(bundle, sizeof(E2kBundle));
    bundle->res = RES_INIT;
    bundle->f1 = 1;     /* HS itself */
}

/* Bundle window for disp lift */
static SLJIT_INLINE void push_bundle_track(struct sljit_compiler *compiler,
                                           E2kBundle *bundle,
                                           sljit_u32 *start_addr,
                                           sljit_sw offset)
{
    /* Fragment boundary is strong limit */
    if ((bundle_tracker.count > 0)
            && (compiler->buf != bundle_tracker.tracker_frag)) {
        bundle_tracker.count = 0;
    }
    bundle_tracker.tracker_frag = compiler->buf;

    if (bundle_tracker.count >= DISP_LIFT_MAX_TRACK) {
        memmove(bundle_tracker.entries,
                bundle_tracker.entries + 1,
                sizeof(BundleTrackEntry) * (bundle_tracker.count - 1));
        bundle_tracker.count--;
    }
    bundle_tracker.entries[bundle_tracker.count].start_addr = start_addr;
    bundle_tracker.entries[bundle_tracker.count].offset = offset;
    memcpy(&bundle_tracker.entries[bundle_tracker.count].bundle, bundle, sizeof(E2kBundle));
    bundle_tracker.count++;
}

/* Try move disp upper else fallback without current bundle change */
static sljit_s32 try_lift_disp(struct sljit_compiler *compiler,
                               E2kBundle *bundle)
{
    struct sljit_jump *jump;
    sljit_s32 bt_count = bundle_tracker.count;
    sljit_u32 target_orig_size, target_new_size, target_diff_size;
    sljit_u32 *target_buf;
    E2kBundle *target_bundle_tmp = NULL;
    E2kBundle *target_bundle = NULL;
    sljit_s32 target_entry_idx = -1;
    sljit_u32 new_jump_offset;
    struct sljit_jump *jump_tmp = NULL;
    struct sljit_jump *jump_end = NULL;

    sljit_s32 is_return = (((bundle->cs[0] >> 28) & 0x3) == E2K_RETURN);

    /* Nothing to lift */
    if (bundle->res & RES_CS0) {
        return bundle->f1 > 1;
    }

    /* lift after set_jump only */
    if (!is_return) {
        SLJIT_ASSERT(compiler->last_jump);
    }
    jump = compiler->last_jump;

    /* Check for fragment bound crossing */
    if ((bt_count < 1) || (compiler->buf != bundle_tracker.tracker_frag)) {
        return bundle->f1 > 1;
    }

    /* Find optimal bundle */
    target_entry_idx = bt_count - 1;
    while (target_entry_idx >= 0) {
        target_bundle_tmp = &bundle_tracker.entries[target_entry_idx].bundle;

        /* Deny control preparation mix */
        if ((target_bundle_tmp->res & RES_SS) == 0) {
            break;
        }

        /* multi disp or setr (setwd, etc.) */
        if (((target_bundle_tmp->res & RES_CS0) == 0)
                || ((target_bundle_tmp->res & RES_CS1) == 0)) {
            break;
        }

        /* Deny forcing label bound */
        if (compiler->last_label) {
            if (bundle_tracker.entries[target_entry_idx].offset <= compiler->last_label->size) {
                break;
            }
        }

        /* Deny forcing last const bound */
        if (compiler->last_const) {
            if (bundle_tracker.entries[target_entry_idx].offset <= compiler->last_const->addr) {
                break;
            }
        }

        /* Deny break patched bundles, i.e. sljit_emit_op_addr.
           Skip last jump. */
        jump_tmp = compiler->jumps;
        if (!is_return) {
            jump_end = compiler->last_jump;
        }
        while (jump_tmp != jump_end) {
            if (bundle_tracker.entries[target_entry_idx].offset <= jump_tmp->addr) {
                break;
            }
            jump_tmp = jump_tmp->next;
        }
        if (jump_tmp != jump_end) {
            break;
        }

        target_bundle = target_bundle_tmp;
        target_entry_idx--;
    }
    if (!target_bundle) {
        return bundle->f1 > 1;
    }
    target_entry_idx++;         /* Restore actual idx */

    /* Check for fragment bound, max extra size */
    if ((compiler->buf->used_size + 2*sizeof(sljit_s32)) >= BUF_SIZE) {
        return bundle->f1 > 1;
    }

    /* Insert disp upper */
    target_orig_size = bundle_size_in_words(target_bundle);
    if (!is_return) {
        emit_copf2(compiler, target_bundle, E2K_DISP, 1 | E2K_CTPR, 0);
    } else {
        emit_copf1(compiler, target_bundle, E2K_RETURN, E2K_CTPR | 3, 0);
    }
    target_new_size = bundle_size_in_words(target_bundle);
    target_diff_size = target_new_size - target_orig_size;

    /* Remove disp in current bundle.
       Supported only RES_CS0 now. */
    bundle->res |= RES_CS0;
    bundle->f1--;
    bundle->cs[0] = 0;

    if (!is_return) {
        new_jump_offset = (sljit_u32)(target_bundle->f1 - 1);
        jump->addr = bundle_tracker.entries[target_entry_idx].offset;
        /* Not use set_jump to prevent next overwriting */
        jump->flags = ((sljit_u32)jump->flags & SLJIT_REWRITABLE_JUMP)
                      | IS_DISP | (new_jump_offset << 2);
    }

    /* Bundles move right step */
    if (target_diff_size) {
        ensure_buf(compiler, sizeof(sljit_u32)*target_diff_size);
        compiler->size += sizeof(sljit_u32)*target_diff_size;
    }

    target_buf = bundle_tracker.entries[target_entry_idx].start_addr;
    emit_bundle_finalize(compiler, target_bundle, target_buf);
    target_entry_idx++;

    while (target_entry_idx < bt_count) {
        target_buf = bundle_tracker.entries[target_entry_idx].start_addr
                     + target_diff_size;
        target_bundle_tmp = &bundle_tracker.entries[target_entry_idx].bundle;
        emit_bundle_finalize(compiler, target_bundle_tmp, target_buf);
        target_entry_idx++;
    }

    /* reset counter to prevent something wrong */
    bundle_tracker.count = 0;

    /* prevent extra nop */
    return bundle->f1 > 1;
}

/* Try optimize bundle and then finalize */
static void emit_bundle_optimize_and_finalize(struct sljit_compiler *compiler,
                                              E2kBundle *bundle)
{
    sljit_s32 bundle_non_empty;

    /* Lift disp to prevent prep-ct lock */
    bundle_non_empty = try_lift_disp(compiler, bundle);

    if (bundle_non_empty) {
        emit_bundle_finalize(compiler, bundle, NULL);
    }
}

/* inline memset in emit_bundle_setup */
__attribute__((optimize("-O4")))
static void emit_bundle_finalize(struct sljit_compiler *compiler,
                                 E2kBundle *bundle,
                                 sljit_u32 *target_addr)
{
    sljit_u32 *inst;
    sljit_u16 *inst16;
    sljit_u32 *bundle_addr;
    sljit_u32 used_lts;
    sljit_s32 i;
    sljit_uw compiler_size_orig;

    sljit_u32 hs_x_s_sw = (bundle->res & RES_SS) ? 0 : 0x2;
    sljit_u32 hs_c =    (sljit_u32)((~bundle->res & RES_CS_ALL) >> RES_CS_SHIFT);
    sljit_u32 hs_cds =  (bundle->hs_cds + 1) & (sljit_u32)-2;
    sljit_u32 hs_ales = (sljit_u32)((~bundle->res & RES_ALES_ALL) >> RES_ALES_SHIFT);
    sljit_u32 hs_als =  (sljit_u32)(~bundle->res & RES_ALS_ALL);

    sljit_u8 f1 =       bundle->f1;
    sljit_u8 f2 =       bundle->f2;
    sljit_u8 half_pad = (sljit_u8)(bundle->f3 & 0x1);
    sljit_u8 f3 =       (sljit_u8)((bundle->f3 >> 1) + half_pad);
    sljit_u8 f4 =       (sljit_u8)(bundle->f4 + (hs_cds >> 1));
    sljit_u8 lng =      (sljit_u8)(f1 + f2 + f3 + f4);
    sljit_u8 hs_lng =   (sljit_u8)((lng + 1) & (sljit_u32)-2);

    E2kHS hs;
    hs.i = 0;
    hs.fields.mdl = f1 + f2 - 1;
    hs.fields.lng = (hs_lng >> 1) - 1;
    hs.fields.nop = bundle->nop;
    hs.fields.x_s_sw = hs_x_s_sw;
    hs.fields.c = hs_c;
    hs.fields.cds = hs_cds >> 1;
    hs.fields.ales = hs_ales;
    hs.fields.als = hs_als;

    if (target_addr == NULL) {
        inst = (sljit_u32*)ensure_buf(compiler, sizeof(sljit_u32)*hs_lng);
    } else {
        inst = target_addr;
    }
    bundle_addr = inst;
    compiler_size_orig = compiler->size;
    if (target_addr == NULL) {
        compiler->size += sizeof(sljit_u32)*hs_lng;
    }

    /* --- f1 ---------------------------------------------------------------*/
    /* HS and SS */
    *inst++ = hs.i;
    if (hs_x_s_sw) *inst++ = bundle->ss;
    /* ALS[0-5], 32-bit */
    if (hs_als & 0x01) *inst++ = bundle->als[0];
    if (hs_als & 0x02) *inst++ = bundle->als[1];
    if (hs_als & 0x04) *inst++ = bundle->als[2];
    if (hs_als & 0x08) *inst++ = bundle->als[3];
    if (hs_als & 0x10) *inst++ = bundle->als[4];
    if (hs_als & 0x20) *inst++ = bundle->als[5];
    /* CS0, 32-bit */
    if (hs_c & 0x01) *inst++ = bundle->cs[0];
    /* --- f1 ---------------------------------------------------------------*/

    /* --- f2 ---------------------------------------------------------------*/
    /*
        ALES[2-5], 16-bit
        Now ALES2 and ALES5 (v4+) are not supported, wait for real necessity.
        inst16 = (sljit_u16 *)inst;
        if (hs_ales & 0x04) *inst16++ = bundle->ales[2];
        if (hs_ales & 0x20) *inst16++ = bundle->ales[5];
        if (half_pad) *inst16++ = 0;
        inst = (sljit_u32*)inst16;
    */
    /* CS1, 32-bit */
    if (hs_c & 0x02) *inst++ = bundle->cs[1];
    /* --- f2 ---------------------------------------------------------------*/

    /* --- f3 ---------------------------------------------------------------*/
    /* ALES[0-1,3-4], 16-bit */
    inst16 = (sljit_u16 *)inst;

    /* Decoder works with full words, so we should write padding as lower
       half-word, before ales. */
    if (half_pad) *inst16++ = 0;    /* Align */

    if (hs_ales & 0x01) *inst16++ = bundle->ales[0];
    if (hs_ales & 0x02) *inst16++ = bundle->ales[1];
    if (hs_ales & 0x08) *inst16++ = bundle->ales[3];
    if (hs_ales & 0x10) *inst16++ = bundle->ales[4];
    inst = (sljit_u32*)inst16;

    if (hs_lng != lng) *inst++ = 0; /* Align */
    /* --- f3 ---------------------------------------------------------------*/

    /* --- f4 ---------------------------------------------------------------*/
    /* LTS[3-0], 32-bit, 16-bit half syls not implemented */
    used_lts = (sljit_u32)((~bundle->res & RES_LTS_ALL) >> RES_LTS_SHIFT);
    for (i = 3; i >= 0; i--) {
        if (used_lts & (1 << i)) {
            *inst++ = bundle->lts[i];
        }
    }

    /* PLS not suppoted in current version */

    /* CDS[0-5], 16-bit */
    inst16 = (sljit_u16 *)inst;
    if (hs_cds > 4) {
      *inst16++ = bundle->cds[5];
      *inst16++ = bundle->cds[4];
    }
    if (hs_cds > 2) {
      *inst16++ = bundle->cds[3];
      *inst16++ = bundle->cds[2];
    }
    if (hs_cds > 0) {
      *inst16++ = bundle->cds[1];
      *inst16++ = bundle->cds[0];
    }
    inst = (sljit_u32*)inst16;
    /* --- f4 ---------------------------------------------------------------*/

    if (target_addr == NULL) {
        push_bundle_track(compiler, bundle, bundle_addr, compiler_size_orig);
    } else {
        bundle_tracker.count = 0;
    }

    emit_bundle_setup(compiler, bundle);
}

/* -- Middle level -------------------------------------------------------- */
static void emit_nop_full(struct sljit_compiler *compiler,
                          sljit_s32 count)
{
    BUNDLE_SETUP();
    emit_nop(compiler, bundle, count);
    emit_bundle_finalize(compiler, bundle, NULL);
}

/* Use stepler mechanism carefully. Stepler is used inside of emit_op function.
   Try do not use stepler in other cases.
   This one can provide more effective bundling and less errors. */

/* Without MEM. Only imm and reg. Dst is always reg. */
static void emit_op(struct sljit_compiler* compiler,
                    E2kBundle *bundle,
                    sljit_s32 opcode,
                    sljit_s32 emit_flags,
                    sljit_sw src1,
                    sljit_sw src2,
                    sljit_sw src3,
                    sljit_sw dst,
                    sljit_s32 mode)
{
    sljit_u32 format, src_dst_mask, src_type;
    sljit_u32 src1u32, src1r, src3r, dstr;
    sljit_sw src2r;

    if ((dst != -1) && !(emit_flags & DST_CTPR)) {
        SLJIT_ASSERT(SLJIT_IS_REG(dst));
    }

    /* Filter correct src_type from op emit_flags.
       Input must be 3bit (src1, src2, src3), but opcode formats should be
       transformed into correct bit length: 2bit, 1bit. */
    format = e2kop[opcode].format;
    src_dst_mask = e2kfmt[format].src_dst_mask;   /* real used bits */
    src_type = emit_flags & src_dst_mask;

    src1u32 = (sljit_u32)src1;
    src1r = (sljit_u32)SLJIT_EXTRACT_REG(src1);
    src2r = SLJIT_EXTRACT_REG(src2);
    src3r = (sljit_u32)SLJIT_EXTRACT_REG(src3);
    dstr = (sljit_u32)SLJIT_EXTRACT_REG(dst);

    stepler_check_add_op(compiler, emit_flags,
                         opcode, src1, src2, src3, dst);

    switch(format) {
        case E2K_ALOPF1:
            SLJIT_ASSERT(src3 == 0);
            switch (src_type) {
                case REG_SRC1 | REG_SRC2:
                    emit_alopf1_rr(compiler, bundle, 0, opcode,
                                   src1r, src2r, dstr, mode);
                    break;
                case REG_SRC1:
                    emit_alopf1_ri(compiler, bundle, 0, opcode,
                                   src1r, src2, dstr, mode);
                    break;
                case REG_SRC2:
                    emit_alopf1_ir(compiler, bundle, 0, opcode,
                                   src1u32, src2r, dstr, mode);
                    break;
                case REG_ZERO:
                    emit_alopf1_ii(compiler, bundle, 0, opcode,
                                   src1u32, src2, dstr, mode);
                    break;
                default:
                    SLJIT_ASSERT(0);
                    break;
            }
            break;
        case E2K_ALOPF2:
            SLJIT_ASSERT(src1 == 0);
            SLJIT_ASSERT(src3 == 0);
            switch (src_type) {
                case REG_SRC2 | DST_CTPR:
                    SLJIT_ASSERT(dst & E2K_CTPR);
                    emit_alopf2_r(compiler, bundle, 0, opcode,
                                  SLJIT_EXTRACT_REG(src2),
                                  dst,
                                  mode);
                    break;
                case REG_SRC2:
                    emit_alopf2_r(compiler, bundle, 0, opcode,
                                  SLJIT_EXTRACT_REG(src2),
                                  SLJIT_EXTRACT_REG(dst),
                                  mode);
                    break;
                case REG_ZERO | DST_CTPR:
                    SLJIT_ASSERT(dst & E2K_CTPR);
                    emit_alopf2_i(compiler, bundle, 0, opcode,
                                  src2,
                                  dst,
                                  mode);
                    break;
                case REG_ZERO:
                    emit_alopf2_i(compiler, bundle, 0, opcode,
                                  src2,
                                  SLJIT_EXTRACT_REG(dst),
                                  mode);
                    break;
            }
            break;
        case E2K_ALOPF3:
            SLJIT_ASSERT(SLJIT_IS_REG(src3));
            switch (src_type) {
                case REG_SRC1 | REG_SRC2 | REG_SRC3:
                    emit_alopf3_rr(compiler, bundle, 0, opcode,
                                   src1r, src2r, src3r, mode);
                    break;
                case REG_SRC1 | REG_SRC3:
                    emit_alopf3_ri(compiler, bundle, 0, opcode,
                                   src1r, src2, src3r, mode);
                    break;
                case REG_SRC2 | REG_SRC3:
                    emit_alopf3_ir(compiler, bundle, 0, opcode,
                                   src1u32, src2r, src3r, mode);
                    break;
                case REG_SRC3:
                    emit_alopf3_ii(compiler, bundle, 0, opcode,
                                   src1u32, src2, src3, mode);
                    break;
                default:
                    SLJIT_ASSERT(0);
                    break;
            }
            break;
        case E2K_ALOPF7:
            SLJIT_ASSERT(0);
            break;
        case E2K_ALOPF8:
            SLJIT_ASSERT(0);
            break;
        case E2K_ALOPF10:
            SLJIT_ASSERT(0);
            break;
        case E2K_ALOPF11:
            switch (src_type) {
                case REG_SRC1 | REG_SRC2:
                    emit_alopf11_rr(compiler, bundle, 0, opcode,
                                   src1r, src2r, dstr, mode);
                    break;
                case REG_SRC1:
                    emit_alopf11_ri(compiler, bundle, 0, opcode,
                                   src1r, src2, dstr, mode);
                    break;
                case REG_SRC2:
                    emit_alopf11_ir(compiler, bundle, 0, opcode,
                                   src1u32, src2r, dstr, mode);
                    break;
                case REG_ZERO:
                    emit_alopf11_ii(compiler, bundle, 0, opcode,
                                   src1u32, src2, dstr, mode);
                    break;
            }
            break;
        case E2K_ALOPF12:
            switch (src_type) {
                case REG_SRC2:
                    emit_alopf12_r(compiler, bundle, 0, opcode,
                                   SLJIT_EXTRACT_REG(src2),
                                   dst,
                                   mode);
                    break;
                case REG_ZERO:
                    emit_alopf12_i(compiler, bundle, 0, opcode,
                                   src2,
                                   dst,
                                   mode);
                    break;
            }
            break;
        case E2K_ALOPF13:
            SLJIT_ASSERT(0);
            break;
        case E2K_ALOPF15:
            SLJIT_ASSERT(0);
            break;
        case E2K_ALOPF16:
            SLJIT_ASSERT(0);
            break;
        case E2K_ALOPF17:
            SLJIT_ASSERT(0);
            break;
        case E2K_ALOPF19:
            SLJIT_ASSERT(0);
            break;
        case E2K_ALOPF21:
            switch (src_type) {
                case REG_SRC1 | REG_SRC2 | REG_SRC3:
                    emit_alopf21_rrr(compiler, bundle, 0, opcode,
                                   src1r, src2r, src3r, dstr, mode);
                    break;
                case REG_SRC1 | REG_SRC3:
                    emit_alopf21_rir(compiler, bundle, 0, opcode,
                                   src1r, src2, src3r, dstr, mode);
                    break;
                default:
                    SLJIT_ASSERT(0);
                    break;
            }
            break;
        case E2K_ALOPF22:
            SLJIT_ASSERT(0);
            break;
    }

}

/* Without MEM. Only imm and reg. Dst is always reg. */
static SLJIT_INLINE void emit_op_full(struct sljit_compiler* compiler,
                                      sljit_s32 opcode,
                                      sljit_s32 emit_flags,
                                      sljit_sw src1,
                                      sljit_sw src2,
                                      sljit_sw src3,
                                      sljit_sw dst,
                                      sljit_s32 mode)
{
    BUNDLE_SETUP();
    emit_op(compiler, bundle, opcode, emit_flags, src1, src2, src3, dst, mode);
    BUNDLE_CLOSE();
}

/* -- Emit control-flow instructions -------------------------------------- */
static void emit_ct(struct sljit_compiler *compiler,
                    struct E2kBundle *bundle,
                    sljit_u32 ctpr,
                    sljit_u32 pred,
                    int inverted)
{
    E2kSS syl;

    SLJIT_ASSERT(ctpr & E2K_CTPR);
    ctpr &= 0xFF;
    SLJIT_ASSERT(pred & E2K_PRED);
    pred &= 0xFF;

    get_resource(compiler, bundle, RES_SS); /* Uses only part of RES_SS */
    syl.i = 0;
    if (pred) {     /* conditional */
        if (inverted) {
            syl.fields.ctcond = 0x60 + pred_map[pred];
        } else {
            syl.fields.ctcond = 0x40 + pred_map[pred];
        }
    } else {
        syl.fields.ctcond = 0x20;   /* unconditional */
    }

    syl.fields.ctop = ctpr_map[ctpr];
    syl.fields.ipd = 3;

    bundle->ss = syl.i;
    bundle->f1++;
}

static void emit_ct_full(struct sljit_compiler *compiler,
                         sljit_u32 ctpr,
                         sljit_u32 pred,
                         int inverted)
{
    BUNDLE_SETUP();
    emit_ct(compiler, bundle, ctpr, pred, inverted);
    emit_bundle_finalize(compiler, bundle, NULL);
}

static void emit_call(struct sljit_compiler *compiler,
                      sljit_u32 ctpr,
                      sljit_u32 pred,
                      int inverted,
                      int wbs)
{
    E2kSS syl;
    E2kC1f1 syl2;
    BUNDLE_SETUP();

    SLJIT_ASSERT(ctpr & E2K_CTPR);
    ctpr &= 0xFF;
    SLJIT_ASSERT(pred & E2K_PRED);
    pred &= 0xFF;

    get_resource(compiler, bundle, RES_SS); /* Uses only part of RES_SS */
    syl.i = 0;
    if ((pred & E2K_PRED) == E2K_PRED) {
        if (inverted) {
            syl.fields.ctcond = 0x60 + pred_map[pred];
        } else {
            syl.fields.ctcond = 0x40 + pred_map[pred];
        }
    } else {
        syl.fields.ctcond = 0x20;   /* unconditional */
    }

    syl.fields.ctop = ctpr_map[ctpr];
    syl.fields.ipd = 3;

    bundle->ss = syl.i;
    bundle->f1++;

    get_resource(compiler, bundle, RES_CS1);
    syl2.i = 0;
    syl2.fields.opc = E2K_CALL;
    syl2.fields.params = wbs;
    bundle->cs[1] = syl2.i;
    bundle->f2++;

    BUNDLE_CLOSE();
}

static void emit_return(struct sljit_compiler *compiler,
                        sljit_u32 pred,
                        int inverted)
{
    BUNDLE_SETUP();
    SLJIT_ASSERT(pred & E2K_PRED);

    emit_copf1(compiler, bundle, E2K_RETURN, E2K_CTPR | 3, 0);
    emit_bundle_optimize_and_finalize(compiler, bundle);
    emit_ct_full(compiler, E2K_CTPR | 3, pred, inverted);
}

/* -- Emit generic operations --------------------------------------------- */
static SLJIT_INLINE sljit_u32 get_load_opc(sljit_s32 mode)
{
    sljit_u32 load_opc = 0;
    switch (mode & LOAD_MASK) {
        case LOAD_BYTE:
            load_opc = E2K_LDb;
            break;
        case LOAD_HALFWORD:
            load_opc = E2K_LDh;
            break;
        case LOAD_WORD:
            load_opc = E2K_LDw;
            break;
        case LOAD_DOUBLEWORD:
            load_opc = E2K_LDd;
            break;
        default:
            SLJIT_ASSERT(0);
    }

    return load_opc;
}

static SLJIT_INLINE sljit_s32 get_lts_mode(struct sljit_compiler *compiler,
                                           E2kBundle *bundle,
                                           sljit_sw srcw)
{
    sljit_s32 lts_mode = DOUBLEWORD;
    switch (get_const_type(compiler, bundle, srcw)) {
    case E2K_CONST4:
        lts_mode = BYTE;
        break;
    case E2K_CONST5:
    case E2K_CONST16:
        lts_mode = HALFWORD;
        break;
    case E2K_CONST32:
        lts_mode = WORD;
        break;
    case E2K_CONST64:
        lts_mode = DOUBLEWORD;
        break;
    default:
        SLJIT_ASSERT(0);
    }

    return lts_mode;
}

static SLJIT_INLINE sljit_u32 get_store_opc(sljit_s32 mode) {
    sljit_u32 store_opc = 0;
    switch (mode & STORE_MASK) {
        case STORE_BYTE:
            store_opc = E2K_STb;
            break;
        case STORE_HALFWORD:
            store_opc = E2K_STh;
            break;
        case STORE_WORD:
            store_opc = E2K_STw;
            break;
        case STORE_DOUBLEWORD:
            store_opc = E2K_STd;
            break;
        default:
            SLJIT_ASSERT(0);
    }

    return store_opc;
}

static SLJIT_INLINE sljit_s32 get_latency(sljit_s32 mode, sljit_s32 latency)
{
    if (mode & NO_LATENCY) {
        latency = 0;
    }
    return latency;
}

#define ASSERT_MEM(src, dst) \
    do { \
        if (!SLJIT_IS_MEM(src)) SLJIT_ASSERT(0); \
        if (!SLJIT_IS_REG(dst)) SLJIT_ASSERT(0); \
    } while (0)

/* Load into reg only */
static void emit_load(struct sljit_compiler *compiler,
                      E2kBundle *bundle,
                      sljit_s32 dst,
                      sljit_uw dstw,
                      sljit_s32 src,
                      sljit_uw srcw,
                      sljit_s32 mode)
{
    sljit_u32 load_opc = 0;
    sljit_s32 lts_mode = DOUBLEWORD;
    sljit_s32 latency;
    SLJIT_UNUSED_ARG(dstw);

    ASSERT_MEM(src, dst);
    load_opc = get_load_opc(mode);
    lts_mode = get_lts_mode(compiler, bundle, srcw);
    latency = get_latency(mode, E2K_LOAD_LATENCY);

    if (SLJIT_IS_MEM0(src)) {
        emit_op(compiler, bundle, load_opc, REG_ZERO, 0, srcw, 0, dst, lts_mode);
        emit_nop(compiler, bundle, latency);
    } else if (SLJIT_IS_MEM1(src)) {
        emit_op(compiler, bundle, load_opc, REG_SRC1,
                SLJIT_EXTRACT_REG(src), srcw, 0, dst, lts_mode);
        emit_nop(compiler, bundle, latency);
    } else if (SLJIT_IS_MEM2(src)) {
        emit_op(compiler, bundle, E2K_SHLd, REG_SRC1,
                SLJIT_EXTRACT_SECOND_REG(src), srcw, 0, dst, lts_mode);
        BUNDLE_CLOSE();
        emit_op(compiler, bundle, load_opc, REG_SRC1 | REG_SRC2,
                SLJIT_EXTRACT_REG(src), dst, 0, dst, lts_mode);
        emit_nop(compiler, bundle, latency);
    }
}

static void emit_load_full(struct sljit_compiler *compiler,
                           sljit_s32 dst,
                           sljit_uw dstw,
                           sljit_s32 src,
                           sljit_uw srcw,
                           sljit_s32 mode)
{
    BUNDLE_SETUP();
    emit_load(compiler, bundle, dst, dstw, src, srcw, mode);
    BUNDLE_CLOSE();
}

/* Load into reg only */
static void emit_load_double_full(struct sljit_compiler *compiler,
                                  sljit_s32 dst1,
                                  sljit_uw dst1w,
                                  sljit_s32 dst2,
                                  sljit_uw dst2w,
                                  sljit_s32 src1,
                                  sljit_uw src1w,
                                  sljit_s32 src2,
                                  sljit_uw src2w,
                                  sljit_s32 mode)
{
    sljit_u32 load_opc = 0;
    sljit_s32 latency;
    BUNDLE_SETUP();
    SLJIT_UNUSED_ARG(dst1w);
    SLJIT_UNUSED_ARG(dst2w);

    ASSERT_MEM(src1, dst1);
    ASSERT_MEM(src2, dst2);
    load_opc = get_load_opc(mode);
    latency = get_latency(mode, E2K_LOAD_LATENCY);

    if (SLJIT_IS_MEM0(src1)) {
        emit_nop(compiler, bundle, latency);
        emit_op(compiler, bundle, load_opc, REG_ZERO | STEPLER_CHECK_RESOURCE_ONLY,
                0x0, src1w, 0, dst1, DOUBLEWORD);
        emit_op(compiler, bundle, load_opc, REG_ZERO | STEPLER_CHECK_RESOURCE_ONLY,
                0x0, src2w, 0, dst2, DOUBLEWORD);
    } else if (SLJIT_IS_MEM1(src1)) {
        emit_nop(compiler, bundle, latency);
        emit_op(compiler, bundle, load_opc, REG_SRC1 | STEPLER_CHECK_RESOURCE_ONLY,
                SLJIT_EXTRACT_REG(src1), src1w, 0, dst1, DOUBLEWORD);
        emit_op(compiler, bundle, load_opc, REG_SRC1 | STEPLER_CHECK_RESOURCE_ONLY,
                SLJIT_EXTRACT_REG(src2), src2w, 0, dst2, DOUBLEWORD);
    } else if (SLJIT_IS_MEM2(src1)) {
        emit_op(compiler, bundle, E2K_SHLd, REG_SRC1 | STEPLER_CHECK_RESOURCE_ONLY,
                SLJIT_EXTRACT_SECOND_REG(src1), src1w, 0, TMP_REG1, DOUBLEWORD);
        emit_op(compiler, bundle, E2K_SHLd, REG_SRC1 | STEPLER_CHECK_RESOURCE_ONLY,
                SLJIT_EXTRACT_SECOND_REG(src2), src2w, 0, TMP_REG2, DOUBLEWORD);
        BUNDLE_CLOSE();
        emit_op_full(compiler, E2K_ADDd, REG_SRC1,
                     TMP_REG2, sizeof(sljit_uw), 0, TMP_REG2, DOUBLEWORD);
        emit_nop(compiler, bundle, latency);
        emit_op(compiler, bundle, load_opc, REG_SRC1 | REG_SRC2 | STEPLER_CHECK_RESOURCE_ONLY,
                SLJIT_EXTRACT_REG(src1), TMP_REG1, 0, dst1, DOUBLEWORD);
        emit_op(compiler, bundle, load_opc, REG_SRC1 | REG_SRC2 | STEPLER_CHECK_RESOURCE_ONLY,
                SLJIT_EXTRACT_REG(src2), TMP_REG2, 0, dst2, DOUBLEWORD);
    }

    BUNDLE_CLOSE();
}

/* Store reg only */
static void emit_store(struct sljit_compiler *compiler,
                       E2kBundle *bundle,
                       sljit_s32 dst,
                       sljit_uw dstw,
                       sljit_s32 src,
                       sljit_uw srcw,
                       sljit_s32 mode)
{
    sljit_u32 store_opc = 0;
    sljit_s32 lts_mode = DOUBLEWORD;
    sljit_s32 latency;
    SLJIT_UNUSED_ARG(srcw);

    ASSERT_MEM(dst, src);
    store_opc = get_store_opc(mode);
    lts_mode = get_lts_mode(compiler, bundle, dstw);
    latency = get_latency(mode, E2K_STORE_LATENCY);

    if (SLJIT_IS_MEM0(dst)) {
        emit_nop(compiler, bundle, latency);
        emit_op(compiler, bundle, store_opc, REG_SRC3, 0x0, dstw, src, -1, lts_mode);
    } else if (SLJIT_IS_MEM1(dst)) {
        emit_nop(compiler, bundle, latency);
        emit_op(compiler, bundle, store_opc, REG_SRC1 | REG_SRC3,
                SLJIT_EXTRACT_REG(dst), dstw, src, -1, lts_mode);
    } else if (SLJIT_IS_MEM2(dst)) {
        emit_op(compiler, bundle, E2K_SHLd, REG_SRC1,
                SLJIT_EXTRACT_SECOND_REG(dst), dstw, 0, TMP_REG2, lts_mode);
        BUNDLE_CLOSE();
        emit_nop(compiler, bundle, latency);
        emit_op(compiler, bundle, store_opc, REG_SRC1 | REG_SRC2 | REG_SRC3,
                SLJIT_EXTRACT_REG(dst), TMP_REG2, src, -1, lts_mode);
    }
}

/* Store reg only */
static void emit_store_full(struct sljit_compiler *compiler,
                            sljit_s32 dst,
                            sljit_uw dstw,
                            sljit_s32 src,
                            sljit_uw srcw,
                            sljit_s32 mode)
{
    BUNDLE_SETUP();
    emit_store(compiler, bundle, dst, dstw, src, srcw, mode);
    BUNDLE_CLOSE();
}

/* Store reg only */
static void emit_store_double_full(struct sljit_compiler *compiler,
                                   sljit_s32 dst1,
                                   sljit_uw dst1w,
                                   sljit_s32 dst2,
                                   sljit_uw dst2w,
                                   sljit_s32 src1,
                                   sljit_uw src1w,
                                   sljit_s32 src2,
                                   sljit_uw src2w,
                                   sljit_s32 mode)
{
    sljit_u32 store_opc = 0;
    sljit_s32 latency;
    BUNDLE_SETUP();
    SLJIT_UNUSED_ARG(src1w);
    SLJIT_UNUSED_ARG(src2w);

    ASSERT_MEM(dst1, src1);
    ASSERT_MEM(dst2, src2);
    store_opc = get_store_opc(mode);
    latency = get_latency(mode, E2K_STORE_LATENCY);

    if (SLJIT_IS_MEM0(dst1)) {
        emit_nop(compiler, bundle, latency);
        emit_op(compiler, bundle, store_opc, REG_SRC3 | STEPLER_CHECK_RESOURCE_ONLY,
                0, dst1w, src1, -1, DOUBLEWORD);
        emit_op(compiler, bundle, store_opc, REG_SRC3 | STEPLER_CHECK_RESOURCE_ONLY,
                0, dst2w, src2, -1, DOUBLEWORD);
    } else if (SLJIT_IS_MEM1(dst1)) {
        emit_nop(compiler, bundle, latency);
        emit_op(compiler, bundle, store_opc, REG_SRC1 | REG_SRC3 | STEPLER_CHECK_RESOURCE_ONLY,
                SLJIT_EXTRACT_REG(dst1), dst1w, src1, -1, DOUBLEWORD);
        emit_op(compiler, bundle, store_opc, REG_SRC1 | REG_SRC3 | STEPLER_CHECK_RESOURCE_ONLY,
                SLJIT_EXTRACT_REG(dst2), dst2w, src2, -1, DOUBLEWORD);
    } else if (SLJIT_IS_MEM2(dst1)) {
        emit_op(compiler, bundle, E2K_SHLd, REG_SRC1 | STEPLER_CHECK_RESOURCE_ONLY,
                SLJIT_EXTRACT_SECOND_REG(dst1), dst1w, 0, TMP_REG1, DOUBLEWORD);
        emit_op(compiler, bundle, E2K_SHLd, REG_SRC1 | STEPLER_CHECK_RESOURCE_ONLY,
                SLJIT_EXTRACT_SECOND_REG(dst2), dst2w, 0, TMP_REG2, DOUBLEWORD);
        BUNDLE_CLOSE();
        emit_nop(compiler, bundle, latency);
        emit_op(compiler, bundle, store_opc,
                REG_SRC1 | REG_SRC2 | REG_SRC3 | STEPLER_CHECK_RESOURCE_ONLY,
                SLJIT_EXTRACT_REG(dst1), TMP_REG1, src1, -1, DOUBLEWORD);
        emit_op(compiler, bundle, store_opc,
                REG_SRC1 | REG_SRC2 | REG_SRC3 | STEPLER_CHECK_RESOURCE_ONLY,
                SLJIT_EXTRACT_REG(dst2), TMP_REG2, src2, -1, DOUBLEWORD);
    }

    BUNDLE_CLOSE();
}

/* -- High level ---------------------------------------------------------- */

/* Analyze input types (const, mem, reg) and call correct op emit.
   Global bundling version */
static void signature_switcher2(struct sljit_compiler *compiler,
                                        sljit_u32 op,
                                        sljit_s32 dst,
                                        sljit_sw dstw,
                                        sljit_s32 src1,
                                        sljit_sw src1w,
                                        sljit_s32 src2,
                                        sljit_sw src2w,
                                        sljit_s32 mode)
{
    sljit_uw tmp_operand;
    sljit_u32 tmp_signature;
    SLJIT_UNUSED_ARG(dstw);

    if (SLJIT_IS_REG(dst)) {
        if (SLJIT_IS_REG(src2)) {
            if (SLJIT_IS_REG(src1)) {
                emit_op(compiler, &_BUNDLE, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                        src1, src2, 0, dst, mode);
            } else if (SLJIT_IS_IMM(src1)) {
                /* src1 does not support 32 and 64 imm */
                tmp_operand = src1w;
                tmp_signature = REG_SRC2 | REG_SRC3;
                if NOT_SRC1(src1w) {
                    emit_op(compiler, &_BUNDLE, E2K_ADDd, REG_ZERO,
                            0, src1w, 0, TMP_REG1, mode);
                    tmp_operand = TMP_REG1;
                    tmp_signature = REG_SRC1 | REG_SRC2 | REG_SRC3;
                    emit_op(compiler, &_BUNDLE, op, tmp_signature,
                            tmp_operand, src2, 0, dst, mode);
                } else {
                    emit_op(compiler, &_BUNDLE, op, tmp_signature,
                            tmp_operand, src2, 0, dst, mode);
                }
            } else if (SLJIT_IS_MEM(src1)) {
                emit_load(compiler, &_BUNDLE, TMP_REG1, 0, src1, src1w, mode);
                BUNDLE_CLOSE();
                emit_op(compiler, &_BUNDLE, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                        TMP_REG1, src2, 0, dst, mode);
            } else {
                SLJIT_ASSERT(0);
            }
        } else if (SLJIT_IS_IMM(src2)) {
            if (SLJIT_IS_REG(src1)) {
                emit_op(compiler, &_BUNDLE, op, REG_SRC1 | REG_SRC3,
                        src1, src2w, 0, dst, mode);
            } else if (SLJIT_IS_IMM(src1)) {
                /* src1 does not support 32 and 64 imm */
                tmp_operand = src1w;
                tmp_signature = REG_SRC3;
                if NOT_SRC1(src1w) {
                    emit_op(compiler, &_BUNDLE, E2K_ADDd, REG_ZERO,
                            0, src1w, 0, TMP_REG1, mode);
                    tmp_operand = TMP_REG1;
                    tmp_signature = REG_SRC1 | REG_SRC3;
                    emit_op(compiler, &_BUNDLE, op, tmp_signature,
                            tmp_operand, src2w, 0, dst, mode);
                } else {
                    emit_op(compiler, &_BUNDLE, op, tmp_signature,
                            tmp_operand, src2w, 0, dst, mode);
                }
            } else if (SLJIT_IS_MEM(src1)) {
                if (NON_AFFECTING_OP(op, src2w)) {
                    emit_load(compiler, &_BUNDLE, dst, dstw, src1, src1w, mode);
                } else {
                    emit_load(compiler, &_BUNDLE, TMP_REG1, 0, src1, src1w, mode);
                    emit_op(compiler, &_BUNDLE, op, REG_SRC1 | REG_SRC3,
                            TMP_REG1, src2w, 0, dst, mode);
                }
            } else {
                SLJIT_ASSERT(0);
            }
        } else if (SLJIT_IS_MEM(src2)) {
            if (SLJIT_IS_REG(src1)) {
                emit_load(compiler, &_BUNDLE, TMP_REG1, 0, src2, src2w, mode);
                emit_op(compiler, &_BUNDLE, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                        src1, TMP_REG1, 0, dst, mode);
            } else if (SLJIT_IS_IMM(src1)) {
                if (NON_AFFECTING_OP(op, src1w)) {
                    emit_load(compiler, &_BUNDLE, dst, dstw, src2, src2w, mode);
                } else {
                    emit_load(compiler, &_BUNDLE, TMP_REG1, 0, src2, src2w, mode);
                    /* src1 does not support 32 and 64 imm */
                    tmp_operand = src1w;
                    tmp_signature = REG_SRC2 | REG_SRC3;
                    if NOT_SRC1(src1w) {
                        emit_op(compiler, &_BUNDLE, E2K_ADDd, REG_ZERO,
                                0, src1w, 0, TMP_REG2, mode);
                        tmp_operand = TMP_REG2;
                        tmp_signature = REG_SRC1 | REG_SRC2 | REG_SRC3;
                    }
                    emit_op(compiler, &_BUNDLE, op, tmp_signature,
                            tmp_operand, TMP_REG1, 0, dst, mode);
                }
            } else if (SLJIT_IS_MEM(src1)) {
                emit_load(compiler, &_BUNDLE, TMP_REG1, 0, src2, src2w, mode);
                emit_load(compiler, &_BUNDLE, TMP_REG2, 0, src1, src1w, mode);
                emit_op(compiler, &_BUNDLE, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                        TMP_REG2, TMP_REG1, 0, dst, mode);
            } else {
                SLJIT_ASSERT(0);
            }
        } else {
            SLJIT_ASSERT(0);
        }
    } else if (SLJIT_IS_MEM(dst)) {
        if (SLJIT_IS_REG(src2)) {
            if (SLJIT_IS_REG(src1)) {
                emit_op(compiler, &_BUNDLE, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                        src1, src2, 0, TMP_REG1, mode);
                BUNDLE_CLOSE();
                emit_store(compiler, &_BUNDLE, dst, dstw, TMP_REG1, 0, mode);
            } else if (SLJIT_IS_IMM(src1)) {
                if (NON_AFFECTING_OP(op, src1w)) {
                    emit_store(compiler, &_BUNDLE, dst, dstw, src2, 0, mode);
                } else {
                    /* src1 does not support 32 and 64 imm */
                    tmp_operand = src1w;
                    tmp_signature = REG_SRC2 | REG_SRC3;
                    if NOT_SRC1(src1w) {
                        emit_op(compiler, &_BUNDLE, E2K_ADDd, REG_ZERO,
                                0, src1w, 0, TMP_REG1, mode);
                        tmp_operand = TMP_REG1;
                        tmp_signature = REG_SRC1 | REG_SRC2 | REG_SRC3;
                    }
                    emit_op(compiler, &_BUNDLE, op, tmp_signature,
                            tmp_operand, src2, 0, TMP_REG1, mode);
                    BUNDLE_CLOSE();
                    emit_store(compiler, &_BUNDLE, dst, dstw, TMP_REG1, 0, mode);
                }
            } else if (SLJIT_IS_MEM(src1)) {
                emit_load(compiler, &_BUNDLE, TMP_REG1, 0, src1, src1w, mode);
                BUNDLE_CLOSE();
                emit_op(compiler, &_BUNDLE, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                        TMP_REG1, src2, 0, TMP_REG1, mode);
                BUNDLE_CLOSE();
                emit_store(compiler, &_BUNDLE, dst, dstw, TMP_REG1, 0, mode);
            } else {
                SLJIT_ASSERT(0);
            }
        } else if (SLJIT_IS_IMM(src2)) {
            if (SLJIT_IS_REG(src1)) {
                emit_op(compiler, &_BUNDLE, op, REG_SRC1 | REG_SRC3,
                        src1, src2w, 0, TMP_REG1, mode);
                BUNDLE_CLOSE();
                emit_store(compiler, &_BUNDLE, dst, dstw, TMP_REG1, 0, mode);
            } else if (SLJIT_IS_IMM(src1)) {
                /* src1 does not support 32 and 64 imm */
                tmp_operand = src1w;
                tmp_signature = REG_SRC3;
                if NOT_SRC1(src1w) {
                    emit_op(compiler, &_BUNDLE, E2K_ADDd, REG_ZERO,
                            0, src1w, 0, TMP_REG1, mode);
                    BUNDLE_CLOSE();
                    tmp_operand = TMP_REG1;
                    tmp_signature = REG_SRC1 | REG_SRC3;
                }
                emit_op(compiler, &_BUNDLE, op, tmp_signature,
                        tmp_operand, src2w, 0, TMP_REG1, mode);
                BUNDLE_CLOSE();
                emit_store(compiler, &_BUNDLE, dst, dstw, TMP_REG1, 0, mode);
            } else if (SLJIT_IS_MEM(src1)) {
                emit_load(compiler, &_BUNDLE, TMP_REG1, 0, src1, src1w, mode);
                BUNDLE_CLOSE();
                if (!NON_AFFECTING_OP(op, src2w)) {
                    emit_op(compiler, &_BUNDLE, op, REG_SRC1 | REG_SRC3,
                            TMP_REG1, src2w, 0, TMP_REG1, mode);
                    BUNDLE_CLOSE();
                }
                emit_store(compiler, &_BUNDLE, dst, dstw, TMP_REG1, 0, mode);
            } else {
                SLJIT_ASSERT(0);
            }
        } else if (SLJIT_IS_MEM(src2)) {
            if (SLJIT_IS_REG(src1)) {
                emit_load(compiler, &_BUNDLE, TMP_REG1, 0, src2, src2w, mode);
                BUNDLE_CLOSE();
                emit_op(compiler, &_BUNDLE, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                        src1, TMP_REG1, 0, TMP_REG1, mode);
                BUNDLE_CLOSE();
                emit_store(compiler, &_BUNDLE, dst, dstw, TMP_REG1, 0, mode);
            } else if (SLJIT_IS_IMM(src1)) {
                emit_load(compiler, &_BUNDLE, TMP_REG1, 0, src2, src2w, mode);
                if (!NON_AFFECTING_OP(op, src1w)) {
                    /* src1 does not support 32 and 64 imm */
                    tmp_operand = src1w;
                    tmp_signature = REG_SRC2 | REG_SRC3;
                    if NOT_SRC1(src1w) {
                        emit_op(compiler, &_BUNDLE, E2K_ADDd, REG_ZERO,
                                0, src1w, 0, TMP_REG2, mode);
                        BUNDLE_CLOSE();
                        tmp_operand = TMP_REG2;
                        tmp_signature = REG_SRC1 | REG_SRC2 | REG_SRC3;
                    }
                    emit_op(compiler, &_BUNDLE, op, tmp_signature,
                            tmp_operand, TMP_REG1, 0, TMP_REG1, mode);
                    BUNDLE_CLOSE();
                }
                emit_store(compiler, &_BUNDLE, dst, dstw, TMP_REG1, 0, mode);
            } else if (SLJIT_IS_MEM(src1)) {
                emit_load(compiler, &_BUNDLE, TMP_REG1, 0, src2, src2w, mode);
                emit_load(compiler, &_BUNDLE, TMP_REG2, 0, src1, src1w, mode);
                emit_op(compiler, &_BUNDLE, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                        TMP_REG2, TMP_REG1, 0, TMP_REG1, mode);
                BUNDLE_CLOSE();
                emit_store(compiler, &_BUNDLE, dst, dstw, TMP_REG1, 0, mode);
            } else {
                SLJIT_ASSERT(0);
            }
        } else {
            SLJIT_ASSERT(0);
        }
    } else {
        SLJIT_ASSERT(0);
    }
}

/* Analyze input types (const, mem, reg) and call correct op emit. */
static void signature_switcher1(struct sljit_compiler *compiler,
                                sljit_u32 op,
                                sljit_s32 dst,
                                sljit_sw dstw,
                                sljit_s32 src,
                                sljit_sw srcw,
                                sljit_s32 mode)
{
    if (SLJIT_IS_REG(dst)) {
        if (SLJIT_IS_REG(src)) {
            emit_op_full(compiler, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                         0, src, 0, dst, mode);
        } else if (SLJIT_IS_IMM(src)) {
            emit_op_full(compiler, op, REG_SRC1 | REG_SRC3,
                         0, srcw, 0, dst, mode);
        } else if (SLJIT_IS_MEM(src)) {
            emit_load_full(compiler, TMP_REG1, 0, src, srcw, mode);
            emit_op_full(compiler, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                         0, TMP_REG1, 0, dst, mode);
        } else {
            SLJIT_ASSERT(0);
        }
    } else if (SLJIT_IS_MEM(dst)) {
        if (SLJIT_IS_REG(src)) {
            emit_op_full(compiler, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                         0, src, 0, TMP_REG1, mode);
        } else if (SLJIT_IS_IMM(src)) {
            emit_op_full(compiler, op, REG_SRC1 | REG_SRC3,
                         0, srcw, 0, TMP_REG1, mode);
        } else if (SLJIT_IS_MEM(src)) {
            emit_load_full(compiler, TMP_REG1, 0, src, srcw, mode);
            emit_op_full(compiler, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                         0, TMP_REG1, 0, TMP_REG1, mode);
        } else {
            SLJIT_ASSERT(0);
        }
        emit_store_full(compiler, dst, dstw, TMP_REG1, 0, mode);
    } else {
        SLJIT_ASSERT(0);
    }
}

/* -- Code generation ----------------------------------------------------- */
struct alignment_bound_vector {
    sljit_s32 size;
    sljit_s32 used;
    sljit_uw *bound;
};

static void alignment_bound_vector_add(struct alignment_bound_vector *align_vector,
                                       sljit_uw addr)
{
    sljit_s32 add_chunk = 32;
    sljit_s32 add_size = (sljit_s32)(add_chunk * sizeof(sljit_uw));

    /* only for init */
    if (align_vector->size == 0) {
        align_vector->bound = (sljit_uw*)malloc(add_size);
        align_vector->size = add_chunk;
    }
    /* extend */
    if (align_vector->size == align_vector->used) {
        align_vector->bound = (sljit_uw*)realloc(align_vector->bound,
                                                 align_vector->size + add_size);
        align_vector->size = align_vector->size + add_size;
    }
    align_vector->bound[align_vector->used] = addr;
    align_vector->used++;
}

static void patch_disp(sljit_uw addr, sljit_uw target, sljit_uw offset)
{
    sljit_s32 opc, disp;
    sljit_uw start_addr, end_addr;
    offset = offset ? offset : 1;   /* One instr bundle */
    start_addr = addr + sizeof(sljit_s32) * offset;
    end_addr   = addr + sizeof(sljit_s32) * (offset + 1);

    opc = (*(sljit_s32*)(start_addr) & 0xF0000000);
    disp = (sljit_s32)((target - addr) >> 3);
    disp &= 0x0FFFFFFF;     /* for negative values */
    disp = opc | disp;

    SLJIT_UPDATE_WX_FLAGS(start_addr, end_addr, 0);
    SLJIT_MEMCPY((void*)(start_addr),
                 &disp, sizeof(sljit_u32));
    SLJIT_UPDATE_WX_FLAGS(start_addr, end_addr, 1);
    SLJIT_CACHE_FLUSH(start_addr, end_addr);
}

SLJIT_API_FUNC_ATTRIBUTE
void* sljit_generate_code(struct sljit_compiler *compiler,
                          sljit_s32 options,
                          void *exec_allocator_data)
{
    struct sljit_memory_fragment *buf;
    sljit_u8 *code;
    sljit_u8 *code_ptr;
    sljit_sw executable_offset;

    struct sljit_label *label;
    struct sljit_jump *jump;
    struct sljit_const *const_;

    sljit_s32 align_size = sizeof(sljit_uw);    /* e2k inst alignment */
    struct alignment_bound_vector align_bound;
    sljit_uw align_bound_addr;

    BUNDLE_CLOSE();
    CHECK_ERROR_PTR();
    CHECK_PTR(check_sljit_generate_code(compiler, options));

    /* Second code generation pass. */
    code = (sljit_u8*)allocate_executable_memory(compiler->size,
                                                 options, exec_allocator_data,
                                                 &executable_offset);
    PTR_FAIL_WITH_EXEC_IF(code);

    reverse_buf(compiler);
    buf = compiler->buf;

    code_ptr = code;
    label = compiler->labels;
    jump = compiler->jumps;
    const_ = compiler->consts;

#if (defined SLJIT_DEBUG && SLJIT_DEBUG)
    if (getenv("SLJIT_DEBUG")) {
        fprintf(stderr, "[gen] addr: %p\n", code_ptr);
    }
#endif /* SLJIT_DEBUG */
    while(buf) {
        SLJIT_MEMCPY(code_ptr, buf->memory, buf->used_size);
        compiler->executable_size += buf->used_size;
        code_ptr += buf->used_size;
        buf = buf->next;
    }

    align_bound.size = 0;
    align_bound.used = 0;
    align_bound.bound = NULL;

    while(label) {
        /* align next labels */
        if (align_bound.used) {
            label->size -= (align_size*(align_bound.used));
        }

        if (label->u.index >= SLJIT_LABEL_ALIGNED) {
            /* In emit_aligned_label we provide aligned access with nop,
               so we can move real code up to save alignment in case of
               unaligned mmap */
            struct sljit_extended_label *ext_label = (struct sljit_extended_label*)label;
            if (ext_label->data == SLJIT_LABEL_ALIGN_16) {
                sljit_uw current_addr = (sljit_uw)code + label->size;

                if ((current_addr & 0xF) != 0) {
                    sljit_uw copy_size = (sljit_uw)code
                                         + compiler->executable_size
                                         - current_addr;
                    void *dst = (void *)(current_addr - align_size);
                    void *src = (void*)current_addr;
                    SLJIT_MEMMOVE(dst, src, copy_size);
                    label->size -= align_size;
                    align_bound_addr = (sljit_uw)code + label->size;
                    alignment_bound_vector_add(&align_bound, align_bound_addr);
                }
            }
        }
        label->u.addr = (sljit_uw)code + label->size;
        label = label->next;
    }

    while (jump) {
        sljit_uw flags = jump->flags;
        sljit_uw offset = (flags >> 2) & 0x7;
        sljit_uw addr = (flags & JUMP_ADDR) ? jump->u.target
                                            : jump->u.label->u.addr;

        jump->addr = (sljit_uw)code + jump->addr;
        /* do offset with alignment number */
        for (sljit_s32 i = 0; i < align_bound.used; i++) {
            if (jump->addr >= align_bound.bound[i]) {
                jump->addr -= align_size;
            } else {
                break;
            }
        }

        if (flags & IS_DISP) {
            patch_disp(jump->addr, addr, offset);
        } else {
            sljit_set_const(jump->addr, SLJIT_MOV, addr, executable_offset);
        }
        jump = jump->next;
    }

    while(const_) {
        const_->addr = (sljit_uw)code + const_->addr;

        /* do offset with alignment number */
        for (sljit_s32 i = 0; i < align_bound.used; i++) {
            if (const_->addr >= align_bound.bound[i]) {
                const_->addr -= align_size;
            } else {
                break;
            }
        }

        const_ = const_->next;
    }

    compiler->error = SLJIT_ERR_COMPILED;
    compiler->executable_offset = executable_offset;
    compiler->executable_size = (sljit_uw)(compiler->size);

    code = (sljit_u8*)SLJIT_ADD_EXEC_OFFSET(code, executable_offset);

    free(align_bound.bound);

    SLJIT_CACHE_FLUSH(code, code + compiler->size);
    SLJIT_UPDATE_WX_FLAGS(code, (sljit_u8*)SLJIT_ADD_EXEC_OFFSET(code_ptr, executable_offset), 1);
    return (void*)code;
}

/* -- SLJIT layer --------------------------------------------------------- */
/* TODO: Support */
SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_has_cpu_feature(sljit_s32 feature_type)
{
    switch (feature_type) {
    case SLJIT_HAS_FPU:
    /* case SLJIT_HAS_ZERO_REGISTER: */
    case SLJIT_HAS_CLZ:
    case SLJIT_HAS_ROT:
    /* case SLJIT_HAS_PREFETCH: */
    case SLJIT_HAS_COPY_F32:
    case SLJIT_HAS_COPY_F64:
    /* case SLJIT_HAS_SIMD: */
    /* case SLJIT_SIMD_REGS_ARE_PAIRS: */
    /* case SLJIT_HAS_ATOMIC: */
    /* case SLJIT_HAS_MEMORY_BARRIER: */
        return 1;

    default:
        return 0;
    }
}

#define ALIGN_QUADROWORD(x) (((x) + 15) & ~15)

#define PARSE_CLEAN 0
#define PARSE_DISTRIBUTE_ARGS 1
#define PARSE_PREPARE_CALL 2
static SLJIT_INLINE void parse_arg_types(struct sljit_compiler* compiler,
                                         E2kBundle *bundle,
                                         sljit_s32 arg_types_orig,
                                         sljit_s32 mode)
{
    /* reg idx, saved idx, float idx, idx */
    sljit_s32 ri = 0, si = 0, fri = 0, arg_num = 0;
    sljit_s32 op;
    sljit_s32 arg_types, arg_type, arg_type_full;
    sljit_s32 is_float, is_float32, is_scratch, is_32;
    sljit_sw dst_reg, src_reg;

    /* skip return */
    arg_types = arg_types_orig >> SLJIT_ARG_SHIFT;
    while (arg_types) {
        arg_type_full = arg_types & SLJIT_ARG_FULL_MASK;
        arg_type = arg_types & SLJIT_ARG_MASK;
        is_float = arg_type & SLJIT_ARG_TYPE_F64;
        is_float32 = arg_type == SLJIT_ARG_TYPE_F32;
        is_scratch = arg_type_full & SLJIT_ARG_TYPE_SCRATCH_REG;
        is_32 = arg_type == SLJIT_ARG_TYPE_32;

        op = E2K_ADDd;
        if (is_32 || is_float32) {
            op = E2K_ADDs;
        }

        if (is_float) {
            dst_reg = E2K_FREG(SLJIT_FR(fri));
            src_reg = E2K_FREG(SLJIT_FR(fri));
            fri++;
        } else {
            src_reg = SLJIT_R(ri);
            if (is_scratch) {
                dst_reg = SLJIT_R(ri);
            } else {
                dst_reg = SLJIT_S(si);
                si++;
            }
            ri++;
        }

        if (mode == PARSE_CLEAN) {
            if (is_32 || is_float32) {
                emit_op(compiler, bundle, E2K_ADDd, REG_ZERO | STEPLER_CHECK_RESOURCE_ONLY,
                        0, 0, 0, dst_reg, DOUBLEWORD);
            }
        } else if (mode == PARSE_DISTRIBUTE_ARGS) {
            emit_op(compiler, bundle, op, REG_SRC1 | STEPLER_CHECK_RESOURCE_ONLY,
                    SLJIT_R(R0_OFFSET+arg_num), 0, 0, dst_reg, DOUBLEWORD);
            arg_num++;
        } else if (mode == PARSE_PREPARE_CALL) {
            emit_op(compiler, bundle, E2K_ADDd, REG_SRC1 | STEPLER_CHECK_RESOURCE_ONLY,
                    src_reg, 0, 0, SLJIT_R(arg_num), DOUBLEWORD);
            arg_num++;
        }

        arg_types = (arg_types >> SLJIT_ARG_SHIFT);
    }
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_enter(struct sljit_compiler *compiler,
                           sljit_s32 options,
                           sljit_s32 arg_types,
                           sljit_s32 scratches,
                           sljit_s32 saveds,
                           sljit_s32 local_size)
{
    E2kSETR setr;
    E2kSETR_LTS0 lts0;

    sljit_u32 src2;
    sljit_s32 is_enter_reg_arg = (options & SLJIT_ENTER_REG_ARG) == SLJIT_ENTER_REG_ARG;
    sljit_s32 return_code = SLJIT_SUCCESS;
    BUNDLE_SETUP();

    return_code = sljit_set_context(compiler, options, arg_types, scratches, saveds, local_size);
    if (return_code != SLJIT_SUCCESS)
        return return_code;

    if (!is_enter_reg_arg) {
        /* General enter */

        /* -- setwd, setbn, setbp ---------------*/
        get_resource(compiler, bundle, RES_CS1);

        setr.fields.rbs = E2K_ROTATED_REG_BASE;
        setr.fields.rsz    = E2K_ROTATED_REG_SIZE - 1;
        setr.fields.rcur   = 0x0;
        setr.fields.psz    = E2K_PREG_WINDOW_SIZE;
        setr.fields.x      = 0x0;
        setr.fields.setbn  = 0x1;
        setr.fields.setbp  = 0x1;
        setr.fields.opc    = E2K_SETR0;
        bundle->cs[1] = setr.i;
        bundle->f1++;

        get_resource(compiler, bundle, RES_LTS0);
        lts0.fields.x      = 0x0;
        lts0.fields.dbl    = 0x0;
        lts0.fields.nfx    = 0x1;
        lts0.fields.wsz    = E2K_REG_WINDOW_SIZE;
        lts0.fields.rpsz   = 0x0;
        lts0.fields.x1     = 0x0;
        bundle->lts[0] = lts0.i;
        bundle->f4++;
        /* -- setwd, setbn, setbp ---------------*/
    }

    /* Fast call without reg window management.
       Save return addr.

       All registers are saved due to E2K register window mechanism.
       So we shouldn't save all saved registers on stack.
       We don't reserve stack for call, this one will be done in sljit_[i]call. */

    /*  Stack
        -----------
        |..       |
        |FS0      |
        |..       |
        |S1       |
        |S0       |   <-- saveds start = locals start + local_size
        |return   |   <-- save return addr in case SLJIT_RETURN_REG only
-> call |         |
        |         |   <-- bottom = SP = locals start
        -----------
        Saved area reserved in call. */
    if (is_enter_reg_arg) {
        emit_store_full(compiler, SLJIT_MEM1(SLJIT_SP), 0,
                   TMP_REG1, 0, DOUBLEWORD);
    }

    /* allocate locals stack */
    src2 = -ALIGN_QUADROWORD(local_size);
    emit_alopf12_i(compiler, bundle, 0, E2K_GETSP, src2, SLJIT_SP, WORD);
    emit_bundle_finalize(compiler, bundle, NULL);
    stepler_setup();

    if (!is_enter_reg_arg) {
        /* Clean worker regs for input 32bit value, due to
           high 32 bits may content diagnostic type.
           In particular this clean high 32 bits of worker regs.
           This allows use 32bit ops in the next stage to prevent failure with
           diagnostic value.
           Reproduces with compiling >= -O2 only. */
        parse_arg_types(compiler, bundle, arg_types, PARSE_CLEAN);
    }

    if (!is_enter_reg_arg) {
        /* Distribute args to regs due to SLJIT_RETURN_REG is considered as
           in callee func */
        parse_arg_types(compiler, bundle, arg_types, PARSE_DISTRIBUTE_ARGS);
        BUNDLE_CLOSE();
    }

    return return_code;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_set_context(struct sljit_compiler *compiler,
                            sljit_s32 options,
                            sljit_s32 arg_types,
                            sljit_s32 scratches,
                            sljit_s32 saveds,
                            sljit_s32 local_size)
{
    BUNDLE_SETUP();

    CHECK_ERROR();
    CHECK(check_sljit_emit_enter(compiler, options, arg_types, scratches, saveds, local_size));
    set_emit_enter(compiler, options, arg_types, scratches, saveds, local_size);

    SLJIT_ASSERT(local_size < 0x7FFFFFFF);  /* 2**32 - 1 */
    compiler->local_size = local_size;

    return SLJIT_SUCCESS;
}

static SLJIT_INLINE sljit_s32 emit_mov_before_return(struct sljit_compiler *compiler,
                                                     sljit_s32 op,
                                                     sljit_s32 src,
                                                     sljit_sw srcw)
{
    /* At the moment the pointer size is always equal to sljit_sw.
       May be changed in the future. */
    if (src == SLJIT_RETURN_REG && (op == SLJIT_MOV || op == SLJIT_MOV_P))
        return SLJIT_SUCCESS;

    SLJIT_SKIP_CHECKS(compiler);
    return sljit_emit_op1(compiler, op, SLJIT_RETURN_REG, 0, src, srcw);
}

static SLJIT_INLINE sljit_s32 emit_fmov_before_return(struct sljit_compiler *compiler,
                                                      sljit_s32 op,
                                                      sljit_s32 src,
                                                      sljit_sw srcw)
{
    BUNDLE_CLOSE();
    SLJIT_SKIP_CHECKS(compiler);
    if (src != SLJIT_RETURN_FREG) {
        sljit_emit_fop1(compiler, op, SLJIT_RETURN_FREG, 0, src, srcw);
    }

    return sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_RETURN_REG, 0,
                          E2K_FREG(SLJIT_RETURN_FREG), 0);
}

#define E2K_FAST_SYS_ENTRY_32 5
#define E2K_FAST_SYS_ENTRY_64 6
/* Due to special ABI substitute return addr via syscall */
static void substitute_return_addr(struct sljit_compiler *compiler,
                                   sljit_s32 src,
                                   sljit_sw srcw)
{
    sljit_s32 ip_reg = TMP_REG1;
    BUNDLE_DECLARATION();

    emit_copf2(compiler, bundle, E2K_SDISP, E2K_CTPR | 1, E2K_FAST_SYS_ENTRY_64);

    if (SLJIT_IS_REG(src)) {
        ip_reg = src;
    } else {
        signature_switcher2(compiler, E2K_ADDd, ip_reg, 0, SLJIT_IMM, 0,
                            src, srcw, DOUBLEWORD);
    }

    emit_op(compiler, bundle, E2K_ADDd, REG_ZERO, __NR_fast_sys_set_return, 0, 0, SLJIT_R0, DOUBLEWORD);
    emit_op(compiler, bundle, E2K_ADDd, REG_SRC2, 0, ip_reg, 0, SLJIT_R1, DOUBLEWORD);
    emit_op(compiler, bundle, E2K_ADDd, REG_ZERO, 0, 0, 0, SLJIT_R2, DOUBLEWORD);
    BUNDLE_CLOSE();

    emit_call(compiler, E2K_CTPR | 1, E2K_PRED | 0, 0, E2K_ROTATED_REG_BASE);
}

/*  1. Manual context restore for "fast" calls.
    2. Free stack context area.
    See stack scheme in sljit_emit_enter. */
static void restore_reg_arg_context(struct sljit_compiler *compiler,
                                    sljit_s32 start_reg)
{
    sljit_s32 stack_offset = 1 + start_reg;     /* skip return addr */
    sljit_s32 all_regs;
    sljit_u32 stack_reg_arg_sz;
    sljit_u32 src2;

    BUNDLE_DECLARATION();

    for (sljit_s32 i = start_reg; i < SLJIT_NUMBER_OF_SAVED_REGISTERS; i++, stack_offset++) {
        emit_load(compiler, bundle, SLJIT_S(i), 0,
                  SLJIT_MEM1(SLJIT_SP), stack_offset*sizeof(sljit_uw),
                  LOAD_DOUBLEWORD | NO_LATENCY);
    }

    for (sljit_s32 i = 0; i < SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS; i++, stack_offset++) {
        emit_load(compiler, bundle, E2K_FREG(SLJIT_FS(i)), 0,
                  SLJIT_MEM1(SLJIT_SP), stack_offset*sizeof(sljit_uw),
                  LOAD_DOUBLEWORD | NO_LATENCY);
    }

    /* Clean context area */
    all_regs = SLJIT_NUMBER_OF_SAVED_REGISTERS + SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS;
    stack_reg_arg_sz = (sljit_u32)(sizeof(sljit_uw) * (all_regs + 1));
    src2 = ALIGN_QUADROWORD(stack_reg_arg_sz);
    emit_op(compiler, bundle, E2K_GETSP, REG_ZERO, 0, src2, 0, SLJIT_SP, WORD);
    BUNDLE_CLOSE();
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_return_void(struct sljit_compiler *compiler)
{
    sljit_u32 src2;
    sljit_s32 is_enter_reg_arg = (compiler->options & SLJIT_ENTER_REG_ARG) == SLJIT_ENTER_REG_ARG;
    BUNDLE_SETUP();

    CHECK_ERROR();
    CHECK(check_sljit_emit_return_void(compiler));

    if (is_enter_reg_arg) {
        /* clean locals */
        src2 = ALIGN_QUADROWORD(compiler->local_size);
        emit_op_full(compiler, E2K_GETSP, REG_ZERO, 0, src2, 0, SLJIT_SP, WORD);

        /* prep return addr */
        emit_load_full(compiler, TMP_REG1, 0,
                       SLJIT_MEM1(SLJIT_SP), 0, LOAD_DOUBLEWORD);

        emit_op(compiler, bundle, E2K_MOVTd, REG_SRC2 | DST_CTPR,
                0, TMP_REG1, 0, E2K_CTPR | 1, DOUBLEWORD);
        BUNDLE_CLOSE();

        /* keep saved regs */
        for (sljit_s32 i = 0; i < (compiler->options & 0x3); i++) {
            emit_store(compiler, bundle, SLJIT_MEM1(SLJIT_SP), (i+1)*sizeof(sljit_uw),
                       SLJIT_S(i), 0, DOUBLEWORD);
        }
        BUNDLE_CLOSE();

        restore_reg_arg_context(compiler, 0);
        emit_ct_full(compiler, E2K_CTPR | 1, E2K_PRED | 0, 0);
        return SLJIT_SUCCESS;
    }

    emit_return(compiler, E2K_PRED | 0, 0);

    return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_return(struct sljit_compiler *compiler,
                            sljit_s32 op,
                            sljit_s32 src,
                            sljit_sw srcw)
{
    BUNDLE_DECLARATION();
    CHECK_ERROR();
    CHECK(check_sljit_emit_return(compiler, op, src, srcw));

    if (GET_OPCODE(op) < SLJIT_MOV_F64) {
        FAIL_IF(emit_mov_before_return(compiler, op, src, srcw));
    } else {
        FAIL_IF(emit_fmov_before_return(compiler, op, src, srcw));
    }

    /* Correct return code into r0.
       Close bundle before moving into return reg */
    emit_op(compiler, bundle, E2K_ADDd, REG_SRC1,
            SLJIT_RETURN_REG, 0, 0, SLJIT_R(R0_OFFSET), DOUBLEWORD);

    SLJIT_SKIP_CHECKS(compiler);
    return sljit_emit_return_void(compiler);
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_return_to(struct sljit_compiler *compiler,
                               sljit_s32 src,
                               sljit_sw srcw)
{
    sljit_u32 src2;
    sljit_s32 is_enter_reg_arg = (compiler->options & SLJIT_ENTER_REG_ARG) == SLJIT_ENTER_REG_ARG;
    BUNDLE_SETUP();

    if (is_enter_reg_arg) {
        /* prep return addr */
        sljit_s32 ip_reg = TMP_REG1;
        if (SLJIT_IS_REG(src)) {
            ip_reg = src;
        } else {
            signature_switcher2(compiler, E2K_ADDd, ip_reg, 0, SLJIT_IMM, 0,
                                src, srcw, DOUBLEWORD);
        }

        emit_op(compiler, bundle, E2K_MOVTd, REG_SRC2 | DST_CTPR,
                0, ip_reg, 0, E2K_CTPR | 1, DOUBLEWORD);

        /* Cleaning locals must be after addr preparing due to addr may be
           located on stack. */
        src2 = ALIGN_QUADROWORD(compiler->local_size);
        emit_op(compiler, bundle, E2K_GETSP, REG_ZERO, 0, src2, 0, SLJIT_SP, WORD);
        BUNDLE_CLOSE();

        /* restore only not kept regs */
        restore_reg_arg_context(compiler, compiler->options & 0x3);
        emit_ct_full(compiler, E2K_CTPR | 1, E2K_PRED | 0, 0);
        return SLJIT_SUCCESS;
    } else {
        /* General return */
        emit_op(compiler, bundle, E2K_ADDd, REG_SRC1,
                SLJIT_RETURN_REG, 0, 0, SLJIT_R(R0_OFFSET), DOUBLEWORD);
        BUNDLE_CLOSE();

        substitute_return_addr(compiler, src, srcw);

        SLJIT_SKIP_CHECKS(compiler);
        return sljit_emit_return_void(compiler);
    }
}

#define GET_E2K_OPC(opc) mode32 ? opc##s : opc##d

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_op0(struct sljit_compiler *compiler,
                         sljit_s32 op)
{
    sljit_u32 mode32 = op & SLJIT_32;
    sljit_u32 mode = mode32 ? WORD : DOUBLEWORD;

    BUNDLE_CLOSE();
    CHECK_ERROR();
    CHECK(check_sljit_emit_op0(compiler, op));

    switch (op) {
    case SLJIT_BREAKPOINT:
    case SLJIT_NOP:
        emit_nop_full(compiler, 1);
        break;
    case SLJIT_LMUL_UW:
    case SLJIT_LMUL_SW:
        /* mul SLJIT_R0 and SLJIT_R1
           Result into R1:R0 */
        emit_op_full(compiler, E2K_ADDd, REG_SRC1,
                     SLJIT_R1, 0, 0, TMP_REG1, DOUBLEWORD);
        if (op == SLJIT_LMUL_UW) {
            emit_op_full(compiler, E2K_UMULHd, REG_SRC1 | REG_SRC2 | REG_SRC3,
                         SLJIT_R0, SLJIT_R1, 0, SLJIT_R1, DOUBLEWORD);
        } else {
            emit_op_full(compiler, E2K_SMULHd, REG_SRC1 | REG_SRC2 | REG_SRC3,
                         SLJIT_R0, SLJIT_R1, 0, SLJIT_R1, DOUBLEWORD);
        }
        emit_nop_full(compiler, 5);
        emit_op_full(compiler, E2K_MULd, REG_SRC1 | REG_SRC2 | REG_SRC3,
                     SLJIT_R0, TMP_REG1, 0, SLJIT_R0, DOUBLEWORD);
        emit_nop_full(compiler, 5);
        break;
    case SLJIT_DIVMOD_UW:
    case SLJIT_DIVMOD_U32:
    case SLJIT_DIVMOD_SW:
    case SLJIT_DIVMOD_S32:
        /* Div R0 by the R1
           Result -> R0
           Reminder -> R1 */

        emit_op_full(compiler, GET_E2K_OPC(E2K_ADD), REG_SRC1, SLJIT_R0, 0, 0, TMP_REG1, mode);
        if (GET_OPCODE(op) == SLJIT_DIVMOD_UW) {
            emit_op_full(compiler, GET_E2K_OPC(E2K_UDIV), REG_SRC1 | REG_SRC2,
                         SLJIT_R0, SLJIT_R1, 0, SLJIT_R0, mode);
        } else {
            emit_op_full(compiler, GET_E2K_OPC(E2K_SDIV), REG_SRC1 | REG_SRC2,
                         SLJIT_R0, SLJIT_R1, 0, SLJIT_R0, mode);
        }
        emit_op_full(compiler, GET_E2K_OPC(E2K_MUL), REG_SRC1 | REG_SRC2,
                     SLJIT_R0, SLJIT_R1, 0, TMP_REG2, mode);
        emit_op_full(compiler, GET_E2K_OPC(E2K_SUB), REG_SRC1 | REG_SRC2,
                     TMP_REG1, TMP_REG2, 0, SLJIT_R1, mode);
        break;
    case SLJIT_DIV_UW:
    case SLJIT_DIV_SW:
    case SLJIT_DIV_U32:
    case SLJIT_DIV_S32:
        /* Div R0 by R1
           Result -> R0 */
        if (GET_OPCODE(op) == SLJIT_DIV_UW) {
            emit_op_full(compiler, GET_E2K_OPC(E2K_UDIV), REG_SRC1 | REG_SRC2 | REG_SRC3,
                         SLJIT_R0, SLJIT_R1, 0, SLJIT_R0, mode);
        } else {
            emit_op_full(compiler, GET_E2K_OPC(E2K_SDIV), REG_SRC1 | REG_SRC2 | REG_SRC3,
                         SLJIT_R0, SLJIT_R1, 0, SLJIT_R0, mode);
        }
        break;
    case SLJIT_MEMORY_BARRIER:
        return SLJIT_ERR_UNSUPPORTED;
    case SLJIT_ENDBR:
        return SLJIT_ERR_UNSUPPORTED;
    case SLJIT_SKIP_FRAMES_BEFORE_RETURN:
        return SLJIT_ERR_UNSUPPORTED;
    default:
        return SLJIT_ERR_UNSUPPORTED;
    }

    return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_op1(struct sljit_compiler *compiler,
                         sljit_s32 op,
                         sljit_s32 dst,
                         sljit_sw dstw,
                         sljit_s32 src,
                         sljit_sw srcw)
{
    sljit_s32 src_reg = src, dst_reg = dst;
    sljit_u32 mode32 = op & SLJIT_32;
    sljit_u32 mode = mode32 ? WORD : DOUBLEWORD;
    sljit_u32 mode_store;
    sljit_uw order_const;

    stepler_high_level_check(compiler, op, dst, dstw, src, srcw, 0, 0);

    CHECK_ERROR();
    CHECK(check_sljit_emit_op1(compiler, op, dst, dstw, src, srcw));
    ADJUST_LOCAL_OFFSET(dst, dstw);
    ADJUST_LOCAL_OFFSET(src, srcw);

    switch (op) {
    case SLJIT_MOV:
    case SLJIT_MOV_P:
         signature_switcher2(compiler, E2K_ADDd, dst, dstw,
                             SLJIT_IMM, 0, src, srcw, DOUBLEWORD);
        break;
    case SLJIT_MOV_U8:
    case SLJIT_MOV32_U8:
        signature_switcher2(compiler, GET_E2K_OPC(E2K_AND), dst, dstw,
                            src, srcw, SLJIT_IMM, 0xFF,
                            BYTE | EXTEND_LTS);
        break;
    case SLJIT_MOV32_S8:
    case SLJIT_MOV_S8:
        signature_switcher2(compiler, E2K_SXT, dst, dstw,
                            SLJIT_IMM, SXT_EXTEND_8, src, srcw, BYTE);
        break;
    case SLJIT_MOV32_U16:
    case SLJIT_MOV_U16:
        signature_switcher2(compiler, GET_E2K_OPC(E2K_AND), dst, dstw,
                            src, srcw, SLJIT_IMM, 0xFFFF, HALFWORD | EXTEND_LTS);
        break;
    case SLJIT_MOV32_S16:
    case SLJIT_MOV_S16:
        signature_switcher2(compiler, E2K_SXT, dst, dstw,
                            SLJIT_IMM, SXT_EXTEND_16, src, srcw, HALFWORD);
        break;
    case SLJIT_MOV_U32:
        signature_switcher2(compiler, GET_E2K_OPC(E2K_AND), dst, dstw,
                            src, srcw, SLJIT_IMM, 0xFFFFFFFF, WORD | EXTEND_LTS);
        break;
    case SLJIT_MOV_S32:
        signature_switcher2(compiler, E2K_SXT, dst, dstw,
                            SLJIT_IMM, SXT_EXTEND_32, src, srcw, WORD);
        break;
    case SLJIT_MOV32:
        signature_switcher2(compiler, E2K_ANDd, dst, dstw,
                            src, srcw, SLJIT_IMM, 0xFFFFFFFF, WORD);
        break;
    case SLJIT_CLZ:
        signature_switcher1(compiler, E2K_LZCNTd, dst, dstw, src, srcw, DOUBLEWORD);
        break;
    case SLJIT_CLZ32:
        signature_switcher1(compiler, E2K_LZCNTs, dst, dstw, src, srcw, WORD);
        break;
    case SLJIT_CTZ:
        signature_switcher1(compiler, E2K_BITREVd, TMP_REG1, dstw, src, srcw, DOUBLEWORD);
        signature_switcher1(compiler, E2K_LZCNTd, dst, dstw, TMP_REG1, srcw, DOUBLEWORD);
        break;
    case SLJIT_CTZ32:
        signature_switcher1(compiler, E2K_BITREVs, TMP_REG1, dstw, src, srcw, WORD);
        signature_switcher1(compiler, E2K_LZCNTs, dst, dstw, TMP_REG1, srcw, WORD);
        break;
    case SLJIT_REV:
    case SLJIT_REV32:
    case SLJIT_REV_U32:
    case SLJIT_REV_S32:
    case SLJIT_REV_U16:
    case SLJIT_REV32_U16:
    case SLJIT_REV_S16:
    case SLJIT_REV32_S16:
        /* prep src */
        if (SLJIT_IS_REG(src)) {
            src_reg = src;
        } else if (SLJIT_IS_IMM(src)) {
            emit_op_full(compiler, E2K_ADDd, REG_ZERO, 0, srcw, 0, TMP_REG3, mode);
            src_reg = TMP_REG3;
        } else if (SLJIT_IS_MEM(src)) {
            emit_load_full(compiler, TMP_REG3, 0, src, srcw, mode);
            src_reg = TMP_REG3;
        } else {
            SLJIT_ASSERT(0);
        }

        /* prep dst */
        if (SLJIT_IS_REG(dst)) {
            dst_reg = dst;
        } else if (SLJIT_IS_MEM(dst)) {
            dst_reg = TMP_REG3;
        } else {
            SLJIT_ASSERT(0);
        }

        /* prevent store and sxt write beyond dst */
        mode_store = mode32 ? WORD : DOUBLEWORD;

        /* prep controlling const */
        order_const = mode32 ? 0x405060700010203 : 0x1020304050607;
        switch(GET_OPCODE(op)) {
            case SLJIT_REV_U16:
            case SLJIT_REV_S16:
                order_const = 0x203040506070001;
                mode_store = HALFWORD;
                break;
            case SLJIT_REV_U32:
            case SLJIT_REV_S32:
                order_const = 0x405060700010203;
                mode_store = WORD;
                break;
        }
        emit_op_full(compiler, E2K_ADDd, REG_ZERO, 0, order_const, 0, TMP_REG2, mode);

        /* core op: pshufb */
        emit_op_full(compiler, E2K_PSHUFB, REG_SRC1 | REG_SRC2 | REG_SRC3,
                     src_reg, src_reg, TMP_REG2, dst_reg, mode);
        emit_nop_full(compiler, 3);

        /* cut or extend */
        switch(GET_OPCODE(op)) {
            case SLJIT_REV_U16:
                emit_op_full(compiler, GET_E2K_OPC(E2K_AND), REG_SRC1,
                             dst_reg, 0xFFFF, 0, dst_reg, mode);
                break;
            case SLJIT_REV_U32:
                emit_op_full(compiler, GET_E2K_OPC(E2K_AND), REG_SRC1,
                             dst_reg, 0xFFFFFFFF, 0, dst_reg, mode);
                break;
            case SLJIT_REV_S16:
                emit_op_full(compiler, E2K_SXT, REG_SRC2,
                             SXT_EXTEND_16, dst_reg, 0, dst_reg, mode_store);
                break;
            case SLJIT_REV_S32:
                emit_op_full(compiler, E2K_SXT, REG_SRC2,
                             SXT_EXTEND_32, dst_reg, 0, dst_reg, mode_store);
                break;
        }

        if (SLJIT_IS_MEM(dst)) {
            emit_store_full(compiler, dst, dstw, dst_reg, 0, mode_store);
        }
        break;
    default:
        SLJIT_ASSERT(0);
        return SLJIT_ERR_UNSUPPORTED;
    }

    return SLJIT_SUCCESS;
}

#define SET_OPC_AND_OPC_FD(opc) \
    e2kopc = mode32 ? opc##s : opc##d; \
    e2kopc_fd = mode32 ? opc##s_fd : opc##d_fd

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_op2(struct sljit_compiler *compiler,
                         sljit_s32 op,
                         sljit_s32 dst,
                         sljit_sw dstw,
                         sljit_s32 src1,
                         sljit_sw src1w,
                         sljit_s32 src2,
                         sljit_sw src2w)
{
    sljit_u32 mode32 = op & SLJIT_32;
    sljit_u32 e2kopc = 0, e2kopc_fd = 0;
    sljit_s32 mode = mode32 ? WORD : DOUBLEWORD;
    sljit_s32 als_idx;
    BUNDLE_DECLARATION();

    stepler_high_level_check(compiler, op, dst, dstw, src1, src1w, src2, src2w);

    CHECK_ERROR();
    CHECK(check_sljit_emit_op2(compiler, op, 0, dst, dstw, src1, src1w, src2, src2w));
    ADJUST_LOCAL_OFFSET(dst, dstw);
    ADJUST_LOCAL_OFFSET(src1, src1w);
    ADJUST_LOCAL_OFFSET(src2, src2w);

    switch (GET_OPCODE(op)) {
    case SLJIT_ADD:
    case SLJIT_ADDC:
        SET_OPC_AND_OPC_FD(E2K_ADD);
        break;
    /*  In case of flags use E2K_SUBd_f and then use
              CCTO* and MERGE in emit_op_flags. In case of TMP_REG1 as dst.
        To save flag for future operations use this chain:
        SUB -> CCTO* -> JUMP   | MERGE
        FLAG   pred31   pred31 | FLAG with required opcode */
    case SLJIT_SUB:
    case SLJIT_SUBC:
        SET_OPC_AND_OPC_FD(E2K_SUB);
        break;
    case SLJIT_MUL:
        e2kopc = mode32 ? E2K_MULs : E2K_MULd;
        if (GET_OPCODE(op) == SLJIT_MUL) {
            if ((dst == FLAG_REG) || HAS_FLAGS(op)) {
                if (mode32) {
                    signature_switcher2(compiler, E2K_SMULX_fd, FLAG_REG, 0,
                                        src1, src1w, src2, src2w, mode);
                    emit_nop_full(compiler, 5);
                } else {
                    /* TODO: v6+ can use CMUL */
                    /* E2K doesn't fix flags in case of 64bit mul */

                    /* get high 64 */
                    signature_switcher2(compiler, E2K_SMULHd, TMP_REG3, 0,
                                        src1, src1w, src2, src2w, mode);
                    emit_nop_full(compiler, 5);
                    /* get low 64 */
                    signature_switcher2(compiler, E2K_MULd, TMP_REG2, 0,
                                        src1, src1w, src2, src2w, mode);
                    emit_nop_full(compiler, 5);
                    /* extend low 64 */
                    signature_switcher2(compiler, E2K_SARd, TMP_REG2, 0,
                                        TMP_REG2, 0, SLJIT_IMM, 63, mode);
                    BUNDLE_CLOSE();

                    /* compare sign ext with high 64 bits */
                    emit_alopf7_rr(compiler, bundle, 0, E2K_CMPEdb,
                                   TMP_REG3, TMP_REG2, E2K_PRED | TMP_PREG1, mode);
                    BUNDLE_CLOSE();

                    /* Set flag in case of overflow else clean flag */
                    als_idx = emit_alopf1_ii(compiler, bundle, 0, E2K_ADDd,
                                                 0x0, IFL_OF, FLAG_REG, DOUBLEWORD);
                    emit_rlp(compiler, bundle, als_idx, E2K_PRED | TMP_PREG1, 1);
                    als_idx = emit_alopf1_ii(compiler, bundle, 0, E2K_ADDd,
                                             0x0, IFL_ZERO, FLAG_REG, DOUBLEWORD);
                    emit_rlp(compiler, bundle, als_idx, E2K_PRED | TMP_PREG1, 0);
                    BUNDLE_CLOSE();
                }
            }
            if (dst != FLAG_REG) {
                signature_switcher2(compiler, e2kopc, dst, dstw,
                                    src1, src1w, src2, src2w, mode);
            }
        }
        return SLJIT_SUCCESS;
        break;
    case SLJIT_AND:
        SET_OPC_AND_OPC_FD(E2K_AND);
        break;
    case SLJIT_OR:
        SET_OPC_AND_OPC_FD(E2K_OR);
        break;
    case SLJIT_XOR:
        SET_OPC_AND_OPC_FD(E2K_XOR);
        break;
    case SLJIT_MSHL:
    case SLJIT_SHL:
        SET_OPC_AND_OPC_FD(E2K_SHL);
        break;
    case SLJIT_MLSHR:
    case SLJIT_LSHR:
        SET_OPC_AND_OPC_FD(E2K_SHR);
        break;
    case SLJIT_MASHR:
    case SLJIT_ASHR:
        SET_OPC_AND_OPC_FD(E2K_SAR);
        break;
    case SLJIT_ROTL:
        SET_OPC_AND_OPC_FD(E2K_SCL);
        break;
    case SLJIT_ROTR:
        SET_OPC_AND_OPC_FD(E2K_SCR);
        break;
    default:
        SLJIT_ASSERT(0);
        return SLJIT_ERR_UNSUPPORTED;
    }

    if ((GET_OPCODE(op) == SLJIT_ADDC) || (GET_OPCODE(op) == SLJIT_SUBC)) {
        /* E2K does not support automatic flag setting */
        signature_switcher2(compiler, GET_E2K_OPC(E2K_AND), TMP_REG3, 0,
                            FLAG_REG, 0, SLJIT_IMM, IFL_CF, mode);
        if ((dst == FLAG_REG) || HAS_FLAGS(op)) {
            signature_switcher2(compiler, e2kopc_fd, FLAG_REG, 0,
                                src1, src1w, src2, src2w, mode);
        }
        /* prevent TMP_REG2 rewriting, so use duplicate conditions */
        signature_switcher2(compiler, e2kopc, TMP_REG2, 0,
                            src1, src1w, src2, src2w, mode);
        if ((dst == FLAG_REG) || HAS_FLAGS(op)) {
            signature_switcher2(compiler, e2kopc_fd, TMP_REG1, 0,
                                TMP_REG2, 0, TMP_REG3, 0, mode);
            signature_switcher2(compiler, GET_E2K_OPC(E2K_OR), FLAG_REG, 0,
                                FLAG_REG, 0, TMP_REG1, 0, mode);
        }
        if (dst != FLAG_REG) {
            signature_switcher2(compiler, e2kopc, dst, dstw,
                                TMP_REG2, 0, TMP_REG3, 0, mode);
        }
    } else {
        if ((dst == FLAG_REG) || HAS_FLAGS(op)) {
            signature_switcher2(compiler, e2kopc_fd, FLAG_REG, 0,
                                src1, src1w, src2, src2w, mode);
        }
        if (dst != FLAG_REG) {
            signature_switcher2(compiler, e2kopc, dst, dstw,
                                src1, src1w, src2, src2w, mode);
        }
    }

    return SLJIT_SUCCESS;
}


SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_op2u(struct sljit_compiler *compiler,
                          sljit_s32 op,
                          sljit_s32 src1,
                          sljit_sw src1w,
                          sljit_s32 src2,
                          sljit_sw src2w)
{
    CHECK_ERROR();
    CHECK(check_sljit_emit_op2(compiler, op, 1, 0, 0, src1, src1w, src2, src2w));
    BUNDLE_CLOSE();

    SLJIT_SKIP_CHECKS(compiler);
    return sljit_emit_op2(compiler, op, FLAG_REG, 0, src1, src1w, src2, src2w);
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_op2r(struct sljit_compiler *compiler,
                          sljit_s32 op,
                          sljit_s32 dst_reg,
                          sljit_s32 src1,
                          sljit_sw src1w,
                          sljit_s32 src2,
                          sljit_sw src2w)
{
    sljit_u32 mode32 = op & SLJIT_32;
    sljit_s32 mode = mode32 ? WORD : DOUBLEWORD;
    sljit_s32 mul = mode32 ? E2K_MULs : E2K_MULd;

    BUNDLE_CLOSE();
    CHECK_ERROR();
    CHECK(check_sljit_emit_op2r(compiler, op, dst_reg, src1, src1w, src2, src2w));

    switch (GET_OPCODE(op)) {
    case SLJIT_MULADD:
        signature_switcher2(compiler, mul, TMP_REG2, 0,
                            src1, src1w, src2, src2w, mode);
        emit_nop_full(compiler, 5);
        signature_switcher2(compiler, GET_E2K_OPC(E2K_ADD), dst_reg, 0,
                            TMP_REG2, 0, dst_reg, 0, mode);
    }

    return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
struct sljit_jump* sljit_emit_cmp(struct sljit_compiler *compiler,
                                  sljit_s32 type,
                                  sljit_s32 src1,
                                  sljit_sw src1w,
                                  sljit_s32 src2,
                                  sljit_sw src2w)
{
    /* Default compare for most architectures. */
    sljit_s32 tmp_src, condition;
    sljit_sw tmp_srcw;
    struct sljit_jump *jump;
    sljit_s32 mode32 = type & SLJIT_32;
    sljit_s32 mode   = mode32 ? WORD : DOUBLEWORD;
    sljit_s32 opc = 0, offset;
    sljit_u32 src1_u32 = 0, src2_u32 = 0;
    sljit_sw  src1_sw = 0, src2_sw = 0;
    sljit_s32 neg = 0;
    sljit_s32 disp_bundled = 1;
    sljit_s32 emit_flags = REG_ZERO;
    BUNDLE_DECLARATION();

    CHECK_ERROR_PTR();
    CHECK_PTR(check_sljit_emit_cmp(compiler, type, src1, src1w, src2, src2w));

    condition = type & 0xff;

    if (SLJIT_UNLIKELY(src1 == SLJIT_IMM && src2 != SLJIT_IMM)) {
        /* Immediate is preferred as second argument by most architectures. */
        switch (condition) {
        case SLJIT_LESS:
            condition = SLJIT_GREATER;
            break;
        case SLJIT_GREATER_EQUAL:
            condition = SLJIT_LESS_EQUAL;
            break;
        case SLJIT_GREATER:
            condition = SLJIT_LESS;
            break;
        case SLJIT_LESS_EQUAL:
            condition = SLJIT_GREATER_EQUAL;
            break;
        case SLJIT_SIG_LESS:
            condition = SLJIT_SIG_GREATER;
            break;
        case SLJIT_SIG_GREATER_EQUAL:
            condition = SLJIT_SIG_LESS_EQUAL;
            break;
        case SLJIT_SIG_GREATER:
            condition = SLJIT_SIG_LESS;
            break;
        case SLJIT_SIG_LESS_EQUAL:
            condition = SLJIT_SIG_GREATER_EQUAL;
            break;
        }

        type = condition | (type & (SLJIT_32 | SLJIT_REWRITABLE_JUMP));
        tmp_src = src1;
        src1 = src2;
        src2 = tmp_src;
        tmp_srcw = src1w;
        src1w = src2w;
        src2w = tmp_srcw;
    }

    /* Prep operands */
    if (SLJIT_IS_IMM(src1)) {
        if (NOT_SRC1(src1w)) {
            emit_op(compiler, bundle, E2K_ADDd, REG_ZERO, 0, src1w, 0,
                    TMP_REG1, DOUBLEWORD);
        }
    }
    if (SLJIT_IS_MEM(src1)) {
        emit_load(compiler, bundle, TMP_REG1, 0, src1, src1w, DOUBLEWORD);
    }

    if (SLJIT_IS_MEM(src2)) {
        emit_load(compiler, bundle, TMP_REG2, 0, src2, src2w, DOUBLEWORD);
    }

    /* Prepare jump */
    jump = (struct sljit_jump*)ensure_abuf(compiler, sizeof(struct sljit_jump));
    PTR_FAIL_IF_NULL(jump);
    jump->addr = compiler->size;

    /* Without nop due to: (possible load) and CCTOPL op*/
    emit_copf2(compiler, bundle, E2K_DISP, 1 | E2K_CTPR, 0);

    /* Prep operands for check */
    /* ----------------------- */
    if (SLJIT_IS_REG(src1)) {
        src1_sw = src1;
        emit_flags |= REG_SRC1;
    } else if (SLJIT_IS_IMM(src1)) {
        if (NOT_SRC1(src1w)) {
            src1_sw = TMP_REG1;
            emit_flags |= REG_SRC1;
        } else {
            src1_sw = src1w;
        }
    } else if (SLJIT_IS_MEM(src1)) {
        src1_sw = TMP_REG1;
        emit_flags |= REG_SRC1;
    }

    if (SLJIT_IS_REG(src2)) {
        src2_sw = src2;
        emit_flags |= REG_SRC2;
    } else if (SLJIT_IS_IMM(src2)) {
        src2_sw = src2w;
    } else if (SLJIT_IS_MEM(src2)) {
        src2_sw = TMP_REG2;
        emit_flags |= REG_SRC2;
    }
    /* ----------------------- */

    /* schedule CMP */
    offset = bundle->f1 - 1;    /* minus CS0 */
    if (stepler_check_op(compiler, emit_flags, E2K_CMPEdb, src1_sw, src2_sw, -1, -1)) {
        stepler_add_op(compiler, emit_flags, E2K_CMPEdb, src1_sw, src2_sw, -1, -1);
        offset++;               /* keep cmpe in mind */
        set_jump(jump, compiler, type & SLJIT_REWRITABLE_JUMP
                                 | IS_DISP | offset << 2);
    } else {
        /* Lift disp here to prevent loss disp in next bundle*/
        disp_bundled = 0;
        set_jump(jump, compiler, type & SLJIT_REWRITABLE_JUMP
                                 | IS_DISP | offset << 2);
        emit_bundle_optimize_and_finalize(compiler, bundle); 
    }

    /* Prep operands for op
       We cant move this block up due to LTS must be packed in cmpe op */
    if (SLJIT_IS_REG(src1)) {
        src1_u32 = RSRC1(src1);
    } else if (SLJIT_IS_IMM(src1)) {
        if (NOT_SRC1(src1w)) {
            src1_u32 = RSRC1(TMP_REG1);
        } else {
            src1_u32 = ISRC1((sljit_u32)src1w);
        }
    } else if (SLJIT_IS_MEM(src1)) {
        src1_u32 = RSRC1(TMP_REG1);
    }

    if (SLJIT_IS_REG(src2)) {
        src2_u32 = RSRC2(src2);
    } else if (SLJIT_IS_IMM(src2)) {
        src2_u32 = ISRC2(src2w, mode);
    } else if (SLJIT_IS_MEM(src2)) {
        src2_u32 = RSRC2(TMP_REG2);
    }

    switch (condition) {
        case SLJIT_EQUAL:
            opc = mode32 ? E2K_CMPEsb : E2K_CMPEdb;
            break;
        case SLJIT_NOT_EQUAL:
            opc = mode32 ? E2K_CMPEsb : E2K_CMPEdb;
            neg = 1;
            break;
        case SLJIT_CARRY:
        case SLJIT_LESS:
            opc = mode32 ? E2K_CMPBsb : E2K_CMPBdb;
            break;
        case SLJIT_NOT_CARRY:
        case SLJIT_GREATER_EQUAL:
            opc = mode32 ? E2K_CMPBsb : E2K_CMPBdb;
            neg = 1;
            break;
        case SLJIT_GREATER:
            opc = mode32 ? E2K_CMPBEsb : E2K_CMPBEdb;
            neg = 1;
            break;
        case SLJIT_LESS_EQUAL:
            opc = mode32 ? E2K_CMPBEsb : E2K_CMPBEdb;
            break;
        case SLJIT_SIG_LESS:
            opc = mode32 ? E2K_CMPLsb : E2K_CMPLdb;
            break;
        case SLJIT_SIG_GREATER_EQUAL:
            opc = mode32 ? E2K_CMPLsb : E2K_CMPLdb;
            neg = 1;
            break;
        case SLJIT_SIG_GREATER:
            opc = mode32 ? E2K_CMPLEsb : E2K_CMPLEdb;
            neg = 1;
            break;
        case SLJIT_SIG_LESS_EQUAL:
            opc = mode32 ? E2K_CMPLEsb : E2K_CMPLEdb;
            break;
        case SLJIT_OVERFLOW:
            opc = mode32 ? E2K_CMPOsb : E2K_CMPOdb;
            break;
        case SLJIT_NOT_OVERFLOW:
            opc = mode32 ? E2K_CMPOsb : E2K_CMPOdb;
            neg = 1;
            break;
        /* TODO: Support */
        case SLJIT_ATOMIC_STORED:
        case SLJIT_ATOMIC_NOT_STORED:
            SLJIT_ASSERT(0);
            break;
        default:
            SLJIT_UNREACHABLE();
            break;
    }

    emit_alopf7(compiler, bundle, 0, opc, src1_u32, src2_u32, E2K_PRED | FLAG_PREG);

    if (disp_bundled) {
        emit_bundle_optimize_and_finalize(compiler, bundle); 
    }
    emit_ct_full(compiler, E2K_CTPR | 1, E2K_PRED | FLAG_PREG, neg);
    return compiler->last_jump;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_shift_into(struct sljit_compiler *compiler,
                                sljit_s32 op,
                                sljit_s32 dst_reg,
                                sljit_s32 src1_reg,
                                sljit_s32 src2_reg,
                                sljit_s32 src3,
                                sljit_sw src3w)
{
    sljit_u32 mode32 = op & SLJIT_32;
    sljit_u32 mode = mode32 ? WORD : DOUBLEWORD;
    sljit_u32 value_mask = mode32 ? 31 : 63;
    sljit_u32 src3_operand = 0;
    BUNDLE_SETUP();

    CHECK_ERROR();
    CHECK(check_sljit_emit_shift_into(compiler, op, dst_reg, src1_reg, src2_reg, src3, src3w));

    if (SLJIT_IS_REG(src3)) {
        src3_operand = src3;
    } else if (SLJIT_IS_IMM(src3)) {
        signature_switcher2(compiler, GET_E2K_OPC(E2K_ADD), TMP_REG3, 0,
                            SLJIT_IMM, 0, src3, src3w, mode);
        BUNDLE_CLOSE();
        src3_operand = TMP_REG3;
    } else if (SLJIT_IS_MEM(src3)) {
        src3_operand = TMP_REG3;
        emit_load_full(compiler, TMP_REG3, 0, src3, src3w, mode);
    } else {
        SLJIT_UNREACHABLE();
    }

    if ((GET_OPCODE(op) == SLJIT_MSHL) || (GET_OPCODE(op) == SLJIT_MLSHR)) {
        emit_op_full(compiler, GET_E2K_OPC(E2K_AND), REG_SRC1,
                     src3_operand, value_mask, 0, src3_operand, mode);
    }

    switch(GET_OPCODE(op)) {
        case SLJIT_SHL:
        case SLJIT_MSHL:
            emit_op(compiler, bundle, GET_E2K_OPC(E2K_SHL),
                    REG_SRC1 | REG_SRC2 | STEPLER_CHECK_RESOURCE_ONLY,
                    src1_reg, src3_operand, 0, dst_reg, mode);
            emit_op(compiler, bundle, GET_E2K_OPC(E2K_SHR), REG_SRC1 | STEPLER_CHECK_RESOURCE_ONLY,
                    src2_reg, 1, 0, TMP_REG1, mode);
            emit_op(compiler, bundle, GET_E2K_OPC(E2K_XOR), REG_SRC1 | STEPLER_CHECK_RESOURCE_ONLY,
                    src3_operand, value_mask, 0, TMP_REG2, mode);
            emit_bundle_finalize(compiler, bundle, NULL);
            emit_op_full(compiler, GET_E2K_OPC(E2K_SHR), REG_SRC1 | REG_SRC2,
                         TMP_REG1, TMP_REG2, 0, TMP_REG1, mode);
            break;
        case SLJIT_LSHR:
        case SLJIT_MLSHR:
            emit_op(compiler, bundle, GET_E2K_OPC(E2K_SHR),
                    REG_SRC1 | REG_SRC2 | STEPLER_CHECK_RESOURCE_ONLY,
                    src1_reg, src3_operand, 0, dst_reg, mode);
            emit_op(compiler, bundle, GET_E2K_OPC(E2K_SHL), REG_SRC1 | STEPLER_CHECK_RESOURCE_ONLY,
                    src2_reg, 1, 0, TMP_REG1, mode);
            emit_op(compiler, bundle, GET_E2K_OPC(E2K_XOR), REG_SRC1 | STEPLER_CHECK_RESOURCE_ONLY,
                    src3_operand, value_mask, 0, TMP_REG2, mode);
            emit_bundle_finalize(compiler, bundle, NULL);
            emit_op_full(compiler, GET_E2K_OPC(E2K_SHL), REG_SRC1 | REG_SRC2,
                         TMP_REG1, TMP_REG2, 0, TMP_REG1, mode);
            break;
    }
    emit_op_full(compiler, GET_E2K_OPC(E2K_OR), REG_SRC1 | REG_SRC2,
                 dst_reg, TMP_REG1, 0, dst_reg, mode);

    return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_op2_shift(struct sljit_compiler *compiler,
                               sljit_s32 op,
                               sljit_s32 dst,
                               sljit_sw dstw,
                               sljit_s32 src1,
                               sljit_sw src1w,
                               sljit_s32 src2,
                               sljit_sw src2w,
                               sljit_sw shift_arg)
{
    sljit_u32 mode32 = op & SLJIT_32;
    sljit_u32 mode = mode32 ? WORD : DOUBLEWORD;

    BUNDLE_CLOSE();
    CHECK_ERROR();
    CHECK(check_sljit_emit_op2_shift(compiler, op, dst, dstw, src1, src1w, src2, src2w, shift_arg));
    ADJUST_LOCAL_OFFSET(dst, dstw);
    ADJUST_LOCAL_OFFSET(src1, src1w);
    ADJUST_LOCAL_OFFSET(src2, src2w);
    shift_arg &= (sljit_sw)((sizeof(sljit_sw) * 8) - 1);

    if (src2 == SLJIT_IMM) {
        src2w = src2w << shift_arg;
        shift_arg = 0;
    }

    if (shift_arg == 0) {
        SLJIT_SKIP_CHECKS(compiler);
        return sljit_emit_op2(compiler, GET_OPCODE(op), dst, dstw, src1, src1w, src2, src2w);
    }

    signature_switcher2(compiler, GET_E2K_OPC(E2K_SHL), TMP_REG2, 0,
                        src2, src2w, SLJIT_IMM, shift_arg, mode);
    signature_switcher2(compiler, GET_E2K_OPC(E2K_ADD), dst, dstw,
                        src1, src1w, TMP_REG2, 0, mode);

    return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_op_src(struct sljit_compiler *compiler,
                            sljit_s32 op,
                            sljit_s32 src,
                            sljit_sw srcw)
{
    BUNDLE_CLOSE();
    CHECK_ERROR();
    CHECK(check_sljit_emit_op_src(compiler, op, src, srcw));
    ADJUST_LOCAL_OFFSET(src, srcw);

    /* TODO: Support all */
    switch (op) {
        case SLJIT_FAST_RETURN:
            sljit_emit_ijump(compiler, SLJIT_JUMP, src, srcw);
            break;
        case SLJIT_SKIP_FRAMES_BEFORE_FAST_RETURN:
            return SLJIT_ERR_UNSUPPORTED;
        case SLJIT_PREFETCH_L1:
        case SLJIT_PREFETCH_L2:
        case SLJIT_PREFETCH_L3:
        case SLJIT_PREFETCH_ONCE:
            return SLJIT_ERR_UNSUPPORTED;
        default:
            SLJIT_ASSERT(0);
            return SLJIT_ERR_UNSUPPORTED;
    }

    return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_op_dst(struct sljit_compiler *compiler,
                            sljit_s32 op,
                            sljit_s32 dst,
                            sljit_sw dstw)
{
    sljit_s32 is_enter_reg_arg = (compiler->options & SLJIT_ENTER_REG_ARG) == SLJIT_ENTER_REG_ARG;
    BUNDLE_DECLARATION();

    CHECK_ERROR();
    CHECK(check_sljit_emit_op_dst(compiler, op, dst, dstw));
    ADJUST_LOCAL_OFFSET(dst, dstw);

    switch (op) {
    case SLJIT_FAST_ENTER:
        signature_switcher2(compiler, E2K_ADDd, dst, dstw,
                            SLJIT_IMM, 0, TMP_REG1, 0, DOUBLEWORD);
        break;
    case SLJIT_GET_RETURN_ADDRESS:
        if (is_enter_reg_arg) {
            signature_switcher2(compiler, E2K_ADDd, dst, dstw,
                                SLJIT_IMM, 0,
                                SLJIT_MEM1(SLJIT_SP), ALIGN_QUADROWORD(compiler->local_size),
                                DOUBLEWORD);
        } else {
            emit_alopf16(compiler, bundle, 0, E2K_RRd, cr0_hi, RSRC2(TMP_REG1));
            BUNDLE_CLOSE();
            signature_switcher2(compiler, E2K_ANDd, dst, dstw,
                                TMP_REG1, 0,
                                SLJIT_IMM, VA_MASK, DOUBLEWORD);
        }
    }

    return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_get_register_index(sljit_s32 type, sljit_s32 reg)
{
    CHECK_REG_INDEX(check_sljit_get_register_index(type, reg));

    switch (type) {
        case SLJIT_GP_REGISTER:
            /* rotated base */
            if ((reg_map[reg] & REG_MASK) == reg_map[reg]) {
                return (reg_map[reg] & REG_MASK) + (E2K_ROTATED_REG_BASE << 1);
            } else {
                return reg_map[reg] & REG_MASK;
            }
        case SLJIT_FLOAT_REGISTER:
            return reg_map[E2K_FREG(reg)];
        default:
            return -1;
    }
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_op_custom(struct sljit_compiler *compiler,
                               void *instruction,
                               sljit_u32 size)
{
    sljit_u32 *inst;
    BUNDLE_CLOSE();
    CHECK_ERROR();
    CHECK(check_sljit_emit_op_custom(compiler, instruction, size));

    inst = (sljit_u32*)ensure_buf(compiler, size);
    SLJIT_MEMCPY(inst, instruction, size);
    compiler->size += size;

    return SLJIT_SUCCESS;
}

static void emit_conv_from_uw_to_float(struct sljit_compiler *compiler,
                                       sljit_s32 src,
                                       sljit_uw srcw,
                                       sljit_s32 dst,
                                       sljit_uw dstw,
                                       sljit_s32 mode32)
{
    sljit_s32 mode = mode32 ? LOAD_DOUBLEWORD | STORE_WORD
                            : LOAD_DOUBLEWORD | STORE_DOUBLEWORD;
    sljit_uw in_reg = src;
    BUNDLE_SETUP();

    if (SLJIT_IS_MEM(src)) {
        emit_load_full(compiler, TMP_REG1, 0, src, srcw, mode);
        in_reg = TMP_REG1;
    } else if (SLJIT_IS_IMM(src)) {
        emit_op_full(compiler, E2K_ADDd, REG_ZERO, 0, srcw, 0, TMP_REG1, mode);
        in_reg = TMP_REG1;
    }

    /* div by 2 with parity save */
    emit_op(compiler, bundle, E2K_SHRd, REG_SRC1, in_reg, 0x1, 0, TMP_REG1, mode);
    emit_op(compiler, bundle, E2K_ANDd, REG_SRC1, in_reg, 0x1, 0, TMP_REG2, mode);

    BUNDLE_CLOSE();
    emit_op_full(compiler, E2K_ORd, REG_SRC1 | REG_SRC2,
                 TMP_REG1, TMP_REG2, 0, TMP_REG1, mode);
    /* conv */
    if (mode32)
        emit_op_full(compiler, E2K_IDTOFS, REG_SRC2, 0, TMP_REG1, 0, TMP_REG1, mode);
    else
        emit_op_full(compiler, E2K_IDTOFD, REG_SRC2, 0, TMP_REG1, 0, TMP_REG1, mode);

    /* fadd (mul 2) */
    if (mode32)
        signature_switcher2(compiler, E2K_FADDs, dst, dstw,
                            TMP_REG1, 0, TMP_REG1, 0, mode);
    else
        signature_switcher2(compiler, E2K_FADDd, dst, dstw,
                            TMP_REG1, 0, TMP_REG1, 0, mode);
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_fop1(struct sljit_compiler *compiler,
                          sljit_s32 op,
                          sljit_s32 dst,
                          sljit_sw dstw,
                          sljit_s32 src,
                          sljit_sw srcw)
{
    /* Args maybe integer for some ops */
    sljit_s32 dst_orig = dst, src_orig = src;
    /* Choose reg map */
    sljit_s32 fdst = dst, fsrc = src;

    sljit_u32 mode32 = op & SLJIT_32;
    sljit_u32 e2kopc = 0;
    sljit_s32 mode = mode32 ? WORD : DOUBLEWORD;
    sljit_uw abs_imm = mode32 ? 0xFFFFFFFF : 0xFFFFFFFFFFFFFFFF;

    BUNDLE_CLOSE();
    CHECK_ERROR();
    ADJUST_LOCAL_OFFSET(dst, dstw);
    ADJUST_LOCAL_OFFSET(src, srcw);

    if (SLJIT_IS_REG(dst)) {
        fdst = E2K_FREG(fdst);
    }
    if (SLJIT_IS_REG(src)) {
        fsrc = E2K_FREG(fsrc);
    }

    /* Current ops use integet opcodes because e2k registers are universal (for
       integer and float). */

    switch (GET_OPCODE(op)) {
    case SLJIT_MOV_F64:                     /* and SLJIT_MOV_F32 */
        signature_switcher2(compiler, E2K_ANDd, fdst, dstw,
                            fsrc, srcw, SLJIT_IMM, abs_imm, mode);
        break;
    case SLJIT_CONV_F64_FROM_F32:           /* and SLJIT_CONV_F32_FROM_F64 */
        e2kopc = mode32 ? E2K_FDTOFS : E2K_FSTOFD;
        mode = mode32 ? LOAD_DOUBLEWORD | STORE_WORD : LOAD_WORD | STORE_DOUBLEWORD;
        signature_switcher1(compiler, e2kopc, fdst, dstw, fsrc, srcw, mode);
        break;
    case SLJIT_CONV_SW_FROM_F64:            /* and SLJIT_CONV_SW_FROM_F32 */
        e2kopc = mode32 ? E2K_FSTOIDtr : E2K_FDTOIDtr;
        mode = mode32 ? LOAD_WORD | STORE_DOUBLEWORD : LOAD_DOUBLEWORD | STORE_DOUBLEWORD;
        signature_switcher1(compiler, e2kopc, dst_orig, dstw, fsrc, srcw, mode);
        break;
    case SLJIT_CONV_S32_FROM_F64:           /* and SLJIT_CONV_S32_FROM_F32 */
        e2kopc = mode32 ? E2K_FSTOIStr : E2K_FDTOIStr;
        mode = mode32 ? LOAD_WORD | STORE_WORD : LOAD_DOUBLEWORD | STORE_WORD;
        signature_switcher1(compiler, e2kopc, dst_orig, dstw, fsrc, srcw, mode);
        break;
    case SLJIT_CONV_F64_FROM_SW:            /* and SLJIT_CONV_F32_FROM_SW */
        e2kopc = mode32 ? E2K_IDTOFS : E2K_IDTOFD;
        mode = mode32 ? LOAD_DOUBLEWORD | STORE_WORD : LOAD_DOUBLEWORD | STORE_DOUBLEWORD;
        signature_switcher1(compiler, e2kopc, fdst, dstw, src_orig, srcw, mode);
        break;
    case SLJIT_CONV_F64_FROM_S32:           /* and SLJIT_CONV_F32_FROM_S32 */
        e2kopc = mode32 ? E2K_ISTOFS : E2K_ISTOFD;
        mode = mode32 ? LOAD_WORD | STORE_WORD : LOAD_WORD | STORE_DOUBLEWORD;
        signature_switcher1(compiler, e2kopc, fdst, dstw, src_orig, srcw, mode);
        break;
    case SLJIT_CONV_F64_FROM_UW:            /* and SLJIT_CONV_F32_FROM_UW */
        e2kopc = mode32 ? E2K_IDTOFS : E2K_IDTOFD;
        emit_conv_from_uw_to_float(compiler, src_orig, srcw, fdst, dstw, mode32);
        break;
    case SLJIT_CONV_F64_FROM_U32:           /* and SLJIT_CONV_F32_FROM_U32 */
        e2kopc = mode32 ? E2K_IDTOFS : E2K_IDTOFD;
        mode = mode32 ? LOAD_WORD | STORE_WORD : LOAD_WORD | STORE_DOUBLEWORD;
        if (SLJIT_IS_IMM(src_orig)) {
            signature_switcher1(compiler, e2kopc, fdst, dstw,
                                src_orig, srcw & 0xFFFFFFFF, mode);
        } else {
            if (SLJIT_IS_MEM(src_orig)) {
                emit_load_full(compiler, TMP_REG1, 0, src_orig, srcw, mode);
                emit_op_full(compiler, E2K_ANDd, REG_SRC1, TMP_REG1, 0xFFFFFFFF,
                             0, TMP_REG1, DOUBLEWORD);
            } else {
                emit_op_full(compiler, E2K_ANDd, REG_SRC1, src_orig, 0xFFFFFFFF,
                             0, TMP_REG1, DOUBLEWORD);
            }
            signature_switcher1(compiler, e2kopc, fdst, dstw,
                                TMP_REG1, 0, mode);
        }
        break;
    case SLJIT_CMP_F64:                     /* and SLJIT_CMP_F32 */
        CHECK(check_sljit_emit_fop1_cmp(compiler, op, dst, dstw, src, srcw));
        e2kopc = mode32 ? E2K_FCMPODsf : E2K_FCMPODdf;
        signature_switcher2(compiler, e2kopc, FLAG_REG, 0, fdst, dstw,
                            fsrc, srcw, mode);
        break;
    case SLJIT_NEG_F64:                     /* and SLJIT_NEG_F32 */
        e2kopc = mode32 ? E2K_XORs : E2K_XORd;
        abs_imm = mode32 ? 0x80000000 : 0x8000000000000000;
        signature_switcher2(compiler, e2kopc, fdst, dstw,
                            fsrc, srcw, SLJIT_IMM, abs_imm, mode);
        break;
    case SLJIT_ABS_F64:                     /* and SLJIT_ABS_F32 */
        e2kopc = mode32 ? E2K_ANDs : E2K_ANDd;
        abs_imm = mode32 ? 0x7FFFFFFF : 0x7FFFFFFFFFFFFFFF;
        signature_switcher2(compiler, e2kopc, fdst, dstw,
                            fsrc, srcw, SLJIT_IMM, abs_imm, mode);
        break;
    default:
        SLJIT_ASSERT(0);
        return SLJIT_ERR_UNSUPPORTED;
    }

    return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_fop2(struct sljit_compiler *compiler,
                          sljit_s32 op,
                          sljit_s32 dst,
                          sljit_sw dstw,
                          sljit_s32 src1,
                          sljit_sw src1w,
                          sljit_s32 src2,
                          sljit_sw src2w)
{
    sljit_s32 fdst = dst, fsrc1 = src1, fsrc2 = src2;

    sljit_u32 mode32 = op & SLJIT_32;
    sljit_u32 e2kopc = 0;
    sljit_s32 mode = mode32 ? WORD : DOUBLEWORD;
    sljit_uw abs = mode32 ? 0x7FFFFFFF : 0x7FFFFFFFFFFFFFFF;
    sljit_uw sign = mode32 ? 0x80000000 : 0x8000000000000000;
    sljit_s32 finalize = 0;

    BUNDLE_SETUP();

    CHECK_ERROR();
    CHECK(check_sljit_emit_fop2(compiler, op, dst, dstw, src1, src1w, src2, src2w));
    ADJUST_LOCAL_OFFSET(dst, dstw);
    ADJUST_LOCAL_OFFSET(src1, src1w);
    ADJUST_LOCAL_OFFSET(src2, src2w);

    /* Choose reg map */
    if (SLJIT_IS_REG(dst)) {
        fdst = E2K_FREG(fdst);
    }
    if (SLJIT_IS_REG(src1)) {
        fsrc1 = E2K_FREG(fsrc1);
    }
    if (SLJIT_IS_REG(src2)) {
        fsrc2 = E2K_FREG(fsrc2);
    }

    switch (GET_OPCODE(op)) {
    case SLJIT_ADD_F64:             /* and SLJIT_ADD_F32 */
        e2kopc = mode32 ? E2K_FADDs : E2K_FADDd;
        break;
    case SLJIT_SUB_F64:             /* and SLJIT_SUB_F32 */
        e2kopc = mode32 ? E2K_FSUBs : E2K_FSUBd;
        break;
    case SLJIT_MUL_F64:             /* and SLJIT_MUL_F32 */
        e2kopc = mode32 ? E2K_FMULs : E2K_FMULd;
        break;
    case SLJIT_DIV_F64:             /* and SLJIT_DIV_F32 */
        e2kopc = mode32 ? E2K_FDIVs : E2K_FDIVd;
        break;
    case SLJIT_COPYSIGN_F64:        /* and SLJIT_COPYSIGN_F32 */
        if (SLJIT_IS_MEM(fsrc1)) {
            emit_load(compiler, bundle, TMP_REG1, 0, fsrc1, src1w, mode);
            fsrc1 = TMP_REG1;
            finalize = 1;
        } else if (SLJIT_IS_IMM(fsrc1)) {
            emit_op(compiler, bundle, GET_E2K_OPC(E2K_ADD), REG_ZERO, 0,
                    fsrc1, 0, TMP_REG1, mode);
            fsrc1 = TMP_REG1;
            finalize = 1;
        }
        if (SLJIT_IS_MEM(fsrc2)) {
            emit_load(compiler, bundle, TMP_REG2, 0, fsrc2, src2w, mode);
            fsrc2 = TMP_REG2;
            finalize = 1;
        } else if (SLJIT_IS_IMM(fsrc2)) {
            emit_op(compiler, bundle, GET_E2K_OPC(E2K_ADD), REG_ZERO, 0,
                    fsrc2, 0, TMP_REG2, mode);
            fsrc2 = TMP_REG2;
            finalize = 1;
        }
        if (finalize) {
            emit_bundle_finalize(compiler, bundle, NULL);
        }

        emit_op(compiler, bundle, GET_E2K_OPC(E2K_AND), REG_SRC1,
                fsrc1, abs, 0, TMP_REG1, mode);
        emit_op(compiler, bundle, GET_E2K_OPC(E2K_AND), REG_SRC1,
                fsrc2, sign, 0, TMP_REG2, mode);
        emit_bundle_finalize(compiler, bundle, NULL);

        e2kopc = GET_E2K_OPC(E2K_OR);
        fsrc1 = TMP_REG1;
        fsrc2 = TMP_REG2;
        break;
    default:
        SLJIT_ASSERT(0);
        return SLJIT_ERR_UNSUPPORTED;
    }

    signature_switcher2(compiler, e2kopc, fdst, dstw, fsrc1, src1w,
                        fsrc2, src2w, mode);

    return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_fset32(struct sljit_compiler *compiler,
                            sljit_s32 freg,
                            sljit_f32 value)
{
    union {
        sljit_s32 imm;
        sljit_f32 value;
    } u;

    BUNDLE_CLOSE();
    CHECK_ERROR();
    CHECK(check_sljit_emit_fset32(compiler, freg, value));

    freg = E2K_FREG(freg);
    u.value = value;

    emit_op_full(compiler, E2K_ADDs, REG_ZERO, 0, u.imm, 0, freg, DOUBLEWORD);
    return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_fset64(struct sljit_compiler *compiler,
                            sljit_s32 freg,
                            sljit_f64 value)
{
    union {
        sljit_sw imm;
        sljit_f64 value;
    } u;

    BUNDLE_CLOSE();
    CHECK_ERROR();
    CHECK(check_sljit_emit_fset64(compiler, freg, value));

    freg = E2K_FREG(freg);
    u.value = value;

    emit_op_full(compiler, E2K_ADDd, REG_ZERO, 0, u.imm, 0, freg, DOUBLEWORD);
    return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_fcopy(struct sljit_compiler *compiler,
                           sljit_s32 op,
                           sljit_s32 freg,
                           sljit_s32 reg)
{
    BUNDLE_SETUP();

    CHECK_ERROR();
    CHECK(check_sljit_emit_fcopy(compiler, op, freg, reg));

    freg = E2K_FREG(freg);

    switch(op & 0xF) {
        case SLJIT_COPY_TO_F64:
            emit_op_full(compiler, E2K_ADDd, REG_SRC1,
                         reg, 0, 0, freg, DOUBLEWORD);
            break;
        case SLJIT_COPY32_TO_F32:
            emit_op(compiler, bundle, E2K_SHLd, REG_SRC1,
                    SLJIT_EXTRACT_REG(reg), 32, 0, TMP_REG1, DOUBLEWORD);
            if (SLJIT_IS_REG_PAIR(reg)) {
                emit_op(compiler, bundle, E2K_ANDd, REG_SRC1,
                        SLJIT_EXTRACT_SECOND_REG(reg), 0xFFFFFFFF, 0, TMP_REG2, DOUBLEWORD);
            } else {
                emit_op(compiler, bundle, E2K_ANDd, REG_SRC1,
                        freg, 0xFFFFFFFF, 0, TMP_REG2, DOUBLEWORD);
            }
            emit_bundle_finalize(compiler, bundle, NULL);
            emit_op_full(compiler, E2K_ORd, REG_SRC1 | REG_SRC2,
                         TMP_REG1, TMP_REG2, 0, freg, DOUBLEWORD);
            break;
        case SLJIT_COPY_FROM_F64:
            emit_op_full(compiler, E2K_ADDd, REG_SRC1,
                         freg, 0, 0, reg, DOUBLEWORD);
            break;
        case SLJIT_COPY32_FROM_F32:
            emit_op(compiler, bundle, E2K_SHLd, REG_SRC1,
                    SLJIT_EXTRACT_REG(freg), 32, 0, TMP_REG1, DOUBLEWORD);
            if (SLJIT_IS_REG_PAIR(reg)) {
                emit_op(compiler, bundle, E2K_ANDd, REG_SRC1,
                        SLJIT_EXTRACT_SECOND_REG(freg), 0xFFFFFFFF, 0, TMP_REG2, DOUBLEWORD);
            } else {
                emit_op(compiler, bundle, E2K_ANDd, REG_SRC1,
                        reg, 0xFFFFFFFF, 0, TMP_REG2, DOUBLEWORD);
            }
            emit_bundle_finalize(compiler, bundle, NULL);
            emit_op_full(compiler, E2K_ORd, REG_SRC1 | REG_SRC2,
                         TMP_REG1, TMP_REG2, 0, reg, DOUBLEWORD);
            break;
        default:
            SLJIT_UNREACHABLE();
            break;

    }
    return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
struct sljit_label* sljit_emit_label(struct sljit_compiler *compiler)
{
    struct sljit_label *label;

    BUNDLE_CLOSE();
    bundle_tracker.count = 0;
    CHECK_ERROR_PTR();
    CHECK_PTR(check_sljit_emit_label(compiler));

    if (compiler->last_label && compiler->last_label->size == compiler->size)
        return compiler->last_label;

    label = (struct sljit_label*)ensure_abuf(compiler, sizeof(struct sljit_label));
    PTR_FAIL_IF(!label);
    set_label(label, compiler);

    label->size = compiler->size;

    return label;
}

SLJIT_API_FUNC_ATTRIBUTE
struct sljit_label* sljit_emit_aligned_label(struct sljit_compiler *compiler,
                                             sljit_s32 alignment,
                                             struct sljit_read_only_buffer *buffers)
{
    sljit_uw mask, i;
    struct sljit_label *label;
    struct sljit_label *next_label;
    struct sljit_extended_label *ext_label;
    BUNDLE_SETUP();

    CHECK_ERROR_PTR();
    CHECK_PTR(check_sljit_emit_aligned_label(compiler, alignment, buffers));

    sljit_reset_read_only_buffers(buffers);

    if (alignment <= SLJIT_LABEL_ALIGN_8) {
        SLJIT_SKIP_CHECKS(compiler);
        label = sljit_emit_label(compiler);
        PTR_FAIL_IF(!label);
    } else {
        /* The used space is filled with NOPs. */
        mask = ((sljit_uw)1 << alignment);

        /* We always fill with nops because we dont know alignment of address
           from mmap */
        for (i = (mask >> 4); i > 0; i--){
            emit_bundle_finalize(compiler, bundle, NULL);
        }

        ext_label = (struct sljit_extended_label*)ensure_abuf(compiler, sizeof(struct sljit_extended_label));
        PTR_FAIL_IF(!ext_label);
        set_extended_label(ext_label, compiler, SLJIT_LABEL_ALIGNED, alignment);
        label = &ext_label->label;
    }

    if (buffers == NULL)
        return label;

    next_label = label;

    while (1) {
        buffers->u.label = next_label;

        /* Aligned inst align size.
           Keep in mind size may be less then DOUBLEWORD. */
        for (i = (buffers->size + 0x7) >> 3; i > 0; i--)
            emit_bundle_finalize(compiler, bundle, NULL);

        buffers = buffers->next;

        if (buffers == NULL)
            break;

        SLJIT_SKIP_CHECKS(compiler);
        next_label = sljit_emit_label(compiler);
        PTR_FAIL_IF(!next_label);
    }

    return label;
}

static sljit_u32 emit_flag_into_pred(struct sljit_compiler *compiler, sljit_s32 type)
{
    sljit_u32 neg = 0;
    sljit_s32 als_idx;
    sljit_u32 e2kopc = 0;

    /* WARNING
       Sync with sljit_cmp_info */

    BUNDLE_SETUP();
    if (type < SLJIT_F_EQUAL) {
        switch (type) {
            case SLJIT_EQUAL:
                e2kopc = E2K_CCTOPE;
                break;
            case SLJIT_NOT_EQUAL:
                e2kopc = E2K_CCTOPE;
                neg = 1;
                break;
            case SLJIT_CARRY:
            case SLJIT_LESS:
                e2kopc = E2K_CCTOPB;
                break;
            case SLJIT_NOT_CARRY:
            case SLJIT_GREATER_EQUAL:
                e2kopc = E2K_CCTOPB;
                neg = 1;
                break;
            case SLJIT_GREATER:
                e2kopc = E2K_CCTOPBE;
                neg = 1;
                break;
            case SLJIT_LESS_EQUAL:
                e2kopc = E2K_CCTOPBE;
                break;
            case SLJIT_SIG_LESS:
                e2kopc = E2K_CCTOPL;
                break;
            case SLJIT_SIG_GREATER_EQUAL:
                e2kopc = E2K_CCTOPL;
                neg = 1;
                break;
            case SLJIT_SIG_GREATER:
                e2kopc = E2K_CCTOPLE;
                neg = 1;
                break;
            case SLJIT_SIG_LESS_EQUAL:
                e2kopc = E2K_CCTOPLE;
                break;
            case SLJIT_OVERFLOW:
                e2kopc = E2K_CCTOPO;
                break;
            case SLJIT_NOT_OVERFLOW:
                e2kopc = E2K_CCTOPO;
                neg = 1;
                break;
            /* TODO: Support */
            case SLJIT_ATOMIC_STORED:
            case SLJIT_ATOMIC_NOT_STORED:
                SLJIT_ASSERT(0);
                break;
            default:
                SLJIT_UNREACHABLE();
                break;
        }

        emit_alopf8_r(compiler, bundle, 0, e2kopc,
                      FLAG_REG, E2K_PRED | FLAG_PREG, DOUBLEWORD);

    } else if (type <= SLJIT_SET_ORDERED_LESS_EQUAL) {
        /* Float */
        switch (type) {
            case SLJIT_F_EQUAL:
            case SLJIT_ORDERED_EQUAL:
                signature_switcher2(compiler, E2K_ANDs, TMP_REG2, 0,
                                    FLAG_REG, 0, SLJIT_IMM, IFL_ZF | IFL_PF | IFL_CF,
                                    DOUBLEWORD);
                BUNDLE_CLOSE();
                emit_alopf7_ri(compiler, bundle, 0, E2K_CMPEsb,
                               TMP_REG2, IFL_ZF, E2K_PRED | FLAG_PREG, DOUBLEWORD);
                break;
            case SLJIT_F_NOT_EQUAL:
            case SLJIT_ORDERED_NOT_EQUAL:
                signature_switcher2(compiler, E2K_ANDs, TMP_REG2, 0,
                                    FLAG_REG, 0, SLJIT_IMM, IFL_ZF | IFL_PF,
                                    DOUBLEWORD);
                BUNDLE_CLOSE();
                emit_alopf7_ri(compiler, bundle, 0, E2K_CMPEsb,
                               TMP_REG2, IFL_ZERO, E2K_PRED | FLAG_PREG, DOUBLEWORD);
                break;
            case SLJIT_F_LESS:
            case SLJIT_ORDERED_LESS:
                signature_switcher2(compiler, E2K_ANDs, TMP_REG2, 0,
                                    FLAG_REG, 0, SLJIT_IMM, IFL_ZF | IFL_PF | IFL_CF,
                                    DOUBLEWORD);
                BUNDLE_CLOSE();
                emit_alopf7_ri(compiler, bundle, 0, E2K_CMPEsb,
                               TMP_REG2, IFL_CF, E2K_PRED | FLAG_PREG, DOUBLEWORD);
                break;
            case SLJIT_F_GREATER_EQUAL:
            case SLJIT_ORDERED_GREATER_EQUAL:
                signature_switcher2(compiler, E2K_ANDs, TMP_REG2, 0,
                                    FLAG_REG, 0, SLJIT_IMM, IFL_PF | IFL_CF,
                                    DOUBLEWORD);
                BUNDLE_CLOSE();
                emit_alopf7_ri(compiler, bundle, 0, E2K_CMPEsb,
                               TMP_REG2, IFL_ZERO, E2K_PRED | FLAG_PREG, DOUBLEWORD);
                break;
            case SLJIT_F_GREATER:
            case SLJIT_ORDERED_GREATER:
                signature_switcher2(compiler, E2K_ANDs, TMP_REG2, 0,
                                    FLAG_REG, 0, SLJIT_IMM, IFL_ZF | IFL_PF | IFL_CF,
                                    DOUBLEWORD);
                BUNDLE_CLOSE();
                emit_alopf7_ri(compiler, bundle, 0, E2K_CMPEsb,
                               TMP_REG2, IFL_ZERO, E2K_PRED | FLAG_PREG, DOUBLEWORD);
                break;
            case SLJIT_F_LESS_EQUAL:
            case SLJIT_ORDERED_LESS_EQUAL:
                signature_switcher2(compiler, E2K_ANDs, TMP_REG2, 0,
                                    FLAG_REG, 0, SLJIT_IMM, IFL_ZF | IFL_PF | IFL_CF,
                                    DOUBLEWORD);
                BUNDLE_CLOSE();
                emit_alopf7_ri(compiler, bundle, 0, E2K_CMPBEsb,
                               TMP_REG2, IFL_ZERO, E2K_PRED | FLAG_PREG, DOUBLEWORD);
                BUNDLE_CLOSE();
                /* ordered */
                als_idx = emit_alopf7_ri(compiler, bundle, 0, E2K_CMPEsb,
                                         TMP_REG2, IFL_ZF | IFL_PF | IFL_CF,
                                         E2K_PRED | FLAG_PREG, DOUBLEWORD);
                emit_rlp(compiler, bundle, als_idx, E2K_PRED | FLAG_PREG, 1);
                neg = 1;
                break;
            case SLJIT_UNORDERED:
                signature_switcher2(compiler, E2K_ANDs, TMP_REG2, 0,
                                    FLAG_REG, 0, SLJIT_IMM, IFL_ZF | IFL_PF | IFL_CF,
                                    DOUBLEWORD);
                BUNDLE_CLOSE();
                emit_alopf7_ri(compiler, bundle, 0, E2K_CMPEsb,
                               TMP_REG2, IFL_ZF | IFL_PF | IFL_CF,
                               E2K_PRED | FLAG_PREG, DOUBLEWORD);
                break;
            case SLJIT_ORDERED:
                signature_switcher2(compiler, E2K_ANDs, TMP_REG2, 0,
                                    FLAG_REG, 0, SLJIT_IMM, IFL_PF,
                                    DOUBLEWORD);
                BUNDLE_CLOSE();
                emit_alopf7_ri(compiler, bundle, 0, E2K_CMPEsb,
                               TMP_REG2, IFL_ZERO, E2K_PRED | FLAG_PREG, DOUBLEWORD);
                break;

            /* Unordered */
            case SLJIT_UNORDERED_OR_NOT_EQUAL:
                signature_switcher2(compiler, E2K_ANDs, TMP_REG2, 0,
                                    FLAG_REG, 0, SLJIT_IMM, IFL_ZF | IFL_PF | IFL_CF,
                                    DOUBLEWORD);
                BUNDLE_CLOSE();
                emit_alopf7_ri(compiler, bundle, 0, E2K_CMPEsb,
                               TMP_REG2, IFL_ZF,
                               E2K_PRED | FLAG_PREG, DOUBLEWORD);
                neg = 1;
                break;
            case SLJIT_UNORDERED_OR_GREATER_EQUAL:
                signature_switcher2(compiler, E2K_ANDs, TMP_REG2, 0,
                                    FLAG_REG, 0, SLJIT_IMM, IFL_ZF | IFL_PF | IFL_CF,
                                    DOUBLEWORD);
                BUNDLE_CLOSE();
                emit_alopf7_ri(compiler, bundle, 0, E2K_CMPEsb,
                               TMP_REG2, IFL_CF,
                               E2K_PRED | FLAG_PREG, DOUBLEWORD);
                neg = 1;
                break;
            case SLJIT_UNORDERED_OR_LESS_EQUAL:
                signature_switcher2(compiler, E2K_ANDs, TMP_REG2, 0,
                                    FLAG_REG, 0, SLJIT_IMM, IFL_ZF | IFL_PF | IFL_CF,
                                    DOUBLEWORD);
                BUNDLE_CLOSE();
                emit_alopf7_ri(compiler, bundle, 0, E2K_CMPEsb,
                               TMP_REG2, IFL_ZERO,
                               E2K_PRED | FLAG_PREG, DOUBLEWORD);
                neg = 1;
                break;
            case SLJIT_UNORDERED_OR_EQUAL:
                signature_switcher2(compiler, E2K_ANDs, TMP_REG2, 0,
                                    FLAG_REG, 0, SLJIT_IMM, IFL_ZF,
                                    DOUBLEWORD);
                BUNDLE_CLOSE();
                emit_alopf7_ri(compiler, bundle, 0, E2K_CMPEsb,
                               TMP_REG2, IFL_ZF,
                               E2K_PRED | FLAG_PREG, DOUBLEWORD);
                break;
            case SLJIT_UNORDERED_OR_LESS:
                signature_switcher2(compiler, E2K_ANDs, TMP_REG2, 0,
                                    FLAG_REG, 0, SLJIT_IMM, IFL_CF,
                                    DOUBLEWORD);
                BUNDLE_CLOSE();
                emit_alopf7_ri(compiler, bundle, 0, E2K_CMPEsb,
                               TMP_REG2, IFL_CF,
                               E2K_PRED | FLAG_PREG, DOUBLEWORD);
                break;
            case SLJIT_UNORDERED_OR_GREATER:
                signature_switcher2(compiler, E2K_ANDs, TMP_REG2, 0,
                                    FLAG_REG, 0, SLJIT_IMM, IFL_ZF | IFL_PF | IFL_CF,
                                    DOUBLEWORD);
                BUNDLE_CLOSE();
                /* greater */
                emit_alopf7_ri(compiler, bundle, 0, E2K_CMPEsb,
                               TMP_REG2, IFL_ZERO,
                               E2K_PRED | FLAG_PREG, DOUBLEWORD);
                BUNDLE_CLOSE();
                /* unordered */
                als_idx = emit_alopf7_ri(compiler, bundle, 0, E2K_CMPEsb,
                                         TMP_REG2, IFL_ZF | IFL_PF | IFL_CF,
                                         E2K_PRED | FLAG_PREG, DOUBLEWORD);
                emit_rlp(compiler, bundle, als_idx, E2K_PRED | FLAG_PREG, 1);
                break;

            default:
                SLJIT_ASSERT(0);
                break;
        }
    } else {
        SLJIT_UNREACHABLE();
    }

    BUNDLE_CLOSE();
    return neg;
}

SLJIT_API_FUNC_ATTRIBUTE
struct sljit_jump* sljit_emit_jump(struct sljit_compiler *compiler, sljit_s32 type)
{
    struct sljit_jump *jump;
    sljit_u32 neg = 0;
    sljit_u32 flag = type & 0xFF;
    BUNDLE_SETUP();

    CHECK_ERROR_PTR();
    CHECK_PTR(check_sljit_emit_jump(compiler, type));

    /* prepared in other function */
    if (!(type & IS_PREPARED)) {
        jump = (struct sljit_jump*)ensure_abuf(compiler, sizeof(struct sljit_jump));
        PTR_FAIL_IF_NULL(jump);
        /* Put offset into flags as low bits */
        set_jump(jump, compiler, type & SLJIT_REWRITABLE_JUMP | IS_DISP | (1 << 2));

        /* Target:
            CS0: HS, CS - 1.
        */
        jump->addr = compiler->size;

        emit_copf2(compiler, bundle, E2K_DISP, 1 | E2K_CTPR, 0);
        emit_bundle_optimize_and_finalize(compiler, bundle);
    }

    if(flag < SLJIT_JUMP){
        neg = emit_flag_into_pred(compiler, flag);
        emit_ct_full(compiler, E2K_CTPR | 1, E2K_PRED | FLAG_PREG, neg);
    } else {
        /* Force save return addr into TMP_REG1 */
        if (flag == SLJIT_FAST_CALL) {
            /* Save next IP after ct */
            emit_alopf16(compiler, bundle, 0, E2K_RRd, nip, RSRC2(TMP_REG1));
        }

        emit_ct(compiler, bundle, E2K_CTPR | 1, E2K_PRED | 0, 0);
        BUNDLE_CLOSE();
    }

    return compiler->last_jump;
}

SLJIT_API_FUNC_ATTRIBUTE
struct sljit_jump* sljit_emit_call(struct sljit_compiler *compiler,
                                   sljit_s32 type,
                                   sljit_s32 arg_types)
{
    struct sljit_jump *jump;

    BUNDLE_CLOSE();
    CHECK_ERROR_PTR();
    CHECK_PTR(check_sljit_emit_call(compiler, type, arg_types));

    jump = (struct sljit_jump*)ensure_abuf(compiler, sizeof(struct sljit_jump));
    PTR_FAIL_IF(!jump);

#if 1
    set_jump(jump, compiler, type & SLJIT_REWRITABLE_JUMP);

    /* LTS: HS, ALS, LTS_hi, LTS_lo - 2 */
    jump->addr = compiler->size + 2*sizeof(sljit_s32);
    emit_op_full(compiler, E2K_ADDd, REG_ZERO, 0, 0, 0, TMP_REG1,
                 DOUBLEWORD | FORCE_LTS);

    sljit_emit_icall(compiler, type & (~SLJIT_REWRITABLE_JUMP),
                     arg_types, TMP_REG1, 0);;
#else
    /* TODO: Optimize by DISP */
    // Move code from icall into functions to prevent duplication
    jump->addr = compiler->size;
    emit_copf2(compiler, bundle, E2K_DISP, 1 | E2K_CTPR, 0);
    offset = bundle->f1 - 1;    /* minus CS0 */
    set_jump(jump, compiler, type & SLJIT_REWRITABLE_JUMP
                             | IS_DISP | offset << 2);
    emit_bundle_optimize_and_finalize(compiler, bundle); 
    emit_ct_full(compiler, E2K_CTPR | 1, 0, 0);
#endif
    return jump;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_ijump(struct sljit_compiler *compiler,
                           sljit_s32 type,
                           sljit_s32 src,
                           sljit_sw srcw)
{
    struct sljit_jump *jump;
    sljit_u32 flag = type & 0xFF;
    sljit_s32 jump_reg = TMP_REG1;
    BUNDLE_SETUP();

    CHECK_ERROR();
    CHECK(check_sljit_emit_ijump(compiler, type, src, srcw));

    /* Make possible to read variable from callee function */
    ADJUST_LOCAL_OFFSET(src, srcw);

    if (SLJIT_IS_IMM(src)) {
        if (flag == SLJIT_JUMP) {
            /* Patch in the future */
            jump = (struct sljit_jump*)ensure_abuf(compiler, sizeof(struct sljit_jump));
            FAIL_IF_NULL(jump);
            set_jump(jump, compiler, (sljit_u32)JUMP_ADDR);
            jump->u.target = (sljit_uw)srcw;
            jump->addr = compiler->size + 2*sizeof(sljit_u32);
        }

        emit_op_full(compiler, E2K_ADDd, REG_ZERO, 0, srcw, 0, jump_reg, DOUBLEWORD);
    } else if (SLJIT_IS_MEM(src)) {
        emit_load_full(compiler, jump_reg, 0, src, srcw, DOUBLEWORD);
    } else if (SLJIT_IS_REG(src)) {
        jump_reg = src;
    } else {
        SLJIT_ASSERT(0);
    }

    emit_alopf2_r(compiler, bundle, 1, E2K_MOVTd, jump_reg, E2K_CTPR | 1, DOUBLEWORD);
    emit_nop(compiler, bundle, 5);
    BUNDLE_CLOSE();

    /* Force save return addr into TMP_REG1 */
    if (flag == SLJIT_FAST_CALL) {
        emit_alopf16(compiler, bundle, 0, E2K_RRd, nip, RSRC2(TMP_REG1));
    }

    emit_ct(compiler, bundle, E2K_CTPR | 1, E2K_PRED | 0, 0);
    BUNDLE_CLOSE();

    return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_icall(struct sljit_compiler *compiler,
                           sljit_s32 type,
                           sljit_s32 arg_types,
                           sljit_s32 src,
                           sljit_sw srcw)
{
    sljit_s32 jump_reg = TMP_REG1;
    sljit_s32 arg_return;
    sljit_s32 all_regs;
    sljit_u32 stack_reg_arg_sz;
    sljit_u32 src2;
    sljit_s32 stack_offset;
    sljit_s32 is_call_reg_arg = (type & SLJIT_CALL_REG_ARG) == SLJIT_CALL_REG_ARG;
    BUNDLE_SETUP();

    CHECK_ERROR();
    CHECK(check_sljit_emit_icall(compiler, type, arg_types, src, srcw));

    /* Make possible to read variable from callee function */
    ADJUST_LOCAL_OFFSET(src, srcw);

    if (SLJIT_IS_IMM(src)) {
        emit_op_full(compiler, E2K_ADDd, REG_ZERO, 0, srcw, 0, jump_reg, DOUBLEWORD);
    } else if (SLJIT_IS_MEM(src)) {
        emit_load_full(compiler, jump_reg, 0, src, srcw, DOUBLEWORD);
    } else if (SLJIT_IS_REG(src)) {
        jump_reg = src;
    } else {
        SLJIT_ASSERT(0);
    }

    /* prep jump */
    emit_op(compiler, bundle, E2K_MOVTd, REG_SRC2 | DST_CTPR,
            0, jump_reg, 0, E2K_CTPR | 1, DOUBLEWORD);
    emit_nop(compiler, bundle, 5);
    BUNDLE_CLOSE();

    /* SLJIT does not support arg passing more than E2K ABI.
       Now we don't reserve stack for args due to sljit can operate only with
       4 args. */

    /* Save context
       E2K call saves context, but SLJIT_CALL_REG_ARG not, do it manually. */
    if (is_call_reg_arg) {
        all_regs = SLJIT_NUMBER_OF_SAVED_REGISTERS
                   + SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS;
        stack_reg_arg_sz = (sljit_u32)(sizeof(sljit_uw) * (all_regs + 1));

        /* Reserve area, +1 - return addr */
        src2 = -(ALIGN_QUADROWORD(stack_reg_arg_sz));
        emit_op(compiler, bundle, E2K_GETSP, REG_ZERO, 0, src2, 0, SLJIT_SP, WORD);
        BUNDLE_CLOSE();

        /* skip return addr */
        stack_offset = 1;
        for (sljit_s32 i = 0; i < SLJIT_NUMBER_OF_SAVED_REGISTERS; i++, stack_offset++) {
            emit_store(compiler, bundle, SLJIT_MEM1(SLJIT_SP), stack_offset*sizeof(sljit_uw),
                       SLJIT_S(i), 0, DOUBLEWORD);
        }
        for (sljit_s32 i = 0; i < SLJIT_NUMBER_OF_SAVED_FLOAT_REGISTERS; i++, stack_offset++) {
            emit_store(compiler, bundle, SLJIT_MEM1(SLJIT_SP), stack_offset*sizeof(sljit_uw),
                       E2K_FREG(SLJIT_FS(i)), 0, DOUBLEWORD);
        }

        emit_alopf16(compiler, bundle, 0, E2K_RRd, nip, RSRC2(TMP_REG1));
        emit_ct(compiler, bundle, E2K_CTPR | 1, E2K_PRED | 0, 0);
        BUNDLE_CLOSE();
    }

    /* General call; move return reg into SLJIT_RETURN_REG.
       Integer arg is processed as b0, so we should copy float only. */
    arg_return = arg_types & SLJIT_ARG_MASK;
    if (!is_call_reg_arg) {
        parse_arg_types(compiler, bundle, arg_types, PARSE_PREPARE_CALL);
        emit_call(compiler, E2K_CTPR | 1, E2K_PRED | 0, 0, E2K_ROTATED_REG_BASE);

        if (arg_return >= SLJIT_ARG_TYPE_F64) {
            emit_op(compiler, bundle, E2K_ADDd, REG_SRC2,
                    0, SLJIT_R0, 0, E2K_FREG(SLJIT_RETURN_REG), DOUBLEWORD);
        }
    }

    if (type & SLJIT_CALL_RETURN) {
        /* Provide return value for caller in case of general call */
        if (!is_call_reg_arg) {
            if (arg_return < SLJIT_ARG_TYPE_F64) {
                emit_op(compiler, bundle, E2K_ADDd, REG_SRC2,
                        0, SLJIT_R0, 0, SLJIT_R(R0_OFFSET), DOUBLEWORD);
            }
        }

        SLJIT_SKIP_CHECKS(compiler);
        sljit_emit_return_void(compiler);
    }
    return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_op_flags(struct sljit_compiler *compiler,
                              sljit_s32 op,
                              sljit_s32 dst,
                              sljit_sw dstw,
                              sljit_s32 type)
{
    sljit_u32 neg;
    sljit_s32 als_idx;
    sljit_u32 mode32 = op & SLJIT_32;
    sljit_u32 e2kopc = 0, e2kopc_fd = 0;
    sljit_s32 mode = mode32 ? WORD : DOUBLEWORD;
    BUNDLE_SETUP();

    CHECK_ERROR();
    CHECK(check_sljit_emit_op_flags(compiler, op, dst, dstw, type));
    ADJUST_LOCAL_OFFSET(dst, dstw);

    /* For pred inversion */
    neg = emit_flag_into_pred(compiler, type);

    als_idx = emit_alopf1_ii(compiler, bundle, 0, E2K_MERGEd,
                             0x0, 0x1, TMP_REG2, DOUBLEWORD);
    emit_mrgc(compiler, bundle, als_idx, E2K_PRED | FLAG_PREG, neg);
    emit_bundle_finalize(compiler, bundle, NULL);

    switch (GET_OPCODE(op)) {
    case SLJIT_MOV:
        signature_switcher2(compiler, E2K_ADDd, dst, dstw, TMP_REG2, 0,
                            SLJIT_IMM, 0, DOUBLEWORD);
        return SLJIT_SUCCESS;
    case SLJIT_MOV32:
        signature_switcher2(compiler, E2K_ADDs, dst, dstw, TMP_REG2, 0,
                            SLJIT_IMM, 0, WORD);
        return SLJIT_SUCCESS;
    case SLJIT_AND:
        e2kopc = mode32 ? E2K_ANDs : E2K_ANDd;
        e2kopc_fd = mode32 ? E2K_ANDs_fd : E2K_ANDd_fd;
        break;
    case SLJIT_OR:
        e2kopc = mode32 ? E2K_ORs : E2K_ORd;
        e2kopc_fd = mode32 ? E2K_ORs_fd : E2K_ORd_fd;
        break;
    case SLJIT_XOR:
        e2kopc = mode32 ? E2K_XORs : E2K_XORd;
        e2kopc_fd = mode32 ? E2K_XORs_fd : E2K_XORd_fd;
        break;
    default:
        SLJIT_ASSERT(0);
        return SLJIT_ERR_UNSUPPORTED;
    }

    if (e2kopc_fd) {
        if ((dst == FLAG_REG) || HAS_FLAGS(op)) {
            signature_switcher2(compiler, e2kopc_fd, FLAG_REG, 0,
                                TMP_REG2, 0, dst, dstw, mode);
        }
        if (dst != FLAG_REG) {
            signature_switcher2(compiler, e2kopc, dst, dstw,
                                TMP_REG2, 0, dst, dstw, mode);
        }
    }

    return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_cmp_info(sljit_s32 type)
{
    switch(type) {
        case SLJIT_EQUAL:
        case SLJIT_CARRY:
        case SLJIT_LESS:
        case SLJIT_LESS_EQUAL:
        case SLJIT_SIG_LESS:
        case SLJIT_SIG_LESS_EQUAL:
        case SLJIT_OVERFLOW:
        case SLJIT_NOT_CARRY:
        case SLJIT_GREATER_EQUAL:
        case SLJIT_NOT_EQUAL:
        case SLJIT_GREATER:
        case SLJIT_SIG_GREATER_EQUAL:
        case SLJIT_SIG_GREATER:
        case SLJIT_NOT_OVERFLOW:
            return 0;

        case SLJIT_F_EQUAL:
        case SLJIT_ORDERED_EQUAL:
        case SLJIT_F_NOT_EQUAL:
        case SLJIT_ORDERED_NOT_EQUAL:
        case SLJIT_F_LESS:
        case SLJIT_ORDERED_LESS:
        case SLJIT_F_GREATER_EQUAL:
        case SLJIT_ORDERED_GREATER_EQUAL:
        case SLJIT_F_GREATER:
        case SLJIT_ORDERED_GREATER:
        case SLJIT_F_LESS_EQUAL:
        case SLJIT_UNORDERED:
        case SLJIT_ORDERED:
        case SLJIT_UNORDERED_OR_NOT_EQUAL:
        case SLJIT_UNORDERED_OR_GREATER_EQUAL:
        case SLJIT_UNORDERED_OR_LESS_EQUAL:
        case SLJIT_UNORDERED_OR_EQUAL:
        case SLJIT_UNORDERED_OR_LESS:
            return 1;

        case SLJIT_UNORDERED_OR_GREATER:
        case SLJIT_ORDERED_LESS_EQUAL:
            return 2;

        default:
            SLJIT_ASSERT(0);
            return SLJIT_ERR_UNSUPPORTED;
    }
}

static SLJIT_INLINE sljit_s32 emit_select(struct sljit_compiler *compiler,
                                          sljit_s32 type,
                                          sljit_s32 dst_reg,
                                          sljit_s32 src1,
                                          sljit_sw src1w,
                                          sljit_s32 src2_reg)
{
    sljit_u32 mode32 = type & SLJIT_32;
    sljit_u32 mode = mode32 ? WORD : DOUBLEWORD;
    sljit_u32 neg;
    sljit_s32 e2kopc = mode32 ? E2K_MERGEs : E2K_MERGEd;
    sljit_s32 als_idx = 0;
    BUNDLE_SETUP();

    if (type & SLJIT_COMPARE_SELECT) {
        type &= ~SLJIT_COMPARE_SELECT;
        FAIL_IF(sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_Z | mode32, src1, src1w,
                                src2_reg, 0));
    }

    neg = emit_flag_into_pred(compiler, type & ~SLJIT_32);

    /* Move from pred into reg */
    if (SLJIT_IS_MEM(src1)) {
        emit_load_full(compiler, TMP_REG2, 0, src1, src1w, mode);
        als_idx = emit_alopf1_rr(compiler, bundle, 0, e2kopc,
                                 TMP_REG2, src2_reg, dst_reg, mode);
    } else if (SLJIT_IS_REG(src1)) {
        als_idx = emit_alopf1_rr(compiler, bundle, 0, e2kopc,
                                 src1, src2_reg, dst_reg, mode);
    } else if (SLJIT_IS_IMM(src1)) {
        /* src1 does not support 32 and 64 imm */
        if NOT_SRC1(src1w) {
            emit_op_full(compiler, E2K_ADDd, REG_ZERO,
                         0, src1w, 0, TMP_REG1, mode);
            als_idx = emit_alopf1_rr(compiler, bundle, 0, e2kopc,
                                     TMP_REG1, src2_reg, dst_reg, mode);
        } else {
            als_idx = emit_alopf1_ir(compiler, bundle, 0, e2kopc,
                                     (sljit_u32)src1w, src2_reg, dst_reg, mode);
        }
    } else {
        SLJIT_ASSERT(0);
    }
    /* select: if true 1 else 2
       mrgc:   if true 2 else 1 */
    neg = !neg;
    emit_mrgc(compiler, bundle, als_idx, E2K_PRED | FLAG_PREG, neg);
    emit_bundle_finalize(compiler, bundle, NULL);

    return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_select(struct sljit_compiler *compiler,
                            sljit_s32 type,
                            sljit_s32 dst_reg,
                            sljit_s32 src1,
                            sljit_sw src1w,
                            sljit_s32 src2_reg)
{
    BUNDLE_CLOSE();
    CHECK_ERROR();
    CHECK(check_sljit_emit_select(compiler, type, dst_reg, src1, src1w, src2_reg));
    ADJUST_LOCAL_OFFSET(src1, src1w);

    return emit_select(compiler, type, dst_reg, src1, src1w, src2_reg);
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_fselect(struct sljit_compiler *compiler,
                             sljit_s32 type,
                             sljit_s32 dst_freg,
                             sljit_s32 src1,
                             sljit_sw src1w,
                             sljit_s32 src2_freg)
{
    BUNDLE_CLOSE();
    CHECK_ERROR();
    CHECK(check_sljit_emit_fselect(compiler, type, dst_freg, src1, src1w, src2_freg));
    ADJUST_LOCAL_OFFSET(src1, src1w);

    dst_freg = E2K_FREG(dst_freg);
    if (SLJIT_IS_REG(src1)) {
        src1 = E2K_FREG(src1);
    }
    src2_freg = E2K_FREG(src2_freg);

    return emit_select(compiler, type, dst_freg, src1, src1w, src2_freg);
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_mem(struct sljit_compiler *compiler,
                         sljit_s32 type,
                         sljit_s32 reg,
                         sljit_s32 mem,
                         sljit_sw memw)
{
    sljit_sw mem2w;

    BUNDLE_SETUP();
    CHECK_ERROR();
    CHECK(check_sljit_emit_mem(compiler, type, reg, mem, memw));

    if (!(reg & REG_PAIR_MASK))
        return sljit_emit_mem_unaligned(compiler, type, reg, mem, memw);

    ADJUST_LOCAL_OFFSET(mem, memw);
    if (SLJIT_IS_MEM2(mem)) {
        mem2w = memw;
    } else {
        mem2w = memw + sizeof(sljit_sw);
    }

    if (type & SLJIT_MEM_STORE) {
        emit_store_double_full(compiler, mem, memw, mem, mem2w,
                               SLJIT_EXTRACT_REG(reg), 0, SLJIT_EXTRACT_SECOND_REG(reg), 0, DOUBLEWORD);
    } else {
        emit_load_double_full(compiler, SLJIT_EXTRACT_REG(reg), 0, SLJIT_EXTRACT_SECOND_REG(reg), 0,
                              mem, memw, mem, mem2w, DOUBLEWORD);
    }

    return SLJIT_SUCCESS;
}

/* TODO: Support */
SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_atomic_load(struct sljit_compiler *compiler,
                                 sljit_s32 op,
                                 sljit_s32 dst_reg,
                                 sljit_s32 mem_reg)
{
    SLJIT_UNUSED_ARG(compiler);
    SLJIT_UNUSED_ARG(op);
    SLJIT_UNUSED_ARG(dst_reg);
    SLJIT_UNUSED_ARG(mem_reg);
    SLJIT_ASSERT(0);
    return SLJIT_ERR_UNSUPPORTED;
}


/* TODO: Support */
SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_atomic_store(struct sljit_compiler *compiler,
                                  sljit_s32 op,
                                  sljit_s32 src_reg,
                                  sljit_s32 mem_reg,
                                  sljit_s32 temp_reg)
{
    SLJIT_UNUSED_ARG(compiler);
    SLJIT_UNUSED_ARG(op);
    SLJIT_UNUSED_ARG(src_reg);
    SLJIT_UNUSED_ARG(mem_reg);
    SLJIT_UNUSED_ARG(temp_reg);
    SLJIT_ASSERT(0);
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE
struct sljit_const* sljit_emit_const(struct sljit_compiler *compiler,
                                     sljit_s32 op,
                                     sljit_s32 dst,
                                     sljit_sw dstw,
                                     sljit_sw init_value)
{
    struct sljit_const *const_;

    sljit_u32 mode32 = op & SLJIT_32;
    sljit_s32 mode = mode32 ? WORD : DOUBLEWORD;
    sljit_s32 mode_store = mode;

    /* E2K Supports 64-bit consts in LTS */
    BUNDLE_CLOSE();
    CHECK_ERROR_PTR();
    CHECK_PTR(check_sljit_emit_const(compiler, op, dst, dstw, init_value));
    ADJUST_LOCAL_OFFSET(dst, dstw);

    const_ = (struct sljit_const*)ensure_abuf(compiler, sizeof(struct sljit_const));
    PTR_FAIL_IF(!const_);
    set_const(const_, compiler);
    /*  LTS: HS, ALS, LTS_hi, LTS_lo - 2. */
    const_->addr = const_->addr + 2*sizeof(sljit_u32);

    switch(op & 0xFF) {
    case SLJIT_MOV:
        signature_switcher2(compiler, E2K_ADDd, dst, dstw, SLJIT_IMM, 0,
                            SLJIT_IMM, init_value, DOUBLEWORD | FORCE_LTS);
        break;
    case SLJIT_MOV32:
        signature_switcher2(compiler, E2K_ADDs, dst, dstw, SLJIT_IMM, 0,
                            SLJIT_IMM, init_value & 0xFFFFFFFF,
                            WORD | FORCE_LTS);
        break;
    case SLJIT_MOV_S32:
        signature_switcher2(compiler, E2K_SXT, dst, dstw,
                            SLJIT_IMM, SXT_EXTEND_32,
                            SLJIT_IMM, init_value & 0xFFFFFFFF,
                            DOUBLEWORD | FORCE_LTS);
        break;
    case SLJIT_MOV32_U8:
    case SLJIT_MOV_U8:
        if(SLJIT_IS_REG(dst)) {
            if (((init_value >> 8) & 0x1) == 1) {
                init_value = 0xFFFFFFFFFFFFFF00 | init_value;
                mode_store = DOUBLEWORD;
                if (mode32) {
                    init_value = 0xFFFFFF00 | init_value;
                    mode_store = WORD;
                }
            } else {
                init_value = init_value & 0xFF;
                mode_store = DOUBLEWORD;
                if (mode32) {
                    mode_store = WORD;
                }
            }
        } else {
            init_value = init_value & 0xFF;
            mode_store = BYTE;
        }
        signature_switcher2(compiler, GET_E2K_OPC(E2K_ADD), dst, dstw,
                            SLJIT_IMM, 0, SLJIT_IMM, init_value,
                            mode_store | FORCE_LTS);
        break;
    default:
        SLJIT_ASSERT(0);
    }

    BUNDLE_CLOSE();
    return const_;
}

SLJIT_API_FUNC_ATTRIBUTE
struct sljit_jump* sljit_emit_op_addr(struct sljit_compiler *compiler,
                                      sljit_s32 op,
                                      sljit_s32 dst,
                                      sljit_sw dstw)
{
    struct sljit_jump *jump;

    BUNDLE_CLOSE();
    CHECK_ERROR_PTR();
    CHECK_PTR(check_sljit_emit_op_addr(compiler, op, dst, dstw));
    ADJUST_LOCAL_OFFSET(dst, dstw);

    jump = (struct sljit_jump*)ensure_abuf(compiler, sizeof(struct sljit_jump));
    PTR_FAIL_IF(!jump);
    set_mov_addr(jump, compiler, 0);

    /* LTS: HS, ALS, LTS_hi, LTS_lo - 2 */
    jump->addr = compiler->size + 2*sizeof(sljit_u32);

    switch(GET_OPCODE(op)) {
    case SLJIT_MOV_ADDR:
        signature_switcher2(compiler, E2K_ADDd, dst, dstw, SLJIT_IMM, 0,
                            SLJIT_IMM, 0, DOUBLEWORD | FORCE_LTS);
        break;
    case SLJIT_MOV_ABS_ADDR:
        signature_switcher2(compiler, E2K_ADDd, dst, dstw, SLJIT_IMM, 0,
                            SLJIT_IMM, 0, DOUBLEWORD | FORCE_LTS);
        break;
    case SLJIT_ADD_ABS_ADDR:
        if (SLJIT_IS_MEM(dst)) {
            /* ldd with LTS
               or
               ldd with shld without LTS */
            jump->addr += 2*sizeof(sljit_uw);
        }

        /* To prevent optimizing zero use 1 instead of 0*/
        signature_switcher2(compiler, E2K_ADDd, dst, dstw, dst, dstw,
                            SLJIT_IMM, 1, DOUBLEWORD | FORCE_LTS);
        break;
    default:
        SLJIT_ASSERT(0);
    }

    BUNDLE_CLOSE();
    return jump;
}

SLJIT_API_FUNC_ATTRIBUTE
void sljit_set_jump_addr(sljit_uw addr, sljit_uw new_target, sljit_sw executable_offset)
{
    void *start_addr, *end_addr;
    sljit_s32 code, offset;
    sljit_s32 *addr_ptr = (sljit_s32*)addr;
    sljit_s32 new_target_hi = (sljit_s32)((new_target >> 32) & 0xFFFFFFFF);
    sljit_s32 new_target_lo = (sljit_s32)(new_target & 0xFFFFFFFF);
    SLJIT_UNUSED_ARG(executable_offset);

    if ((((addr ^ new_target) >> 3) >> 28) == 0) {
        /* Now no bugs:
           - no FX operations in ALS
           - no control operations with jump preparing
           The scheduler might trigger a bug in the next versions. */
        /* check for CS0 and skip for HS */
        SLJIT_ASSERT((*addr_ptr) & 0x00004000);
        offset = 1;
        addr_ptr++;

        code = *addr_ptr;
        /* DISP CS0.opc = 0100 = 0x4*/
        while ((code & 0xF0000000) != 0x40000000) {
            addr_ptr++;
            code = *addr_ptr;
            offset++;
        }
        patch_disp(addr, new_target, offset);
    } else {
        start_addr = (void*)(addr);
        end_addr = start_addr + sizeof(sljit_uw);
        SLJIT_UPDATE_WX_FLAGS(start_addr, end_addr, 0);
        SLJIT_MEMCPY((void*)addr,
                     &new_target_hi, sizeof(new_target_hi));
        SLJIT_MEMCPY((void*)(addr+sizeof(new_target_hi)),
                     &new_target_lo, sizeof(new_target_lo));
        SLJIT_UPDATE_WX_FLAGS(start_addr, end_addr, 1);
        SLJIT_CACHE_FLUSH(start_addr, end_addr);
    }
    return;
}

SLJIT_API_FUNC_ATTRIBUTE
void sljit_set_const(sljit_uw addr,
                     sljit_s32 op,
                     sljit_sw new_constant,
                     sljit_sw executable_offset)
{
    void *start_addr = NULL, *end_addr = NULL;
    sljit_s32 new_constant_hi = (sljit_s32)((new_constant >> 32) & 0xFFFFFFFF);
    sljit_s32 new_constant_lo = (sljit_s32)(new_constant & 0xFFFFFFFF);
    sljit_s32 hi_enabled = 1;
    SLJIT_UNUSED_ARG(executable_offset);

    /* HS, ALS <---> LTS - 2 words*/

    switch(op & 0xFF) {
    case SLJIT_MOV:
        start_addr = (void*)(addr);
        end_addr = start_addr + sizeof(sljit_uw);
        break;
    case SLJIT_MOV_S32:
        new_constant_hi = (sljit_s32)(((sljit_sw)new_constant_lo >> 32)
                                      & 0xFFFFFFFF);
        start_addr = (void*)(addr);
        end_addr = start_addr + sizeof(sljit_uw);
        break;
    case SLJIT_MOV32:
        hi_enabled = 0;
        start_addr = (void*)(addr);
        end_addr = start_addr + sizeof(sljit_u32);
        break;
    case SLJIT_MOV32_U8:
    case SLJIT_MOV_U8:
        if ((op & SLJIT_32) == SLJIT_32)
            hi_enabled = 0;

        /* Possible bug here because we cant sure dst was reg.
           Using 9bit value for 8bit operation is bad idea... */

        /* Get sign of current constant. */
        if (((new_constant >> 8) & 0x1) == 1) {
            new_constant = 0xFFFFFFFFFFFFFF00 | new_constant;
        } else {
            new_constant = new_constant & 0xFF;
        }
        new_constant_hi = (sljit_s32)((new_constant >> 32) & 0xFFFFFFFF);
        new_constant_lo = (sljit_s32)(new_constant & 0xFFFFFFFF);
        break;
    default:
        SLJIT_ASSERT(0);
        break;
    }

    SLJIT_UPDATE_WX_FLAGS(start_addr, end_addr, 0);
    if (hi_enabled) {
        SLJIT_MEMCPY((void*)addr,
                     &new_constant_hi, sizeof(new_constant_hi));
    }
    SLJIT_MEMCPY((void*)(addr+sizeof(new_constant_hi)),
                 &new_constant_lo, sizeof(new_constant_lo));
    SLJIT_UPDATE_WX_FLAGS(start_addr, end_addr, 1);
    SLJIT_CACHE_FLUSH(start_addr, end_addr);
    return;
}
