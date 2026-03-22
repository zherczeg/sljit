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
          |
         \ /
          v
- [optionally] signature_switcher
  Helper to choose correct signature for operation.
          |
         \ /
          v
- emit_[full_]op | emit_<special_OP>
  Generate human readable op.
          |
         \ /
          v
- emit_<FORMAT>
  Generate op of e2k format.
          |
         \ /
          v
- emit_src | codegen
  - Choose reg from reg map.
  - Encode operation.
*/

/* TODO:
0. Codestyle to compiler via Makefile
1. Support >=v5 *
2. Support aas ops **
3. Support short jumps **
4. *Support scheduler **
5. CMP as bundle **
   - Emit flags
   - Emit pred
   - Replace slow flag generation by fast cmp native ops
6. Full 32bit support *
7. Check for nops in div and mul operations

* - func extension.
** - perf extension.
*/


SLJIT_API_FUNC_ATTRIBUTE const char* sljit_get_platform_name(void)
{
    return "e2k" SLJIT_CPUINFO;
}

#define E2K_CHANNEL_NUMBER 6

/* INT */
/* SLJIT_SP */
#define TMP_REG1 (SLJIT_SP + 1)         // signature_switcher and lower
#define TMP_REG2 TMP_REG1 + 1           // all layers
#define TMP_REG3 TMP_REG2 + 1           // upper than signature_switcher
#define FLAG_REG TMP_REG3 + 1
#define EXTRA_NREGS_NUM (FLAG_REG - SLJIT_SP + 1)
/* FLOAT */
// signature_switcher and lower
#define TMP_FREG1 (SLJIT_NUMBER_OF_FLOAT_REGISTERS + 1)
#define TMP_FREG2 TMP_FREG1 + 1           // all layers
#define TMP_FREG3 TMP_FREG2 + 1           // upper than signature_switcher
#define EXTRA_FREGS_NUM (TMP_FREG3 - TMP_FREG1 + 1)

#define FLAG_PREG 16
#define TMP_PREG1 17
#define TMP_PREG2 18

// help
#define FIELD(WORD, BIT) (WORD >> (BIT - 1))
#define FIELD_BIT(WORD, BIT) ((WORD >> (BIT - 1)) & 0x1)

// TODO
#define SLJIT_E2K_ALES_SUPPORT      0
#define SLJIT_E2K_AAS_SUPPORT       0
#define SLJIT_E2K_LOOP_MODE_SUPPORT 0

/*
    v4 compatible with v6 latencies.
*/

/*
    All E2K registers ids.
    Number registers.
    Predicates.
    Control.
*/

#define E2K_NREG_ID_START (sljit_u32)1
#define E2K_NREG_NUMBER (sljit_u32)(SLJIT_NUMBER_OF_REGISTERS \
                        + EXTRA_NREGS_NUM)
#define E2K_FREG_ID_START (sljit_u32)(E2K_NREG_ID_START + E2K_NREG_NUMBER)
#define E2K_FREG_NUMBER (sljit_u32)(SLJIT_NUMBER_OF_FLOAT_REGISTERS \
                        + EXTRA_FREGS_NUM)
#define E2K_ALLREG_NUMBER (sljit_u32)(E2K_NREG_NUMBER + E2K_FREG_NUMBER)
#define E2K_PRED_ID_START (sljit_u32)(E2K_FREG_ID_START + E2K_FREG_NUMBER)
#define E2K_PRED_NUMBER (sljit_u32)4
#define E2K_CTPR_ID_START (sljit_u32)(E2K_PRED_ID_START + E2K_PRED_NUMBER)
#define E2K_CTPR_NUMBER (sljit_u32)3
#define E2K_REG_MAX (sljit_u32)(E2K_CTPR_ID_START + E2K_CTPR_NUMBER)

#define E2K_PRED(i) (sljit_u32)(E2K_PRED_ID_START + i)
#define E2K_CTPR(i) (sljit_u32)(E2K_CTPR_ID_START + i)

// QP terms
#define E2K_PREG_WINDOW_SIZE 24
#define E2K_ROTATED_REG_BASE (E2K_ALLREG_NUMBER >> 1)
#define E2K_ROTATED_REG_SIZE 4
#define E2K_REG_WINDOW_SIZE  (E2K_ALLREG_NUMBER >> 1) + E2K_ROTATED_REG_SIZE


/*
    64 regs only cause of ENTER_GET_REGS has mask 0xff
*/

// + zero
static const sljit_u8 reg_map[E2K_NREG_NUMBER + 1] = {
    0,
    // scratch
    0, 1, 2, 3, 4, 5, 6, 7,
    8, 9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19,
    // saved
    20, 21, 22, 23, 24, 25, 26, 27,
    28, 29, 30, 31, 32, 33, 34, 35,
    36, 37, 38, 39,
    40, // SLJIT_SP
    41, // TMP_REG1
    42, // TMP_REG2
    43, // TMP_REG3
    44, // FLAG_REG
};

// + zero
static const sljit_u8 freg_map[E2K_FREG_NUMBER + 1] = {
    0,
    // scratch
    45, 46, 47, 48, 49, 50, 51, 52,
    // saved
    53, 54, 55, 56, 57, 58, 59, 60,
    61, // TMP_FREG1
    62, // TMP_FREG2
    63, // TMP_FREG3
};

// label for ctpr
// 0 reg is empty reg, because ctpr starts with 1.
static const sljit_u8 ctpr_map[4] = {
    0, 0x11, 0x12, 0x13
};

// pdst
// Direct mapping
static const sljit_u8 pred_map[E2K_PREG_WINDOW_SIZE] = {
    // base
    0,  1,  2,  3,  4,  5,  6,  7,
    8,  9,  10, 11, 12, 13, 14, 15,
    // special
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

// E2K Format description
typedef enum SrcDstMask {
    FMT_ZERO = 0x0,
    FMT_SRC1 = 0x1,
    FMT_SRC2 = 0x2,
    FMT_SRC3 = 0x4,
    FMT_DST  = 0x8,
    FMT_DST2 = 0x10,
} SrcDstMask;

// Correct input src type, sync with SrcDstMask
// Imm in case of 0, Reg in case of 1
typedef enum RegImmMask {
    REG_ZERO = 0x0,
    REG_SRC1 = 0x1,
    REG_SRC2 = 0x2,
    REG_SRC3 = 0x4,
} RegImmMask;

// IFL
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

/* -- Instructions -------------------------------------------------------- */

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
      sljit_u32 disp    : 28;
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

typedef enum {
    E2K_CONST =     0x0,
    E2K_CONST4 =    0x4,
    E2K_CONST5 =    0x5,
    E2K_CONST16 =   0x10,
    E2K_CONST32 =   0x20,
    E2K_CONST64 =   0x40,
    E2K_REG =       0x80,
    E2K_REG_R =     0x100,
    E2K_REG_FR =    0x200,
    E2K_REG_G =     0x400,
    E2K_REG_PRED =  0x800,
    E2K_REG_CTPR =  0x1000,
    E2K_REG_RARG =  0x2000
} E2kOpT;

/* -- Opcodes ------------------------------------------------------------- */

#define E2K_DISP    0x0
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

typedef struct E2kOp {
    char *name;
    E2kResourceMask mask;
    sljit_u32 opc;
    sljit_u32 opce;
    sljit_u32 opc2;
    sljit_u32 opce2;
    sljit_u32 format;
} E2kOp;

typedef struct E2kFormat {
    char *name;

    /*         src1 src2 src3
       ALOPF1: 1    1    0
       ...
    */
    sljit_u32 src_dst_mask;
} E2kFormat;

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

/* TODO:
    No real commands ANDs_fd, only ANDs_fw.
    But we use fd for unification.
    Maybe these ops will be renamed.
*/
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

// B.1.1
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
    // TODO: FXCMP
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
    // TODO: Skip FX subops
/* --------------------------------------------------- */
    { "RRs",      RES_ALS0,        0x3e, 0,    0x01, 0xc0, E2K_ALOPF16},
    { "RRd",      RES_ALS0,        0x3f, 0,    0x01, 0xc0, E2K_ALOPF16},
    // PSHUFB: opce is src3 here
    { "PSHUFB",   RES_ALS_0134,    0x4d, 0,    0x0f, 0x00, E2K_ALOPF21},
    // TODO: Skip FX ops
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
    // TODO: Skip DRTOAP
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
    // TODO: Skip vector ops
/* --------------------------------------------------- */
    // TODO: Skip array ops
/* --------------------------------------------------- */
    // TODO: Skip almost all
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

// TODO: Think about opce
static const E2kFormat e2kfmt[] = {
/*    name          src_dst_mask                                             */
    /* short encoding */
    { "ALOPF1",     FMT_SRC1 | FMT_SRC2 | FMT_DST },
    { "ALOPF2",     FMT_SRC2 | FMT_DST },
    { "ALOPF3",     FMT_SRC1 | FMT_SRC2 | FMT_SRC3 },
    { "ALOPF7",     FMT_SRC1 | FMT_SRC2 | FMT_DST2},
    { "ALOPF8",     FMT_SRC2 | FMT_DST2 },
    /* long encoding, TODO: not full support */
    { "ALOPF10",    FMT_SRC3 },
    { "ALOPF11",    FMT_SRC1 | FMT_SRC2 },
    { "ALOPF12",    FMT_SRC2 | FMT_DST },
    { "ALOPF13",    FMT_SRC1 | FMT_SRC2 | FMT_SRC3 },
    { "ALOPF15",    FMT_SRC2 },
    { "ALOPF16",    FMT_ZERO },     // TODO: think
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

/* -- Bundling helpers ---------------------------------------------------- */

static SLJIT_INLINE sljit_uw check_resource(struct sljit_compiler *compiler,
                                            E2kBundle *bundle,
                                            sljit_uw mask)
{
    sljit_uw res = bundle->res & mask;
    SLJIT_UNUSED_ARG(compiler);
    SLJIT_ASSERT(res);
    res = res & (-res);     // get the last bit of avaiable resource
    bundle->res &= ~res;    // take resource
    return res;
}

static SLJIT_INLINE int get_sylidx(struct sljit_compiler *compiler,
                                   E2kBundle *bundle,
                                   sljit_uw mask,
                                   sljit_uw shift)
{
    int idx = 0;
    sljit_uw syl = check_resource(compiler, bundle, mask);
    syl = syl >> shift;
    while(syl >>= 1)
      idx++;
    return idx;
}

#define checku4(x)  ((x) == (int32_t)(sljit_u8)(x & 0xf))
#define checku5(x)  ((x) == (int32_t)(sljit_u8)(x & 0x1f))
#define checki16(x) ((x) == (int32_t)(int16_t)(x))
#define checku16(x) ((x) == (int32_t)(sljit_u16)(x))
#define checki32(x) ((x) == (int32_t)(x))
#define checku32(x) ((x) == (sljit_u32)(x))


static E2kOpT get_reg_type(struct sljit_compiler *compiler,
                           E2kBundle *bundle,
                           sljit_uw val)
{
    SLJIT_UNUSED_ARG(compiler);
    SLJIT_UNUSED_ARG(bundle);

    // keep zero reg in mind
    if (val <= E2K_FREG_ID_START)
        return E2K_REG_R;
    else if (val <= E2K_PRED_ID_START)
        return E2K_REG_FR;
    else if (val <= E2K_CTPR_ID_START)
        return E2K_REG_PRED;
    else if (val <= E2K_REG_MAX)
        return E2K_REG_CTPR;
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
    else if (checku16(val))
        return E2K_CONST16;
    else if (checku32(val))
        return E2K_CONST32;
    else
        return E2K_CONST64;
}

/* -- Bundling ------------------------------------------------------------ */

static SLJIT_INLINE void emit_bundle_setup(struct sljit_compiler *compiler,
                                           E2kBundle *bundle)
{
    SLJIT_UNUSED_ARG(compiler);
    SLJIT_ZEROMEM(bundle, sizeof(E2kBundle));
    bundle->res = RES_INIT;
    bundle->f1 = 1; /* HS itself */
}

#define BUNDLE_SETUP() \
        E2kBundle _bundle; \
        E2kBundle *bundle = &_bundle; \
        emit_bundle_setup(compiler, bundle)

static void emit_bundle_finalize(struct sljit_compiler *compiler,
                                 E2kBundle *bundle)
{
    sljit_u32 hs_x_s_sw = (bundle->res & RES_SS) ? 0 : 0x2;
    sljit_u32 hs_c = (~bundle->res & RES_CS_ALL) >> RES_CS_SHIFT;
    sljit_u32 hs_cds = (bundle->hs_cds + 1) & -2;
    sljit_u32 hs_ales = (~bundle->res & RES_ALES_ALL) >> RES_ALES_SHIFT;
    sljit_u32 hs_als = ~bundle->res & RES_ALS_ALL;

    sljit_u8 f1 = bundle->f1;
    sljit_u8 f2 = bundle->f2;
    sljit_u8 half_pad = bundle->f3 & 0x1;
    sljit_u8 f3 = (bundle->f3 >> 1) + half_pad;
    sljit_u8 f4 = bundle->f4 + (hs_cds >> 1);
    sljit_u8 lng = f1 + f2 + f3 + f4;
    sljit_u8 hs_lng = (lng + 1) & -2;

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

    sljit_u32 *inst = (sljit_u32*)ensure_buf(compiler, sizeof(sljit_u32)*hs_lng);
    compiler->size += sizeof(sljit_u32)*hs_lng;
    sljit_u16 *inst16;
    //#if (defined SLJIT_DEBUG && SLJIT_DEBUG)
    //sljit_u32 *inst_start = inst;
    //fprintf(stderr, "Bundle addr: %p\n", inst_start);
    //#endif

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
    // TODO: Not only disp?
    // TODO: Recalculate disp in case of disp, not RETURN
    //if (hs_c & 0x01) {
        //E2kCopf2 cs0 = { bundle->cs[0] };
        //cs0.fields.disp = cs0.fields.disp + (hs_lng >> 1);
        //*inst++ = cs0.i;
    //}
    if (hs_c & 0x01) *inst++ = bundle->cs[0];
    /* --- f1 ---------------------------------------------------------------*/

    /* --- f2 ---------------------------------------------------------------*/
    /* ALES[2-5], 16-bit */
    // TODO: Support ALES2 and ALES5, fill opc2 and opce by EXT and none
    /*
    inst16 = (sljit_u16 *)inst;
    if (hs_ales & 0x04) *inst16++ = bundle->ales[2];
    if (hs_ales & 0x20) *inst16++ = bundle->ales[5];
    if (half_pad) *inst16++ = 0; // Align
    inst = (sljit_u32*)inst16;
    */
    /* CS1, 32-bit */
    if (hs_c & 0x02) *inst++ = bundle->cs[1];
    /* --- f2 ---------------------------------------------------------------*/

    /* --- f3 ---------------------------------------------------------------*/
    /* ALES[0-1,3-4], 16-bit */
    inst16 = (sljit_u16 *)inst;

    /* WARNING
       Decoder works with full words, so we should write padding as lower
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
    int used_lts = ((~bundle->res & RES_LTS_ALL) >> RES_LTS_SHIFT);
    for (int i = 3; i >= 0; i--) {
      if (used_lts & (1 << i)) {
        *inst++ = bundle->lts[i];
      }
    }
    // TODO: PLS
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

    // Reset bundle
    emit_bundle_setup(compiler, bundle);
}

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

    lts = check_resource(compiler, bundle, mask);
    if (type == E2K_CONST64) /* check 2 syls for 64-bit literal */
        check_resource(compiler, bundle, lts << 1);
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
            return reg_map[src1] | 0x80;
        case E2K_REG_FR:
            return freg_map[src1-E2K_FREG_ID_START] | 0x80;
        default:
            SLJIT_ASSERT(0);
            return 0;
        }
    } else {
        switch (get_const_type(compiler, bundle, src1)) {
        case E2K_CONST4: case E2K_CONST5:
            return src1 | 0xc0;
        default:
            SLJIT_ASSERT(0);
            return 0;
        }
    }
}

#define BYTE       sizeof(sljit_u8)
#define HALFWORD   sizeof(sljit_u16)
#define WORD       sizeof(sljit_u32)
#define DOUBLEWORD sizeof(sljit_up)
#define FORCE_LTS  0x4000
// Numbers in additional code, so we should provide correct sign extending.
// This is useful when we want restrict memory transfer but use more size LTS.
#define EXTEND_LTS   0x8000

static sljit_u32 emit_src2(struct sljit_compiler *compiler,
                          E2kBundle *bundle,
                          E2kOpT type,
                          sljit_sw src2,
                          sljit_s32 mode)
{
    if (type == E2K_REG) {
        switch (get_reg_type(compiler, bundle, src2)) {
        case E2K_REG_R:
            return reg_map[src2] | 0x80;
        case E2K_REG_FR:
            return freg_map[src2-E2K_FREG_ID_START] | 0x80;
        default:
            SLJIT_ASSERT(0);
            return 0;
        }
    } else {
// Prevent sign extension in SUBd operation in case of f16 imm operand
#if 0
        switch (get_const_type(compiler, bundle, src2)) {
        case E2K_CONST4:
            return src2 | 0xc0;
        case E2K_CONST5: case E2K_CONST16:
            return (emit_lts(compiler, bundle, E2K_CONST16, src2) | 0xd0);
        case E2K_CONST32:
            return (emit_lts(compiler, bundle, E2K_CONST32, src2) | 0xd8);
        case E2K_CONST64:
            return (emit_lts(compiler, bundle, E2K_CONST64, src2) | 0xdc);
        default:
            SLJIT_ASSERT(0);
            return 0;
        }
#else
        if ((mode & FORCE_LTS) == 0) {
            switch (get_const_type(compiler, bundle, src2)) {
            case E2K_CONST4:
                return src2 | 0xc0;
            case E2K_CONST5:
            case E2K_CONST16:
            case E2K_CONST32:
            case E2K_CONST64:
                break;
            default:
                SLJIT_ASSERT(0);
                return 0;
            }
        }

        switch (mode & (0xF | EXTEND_LTS)) {
            case BYTE:
            case BYTE | EXTEND_LTS:
            case HALFWORD:
                return (emit_lts(compiler, bundle, E2K_CONST16, src2) | 0xd0);
            case WORD:
            case HALFWORD | EXTEND_LTS:
                return (emit_lts(compiler, bundle, E2K_CONST32, src2) | 0xd8);
            case WORD | EXTEND_LTS:
            case DOUBLEWORD:
            case DOUBLEWORD | EXTEND_LTS:
                return (emit_lts(compiler, bundle, E2K_CONST64, src2) | 0xdc);
            default:
                SLJIT_ASSERT(0);
                return 0;
        }
#endif
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
            return reg_map[src3] | 0x80;
        case E2K_REG_FR:
            return freg_map[src3-E2K_FREG_ID_START] | 0x80;
        default:
            SLJIT_ASSERT(0);
        }
    } else {
        SLJIT_ASSERT(0);
    }
}

static sljit_u32 emit_dst(struct sljit_compiler *compiler,
                         E2kBundle *bundle,
                         E2kOpT type,
                         sljit_uw dst)
{
    if (type == E2K_REG) {
        switch (get_reg_type(compiler, bundle, dst)) {
        case E2K_REG_R:
            return reg_map[dst] | 0x80;
        case E2K_REG_FR:
            return freg_map[dst-E2K_FREG_ID_START] | 0x80;
        case E2K_REG_CTPR:
            SLJIT_ASSERT((dst - E2K_CTPR_ID_START) != 0);
            return ctpr_map[(dst - E2K_CTPR_ID_START)] | 0xd0;
        default:
            SLJIT_ASSERT(0);
        }
    } else {
        SLJIT_ASSERT(0);
    }
}

static sljit_u32 emit_pdst(struct sljit_compiler *compiler,
                          E2kBundle *bundle,
                          E2kOpT type,
                          sljit_uw pred)
{
    SLJIT_UNUSED_ARG(compiler);
    SLJIT_UNUSED_ARG(bundle);
    if (type == E2K_REG_PRED) {
        return pred_map[pred - E2K_PRED_ID_START];
    } else {
        SLJIT_ASSERT(0);
        return 0;
    }
}

static int emit_alopf1(struct sljit_compiler *compiler,
                       E2kBundle *bundle,
                       sljit_u32 spec, sljit_u32 op,
                       sljit_u32 src1, sljit_u32 src2, sljit_u32 dst)
{
    sljit_uw mask = e2kop[op].mask;
    int als_idx = get_sylidx(compiler, bundle, mask, RES_ALS_SHIFT);
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

static int emit_alopf2(struct sljit_compiler *compiler,
                       E2kBundle *bundle,
                       sljit_u32 spec, sljit_u32 op,
                       sljit_u32 src2, sljit_u32 dst)
{
    sljit_uw mask = e2kop[op].mask;
    int als_idx = get_sylidx(compiler, bundle, mask, RES_ALS_SHIFT);
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

static int emit_alopf3(struct sljit_compiler *compiler,
                       E2kBundle *bundle,
                       sljit_u32 spec, sljit_u32 op,
                       sljit_uw src1, sljit_u32 src2, sljit_u32 src3)
{
    sljit_uw mask = e2kop[op].mask;
    int als_idx = get_sylidx(compiler, bundle, mask, RES_ALS_SHIFT);
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

static int emit_alopf7(struct sljit_compiler *compiler,
                       E2kBundle *bundle,
                       sljit_u32 spec, sljit_u32 op,
                       sljit_u32 src1, sljit_u32 src2, sljit_u32 pred)
{
    sljit_uw mask = e2kop[op].mask;
    int als_idx = get_sylidx(compiler, bundle, mask, RES_ALS_SHIFT);
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

static int emit_alopf8(struct sljit_compiler *compiler,
                       E2kBundle *bundle,
                       sljit_u32 spec, sljit_u32 op,
                       sljit_u32 src2, sljit_u32 pred)
{
    sljit_uw mask = e2kop[op].mask;
    int als_idx = get_sylidx(compiler, bundle, mask, RES_ALS_SHIFT);
    E2kAlopf8 syl;
    syl.i = 0;
    syl.fields.pdst = pred;
    syl.fields.cmpopce = e2kop[op].opce;
    syl.fields.src2 = src2;
    syl.fields.opce = 0xc0;     // TODO: No conflicts in the future?
    syl.fields.cop = e2kop[op].opc;
    syl.fields.spec = spec;

    bundle->als[als_idx] = syl.i;
    bundle->f1++;
    return als_idx;
}

static void emit_alef1(struct sljit_compiler *compiler, E2kBundle *bundle,
                       sljit_u32 op, sljit_u32 src3, int als_idx)
{
    // TODO: WHAT?
    /* No combined operations on 2 and 5 channels */
    int res = 1 << (RES_ALES_SHIFT + als_idx);
    check_resource(compiler, bundle, res);
    // if (!(res & RES_ALES_25)) {
    if (res) {
        E2kAlef1 syl;
        syl.i = 0;
        syl.fields.src3 = (sljit_u16)src3;
        syl.fields.opc2 = e2kop[op].opc2;
        bundle->ales[als_idx] = syl.i;
        // TODO: This is wrong in case of multiple alef
        bundle->f3++;
    } else {
        SLJIT_ASSERT(0);
    }
}

static void emit_alef2(struct sljit_compiler *compiler, E2kBundle *bundle,
                       sljit_u32 op, int als_idx)
{
    // TODO: WHAT?
    /* No combined operations on 2 and 5 channels */
    int res = 1 << (RES_ALES_SHIFT + als_idx);
    check_resource(compiler, bundle, res);
    // if (!(res & RES_ALES_25)) {
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

static int emit_alopf12(struct sljit_compiler *compiler, E2kBundle *bundle,
                        sljit_u32 spec, sljit_u32 op,
                        sljit_u32 src2, sljit_u32 dst)
{
    int als_idx = emit_alopf2(compiler, bundle, spec, op, src2, dst);
    emit_alef2(compiler, bundle, op, als_idx);
    return als_idx;
}

static int emit_alopf21(struct sljit_compiler *compiler, E2kBundle *bundle,
                        sljit_u32 spec, sljit_u32 op,
                        sljit_u32 src1, sljit_u32 src2, sljit_u32 src3, sljit_u32 dst)
{
    int als_idx = emit_alopf1(compiler, bundle, spec, op, src1, src2, dst);
    emit_alef1(compiler, bundle, op, src3, als_idx);
    return als_idx;
}

static int emit_alopf11(struct sljit_compiler* compiler, E2kBundle *bundle,
                        sljit_u32 spec, sljit_u32 op,
                        sljit_u32 src1, sljit_u32 src2, sljit_u32 dst)
{
    int als_idx = emit_alopf1(compiler, bundle, spec, op, src1, src2, dst);
    emit_alef2(compiler, bundle, op, als_idx);
    return als_idx;
}

static int emit_alopf16(struct sljit_compiler *compiler,
                        E2kBundle *bundle,
                        sljit_u32 spec, sljit_u32 op,
                        sljit_u32 reg, sljit_u32 dst)
{
    int als_idx = emit_alopf1(compiler, bundle, spec, op, reg, 0xc0, dst);
    emit_alef2(compiler, bundle, op, als_idx);
    return als_idx;
}

static int emit_alopf22(struct sljit_compiler *compiler, E2kBundle *bundle,
                        sljit_u32 spec, sljit_u32 op,
                        sljit_u32 src2, sljit_u32 dst)
{
    int als_idx = emit_alopf2(compiler, bundle, spec, op, src2, dst);
    emit_alef2(compiler, bundle, op, als_idx);
    return als_idx;
}

static void emit_cds(struct sljit_compiler *compiler, E2kBundle *bundle,
                     sljit_u32 opc, sljit_u32 als, sljit_u32 pred, sljit_u32 inverted)
{
    int cds_idx = get_sylidx(compiler, bundle, RES_CDS_ALL, RES_CDS_SHIFT);
    E2kCDS syl;
    syl.i = 0;
    syl.fields.pred = pred_map[pred - E2K_PRED_ID_START] | 0x60;
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

static void emit_copf1(struct sljit_compiler *compiler, E2kBundle *bundle,
                       sljit_u32 opc, sljit_u32 ctpr, sljit_uw param)
{
    check_resource(compiler, bundle, RES_CS0);
    E2kCopf1 syl;
    syl.i = 0;
    syl.fields.param = param;
    syl.fields.opc = opc;
//#if (defined SLJIT_DEBUG && SLJIT_DEBUG)
//    fprintf(stderr, "[copf1] ctpr - E2K_CTPR_ID_START = 0x%x - 0x%x = 0x%x\n",
//            ctpr, E2K_CTPR_ID_START, (sljit_u32)ctpr - (sljit_u32)E2K_CTPR_ID_START);
//#endif
    if (ctpr)   // this is not label for ctpr
        syl.fields.ctpr = ctpr - E2K_CTPR_ID_START;

    bundle->cs[0] = syl.i;
    bundle->f1++;
}

static void emit_copf2(struct sljit_compiler *compiler, E2kBundle *bundle,
                       sljit_u32 opc, sljit_u32 ctpr, sljit_uw disp)
{
    check_resource(compiler, bundle, RES_CS0);
    E2kCopf2 syl;
    syl.i = 0;
    syl.fields.disp = disp >> 3;
    syl.fields.opc = opc;
    if (ctpr) /* RID_CTPRX is nonnull */
      syl.fields.ctpr = ctpr - E2K_CTPR_ID_START;

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

// ALOPF1
#define emit_alopf1_rr(compiler, bundle, spec, op, src1, src2, dst, mode) \
  emit_alopf1(compiler, bundle, spec, op, RSRC1(src1), RSRC2(src2), RDST(dst))
#define emit_alopf1_ir(compiler, bundle, spec, op, src1, src2, dst, mode) \
  emit_alopf1(compiler, bundle, spec, op, ISRC1(src1), RSRC2(src2), RDST(dst))
#define emit_alopf1_ri(compiler, bundle, spec, op, src1, src2, dst, mode) \
  emit_alopf1(compiler, bundle, spec, op, RSRC1(src1), ISRC2(src2, mode), RDST(dst))
#define emit_alopf1_ii(compiler, bundle, spec, op, src1, src2, dst, mode) \
  emit_alopf1(compiler, bundle, spec, op, ISRC1(src1), ISRC2(src2, mode), RDST(dst))

// ALOPF2
#define emit_alopf2_i(compiler, bundle, spec, op, src2, dst, mode) \
  emit_alopf2(compiler, bundle, spec, op, ISRC2(src2, mode), RDST(dst))
#define emit_alopf2_r(compiler, bundle, spec, op, src2, dst, mode) \
  emit_alopf2(compiler, bundle, spec, op, RSRC2(src2), RDST(dst))

// ALOPF3
#define emit_alopf3_rr(compiler, bundle, spec, op, src1, src2, src3, mode) \
  emit_alopf3(compiler, bundle, spec, op, RSRC1(src1), RSRC2(src2), RSRC3(src3))
#define emit_alopf3_ir(compiler, bundle, spec, op, src1, src2, src3, mode) \
  emit_alopf3(compiler, bundle, spec, op, ISRC1(src1), RSRC2(src2), RSRC3(src3))
#define emit_alopf3_ri(compiler, bundle, spec, op, src1, src2, src3, mode) \
  emit_alopf3(compiler, bundle, spec, op, RSRC1(src1), ISRC2(src2, mode), RSRC3(src3))
#define emit_alopf3_ii(compiler, bundle, spec, op, src1, src2, src3, mode) \
  emit_alopf3(compiler, bundle, spec, op, ISRC1(src1), ISRC2(src2, mode), RSRC3(src3))

// ALOPF7
#define emit_alopf7_rr(compiler, bundle, spec, op, src1, src2, pred, mode) \
  emit_alopf7(compiler, bundle, spec, op, RSRC1(src1), RSRC2(src2), PDST(pred))
#define emit_alopf7_ri(compiler, bundle, spec, op, src1, src2, pred, mode) \
  emit_alopf7(compiler, bundle, spec, op, RSRC1(src1), ISRC2(src2, mode), PDST(pred))
#define emit_alopf7_ir(compiler, bundle, spec, op, src1, src2, pred, mode) \
  emit_alopf7(compiler, bundle, spec, op, ISRC1(src1), RSRC2(src2), PDST(pred))

// ALOPF8
#define emit_alopf8_r(compiler, bundle, spec, op, src2, pred, mode) \
  emit_alopf8(compiler, bundle, spec, op, RSRC2(src2), PDST(pred))
#define emit_alopf8_i(compiler, bundle, spec, op, src2, pred, mode) \
  emit_alopf8(compiler, bundle, spec, op, ISRC2(src2, mode), PDST(pred))

// ALOPF12
#define emit_alopf12_i(compiler, bundle, spec, op, src2, dst, mode) \
  emit_alopf12(compiler, bundle, spec, op, ISRC2(src2, mode), RDST(dst))
#define emit_alopf12_r(compiler, bundle, spec, op, src2, dst, mode) \
  emit_alopf12(compiler, bundle, spec, op, RSRC2(src2), RDST(dst))

// ALOPF11
#define emit_alopf11_rr(compiler, bundle, spec, op, src1, src2, dst, mode) \
  emit_alopf11(compiler, bundle, spec, op, RSRC1(src1), RSRC2(src2), RDST(dst))
#define emit_alopf11_ir(compiler, bundle, spec, op, src1, src2, dst, mode) \
  emit_alopf11(compiler, bundle, spec, op, ISRC1(src1), RSRC2(src2), RDST(dst))
#define emit_alopf11_ri(compiler, bundle, spec, op, src1, src2, dst, mode) \
  emit_alopf11(compiler, bundle, spec, op, RSRC1(src1), ISRC2(src2, mode), RDST(dst))
#define emit_alopf11_ii(compiler, bundle, spec, op, src1, src2, dst, mode) \
  emit_alopf11(compiler, bundle, spec, op, ISRC1(src1), ISRC2(src2, mode), RDST(dst))

// ALOPF21
#define emit_alopf21_rrr(compiler, bundle, spec, op, src1, src2, src3, dst, mode) \
  emit_alopf21(compiler, bundle, spec, op, RSRC1(src1), RSRC2(src2), RSRC3(src3), RDST(dst))
#define emit_alopf21_rir(compiler, bundle, spec, op, src1, src2, src3, dst, mode) \
  emit_alopf21(compiler, bundle, spec, op, RSRC1(src1), ISRC2(src2, mode), RSRC3(src3), RDST(dst))

#define emit_nop(compiler, bundle, nops) \
  bundle->nop = nops

/* -- Emit control-flow instructions -------------------------------------- */
static void emit_ct(struct sljit_compiler *compiler,
                    sljit_u32 ctpr, sljit_u32 pred, int inverted)
{
    BUNDLE_SETUP();

    check_resource(compiler, bundle, RES_SS); /* Uses only part of RES_SS */
    E2kSS syl;
    syl.i = 0;
    if (pred) { /* RID_PREDX is nonnull  */
        if (inverted) {
            syl.fields.ctcond = 0x60 + (pred - E2K_PRED_ID_START);
        } else {
            syl.fields.ctcond = 0x40 + (pred - E2K_PRED_ID_START);
        }
    } else {
        syl.fields.ctcond = 0x20; /* unconditional */
    }
    if (ctpr)  /* RID_CTPRX is nonnull */
        syl.fields.ctop = ctpr - E2K_CTPR_ID_START;
    syl.fields.ipd = 3;

    bundle->ss = syl.i;
    bundle->f1++;

    emit_bundle_finalize(compiler, bundle);
}

static void emit_call(struct sljit_compiler *compiler,
                      sljit_u32 ctpr, sljit_u32 pred,
                      int inverted, int wbs)
{
    BUNDLE_SETUP();

    check_resource(compiler, bundle, RES_SS); /* Uses only part of RES_SS */
    E2kSS syl;
    syl.i = 0;
    if (pred) { /* RID_PREDX is nonnull  */
        if (inverted) {
            syl.fields.ctcond = 0x60 + (pred - E2K_PRED_ID_START);
        } else {
            syl.fields.ctcond = 0x40 + (pred - E2K_PRED_ID_START);
        }
    } else {
        syl.fields.ctcond = 0x20; /* unconditional */
    }
    if (ctpr)  /* RID_CTPRX is nonnull */
        syl.fields.ctop = ctpr - E2K_CTPR_ID_START;
    syl.fields.ipd = 3;

    bundle->ss = syl.i;
    bundle->f1++;

    check_resource(compiler, bundle, RES_CS1);
    E2kC1f1 syl2;
    syl2.i = 0;
    syl2.fields.opc = E2K_CALL;
    syl2.fields.params = wbs;
    bundle->cs[1] = syl2.i;
    bundle->f2++;

    emit_bundle_finalize(compiler, bundle);
}

/* TODO
static void emit_prepcall(struct sljit_compiler *compiler, E2kBundle *bundle,
                          sljit_u32 ctpr, ASMFunction target sljit_u32 p)
{
    BUNDLE_SETUP();

    // check 28 bit disp
    if (((((uintptr_t)target ^ (uintptr_t)as->mcp) >> 3) >> 28) == 0) {
        ptrdiff_t disp = (ptrdiff_t)((void *) target - (void *)as->mcp);
        emit_copf2(as, E2K_DISP, ctpr, disp, p);
    } else { // Target out of range; need indirect call
        emit_alopf2_i(compiler, bundle, 0, E2K_MOVTD, (intptr_t)target, ctpr, p);
    }

    emit_bundle_finalize(compiler, bundle);
}
*/

static
void emit_nop_full(struct sljit_compiler *compiler,
                          sljit_s32 count)
{
        BUNDLE_SETUP();
		emit_nop(compiler, bundle, count);
        emit_bundle_finalize(compiler, bundle);
}

// Without MEM. Only imm and reg. Dst is always reg.
static void emit_op(struct sljit_compiler* compiler, E2kBundle *bundle,
                    sljit_s32 opcode, sljit_s32 reg_imm_mask,
                    sljit_sw src1, sljit_sw src2,
                    sljit_sw src3, sljit_sw dst,
                    sljit_s32 mode)
{
    SLJIT_ASSERT(SLJIT_IS_REG(dst));

    /* Filter correct src_type from op reg_imm_mask.
       Input must be 3bit (src1, src2, src3), but opcode formats should be
       transformed into correct bit length: 2bit, 1bit. */
    sljit_u32 format = e2kop[opcode].format;
    sljit_u32 src_dst_mask = e2kfmt[format].src_dst_mask;   // real used bits
    sljit_u32 src_type = reg_imm_mask & src_dst_mask;

    switch(format)
    {
        case E2K_ALOPF1:
            SLJIT_ASSERT(src3 == 0);
            switch (src_type)
            {
                case REG_SRC1 | REG_SRC2:
                    emit_alopf1_rr(compiler, bundle, 0, opcode,
                                   SLJIT_EXTRACT_REG(src1),
                                   SLJIT_EXTRACT_REG(src2),
                                   SLJIT_EXTRACT_REG(dst),
                                   mode);
                    break;
                case REG_SRC1:
                    emit_alopf1_ri(compiler, bundle, 0, opcode,
                                   SLJIT_EXTRACT_REG(src1),
                                   src2,
                                   SLJIT_EXTRACT_REG(dst),
                                   mode);
                    break;
                case REG_SRC2:
                    emit_alopf1_ir(compiler, bundle, 0, opcode,
                                   src1,
                                   SLJIT_EXTRACT_REG(src2),
                                   SLJIT_EXTRACT_REG(dst),
                                   mode);
                    break;
                case REG_ZERO:
                    emit_alopf1_ii(compiler, bundle, 0, opcode,
                                   src1,
                                   src2,
                                   SLJIT_EXTRACT_REG(dst),
                                   mode);
                    break;
                default:
                    SLJIT_ASSERT(0);
                    break;
            }
            break;
        case E2K_ALOPF2:
            SLJIT_ASSERT(src1 == 0);
            SLJIT_ASSERT(src3 == 0);
            switch (src_type)
            {
                case REG_SRC2:
                    emit_alopf2_r(compiler, bundle, 0, opcode,
                                  SLJIT_EXTRACT_REG(src2),
                                  SLJIT_EXTRACT_REG(dst),
                                  mode);
                    break;
                case REG_ZERO:
                    emit_alopf2_i(compiler, bundle, 0, opcode,
                                  SLJIT_EXTRACT_REG(src2),
                                  SLJIT_EXTRACT_REG(dst),
                                  mode);
                    break;
            }
            break;
        case E2K_ALOPF3:
            SLJIT_ASSERT(SLJIT_IS_REG(src3));
            switch (src_type)
            {
                case REG_SRC1 | REG_SRC2 | REG_SRC3:
                    emit_alopf3_rr(compiler, bundle, 0, opcode,
                                   SLJIT_EXTRACT_REG(src1),
                                   SLJIT_EXTRACT_REG(src2),
                                   SLJIT_EXTRACT_REG(src3),
                                   mode);
                    break;
                case REG_SRC1 | REG_SRC3:
                    emit_alopf3_ri(compiler, bundle, 0, opcode,
                                   SLJIT_EXTRACT_REG(src1),
                                   src2,
                                   SLJIT_EXTRACT_REG(src3),
                                   mode);
                    break;
                case REG_SRC2 | REG_SRC3:
                    emit_alopf3_ir(compiler, bundle, 0, opcode,
                                   src1,
                                   SLJIT_EXTRACT_REG(src2),
                                   SLJIT_EXTRACT_REG(src3),
                                   mode);
                    break;
                case REG_SRC3:
                    emit_alopf3_ii(compiler, bundle, 0, opcode,
                                   src1,
                                   src2,
                                   SLJIT_EXTRACT_REG(src3),
                                   mode);
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
            switch (src_type)
            {
                case REG_SRC1 | REG_SRC2:
                    emit_alopf11_rr(compiler, bundle, 0, opcode,
                                   SLJIT_EXTRACT_REG(src1),
                                   SLJIT_EXTRACT_REG(src2),
                                   SLJIT_EXTRACT_REG(dst),
                                   mode);
                    break;
                case REG_SRC1:
                    emit_alopf11_ri(compiler, bundle, 0, opcode,
                                   SLJIT_EXTRACT_REG(src1),
                                   src2,
                                   SLJIT_EXTRACT_REG(dst),
                                   mode);
                    break;
                case REG_SRC2:
                    emit_alopf11_ir(compiler, bundle, 0, opcode,
                                   src1,
                                   SLJIT_EXTRACT_REG(src2),
                                   SLJIT_EXTRACT_REG(dst),
                                   mode);
                    break;
                case REG_ZERO:
                    emit_alopf11_ii(compiler, bundle, 0, opcode,
                                   src1,
                                   src2,
                                   SLJIT_EXTRACT_REG(dst),
                                   mode);
                    break;
            }
            break;
        case E2K_ALOPF12:
            switch (src_type)
            {
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
            switch (src_type)
            {
                case REG_SRC1 | REG_SRC2 | REG_SRC3:
                    emit_alopf21_rrr(compiler, bundle, 0, opcode,
                                   SLJIT_EXTRACT_REG(src1),
                                   SLJIT_EXTRACT_REG(src2),
                                   SLJIT_EXTRACT_REG(src3),
                                   SLJIT_EXTRACT_REG(dst),
                                   mode);
                    break;
                case REG_SRC1 | REG_SRC3:
                    emit_alopf21_rir(compiler, bundle, 0, opcode,
                                   SLJIT_EXTRACT_REG(src1),
                                   src2,
                                   SLJIT_EXTRACT_REG(src3),
                                   SLJIT_EXTRACT_REG(dst),
                                   mode);
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

// Without MEM. Only imm and reg. Dst is always reg.
static inline
void emit_op_full(struct sljit_compiler* compiler,
                         sljit_s32 opcode, sljit_s32 reg_imm_mask,
                         sljit_sw src1, sljit_sw src2,
                         sljit_sw src3, sljit_sw dst,
                         sljit_s32 mode)
{
    BUNDLE_SETUP();
    emit_op(compiler, bundle, opcode, reg_imm_mask, src1, src2, src3, dst, mode);
    emit_bundle_finalize(compiler, bundle);
}


static void emit_return(struct sljit_compiler *compiler,
                        sljit_u32 pred, int inverted)
{
    BUNDLE_SETUP();
    emit_copf1(compiler, bundle, E2K_RETURN, E2K_CTPR(3), 0);
    emit_bundle_finalize(compiler, bundle);

    emit_nop_full(compiler, 4);

    emit_bundle_setup(compiler, bundle);
    emit_ct(compiler, E2K_CTPR(3), pred, inverted);
    emit_bundle_finalize(compiler, bundle);
}

static void emit_ibranch(struct sljit_compiler *compiler,
                         sljit_uw disp, sljit_u32 pred,
                         int inverted)
{
    BUNDLE_SETUP();
    emit_copf2(compiler, bundle, E2K_IBRANCH, E2K_CTPR(1), disp);
    emit_bundle_finalize(compiler, bundle);

    emit_bundle_setup(compiler, bundle);
    emit_ct(compiler, pred, inverted, 0);
    emit_bundle_finalize(compiler, bundle);
}

static void emit_jmp(struct sljit_compiler *compiler,
                     sljit_u32 *target)
{
    BUNDLE_SETUP();
    emit_ibranch(compiler,
                 (sljit_sw)((void *)target - (void *)compiler->buf),
                 0, 0);
    emit_bundle_finalize(compiler, bundle);
}

static void emit_ijmp(struct sljit_compiler *compiler, sljit_uw addr,
                      sljit_u32 pred, sljit_u32 neg)
{
    emit_op_full(compiler, E2K_ADDd, REG_ZERO, 0, addr, 0, TMP_REG1, DOUBLEWORD | FORCE_LTS);
    BUNDLE_SETUP();
    emit_alopf2_r(compiler, bundle, 1, E2K_MOVTd, TMP_REG1, E2K_CTPR(1), DOUBLEWORD);
    emit_nop(compiler, bundle, 5);
    emit_bundle_finalize(compiler, bundle);
    emit_ct(compiler, E2K_CTPR(1), pred, neg);
}

/* -- Emit generic operations --------------------------------------------- */

// Load into reg only
static void emit_load(struct sljit_compiler *compiler,
                      sljit_s32 dst, sljit_uw dstw,
                      sljit_s32 src, sljit_uw srcw,
                      sljit_s32 mode) {
    if (!SLJIT_IS_MEM(src)) {
        SLJIT_ASSERT(0);
    }
    if (!SLJIT_IS_REG(dst)) {
        SLJIT_ASSERT(0);
    }

    BUNDLE_SETUP();

    sljit_u32 load_opc = 0;
    switch (mode & 0xF) {
        case BYTE:
            load_opc = E2K_LDb;
            break;
        case HALFWORD:
            load_opc = E2K_LDh;
            break;
        case WORD:
            load_opc = E2K_LDw;
            break;
        case DOUBLEWORD:
            load_opc = E2K_LDd;
            break;
        default:
            SLJIT_ASSERT(0);
    }

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

    if (SLJIT_IS_MEM0(src)) {
        emit_nop(compiler, bundle, 4);
        emit_alopf1_ii(compiler, bundle, 0, load_opc, 0x0, srcw, dst, lts_mode);
    } else if (SLJIT_IS_MEM1(src)) {
        emit_nop(compiler, bundle, 4);
        emit_alopf1_ri(compiler, bundle, 0, load_opc,
                       SLJIT_EXTRACT_REG(src), srcw, dst, lts_mode);
    } else if (SLJIT_IS_MEM2(src)) {
        emit_op_full(compiler, E2K_SHLd, REG_SRC1,
                     SLJIT_EXTRACT_SECOND_REG(src), srcw, 0, dst, lts_mode);
        emit_nop(compiler, bundle, 4);
        emit_alopf1_rr(compiler, bundle, 0, load_opc,
                       SLJIT_EXTRACT_REG(src), dst, dst, lts_mode);
    }

    emit_bundle_finalize(compiler, bundle);
}

// Load into reg only
static void emit_load_double(struct sljit_compiler *compiler,
                             sljit_s32 dst1, sljit_uw dst1w,
                             sljit_s32 dst2, sljit_uw dst2w,
                             sljit_s32 src1, sljit_uw src1w,
                             sljit_s32 src2, sljit_uw src2w,
                             sljit_s32 mode) {
    if (!SLJIT_IS_MEM(src1))
        SLJIT_ASSERT(0);
    if (!SLJIT_IS_MEM(src2))
        SLJIT_ASSERT(0);
    if (!SLJIT_IS_REG(dst1))
        SLJIT_ASSERT(0);
    if (!SLJIT_IS_REG(dst2))
        SLJIT_ASSERT(0);

    BUNDLE_SETUP();

    sljit_u32 load_opc = 0;
    switch (mode) {
        case BYTE:
            load_opc = E2K_LDb;
            break;
        case HALFWORD:
            load_opc = E2K_LDh;
            break;
        case WORD:
            load_opc = E2K_LDw;
            break;
        case DOUBLEWORD:
            load_opc = E2K_LDd;
            break;
        default:
            SLJIT_ASSERT(0);
    }

    if (SLJIT_IS_MEM0(src1)) {
        emit_nop(compiler, bundle, 4);
        emit_alopf1_ii(compiler, bundle, 0, load_opc, 0x0, src1w, dst1, DOUBLEWORD);
        emit_alopf1_ii(compiler, bundle, 0, load_opc, 0x0, src2w, dst2, DOUBLEWORD);
    } else if (SLJIT_IS_MEM1(src1)) {
        emit_nop(compiler, bundle, 4);
        emit_alopf1_ri(compiler, bundle, 0, load_opc,
                       SLJIT_EXTRACT_REG(src1), src1w, dst1, DOUBLEWORD);
        emit_alopf1_ri(compiler, bundle, 0, load_opc,
                       SLJIT_EXTRACT_REG(src2), src2w, dst2, DOUBLEWORD);
    } else if (SLJIT_IS_MEM2(src1)) {
        emit_op(compiler, bundle, E2K_SHLd, REG_SRC1,
                SLJIT_EXTRACT_SECOND_REG(src1), src1w, 0, TMP_REG1, DOUBLEWORD);
        emit_op(compiler, bundle, E2K_SHLd, REG_SRC1,
                SLJIT_EXTRACT_SECOND_REG(src2), src2w, 0, TMP_REG2, DOUBLEWORD);
        emit_bundle_finalize(compiler, bundle);
        emit_op_full(compiler, E2K_ADDd, REG_SRC1,
                    TMP_REG2, sizeof(sljit_uw), 0, TMP_REG2, DOUBLEWORD);
        emit_nop(compiler, bundle, 4);
        emit_alopf1_rr(compiler, bundle, 0, load_opc,
                       SLJIT_EXTRACT_REG(src1), TMP_REG1, dst1, DOUBLEWORD);
        emit_alopf1_rr(compiler, bundle, 0, load_opc,
                       SLJIT_EXTRACT_REG(src2), TMP_REG2, dst2, DOUBLEWORD);
    }

    emit_bundle_finalize(compiler, bundle);
}

// Store reg only
static void emit_store(struct sljit_compiler *compiler,
                       sljit_s32 dst, sljit_uw dstw,
                       sljit_s32 src, sljit_uw srcw,
                       sljit_s32 mode) {
    if (!SLJIT_IS_MEM(dst)) {
        SLJIT_ASSERT(0);
    }
    if (!SLJIT_IS_REG(src)) {
        SLJIT_ASSERT(0);
    }

    BUNDLE_SETUP();

    sljit_u32 store_opc = 0;
    switch (mode & 0xF) {
        case BYTE:
            store_opc = E2K_STb;
            break;
        case HALFWORD:
            store_opc = E2K_STh;
            break;
        case WORD:
            store_opc = E2K_STw;
            break;
        case DOUBLEWORD:
            store_opc = E2K_STd;
            break;
        default:
            SLJIT_ASSERT(0);
    }  

    sljit_s32 lts_mode = DOUBLEWORD;
    switch (get_const_type(compiler, bundle, dstw)) {
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

    if (SLJIT_IS_MEM0(dst)) {
        emit_nop(compiler, bundle, 4);
        emit_alopf3_ii(compiler, bundle, 0, store_opc, 0x0, dstw, src, lts_mode);
    } else if (SLJIT_IS_MEM1(dst)) {
        emit_nop(compiler, bundle, 4);
        emit_alopf3_ri(compiler, bundle, 0, store_opc,
                       SLJIT_EXTRACT_REG(dst), dstw, src, lts_mode);
    } else if (SLJIT_IS_MEM2(dst)) {
        emit_op_full(compiler, E2K_SHLd, REG_SRC1,
                     SLJIT_EXTRACT_SECOND_REG(dst), dstw, 0, TMP_REG2, lts_mode);
        emit_nop(compiler, bundle, 4);
        emit_alopf3_rr(compiler, bundle, 0, store_opc,
                       SLJIT_EXTRACT_REG(dst), TMP_REG2, src, lts_mode);
    }

    emit_bundle_finalize(compiler, bundle);
}

// Store reg only
static void emit_store_double(struct sljit_compiler *compiler,
                              sljit_s32 dst1, sljit_uw dst1w,
                              sljit_s32 dst2, sljit_uw dst2w,
                              sljit_s32 src1, sljit_uw src1w,
                              sljit_s32 src2, sljit_uw src2w,
                              sljit_s32 mode) {
    if (!SLJIT_IS_MEM(dst1))
        SLJIT_ASSERT(0);
    if (!SLJIT_IS_MEM(dst2))
        SLJIT_ASSERT(0);
    if (!SLJIT_IS_REG(src1))
        SLJIT_ASSERT(0);
    if (!SLJIT_IS_REG(src2))
        SLJIT_ASSERT(0);

    BUNDLE_SETUP();

    sljit_u32 store_opc = 0;
    switch (mode) {
        case BYTE:
            store_opc = E2K_STb;
            break;
        case HALFWORD:
            store_opc = E2K_STh;
            break;
        case WORD:
            store_opc = E2K_STw;
            break;
        case DOUBLEWORD:
            store_opc = E2K_STd;
            break;
        default:
            SLJIT_ASSERT(0);
    }

    if (SLJIT_IS_MEM0(dst1)) {
        emit_nop(compiler, bundle, 4);
        emit_alopf3_ii(compiler, bundle, 0, store_opc, 0x0, dst1w, src1, DOUBLEWORD);
        emit_alopf3_ii(compiler, bundle, 0, store_opc, 0x0, dst2w, src2, DOUBLEWORD);
    } else if (SLJIT_IS_MEM1(dst1)) {
        emit_nop(compiler, bundle, 4);
        emit_alopf3_ri(compiler, bundle, 0, store_opc,
                       SLJIT_EXTRACT_REG(dst1), dst1w, src1, DOUBLEWORD);
        emit_alopf3_ri(compiler, bundle, 0, store_opc,
                       SLJIT_EXTRACT_REG(dst2), dst2w, src2, DOUBLEWORD);
    } else if (SLJIT_IS_MEM2(dst1)) {
        emit_op(compiler, bundle, E2K_SHLd, REG_SRC1,
                SLJIT_EXTRACT_SECOND_REG(dst1), dst1w, 0, TMP_REG2, DOUBLEWORD);
        emit_op(compiler, bundle, E2K_SHLd, REG_SRC1,
                SLJIT_EXTRACT_SECOND_REG(dst2), dst2w, 0, TMP_REG1, DOUBLEWORD);
        emit_bundle_finalize(compiler, bundle);
        emit_nop(compiler, bundle, 4);
        emit_alopf3_rr(compiler, bundle, 0, store_opc,
                       SLJIT_EXTRACT_REG(dst1), TMP_REG2, src1, DOUBLEWORD);
        emit_alopf3_rr(compiler, bundle, 0, store_opc,
                       SLJIT_EXTRACT_REG(dst2), TMP_REG1, src2, DOUBLEWORD);
    }

    emit_bundle_finalize(compiler, bundle);
}

// Analyze input types (const, mem, reg) and call correct op emit.
static
void signature_switcher1(struct sljit_compiler *compiler,
                         sljit_u16 op,
                         sljit_s32 dst, sljit_sw dstw,
                         sljit_s32 src, sljit_sw srcw,
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
            emit_load(compiler, TMP_REG1, 0, src, srcw, mode);
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
            emit_load(compiler, TMP_REG1, 0, src, srcw, mode);
            emit_op_full(compiler, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                         0, TMP_REG1, 0, TMP_REG1, mode);
        } else {
            SLJIT_ASSERT(0);
        }
        emit_store(compiler, dst, dstw, TMP_REG1, 0, mode);
    } else {
        SLJIT_ASSERT(0);
    }
}

#define NOT_SRC1(x) ((x < 0) || (x > 0xF))

// Analyze input types (const, mem, reg) and call correct op emit.
// TODO: src1 as imm and mem is banned.
static
void signature_switcher2(struct sljit_compiler *compiler,
                         sljit_u16 op,
                         sljit_s32 dst, sljit_sw dstw,
                         sljit_s32 src1, sljit_sw src1w,
                         sljit_s32 src2, sljit_sw src2w,
                         sljit_s32 mode)
{
    // TODO: Optomize extra moving into TMP_REG1
    if (SLJIT_IS_REG(dst)) {
        if (SLJIT_IS_REG(src2)) {
            if (SLJIT_IS_REG(src1)) {
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                             src1, src2, 0, dst, mode);
            } else if (SLJIT_IS_IMM(src1)) {
                // src1 does not support 32 and 64 imm
                sljit_uw tmp_operand = src1w;
                sljit_u32 tmp_signature = REG_SRC2 | REG_SRC3;
                if NOT_SRC1(src1w) {
                    emit_op_full(compiler, E2K_ADDd, REG_ZERO,
                                 0, src1w, 0, TMP_REG1, mode);
                    tmp_operand = TMP_REG1;
                    tmp_signature = REG_SRC1 | REG_SRC2 | REG_SRC3;
                }
                emit_op_full(compiler, op, tmp_signature,
                             tmp_operand, src2, 0, dst, mode);
            } else if (SLJIT_IS_MEM(src1)) {
                emit_load(compiler, TMP_REG1, 0, src1, src1w, mode);
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                             TMP_REG1, src2, 0, dst, mode);
            } else {
                SLJIT_ASSERT(0);
            }
        } else if (SLJIT_IS_IMM(src2)) {
            if (SLJIT_IS_REG(src1)) {
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC3,
                             src1, src2w, 0, dst, mode);
            } else if (SLJIT_IS_IMM(src1)) {
                // src1 does not support 32 and 64 imm
                sljit_uw tmp_operand = src1w;
                sljit_u32 tmp_signature = REG_SRC3;
                if NOT_SRC1(src1w) {
                    emit_op_full(compiler, E2K_ADDd, REG_ZERO,
                                 0, src1w, 0, TMP_REG1, mode);
                    tmp_operand = TMP_REG1;
                    tmp_signature = REG_SRC1 | REG_SRC3;
                }
                emit_op_full(compiler, op, tmp_signature,
                             tmp_operand, src2w, 0, dst, mode);
            } else if (SLJIT_IS_MEM(src1)) {
                emit_load(compiler, TMP_REG1, 0, src1, src1w, mode);
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC3,
                             TMP_REG1, src2w, 0, dst, mode);
            } else {
                SLJIT_ASSERT(0);
            }
        } else if (SLJIT_IS_MEM(src2)) {
            emit_load(compiler, TMP_REG1, 0, src2, src2w, mode);
            if (SLJIT_IS_REG(src1)) {
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                             src1, TMP_REG1, 0, dst, mode);
            } else if (SLJIT_IS_IMM(src1)) {
                // src1 does not support 32 and 64 imm
                sljit_uw tmp_operand = src1w;
                sljit_u32 tmp_signature = REG_SRC2 | REG_SRC3;
                if NOT_SRC1(src1w) {
                    emit_op_full(compiler, E2K_ADDd, REG_ZERO,
                                 0, src1w, 0, TMP_REG2, mode);
                    tmp_operand = TMP_REG2;
                    tmp_signature = REG_SRC1 | REG_SRC2 | REG_SRC3;
                }
                emit_op_full(compiler, op, tmp_signature,
                             tmp_operand, TMP_REG1, 0, dst, mode);
            } else if (SLJIT_IS_MEM(src1)) {
                emit_load(compiler, TMP_REG2, 0, src1, src1w, mode);
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
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
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                             src1, src2, 0, TMP_REG1, mode);
            } else if (SLJIT_IS_IMM(src1)) {
                // src1 does not support 32 and 64 imm
                sljit_uw tmp_operand = src1w;
                sljit_u32 tmp_signature = REG_SRC2 | REG_SRC3;
                if NOT_SRC1(src1w) {
                    emit_op_full(compiler, E2K_ADDd, REG_ZERO,
                                 0, src1w, 0, TMP_REG1, mode);
                    tmp_operand = TMP_REG1;
                    tmp_signature = REG_SRC1 | REG_SRC2 | REG_SRC3;
                }
                emit_op_full(compiler, op, tmp_signature,
                             tmp_operand, src2, 0, TMP_REG1, mode);
            } else if (SLJIT_IS_MEM(src1)) {
                emit_load(compiler, TMP_REG1, 0, src1, src1w, mode);
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                             TMP_REG1, src2, 0, TMP_REG1, mode);
            } else {
                SLJIT_ASSERT(0);
            }
        } else if (SLJIT_IS_IMM(src2)) {
            if (SLJIT_IS_REG(src1)) {
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC3,
                             src1, src2w, 0, TMP_REG1, mode);
            } else if (SLJIT_IS_IMM(src1)) {
                // src1 does not support 32 and 64 imm
                sljit_uw tmp_operand = src1w;
                sljit_u32 tmp_signature = REG_SRC3;
                if NOT_SRC1(src1w) {
                    emit_op_full(compiler, E2K_ADDd, REG_ZERO,
                                 0, src1w, 0, TMP_REG1, mode);
                    tmp_operand = TMP_REG1;
                    tmp_signature = REG_SRC1 | REG_SRC3;
                }
                emit_op_full(compiler, op, tmp_signature,
                             tmp_operand, src2w, 0, TMP_REG1, mode);
            } else if (SLJIT_IS_MEM(src1)) {
                emit_load(compiler, TMP_REG1, 0, src1, src1w, mode);
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC3,
                             TMP_REG1, src2w, 0, TMP_REG1, mode);
            } else {
                SLJIT_ASSERT(0);
            }
        } else if (SLJIT_IS_MEM(src2)) {
            emit_load(compiler, TMP_REG1, 0, src2, src2w, mode);
            if (SLJIT_IS_REG(src1)) {
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                             src1, TMP_REG1, 0, TMP_REG1, mode);
            } else if (SLJIT_IS_IMM(src1)) {
                // src1 does not support 32 and 64 imm
                sljit_uw tmp_operand = src1w;
                sljit_u32 tmp_signature = REG_SRC2 | REG_SRC3;
                if NOT_SRC1(src1w) {
                    emit_op_full(compiler, E2K_ADDd, REG_ZERO,
                                 0, src1w, 0, TMP_REG2, mode);
                    tmp_operand = TMP_REG2;
                    tmp_signature = REG_SRC1 | REG_SRC2 | REG_SRC3;
                }
                emit_op_full(compiler, op, tmp_signature,
                             tmp_operand, TMP_REG1, 0, TMP_REG1, mode);
            } else if (SLJIT_IS_MEM(src1)) {
                emit_load(compiler, TMP_REG2, 0, src1, src1w, mode);
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                             TMP_REG2, TMP_REG1, 0, TMP_REG1, mode);
            } else {
                SLJIT_ASSERT(0);
            }
        } else {
            SLJIT_ASSERT(0);
        }
        emit_store(compiler, dst, dstw, TMP_REG1, 0, mode);
    } else {
        SLJIT_ASSERT(0);
    }
}

static sljit_u32 emit_flag_into_pred(struct sljit_compiler *compiler, sljit_s32 type)
{
    sljit_u32 neg = 0;
    // WARNING
    // Sync with sljit_cmp_info
    BUNDLE_SETUP();
    if (type < SLJIT_F_EQUAL) {
        switch (type) {
            case SLJIT_EQUAL:
                emit_alopf8_r(compiler, bundle, 0, E2K_CCTOPE, FLAG_REG, E2K_PRED(FLAG_PREG), DOUBLEWORD);
                break;
            case SLJIT_NOT_EQUAL:
                emit_alopf8_r(compiler, bundle, 0, E2K_CCTOPE, FLAG_REG, E2K_PRED(FLAG_PREG), DOUBLEWORD);
                neg = 1;
                break;
            case SLJIT_CARRY:
            case SLJIT_LESS:
                emit_alopf8_r(compiler, bundle, 0, E2K_CCTOPB, FLAG_REG, E2K_PRED(FLAG_PREG), DOUBLEWORD);
                break;
            case SLJIT_NOT_CARRY:
            case SLJIT_GREATER_EQUAL:
                emit_alopf8_r(compiler, bundle, 0, E2K_CCTOPB, FLAG_REG, E2K_PRED(FLAG_PREG), DOUBLEWORD);
                neg = 1;
                break;
            case SLJIT_GREATER:
                emit_alopf8_r(compiler, bundle, 0, E2K_CCTOPBE, FLAG_REG, E2K_PRED(FLAG_PREG), DOUBLEWORD);
                neg = 1;
                break;
            case SLJIT_LESS_EQUAL:
                emit_alopf8_r(compiler, bundle, 0, E2K_CCTOPBE, FLAG_REG, E2K_PRED(FLAG_PREG), DOUBLEWORD);
                break;
            case SLJIT_SIG_LESS:
                emit_alopf8_r(compiler, bundle, 0, E2K_CCTOPL, FLAG_REG, E2K_PRED(FLAG_PREG), DOUBLEWORD);
                break;
            case SLJIT_SIG_GREATER_EQUAL:
                emit_alopf8_r(compiler, bundle, 0, E2K_CCTOPL, FLAG_REG, E2K_PRED(FLAG_PREG), DOUBLEWORD);
                neg = 1;
                break;
            case SLJIT_SIG_GREATER:
                emit_alopf8_r(compiler, bundle, 0, E2K_CCTOPLE, FLAG_REG, E2K_PRED(FLAG_PREG), DOUBLEWORD);
                neg = 1;
                break;
            case SLJIT_SIG_LESS_EQUAL:
                emit_alopf8_r(compiler, bundle, 0, E2K_CCTOPLE, FLAG_REG, E2K_PRED(FLAG_PREG), DOUBLEWORD);
                break;
            case SLJIT_OVERFLOW:
                emit_alopf8_r(compiler, bundle, 0, E2K_CCTOPO, FLAG_REG, E2K_PRED(FLAG_PREG), DOUBLEWORD);
                break;
            case SLJIT_NOT_OVERFLOW:
                emit_alopf8_r(compiler, bundle, 0, E2K_CCTOPO, FLAG_REG, E2K_PRED(FLAG_PREG), DOUBLEWORD);
                neg = 1;
                break;
            // TODO:
            case SLJIT_ATOMIC_STORED:
            case SLJIT_ATOMIC_NOT_STORED:
                SLJIT_ASSERT(0);
                break;
        }
    } else if (type < SLJIT_ORDERED_EQUAL) {
        switch (type) {
            case SLJIT_F_EQUAL:
                signature_switcher2(compiler, E2K_ANDs, TMP_REG2, 0,
                                    FLAG_REG, 0, SLJIT_IMM, IFL_ZF | IFL_PF | IFL_CF,
                                    DOUBLEWORD);
                emit_alopf7_ri(compiler, bundle, 0, E2K_CMPEsb,
                               TMP_REG2, IFL_ZF, E2K_PRED(FLAG_PREG), DOUBLEWORD);
                break;
            case SLJIT_F_NOT_EQUAL:
                signature_switcher2(compiler, E2K_ANDs, TMP_REG2, 0,
                                    FLAG_REG, 0, SLJIT_IMM, IFL_ZF | IFL_PF,
                                    DOUBLEWORD);
                emit_alopf7_ri(compiler, bundle, 0, E2K_CMPEsb,
                               TMP_REG2, IFL_ZERO, E2K_PRED(FLAG_PREG), DOUBLEWORD);
                break;
            case SLJIT_F_LESS:
                signature_switcher2(compiler, E2K_ANDs, TMP_REG2, 0,
                                    FLAG_REG, 0, SLJIT_IMM, IFL_ZF | IFL_PF | IFL_CF,
                                    DOUBLEWORD);
                emit_alopf7_ri(compiler, bundle, 0, E2K_CMPEsb,
                               TMP_REG2, IFL_CF, E2K_PRED(FLAG_PREG), DOUBLEWORD);
                break;
            case SLJIT_F_GREATER_EQUAL:
                signature_switcher2(compiler, E2K_ANDs, TMP_REG2, 0,
                                    FLAG_REG, 0, SLJIT_IMM, IFL_PF | IFL_CF,
                                    DOUBLEWORD);
                emit_alopf7_ri(compiler, bundle, 0, E2K_CMPEsb,
                               TMP_REG2, IFL_ZERO, E2K_PRED(FLAG_PREG), DOUBLEWORD);
                break;
            case SLJIT_F_GREATER:
                signature_switcher2(compiler, E2K_ANDs, TMP_REG2, 0,
                                    FLAG_REG, 0, SLJIT_IMM, IFL_ZF | IFL_PF | IFL_CF,
                                    DOUBLEWORD);
                emit_alopf7_ri(compiler, bundle, 0, E2K_CMPEsb,
                               TMP_REG2, IFL_ZERO, E2K_PRED(FLAG_PREG), DOUBLEWORD);
                break;
            case SLJIT_F_LESS_EQUAL:
                signature_switcher2(compiler, E2K_ANDs, TMP_REG2, 0,
                                    FLAG_REG, 0, SLJIT_IMM, IFL_ZF | IFL_CF,
                                    DOUBLEWORD);
                emit_alopf7_ri(compiler, bundle, 0, E2K_CMPBEsb,
                               TMP_REG2, IFL_ZF, E2K_PRED(FLAG_PREG), DOUBLEWORD);
                break;
            case SLJIT_UNORDERED:
                signature_switcher2(compiler, E2K_ANDs, TMP_REG2, 0,
                                    FLAG_REG, 0, SLJIT_IMM, IFL_ZF | IFL_PF | IFL_CF,
                                    DOUBLEWORD);
                emit_alopf7_ri(compiler, bundle, 0, E2K_CMPEsb,
                               TMP_REG2, IFL_ZF | IFL_PF | IFL_CF, E2K_PRED(FLAG_PREG), DOUBLEWORD);
                break;
            case SLJIT_ORDERED:
                signature_switcher2(compiler, E2K_ANDs, TMP_REG2, 0,
                                    FLAG_REG, 0, SLJIT_IMM, IFL_PF,
                                    DOUBLEWORD);
                emit_alopf7_ri(compiler, bundle, 0, E2K_CMPEsb,
                               TMP_REG2, IFL_ZERO, E2K_PRED(FLAG_PREG), DOUBLEWORD);
                break;
        }
    } else if (type <= SLJIT_SET_ORDERED_LESS_EQUAL) {
        // TODO: ?
        SLJIT_ASSERT(0);
    } else {
        SLJIT_ASSERT(0);
    }

    emit_bundle_finalize(compiler, bundle);
    return neg;
}

SLJIT_API_FUNC_ATTRIBUTE
void* sljit_generate_code(struct sljit_compiler *compiler, sljit_s32 options,
                          void *exec_allocator_data)
{
	struct sljit_memory_fragment *buf;
	sljit_u8 *code;
	sljit_u8 *code_ptr;
	sljit_u8 *buf_ptr;
	sljit_u8 *buf_end;
	sljit_u8 len;
	sljit_sw executable_offset;

	struct sljit_label *label;
	struct sljit_jump *jump;
	struct sljit_const *const_;

	CHECK_ERROR_PTR();
	CHECK_PTR(check_sljit_generate_code(compiler, options));

    // TODO:
	// reduce_code_size(compiler);

	/* Second code generation pass. */
	code = (sljit_u8*)allocate_executable_memory(compiler->size,
                                                 options, exec_allocator_data,
                                                 &executable_offset);
    SLJIT_CACHE_FLUSH(code, code + compiler->size);

	PTR_FAIL_WITH_EXEC_IF(code);

	reverse_buf(compiler);
	buf = compiler->buf;

	code_ptr = code;
	label = compiler->labels;
	jump = compiler->jumps;
	const_ = compiler->consts;

#if (defined SLJIT_DEBUG && SLJIT_DEBUG)
    fprintf(stderr, "[gen] addr: %p\n", code_ptr);
#endif /* SLJIT_DEBUG */
    while(buf) {
        SLJIT_MEMCPY(code_ptr, buf->memory, buf->used_size);
        compiler->executable_size += buf->used_size;
        code_ptr += buf->used_size;
        buf = buf->next;
    }

    // LABELS
    while(label) {
        label->u.addr = (sljit_uw)code + label->size;
        label = label->next;
    }

    // JUMPS
    // TODO: Support short jumps
    while (jump) {
	    sljit_uw flags = jump->flags;
	    sljit_uw addr = (flags & JUMP_ADDR) ? jump->u.target : jump->u.label->u.addr;
	    jump->addr = (sljit_uw)code + jump->addr;
        sljit_set_const(jump->addr, SLJIT_MOV, addr, executable_offset);
        jump = jump->next;
    }

    // CONSTS
    while(const_) {
        const_->addr = (sljit_uw)code + const_->addr;
        const_ = const_->next;
    }


	compiler->error = SLJIT_ERR_COMPILED;
	compiler->executable_offset = executable_offset;
	// compiler->executable_size = (sljit_uw)(code_ptr - code);
	compiler->executable_size = (sljit_uw)(compiler->size);

	code = (sljit_u8*)SLJIT_ADD_EXEC_OFFSET(code, executable_offset);

	SLJIT_UPDATE_WX_FLAGS(code, (sljit_u8*)SLJIT_ADD_EXEC_OFFSET(code_ptr, executable_offset), 1);
	return (void*)code;
}

// TODO:
SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_has_cpu_feature(sljit_s32 feature_type)
{
	switch (feature_type) {
	case SLJIT_HAS_FPU:
    // case SLJIT_HAS_VIRTUAL_REGISTERS:
    // case SLJIT_HAS_ZERO_REGISTER:
	case SLJIT_HAS_CLZ:
	// case SLJIT_HAS_CTZ:
	// case SLJIT_HAS_REV:
	// case SLJIT_HAS_ROT:
	// case SLJIT_HAS_CMOV:
	// case SLJIT_HAS_PREFETCH:
	// case SLJIT_HAS_COPY_F32:
	// case SLJIT_HAS_COPY_F64:
	// case SLJIT_HAS_F64_AS_F32_PAIR:
	// case SLJIT_HAS_SIMD:
	// case SLJIT_SIMD_REGS_ARE_PAIRS:
	// case SLJIT_HAS_ATOMIC:
	// case SLJIT_HAS_MEMORY_BARRIER:
        return 1;

	default:
		return 0;
	}
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_enter(
    struct sljit_compiler *compiler,
    sljit_s32 options, sljit_s32 arg_types,
    sljit_s32 scratches, sljit_s32 saveds, sljit_s32 local_size)
{
    // -- default -------------------------------------------------------------
	sljit_s32 fscratches;
	sljit_s32 fsaveds;
	sljit_s32 size;

    CHECK_ERROR();
    CHECK(check_sljit_emit_enter(compiler, options, arg_types, scratches, saveds, local_size));
    set_emit_enter(compiler, options, arg_types, scratches, saveds, local_size);

    /*
        All registers are scratch.
        So we should save all saved registers on stack.
        We don't reserve stack for call, this one will be done in sljit_[i]call.
    */

    sljit_u32 stack_size = (saveds * 8) + local_size + (E2K_ROTATED_REG_SIZE * 8);
    SLJIT_ASSERT(stack_size < 0x7FFFFFFF); // 2**32 - 1
    compiler->local_size = stack_size;

    // TODO: Add SLJIT_SP for stack pointer register
    // TODO: Think about new reg map
    // ------------------------------------------------------------------------

    BUNDLE_SETUP();

    /* -- setwd, setbn, setbp ---------------*/
    check_resource(compiler, bundle, RES_CS1);
    E2kSETR setr;
    setr.fields.rbs    = E2K_ROTATED_REG_BASE;
    setr.fields.rsz    = E2K_ROTATED_REG_SIZE - 1;
    setr.fields.rcur   = 0x0;
    setr.fields.psz    = E2K_PREG_WINDOW_SIZE;
    setr.fields.x      = 0x0;
    setr.fields.setbn  = 0x1;
    setr.fields.setbp  = 0x1;
    setr.fields.opc    = E2K_SETR0;
    bundle->cs[1] = setr.i;
    bundle->f1++;

    check_resource(compiler, bundle, RES_LTS0);
    E2kSETR_LTS0 lts0;
    lts0.fields.x      = 0x0;
    lts0.fields.dbl    = 0x0;
    lts0.fields.nfx    = 0x1;
    lts0.fields.wsz    = E2K_REG_WINDOW_SIZE;
    lts0.fields.rpsz   = 0x0;
    lts0.fields.x1     = 0x0;
    bundle->lts[0] = lts0.i;
    bundle->f4++;

    //fprintf(stderr, "E2K_REG_WINDOW_SIZE = %d\n", E2K_REG_WINDOW_SIZE);
    //fprintf(stderr, "E2K_NREG_NUMBER = %d\n", E2K_NREG_NUMBER);
    //fprintf(stderr, "E2K_FREG_NUMBER = %d\n", E2K_FREG_NUMBER);
    //fprintf(stderr, "E2K_ALLREG_NUMBER = %d\n", E2K_ALLREG_NUMBER);
    //fprintf(stderr, "E2K_REG_MAX = %d\n", E2K_REG_MAX);
    //fprintf(stderr, "E2K_ROTATED_REG_BASE = %d\n", E2K_ROTATED_REG_BASE);
    /* -- setwd, setbn, setbp ---------------*/

    sljit_u32 src2 = -stack_size;
    emit_alopf12_i(compiler, bundle, 0, E2K_GETSP, src2, SLJIT_SP, WORD);
    emit_bundle_finalize(compiler, bundle);

    // distribute args to regs
    sljit_s32 arg_type;
    sljit_s32 si = 0, fri = 0, fsi = 0, i = 0;
    // skip return
    arg_types = arg_types >> SLJIT_ARG_SHIFT;
    while (arg_types) {
        arg_type = arg_types & SLJIT_ARG_MASK;
        // float
        if (arg_type & SLJIT_ARG_TYPE_F64) {
            emit_op(compiler, bundle, E2K_ADDd, REG_SRC2,
                    0, SLJIT_R(i), 0, SLJIT_FR(fri)+E2K_FREG_ID_START, DOUBLEWORD);
            fri++;
        } else {
            // saved
            if (!(arg_type & SLJIT_ARG_TYPE_SCRATCH_REG)) {
                emit_op(compiler, bundle, E2K_ADDd, REG_SRC2,
                        0, SLJIT_R(i), 0, SLJIT_S(si), DOUBLEWORD);
                si++;
            }
        }
        arg_types = (arg_types >> SLJIT_ARG_SHIFT);
        i++;

        // Limit by channel number
        if ((i % E2K_CHANNEL_NUMBER) == 0) {
            emit_bundle_finalize(compiler, bundle);
        }
    }
    // Maybe prev loop could not fill the wild command
    if ((i % E2K_CHANNEL_NUMBER) != 0) {
        emit_bundle_finalize(compiler, bundle);
    }
    // save input params on saved regs
    // TODO: Think about stack. It's cool e2k has a lot of registers.
    /*
    for (sljit_s32 i = 0; i < saveds; i++) {
        emit_op_full(compiler, E2K_ADDd, REG_SRC1, SLJIT_R0+i, 0, 0, SLJIT_S0-i, DOUBLEWORD);
    }
    */

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_set_context(struct sljit_compiler *compiler,
	                        sljit_s32 options, sljit_s32 arg_types,
	                        sljit_s32 scratches, sljit_s32 saveds,
                            sljit_s32 local_size)
{
    // -- default -------------------------------------------------------------
	sljit_s32 fscratches;
	sljit_s32 fsaveds;
	sljit_s32 size;

    CHECK_ERROR();
    CHECK(check_sljit_emit_enter(compiler, options, arg_types, scratches, saveds, local_size));
    set_emit_enter(compiler, options, arg_types, scratches, saveds, local_size);

    /*
        All registers are scratch.
        So we should save all saved registers on stack.
        We don't reserve stack for call, this one will be done in sljit_[i]call.
    */

    sljit_u32 stack_size = (saveds * 8) + local_size;
    SLJIT_ASSERT(stack_size < 0x7FFFFFFF); // 2**32 - 1
    compiler->local_size = stack_size;
    // ------------------------------------------------------------------------
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_return_void(struct sljit_compiler *compiler)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_return_void(compiler));

    emit_return(compiler, 0, 0);

    return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_return_to(struct sljit_compiler *compiler,
                               sljit_s32 src, sljit_sw srcw)
{
    SLJIT_ASSERT(0);
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_op0(struct sljit_compiler *compiler, sljit_s32 op)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_op0(compiler, op));

    sljit_u32 mode32 = op & SLJIT_32;
    sljit_u32 mode = mode32 ? WORD : DOUBLEWORD;
    sljit_u32 add = mode32 ? E2K_ADDs : E2K_ADDd;
    sljit_u32 udiv = mode32 ? E2K_UDIVs : E2K_UDIVd;
    sljit_u32 sdiv = mode32 ? E2K_SDIVs : E2K_SDIVd;
    sljit_u32 mul = mode32 ? E2K_MULs : E2K_MULd;
    sljit_u32 sub = mode32 ? E2K_SUBs : E2K_SUBd;

	switch (op) {
	case SLJIT_BREAKPOINT:
    case SLJIT_NOP:
		emit_nop_full(compiler, 1);
		break;
	case SLJIT_LMUL_UW:
	case SLJIT_LMUL_SW:
        // mul SLJIT_R0 and SLJIT_R1
        // Result into R1:R0
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
        // Div R0 by the R1
        // Result -> R0
        // Reminder -> R1

        emit_op_full(compiler, add, REG_SRC1, SLJIT_R0, 0, 0, TMP_REG1, mode);
        if (GET_OPCODE(op) == SLJIT_DIVMOD_UW) {
            emit_op_full(compiler, udiv, REG_SRC1 | REG_SRC2,
                         SLJIT_R0, SLJIT_R1, 0, SLJIT_R0, mode);
        } else {
            emit_op_full(compiler, sdiv, REG_SRC1 | REG_SRC2,
                         SLJIT_R0, SLJIT_R1, 0, SLJIT_R0, mode);
        }
        emit_op_full(compiler, mul, REG_SRC1 | REG_SRC2,
                     SLJIT_R0, SLJIT_R1, 0, TMP_REG2, mode);
        emit_op_full(compiler, sub, REG_SRC1 | REG_SRC2,
                     TMP_REG1, TMP_REG2, 0, SLJIT_R1, mode);
        break;
	case SLJIT_DIV_UW:
	case SLJIT_DIV_SW:
	case SLJIT_DIV_U32:
	case SLJIT_DIV_S32:
        // Div R0 by R1
        // Result -> R0
        if (GET_OPCODE(op) == SLJIT_DIV_UW) {
            emit_op_full(compiler, udiv, REG_SRC1 | REG_SRC2 | REG_SRC3,
                         SLJIT_R0, SLJIT_R1, 0, SLJIT_R0, mode);
        } else {
            emit_op_full(compiler, sdiv, REG_SRC1 | REG_SRC2 | REG_SRC3,
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
sljit_s32 sljit_emit_op1(struct sljit_compiler *compiler, sljit_s32 op,
	                     sljit_s32 dst, sljit_sw dstw,
	                     sljit_s32 src, sljit_sw srcw)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_op1(compiler, op, dst, dstw, src, srcw));
	ADJUST_LOCAL_OFFSET(dst, dstw);
	ADJUST_LOCAL_OFFSET(src, srcw);

    sljit_s32 src_reg, dst_reg;
    sljit_u32 mode32 = op & SLJIT_32;
    sljit_u32 mode = mode32 ? WORD : DOUBLEWORD;
    sljit_s32 and = mode32 ? E2K_ANDs : E2K_ANDd;
    sljit_s32 add = mode32 ? E2K_ADDs : E2K_ADDd;

    // TODO: Reduce moving in tmp reg

	switch (op) {
	case SLJIT_MOV:
	case SLJIT_MOV_P:
        signature_switcher2(compiler, E2K_ADDd, dst, dstw,
                            SLJIT_IMM, 0, src, srcw, DOUBLEWORD);
        break;
	case SLJIT_MOV_U8:
	case SLJIT_MOV32_U8:
        signature_switcher2(compiler, and, dst, dstw,
                            SLJIT_IMM, 0xFF, src, srcw,
                            BYTE | EXTEND_LTS);
        break;
	case SLJIT_MOV32_S8:
	case SLJIT_MOV_S8:
        signature_switcher2(compiler, E2K_SXT, dst, dstw,
                            SLJIT_IMM, 0x0, src, srcw, BYTE);
        break;
	case SLJIT_MOV32_U16:
	case SLJIT_MOV_U16:
        signature_switcher2(compiler, and, dst, dstw,
                            SLJIT_IMM, 0xFFFF, src, srcw, HALFWORD | EXTEND_LTS);
        break;
	case SLJIT_MOV32_S16:
	case SLJIT_MOV_S16:
        signature_switcher2(compiler, E2K_SXT, dst, dstw,
                            SLJIT_IMM, 0x1, src, srcw, HALFWORD);
        break;
	case SLJIT_MOV_U32:
        signature_switcher2(compiler, and, dst, dstw,
                            SLJIT_IMM, 0xFFFFFFFF, src, srcw, WORD | EXTEND_LTS);
        break;
	case SLJIT_MOV_S32:
        signature_switcher2(compiler, E2K_SXT, dst, dstw,
                            SLJIT_IMM, 0x3, src, srcw, WORD);
        break;
	case SLJIT_MOV32:
        signature_switcher2(compiler, E2K_ADDs, dst, dstw,
                            SLJIT_IMM, 0, src, srcw, WORD);
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
        // prep src
        if (SLJIT_IS_REG(src)) {
            src_reg = src;
        } else if (SLJIT_IS_IMM(src)) {
            emit_op_full(compiler, E2K_ADDd, REG_ZERO, 0, srcw, 0, TMP_REG3, mode);
            src_reg = TMP_REG3;
        } else if (SLJIT_IS_MEM(src)) {
            emit_load(compiler, TMP_REG3, 0, src, srcw, mode);
            src_reg = TMP_REG3;
        } else {
            SLJIT_ASSERT(0);
        }

        // prep dst
        if (SLJIT_IS_REG(dst)) {
            dst_reg = dst;
        } else if (SLJIT_IS_MEM(dst)) {
            dst_reg = TMP_REG3;
        } else {
            SLJIT_ASSERT(0);
        }

        // prevent store and sxt write beyond dst
        sljit_u32 mode_store = mode32 ? WORD : DOUBLEWORD;

        // prep controlling const
        sljit_uw order_const = mode32 ? 0x405060700010203 : 0x1020304050607;
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

        // core op: pshufb
        emit_op_full(compiler, E2K_PSHUFB, REG_SRC1 | REG_SRC2 | REG_SRC3,
                     src_reg, src_reg, TMP_REG2, dst_reg, mode);
        emit_nop_full(compiler, 3);

        // cut or extend
        switch(GET_OPCODE(op)) {
            case SLJIT_REV_U16:
                emit_op_full(compiler, and, REG_SRC1,
                             dst_reg, 0xFFFF, 0, dst_reg, mode);
                break;
            case SLJIT_REV_U32:
                emit_op_full(compiler, and, REG_SRC1,
                             dst_reg, 0xFFFFFFFF, 0, dst_reg, mode);
                break;
            case SLJIT_REV_S16:
                emit_op_full(compiler, E2K_SXT, REG_SRC2,
                             0x1, dst_reg, 0, dst_reg, mode_store);
                break;
            case SLJIT_REV_S32:
                emit_op_full(compiler, E2K_SXT, REG_SRC2,
                             0x2, dst_reg, 0, dst_reg, mode_store);
                break;
        }

        if (SLJIT_IS_MEM(dst)) {
            emit_store(compiler, dst, dstw, dst_reg, 0, mode_store);
        }
        break;
    default:
        SLJIT_ASSERT(0);
        return SLJIT_ERR_UNSUPPORTED;
	}

	return SLJIT_SUCCESS;
}


SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_op2(struct sljit_compiler *compiler, sljit_s32 op,
	                     sljit_s32 dst, sljit_sw dstw,
	                     sljit_s32 src1, sljit_sw src1w,
	                     sljit_s32 src2, sljit_sw src2w)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_op2(compiler, op, 0, dst, dstw, src1, src1w, src2, src2w));
	ADJUST_LOCAL_OFFSET(dst, dstw);
	ADJUST_LOCAL_OFFSET(src1, src1w);
	ADJUST_LOCAL_OFFSET(src2, src2w);

    sljit_u32 mode32 = op & SLJIT_32;
    sljit_u16 e2kop = 0, e2kop_fd = 0;
    sljit_u16 e2kop_and = mode32 ? E2K_ANDs : E2K_ANDd;
    sljit_u16 e2kop_or  = mode32 ? E2K_ORs : E2K_ORd;
    sljit_s32 mode = mode32 ? WORD : DOUBLEWORD;

	switch (GET_OPCODE(op)) {
	case SLJIT_ADD:
	case SLJIT_ADDC:
        e2kop = mode32 ? E2K_ADDs : E2K_ADDd;
        e2kop_fd = mode32 ? E2K_ADDs_fd : E2K_ADDd_fd;
        break;
    // TODO: Think about carry
    // TODO: Think about emit_cmp. In case of flags use E2K_SUBd_f and then use
    //       CCTO* and MERGE in emit_op_flags. In case of TMP_REG1 as dst.
    // To save flag for future operations use this chain:
    // SUB -> CCTO* -> JUMP   | MERGE
    // FLAG   pred31   pred31 | FLAG with required opcode
	case SLJIT_SUB:
	case SLJIT_SUBC:
        e2kop = mode32 ? E2K_SUBs : E2K_SUBd;
        e2kop_fd = mode32 ? E2K_SUBs_fd : E2K_SUBd_fd;
        break;
	case SLJIT_MUL:
        e2kop = mode32 ? E2K_MULs : E2K_MULd;
        break;
	case SLJIT_AND:
        e2kop = mode32 ? E2K_ANDs : E2K_ANDd;
        e2kop_fd = mode32 ? E2K_ANDs_fd : E2K_ANDd_fd;
        break;
	case SLJIT_OR:
        e2kop = mode32 ? E2K_ORs : E2K_ORd;
        e2kop_fd = mode32 ? E2K_ORs_fd : E2K_ORd_fd;
        break;
	case SLJIT_XOR:
        e2kop = mode32 ? E2K_XORs : E2K_XORd;
        e2kop_fd = mode32 ? E2K_XORs_fd : E2K_XORd_fd;
        break;
    // TODO: Understand what to do
	case SLJIT_MSHL:
	case SLJIT_SHL:
        e2kop = mode32 ? E2K_SHLs : E2K_SHLd;
        e2kop_fd = mode32 ? E2K_SHLs_fd : E2K_SHLd_fd;
        break;
    // TODO: Understand what to do
	case SLJIT_MLSHR:
	case SLJIT_LSHR:
        e2kop = mode32 ? E2K_SHRs : E2K_SHRd;
        e2kop_fd = mode32 ? E2K_SHRs_fd : E2K_SHRd_fd;
        break;
    // TODO: Understand what to do
	case SLJIT_MASHR:
	case SLJIT_ASHR:
        e2kop = mode32 ? E2K_SARs : E2K_SARd;
        e2kop_fd = mode32 ? E2K_SARs_fd : E2K_SARd_fd;
        break;
	case SLJIT_ROTL:
        e2kop = mode32 ? E2K_SCLs : E2K_SCLd;
        e2kop_fd = mode32 ? E2K_SCLs_fd : E2K_SCLd_fd;
        break;
	case SLJIT_ROTR:
        e2kop = mode32 ? E2K_SCRs : E2K_SCRd;
        e2kop_fd = mode32 ? E2K_SCRs_fd : E2K_SCRd_fd;
        break;
    default:
        SLJIT_ASSERT(0);
        return SLJIT_ERR_UNSUPPORTED;
	}

    if (GET_OPCODE(op) == SLJIT_MUL) {
        if ((dst == FLAG_REG) || HAS_FLAGS(op)) {
            if (mode32) {
                signature_switcher2(compiler, E2K_SMULX_fd, FLAG_REG, 0,
                                    src1, src1w, src2, src2w, mode);
                emit_nop_full(compiler, 5);
            } else {
                // E2K doesn't fix flags in case of 64bit mul
                // get high 64
                signature_switcher2(compiler, E2K_SMULHd, TMP_REG3, 0, src1, src1w, src2, src2w, mode);
                emit_nop_full(compiler, 5);
                // get low 64
                signature_switcher2(compiler, E2K_MULd, TMP_REG2, 0, src1, src1w, src2, src2w, mode);
                emit_nop_full(compiler, 5);
                // extend low 64
                signature_switcher2(compiler, E2K_SARd, TMP_REG2, 0, TMP_REG2, 0, SLJIT_IMM, 63, mode);

                // compare sign ext with high 64 bits
                BUNDLE_SETUP();
                emit_alopf7_rr(compiler, bundle, 0, E2K_CMPEdb,
                               TMP_REG3, TMP_REG2, E2K_PRED(TMP_PREG1), mode);
                emit_bundle_finalize(compiler, bundle);
                // Set flag in case of overflow else clean flag
                int als_idx = emit_alopf1_ii(compiler, bundle, 0, E2K_ADDd,
                                             0x0, IFL_OF, FLAG_REG, DOUBLEWORD);
                emit_rlp(compiler, bundle, als_idx, E2K_PRED(TMP_PREG1), 1);
                als_idx = emit_alopf1_ii(compiler, bundle, 0, E2K_ADDd,
                                         0x0, IFL_ZERO, FLAG_REG, DOUBLEWORD);
                emit_rlp(compiler, bundle, als_idx, E2K_PRED(TMP_PREG1), 0);
                emit_bundle_finalize(compiler, bundle);
            }
        }
        if (dst != FLAG_REG) {
            signature_switcher2(compiler, e2kop, dst, dstw, src1, src1w, src2, src2w, mode);
        }
    } else if ((GET_OPCODE(op) == SLJIT_ADDC) || (GET_OPCODE(op) == SLJIT_SUBC)) {
        // HW does not support automatic flag setting
        signature_switcher2(compiler, e2kop_and, TMP_REG3, 0, FLAG_REG, 0, SLJIT_IMM, IFL_CF, mode);
        if ((dst == FLAG_REG) || HAS_FLAGS(op)) {
            signature_switcher2(compiler, e2kop_fd, FLAG_REG, 0, src1, src1w, src2, src2w, mode);
        }
        // prevent TMP_REG2 rewriting, so use duplicate conditions
        signature_switcher2(compiler, e2kop, TMP_REG2, 0, src1, src1w, src2, src2w, mode);
        if ((dst == FLAG_REG) || HAS_FLAGS(op)) {
            signature_switcher2(compiler, e2kop_fd, TMP_REG1, 0, TMP_REG2, 0, TMP_REG3, 0, mode);
            signature_switcher2(compiler, e2kop_or, FLAG_REG, 0, FLAG_REG, 0, TMP_REG1, 0, mode);
        }
        if (dst != FLAG_REG) {
            signature_switcher2(compiler, e2kop, dst, dstw, TMP_REG2, 0, TMP_REG3, 0, mode);
        }
    } else {
        if ((dst == FLAG_REG) || HAS_FLAGS(op)) {
            signature_switcher2(compiler, e2kop_fd, FLAG_REG, 0, src1, src1w, src2, src2w, mode);
        }
        if (dst != FLAG_REG) {
            signature_switcher2(compiler, e2kop, dst, dstw, src1, src1w, src2, src2w, mode);
        }
    }

	return SLJIT_SUCCESS;
}


SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_op2u(struct sljit_compiler *compiler, sljit_s32 op,
	                      sljit_s32 src1, sljit_sw src1w,
	                      sljit_s32 src2, sljit_sw src2w)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_op2(compiler, op, 1, 0, 0, src1, src1w, src2, src2w));

	SLJIT_SKIP_CHECKS(compiler);
	return sljit_emit_op2(compiler, op, FLAG_REG, 0, src1, src1w, src2, src2w);
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_op2r(struct sljit_compiler *compiler, sljit_s32 op,
	sljit_s32 dst_reg,
	sljit_s32 src1, sljit_sw src1w,
	sljit_s32 src2, sljit_sw src2w)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_op2r(compiler, op, dst_reg, src1, src1w, src2, src2w));

    sljit_u32 mode32 = op & SLJIT_32;
    sljit_s32 mode = mode32 ? WORD : DOUBLEWORD;
    sljit_s32 mul = mode32 ? E2K_MULs : E2K_MULd;
    sljit_s32 add = mode32 ? E2K_ADDs : E2K_ADDd;

	switch (GET_OPCODE(op)) {
	case SLJIT_MULADD:
        signature_switcher2(compiler, mul, TMP_REG2, 0, src1, src1w, src2, src2w, mode);
        emit_nop_full(compiler, 5);
        signature_switcher2(compiler, add, dst_reg, 0, TMP_REG2, 0, dst_reg, 0, mode);
	}

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_shift_into(struct sljit_compiler *compiler, sljit_s32 op,
	                            sljit_s32 dst_reg,
	                            sljit_s32 src1_reg,
	                            sljit_s32 src2_reg,
	                            sljit_s32 src3, sljit_sw src3w)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_shift_into(compiler, op, dst_reg, src1_reg, src2_reg, src3, src3w));

    sljit_u32 mode32 = op & SLJIT_32;
    sljit_u32 mode = mode32 ? WORD : DOUBLEWORD;
    sljit_u32 value_mask = mode32 ? 31 : 63;
    sljit_u32 shl = mode32 ? E2K_SHLs : E2K_SHLd;
    sljit_u32 shr = mode32 ? E2K_SHRs : E2K_SHRd;
    sljit_u32 xor = mode32 ? E2K_XORs : E2K_XORd;
    sljit_u32 and = mode32 ? E2K_ANDs : E2K_ANDd;
    sljit_u32 add = mode32 ? E2K_ADDs : E2K_ADDd;
    sljit_u32 or = mode32 ? E2K_ORs : E2K_ORd;
    sljit_u32 ld = mode32 ? E2K_LDw : E2K_LDd;

    BUNDLE_SETUP();

    sljit_u32 src3_operand;
    if (SLJIT_IS_REG(src3)) {
        src3_operand = src3;
    } else if (SLJIT_IS_IMM(src3)) {
        signature_switcher2(compiler, add, TMP_REG3, 0, SLJIT_IMM, 0, src3, src3w, mode);
        src3_operand = TMP_REG3;
    } else if (SLJIT_IS_MEM(src3)) {
        src3_operand = TMP_REG3;
        emit_load(compiler, TMP_REG3, 0, src3, src3w, mode);
    } else {
        SLJIT_ASSERT(0);
    }

    if ((GET_OPCODE(op) == SLJIT_MSHL) || (GET_OPCODE(op) == SLJIT_MLSHR)) {
        emit_op_full(compiler, and, REG_SRC1,
                     src3_operand, value_mask, 0, src3_operand, mode);
    }

    // Yeah: wide command
    switch(GET_OPCODE(op)) {
        case SLJIT_SHL:
        case SLJIT_MSHL:
            emit_op(compiler, bundle, shl, REG_SRC1 | REG_SRC2,
                    src1_reg, src3_operand, 0, dst_reg, mode);
            emit_op(compiler, bundle, shr, REG_SRC1,
                    src2_reg, 1, 0, TMP_REG1, mode);
            emit_op(compiler, bundle, xor, REG_SRC1,
                    src3_operand, value_mask, 0, TMP_REG2, mode);
            emit_bundle_finalize(compiler, bundle);
            emit_op_full(compiler, shr, REG_SRC1 | REG_SRC2,
                         TMP_REG1, TMP_REG2, 0, TMP_REG1, mode);
            break;
        case SLJIT_LSHR:
        case SLJIT_MLSHR:
            emit_op(compiler, bundle, shr, REG_SRC1 | REG_SRC2,
                    src1_reg, src3_operand, 0, dst_reg, mode);
            emit_op(compiler, bundle, shl, REG_SRC1,
                    src2_reg, 1, 0, TMP_REG1, mode);
            emit_op(compiler, bundle, xor, REG_SRC1,
                    src3_operand, value_mask, 0, TMP_REG2, mode);
            emit_bundle_finalize(compiler, bundle);
            emit_op_full(compiler, shl, REG_SRC1 | REG_SRC2,
                         TMP_REG1, TMP_REG2, 0, TMP_REG1, mode);
            break;
    }
    emit_op_full(compiler, or, REG_SRC1 | REG_SRC2,
                 dst_reg, TMP_REG1, 0, dst_reg, mode);

    return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_op2_shift(struct sljit_compiler *compiler, sljit_s32 op,
	                           sljit_s32 dst, sljit_sw dstw,
	                           sljit_s32 src1, sljit_sw src1w,
	                           sljit_s32 src2, sljit_sw src2w,
	                           sljit_sw shift_arg)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_op2_shift(compiler, op, dst, dstw, src1, src1w, src2, src2w, shift_arg));
	ADJUST_LOCAL_OFFSET(dst, dstw);
	ADJUST_LOCAL_OFFSET(src1, src1w);
	ADJUST_LOCAL_OFFSET(src2, src2w);

	shift_arg &= (sljit_sw)((sizeof(sljit_sw) * 8) - 1);
    sljit_u32 mode32 = op & SLJIT_32;
    sljit_u32 mode = mode32 ? WORD : DOUBLEWORD;
    sljit_s32 shl = mode32 ? E2K_SHLs : E2K_SHLd;
    sljit_s32 add = mode32 ? E2K_ADDs : E2K_ADDd;

	if (src2 == SLJIT_IMM) {
		src2w = src2w << shift_arg;
		shift_arg = 0;
	}

	if (shift_arg == 0) {
		SLJIT_SKIP_CHECKS(compiler);
		return sljit_emit_op2(compiler, GET_OPCODE(op), dst, dstw, src1, src1w, src2, src2w);
	}

    signature_switcher2(compiler, shl, TMP_REG2, 0, src2, src2w, SLJIT_IMM, shift_arg, mode);
    signature_switcher2(compiler, add, dst, dstw, src1, src1w, TMP_REG2, 0, mode);

    return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_op_src(struct sljit_compiler *compiler, sljit_s32 op,
                        	sljit_s32 src, sljit_sw srcw)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_op_src(compiler, op, src, srcw));
	ADJUST_LOCAL_OFFSET(src, srcw);

    // TODO: Support all
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
sljit_s32 sljit_emit_op_dst(struct sljit_compiler *compiler, sljit_s32 op,
	                        sljit_s32 dst, sljit_sw dstw)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_op_dst(compiler, op, dst, dstw));
	ADJUST_LOCAL_OFFSET(dst, dstw);

	switch (op) {
	case SLJIT_FAST_ENTER:
        // save return addr from TMP_REG1 into dst
        signature_switcher2(compiler, E2K_ADDd, dst, dstw, SLJIT_IMM, 0, TMP_REG1, 0, DOUBLEWORD);
		break;
	case SLJIT_GET_RETURN_ADDRESS:
        SLJIT_ASSERT(0);
        return SLJIT_ERR_UNSUPPORTED;
	}

	return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_get_register_index(sljit_s32 type, sljit_s32 reg)
{
	CHECK_REG_INDEX(check_sljit_get_register_index(type, reg));

    switch (type) {
        case SLJIT_GP_REGISTER:
        case SLJIT_FLOAT_REGISTER:
		    return reg_map[reg];
        default:
            return -1;
    }
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_op_custom(struct sljit_compiler *compiler,
	                           void *instruction, sljit_u32 size)
{
	SLJIT_UNUSED_ARG(size);

	CHECK_ERROR();
	CHECK(check_sljit_emit_op_custom(compiler, instruction, size));

    sljit_u32 *inst = (sljit_u32*)ensure_buf(compiler, size);
    *inst = *(sljit_u32*)(instruction);
    compiler->size += size;
    return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_fop1(struct sljit_compiler *compiler, sljit_s32 op,
	                      sljit_s32 dst, sljit_sw dstw,
	                      sljit_s32 src, sljit_sw srcw)
{
	CHECK_ERROR();
	ADJUST_LOCAL_OFFSET(dst, dstw);
	ADJUST_LOCAL_OFFSET(src, srcw);

    // Choose reg map
    sljit_s32 fdst = dst, fsrc = src;
    if (SLJIT_IS_REG(dst)) {
        fdst += E2K_FREG_ID_START;
    }
    if (SLJIT_IS_REG(src)) {
        fsrc += E2K_FREG_ID_START;
    }

    sljit_u32 mode32 = op & SLJIT_32;
    sljit_u16 e2kop = 0, e2kop_fd = 0;
    sljit_s32 mode = mode32 ? WORD : DOUBLEWORD;

	switch (GET_OPCODE(op)) {
    case SLJIT_MOV_F64:
    //case SLJIT_MOV_F32:
        e2kop = mode32 ? E2K_FADDs : E2K_FADDd;
        signature_switcher2(compiler, e2kop, fdst, dstw, SLJIT_IMM, 0, fsrc, srcw, mode);
        break;
    case SLJIT_CONV_F64_FROM_F32:
    //case SLJIT_CONV_F32_FROM_F64:
        e2kop = mode32 ? E2K_FDTOFS : E2K_FSTOFD;
        signature_switcher1(compiler, e2kop, fdst, dstw, fsrc, srcw, DOUBLEWORD);
        break;
    case SLJIT_CONV_SW_FROM_F64:
    //case SLJIT_CONV_SW_FROM_F32:
        e2kop = mode32 ? E2K_FSTOID : E2K_FDTOID;
        signature_switcher1(compiler, e2kop, fdst, dstw, fsrc, srcw, mode);
        break;
    case SLJIT_CONV_S32_FROM_F64:
    //case SLJIT_CONV_S32_FROM_F32:
        e2kop = mode32 ? E2K_FSTOIS : E2K_FDTOIS;
        signature_switcher1(compiler, e2kop, fdst, dstw, fsrc, srcw, mode);
        break;
    case SLJIT_CONV_F64_FROM_SW:
    //case SLJIT_CONV_F32_FROM_SW:
        e2kop = mode32 ? E2K_IDTOFD : E2K_IDTOFS;
        signature_switcher1(compiler, e2kop, fdst, dstw, fsrc, srcw, DOUBLEWORD);
        break;
    case SLJIT_CONV_F64_FROM_S32:
    //case SLJIT_CONV_F32_FROM_S32:
        e2kop = mode32 ? E2K_ISTOFS : E2K_ISTOFD;
        signature_switcher1(compiler, e2kop, fdst, dstw, fsrc, srcw, WORD);
        break;
    // TODO:
    case SLJIT_CONV_F64_FROM_UW:
    case SLJIT_CONV_F32_FROM_UW:
    case SLJIT_CONV_F64_FROM_U32:
    case SLJIT_CONV_F32_FROM_U32:
        SLJIT_ASSERT(0);
        return SLJIT_ERR_UNSUPPORTED;
    case SLJIT_CMP_F64:
    //case SLJIT_CMP_F32:
        CHECK(check_sljit_emit_fop1_cmp(compiler, op, dst, dstw, src, srcw)); 
        e2kop = mode32 ? E2K_FCMPODsf : E2K_FCMPODdf;
        //e2kop = mode32 ? E2K_FCMPUDsf : E2K_FCMPUDdf;
        signature_switcher2(compiler, e2kop, FLAG_REG, 0, fdst, dstw,
                            fsrc, srcw, mode);
        break;
    // TODO:
    case SLJIT_NEG_F64:
    //case SLJIT_NEG_F32:
        SLJIT_ASSERT(0);
        break;
    case SLJIT_ABS_F64:
    //case SLJIT_ABS_F32:
    // TODO: Support correctly
        e2kop = mode32 ? E2K_ANDs : E2K_ANDd;
        sljit_uw abs_imm = mode32 ? 0x7FFFFFFF : 0x7FFFFFFFFFFFFFFF;
        signature_switcher2(compiler, e2kop, fdst, dstw, 0, abs_imm,
                            fsrc, srcw, mode);
        break;
    default:
        SLJIT_ASSERT(0);
        return SLJIT_ERR_UNSUPPORTED;
	}

    return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_fop2(struct sljit_compiler *compiler, sljit_s32 op,
	                      sljit_s32 dst, sljit_sw dstw,
	                      sljit_s32 src1, sljit_sw src1w,
	                      sljit_s32 src2, sljit_sw src2w)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_fop2(compiler, op, dst, dstw, src1, src1w, src2, src2w));
	ADJUST_LOCAL_OFFSET(dst, dstw);
	ADJUST_LOCAL_OFFSET(src1, src1w);
	ADJUST_LOCAL_OFFSET(src2, src2w);

	switch (GET_OPCODE(op)) {
    case SLJIT_ADD_F64:
        signature_switcher2(compiler, E2K_FADDd, dst, dstw, src1, src1w,
                            src2, src2w, DOUBLEWORD);
        break;
    case SLJIT_ADD_F32:
        signature_switcher2(compiler, E2K_FADDs, dst, dstw, src1, src1w,
                            src2, src2w, WORD);
        break;
    case SLJIT_SUB_F64:
        signature_switcher2(compiler, E2K_FSUBd, dst, dstw, src1, src1w,
                            src2, src2w, DOUBLEWORD);
        break;
    case SLJIT_SUB_F32:
        signature_switcher2(compiler, E2K_FSUBs, dst, dstw, src1, src1w,
                            src2, src2w, WORD);
        break;
    case SLJIT_MUL_F64:
        signature_switcher2(compiler, E2K_FMULd, dst, dstw, src1, src1w,
                            src2, src2w, DOUBLEWORD);
        break;
    case SLJIT_MUL_F32:
        signature_switcher2(compiler, E2K_FMULs, dst, dstw, src1, src1w,
                            src2, src2w, WORD);
        break;
    case SLJIT_DIV_F64:
        signature_switcher2(compiler, E2K_FDIVd, dst, dstw, src1, src1w,
                            src2, src2w, DOUBLEWORD);
        break;
    case SLJIT_DIV_F32:
        signature_switcher2(compiler, E2K_FDIVs, dst, dstw, src1, src1w,
                            src2, src2w, WORD);
        break;
    default:
        SLJIT_ASSERT(0);
        return SLJIT_ERR_UNSUPPORTED;
	}

    return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_fset32(struct sljit_compiler *compiler,
	                        sljit_s32 freg, sljit_f32 value)
{
    SLJIT_ASSERT(0);
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_fset64(struct sljit_compiler *compiler,
	                        sljit_s32 freg, sljit_f64 value)
{
    SLJIT_ASSERT(0);
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_fcopy(struct sljit_compiler *compiler, sljit_s32 op,
	                       sljit_s32 freg, sljit_s32 reg)
{
    SLJIT_ASSERT(0);
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE
struct sljit_label* sljit_emit_label(struct sljit_compiler *compiler)
{
	struct sljit_label *label;

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
    sljit_s32 alignment, struct sljit_read_only_buffer *buffers)
{
    return NULL;
}

SLJIT_API_FUNC_ATTRIBUTE
struct sljit_jump* sljit_emit_jump(struct sljit_compiler *compiler, sljit_s32 type)
{
	sljit_u8 *inst;
	struct sljit_jump *jump;
    sljit_u32 neg = 0;
    sljit_u32 flag = type & 0xFF;

	CHECK_ERROR_PTR();
	CHECK_PTR(check_sljit_emit_jump(compiler, type));

    if(flag < SLJIT_JUMP){
        neg = emit_flag_into_pred(compiler, flag);
    }

	jump = (struct sljit_jump*)ensure_abuf(compiler, sizeof(struct sljit_jump));
	PTR_FAIL_IF_NULL(jump);
	set_jump(jump, compiler, type);

    /* TODO:
        LTS: HS, ALS, LTS_hi, LTS_lo - 2.
    */
    jump->addr = compiler->size + 2*sizeof(sljit_u32);

    // TODO: Optimize by disp instead of movtd
    if (flag < SLJIT_JUMP) {
        emit_ijmp(compiler, 0, E2K_PRED(FLAG_PREG), neg);
    } else {
        emit_op_full(compiler, E2K_ADDd, REG_ZERO, 0, 0, 0, TMP_REG1, DOUBLEWORD | FORCE_LTS);
        BUNDLE_SETUP();
        emit_alopf2_r(compiler, bundle, 1, E2K_MOVTd, TMP_REG1, E2K_CTPR(1), DOUBLEWORD);
        emit_nop(compiler, bundle, 5);
        emit_bundle_finalize(compiler, bundle);

        // Force save return addr into TMP_REG1
        // TODO: Compact code
        if (flag == SLJIT_FAST_CALL) {
            // Current IP
            // HS + ALS + ext = 4 with align
            emit_alopf16(compiler, bundle, 0, E2K_RRd, 0x81, RSRC2(TMP_REG1));
            emit_bundle_finalize(compiler, bundle);
            // HS + ALS + LTS_hi + LTS_lo = 4
            emit_alopf1_ri(compiler, bundle, 0, E2K_ADDd, TMP_REG1, 10*sizeof(sljit_u32), TMP_REG1, DOUBLEWORD);
            emit_bundle_finalize(compiler, bundle);
        }

        // HS + SS = 2
        emit_ct(compiler, E2K_CTPR(1), 0, 0);
    }

	return jump;
}

SLJIT_API_FUNC_ATTRIBUTE
struct sljit_jump* sljit_emit_call(struct sljit_compiler *compiler, sljit_s32 type,
	                               sljit_s32 arg_types)
{
	struct sljit_jump *jump;

	CHECK_ERROR_PTR();
	CHECK_PTR(check_sljit_emit_call(compiler, type, arg_types));

	jump = (struct sljit_jump*)ensure_abuf(compiler, sizeof(struct sljit_jump));
	PTR_FAIL_IF(!jump);
	set_jump(jump, compiler, type);

    /* TODO:
        LTS: HS, ALS, LTS_hi, LTS_lo - 2.
    */
    jump->addr = compiler->size + 2*sizeof(sljit_u32);
    emit_op_full(compiler, E2K_ADDd, REG_ZERO, 0, 0, 0, TMP_REG1,
                 DOUBLEWORD | FORCE_LTS);

    // TODO: optimize
    sljit_emit_icall(compiler, type & (~SLJIT_REWRITABLE_JUMP),
                     arg_types, TMP_REG1, 0);;
	return jump;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_ijump(struct sljit_compiler *compiler, sljit_s32 type,
                           sljit_s32 src, sljit_sw srcw)
{
    // Note: Maybe SLJIT_JUMP and SLJIT_FAST_CALL only
    struct sljit_jump *jump;
    sljit_u32 *inst;
	CHECK_ERROR();
	CHECK(check_sljit_emit_ijump(compiler, type, src, srcw));

    sljit_u32 flag = type & 0xFF;

    // Make possible to read variable from callee function
	ADJUST_LOCAL_OFFSET(src, srcw);
    sljit_s32 jump_reg = TMP_REG1;

    // direct form, maybe IMM only
    if (SLJIT_IS_IMM(src)) {
        if (flag == SLJIT_JUMP) {
        // Patch in the future
		    jump = (struct sljit_jump*)ensure_abuf(compiler, sizeof(struct sljit_jump));
		    FAIL_IF_NULL(jump);
		    set_jump(jump, compiler, (sljit_u32)JUMP_ADDR);
		    jump->u.target = (sljit_uw)srcw;
		    jump->addr = compiler->size;
        }

        emit_op_full(compiler, E2K_ADDd, REG_ZERO, 0, srcw, 0, jump_reg, DOUBLEWORD);
        // TODO: Now MOVTd only, but for perf we want disp in the future.
        // We should calc jump len and replace movtd by disp.
    // indirect form
    } else if (SLJIT_IS_MEM(src)) {
        emit_load(compiler, jump_reg, 0, src, srcw, DOUBLEWORD);
    // indirect form
    } else if (SLJIT_IS_REG(src)) {
        jump_reg = src;
    } else {
        SLJIT_ASSERT(0);
    }

    BUNDLE_SETUP();
    emit_alopf2_r(compiler, bundle, 1, E2K_MOVTd, jump_reg, E2K_CTPR(1), DOUBLEWORD);
    emit_nop(compiler, bundle, 5);
    emit_bundle_finalize(compiler, bundle);

    // Force save return addr into TMP_REG1
    if (flag == SLJIT_FAST_CALL) {
        // Current IP
        // HS + ALS + ext = 4 with align
        emit_alopf16(compiler, bundle, 0, E2K_RRd, 0x81, RSRC2(TMP_REG1));
        emit_bundle_finalize(compiler, bundle);
        // HS + ALS + LTS_hi + LTS_lo = 4
        emit_alopf1_ri(compiler, bundle, 0, E2K_ADDd, TMP_REG1, 10*sizeof(sljit_u32), TMP_REG1, DOUBLEWORD);
        emit_bundle_finalize(compiler, bundle);
    }

    // HS + SS = 2
    emit_ct(compiler, E2K_CTPR(1), 0, 0);

    return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_icall(struct sljit_compiler *compiler, sljit_s32 type,
	                       sljit_s32 arg_types,
	                       sljit_s32 src, sljit_sw srcw)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_icall(compiler, type, arg_types, src, srcw));

    sljit_u32 flag = type & 0xFF;

    // Make possible to read variable from callee function
	ADJUST_LOCAL_OFFSET(src, srcw);
    sljit_s32 jump_reg = TMP_REG1;

    // direct form, maybe IMM only
    if (SLJIT_IS_IMM(src)) {
        //if (flag == SLJIT_JUMP) {
        //// Patch in the future
		//    jump = (struct sljit_jump*)ensure_abuf(compiler, sizeof(struct sljit_jump));
		//    FAIL_IF_NULL(jump);
		//    set_jump(jump, compiler, (sljit_u32)JUMP_ADDR);
		//    jump->u.target = (sljit_uw)srcw;
		//    jump->addr = compiler->size;
        //}

        emit_op_full(compiler, E2K_ADDd, REG_ZERO, 0, srcw, 0, jump_reg, DOUBLEWORD);
        // TODO: Now MOVTd only, but for perf we want disp in the future.
        // We should calc jump len and replace movtd by disp.
    // indirect form
    } else if (SLJIT_IS_MEM(src)) {
        emit_load(compiler, jump_reg, 0, src, srcw, DOUBLEWORD);
    // indirect form
    } else if (SLJIT_IS_REG(src)) {
        jump_reg = src;
    } else {
        SLJIT_ASSERT(0);
    }

	/* Remove return value. */
    sljit_s32 arg_return = arg_types & SLJIT_ARG_MASK;
	arg_types >>= SLJIT_ARG_SHIFT;

    // Pass args into new window due to e2k ABI
    // TODO: schedule
    BUNDLE_SETUP();
    sljit_u32 arg_num = 0;
    sljit_s32 ri = 0, fri = 0;
	while (arg_types) {
        if ((arg_types & SLJIT_ARG_MASK) >= SLJIT_ARG_TYPE_F64 ) {
            emit_alopf1(compiler, bundle, 0, E2K_ADDd,
                        freg_map[SLJIT_FR(fri)] | 0x80, 0 | 0xc0, arg_num);
            fri++;
        } else {
            emit_alopf1(compiler, bundle, 0, E2K_ADDd,
                        reg_map[SLJIT_R(ri)] | 0x80, 0 | 0xc0, arg_num);
            ri++;
        }
        // emit_bundle_finalize(compiler, bundle);
		arg_types >>= SLJIT_ARG_SHIFT;
        arg_num++;

        // Limit by channel number
        if ((arg_num % E2K_CHANNEL_NUMBER) == 0) {
            emit_bundle_finalize(compiler, bundle);
        }
	}

    // Maybe prev loop could not fill the wild command
    if ((arg_num % E2K_CHANNEL_NUMBER) != 0) {
        emit_bundle_finalize(compiler, bundle);
    }

    emit_alopf2_r(compiler, bundle, 1, E2K_MOVTd, jump_reg, E2K_CTPR(1), DOUBLEWORD);
    emit_nop(compiler, bundle, 5);
    emit_bundle_finalize(compiler, bundle);

    sljit_u32 src2 = -(arg_num * sizeof(sljit_uw));
    emit_alopf12_i(compiler, bundle, 0, E2K_GETSP, src2, SLJIT_SP, WORD);
    emit_bundle_finalize(compiler, bundle);

    // HS + SS = 2
    emit_call(compiler, E2K_CTPR(1), 0, 0, E2K_ROTATED_REG_BASE);

    // move return reg into SLJIT_RETURN_REG
    if (arg_return >= SLJIT_ARG_TYPE_F64) {
        emit_alopf1(compiler, bundle, 0, E2K_ADDd, 0, 0 | 0xc0,
                    freg_map[SLJIT_RETURN_REG] | 0x80);
    } else {
        emit_alopf1(compiler, bundle, 0, E2K_ADDd, 0, 0 | 0xc0,
                    reg_map[SLJIT_RETURN_REG] | 0x80);
    }
    emit_bundle_finalize(compiler, bundle);
    return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_op_flags(struct sljit_compiler *compiler, sljit_s32 op,
	                          sljit_s32 dst, sljit_sw dstw,
	                          sljit_s32 type)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_op_flags(compiler, op, dst, dstw, type));
	ADJUST_LOCAL_OFFSET(dst, dstw);

    // For pred inversion
    sljit_u32 neg = emit_flag_into_pred(compiler, type);

    // Move from pred into reg
    BUNDLE_SETUP();
    sljit_s32 als_idx = emit_alopf1_ii(compiler, bundle, 0, E2K_MERGEd,
                                       0x0, 0x1, TMP_REG2, DOUBLEWORD);
    emit_mrgc(compiler, bundle, als_idx, E2K_PRED(FLAG_PREG), neg);
    emit_bundle_finalize(compiler, bundle);

    sljit_u32 mode32 = op & SLJIT_32;
    sljit_u16 e2kop = 0, e2kop_fd = 0;
    sljit_s32 mode = mode32 ? WORD : DOUBLEWORD;

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
        e2kop = mode32 ? E2K_ANDs : E2K_ANDd;
        e2kop_fd = mode32 ? E2K_ANDs_fd : E2K_ANDd_fd;
        break;
	case SLJIT_OR:
        e2kop = mode32 ? E2K_ORs : E2K_ORd;
        e2kop_fd = mode32 ? E2K_ORs_fd : E2K_ORd_fd;
        break;
	case SLJIT_XOR:
        e2kop = mode32 ? E2K_XORs : E2K_XORd;
        e2kop_fd = mode32 ? E2K_XORs_fd : E2K_XORd_fd;
        break;
    default:
        SLJIT_ASSERT(0);
        return SLJIT_ERR_UNSUPPORTED;
	}

    if (e2kop_fd) {
        if ((dst == FLAG_REG) || HAS_FLAGS(op)) {
            signature_switcher2(compiler, e2kop_fd, FLAG_REG, 0,
                                TMP_REG2, 0, dst, dstw, mode);
        }
        if (dst != FLAG_REG) {
            signature_switcher2(compiler, e2kop, dst, dstw,
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
        case SLJIT_F_NOT_EQUAL:
        case SLJIT_F_LESS:
        case SLJIT_F_GREATER_EQUAL:
        case SLJIT_F_GREATER:
        case SLJIT_F_LESS_EQUAL:
        case SLJIT_UNORDERED:
        case SLJIT_ORDERED:
            return 1;

        default:
            SLJIT_ASSERT(0);
            return SLJIT_ERR_UNSUPPORTED;
    }
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_select(struct sljit_compiler *compiler, sljit_s32 type,
	                        sljit_s32 dst_reg,
	                        sljit_s32 src1, sljit_sw src1w,
	                        sljit_s32 src2_reg)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_select(compiler, type, dst_reg, src1, src1w, src2_reg));
	ADJUST_LOCAL_OFFSET(src1, src1w);

    sljit_u32 mode32 = type & SLJIT_32;
    sljit_u32 mode = mode32 ? WORD : DOUBLEWORD;

    if (type & SLJIT_COMPARE_SELECT) {
        type &= ~SLJIT_COMPARE_SELECT;
        FAIL_IF(sljit_emit_op2u(compiler, SLJIT_SUB | SLJIT_SET_Z | mode32, src1, src1w,
                                src2_reg, 0));
    }

    // For pred inversion
    sljit_u32 neg = emit_flag_into_pred(compiler, type & ~SLJIT_32);

    // Move from pred into reg
    BUNDLE_SETUP();
    sljit_s32 e2kop = mode32 ? E2K_MERGEs : E2K_MERGEd;
    sljit_s32 als_idx;
    if (SLJIT_IS_MEM(src1)) {
        emit_load(compiler, TMP_REG2, 0, src1, src1w, mode);
        als_idx = emit_alopf1_rr(compiler, bundle, 0, e2kop,
                                 TMP_REG2, src2_reg, dst_reg, mode);
    } else if (SLJIT_IS_REG(src1)) {
        als_idx = emit_alopf1_rr(compiler, bundle, 0, e2kop,
                                 src1, src2_reg, dst_reg, mode);
    } else if (SLJIT_IS_IMM(src1)) {
        // src1 does not support 32 and 64 imm
        if NOT_SRC1(src1w) {
            emit_op_full(compiler, E2K_ADDd, REG_ZERO,
                         0, src1w, 0, TMP_REG1, mode);
            als_idx = emit_alopf1_rr(compiler, bundle, 0, e2kop,
                                     TMP_REG1, src2_reg, dst_reg, mode);
        } else {
            als_idx = emit_alopf1_ir(compiler, bundle, 0, e2kop,
                                     src1w, src2_reg, dst_reg, mode);
        }
    } else {
        SLJIT_ASSERT(0);
    }
    // select: if true 1 else 2
    // mrgc:   if true 2 else 1
    neg = !neg;
    emit_mrgc(compiler, bundle, als_idx, E2K_PRED(FLAG_PREG), neg);
    emit_bundle_finalize(compiler, bundle);

    return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_fselect(struct sljit_compiler *compiler, sljit_s32 type,
	                         sljit_s32 dst_freg,
	                         sljit_s32 src1, sljit_sw src1w,
	                         sljit_s32 src2_freg)
{
    SLJIT_ASSERT(0);
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_mem(struct sljit_compiler *compiler, sljit_s32 type,
	                     sljit_s32 reg,
	                     sljit_s32 mem, sljit_sw memw)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_mem(compiler, type, reg, mem, memw));

	if (!(reg & REG_PAIR_MASK))
		return sljit_emit_mem_unaligned(compiler, type, reg, mem, memw);

	ADJUST_LOCAL_OFFSET(mem, memw);
    sljit_sw mem2w;
    if (SLJIT_IS_MEM2(mem)) {
        mem2w = memw;
    } else {
        mem2w = memw + sizeof(sljit_sw);
    }

    if (type & SLJIT_MEM_STORE) {
        emit_store_double(compiler, mem, memw, mem, mem2w,
                          SLJIT_EXTRACT_REG(reg), 0, SLJIT_EXTRACT_SECOND_REG(reg), 0, DOUBLEWORD);
    } else {
        emit_load_double(compiler, SLJIT_EXTRACT_REG(reg), 0, SLJIT_EXTRACT_SECOND_REG(reg), 0,
                         mem, memw, mem, mem2w, DOUBLEWORD);
    }

    return SLJIT_SUCCESS;
}

// TODO
SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_atomic_load(struct sljit_compiler *compiler, sljit_s32 op,
	                             sljit_s32 dst_reg,
	                             sljit_s32 mem_reg)
{
    SLJIT_ASSERT(0);
    return SLJIT_ERR_UNSUPPORTED;
}


// TODO
SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_atomic_store(struct sljit_compiler *compiler, sljit_s32 op,
	                              sljit_s32 src_reg,
	                              sljit_s32 mem_reg,
	                              sljit_s32 temp_reg)
{
    SLJIT_ASSERT(0);
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE
struct sljit_const* sljit_emit_const(struct sljit_compiler *compiler, sljit_s32 op,
	                                 sljit_s32 dst, sljit_sw dstw,
	                                 sljit_sw init_value)
{
    struct sljit_const *const_;

    // E2K Supports 64-bit consts in LTS
	CHECK_ERROR_PTR();
	CHECK_PTR(check_sljit_emit_const(compiler, op, dst, dstw, init_value));
	ADJUST_LOCAL_OFFSET(dst, dstw);

	const_ = (struct sljit_const*)ensure_abuf(compiler, sizeof(struct sljit_const));
	PTR_FAIL_IF(!const_);
    set_const(const_, compiler);
    /* TODO:
        LTS: HS, ALS, LTS_hi, LTS_lo - 2.
        Total - 2.
    */
    const_->addr = const_->addr + 2*sizeof(sljit_u32);

    // TODO: Make operations global as macro
    sljit_u32 mode32 = op & SLJIT_32;
    sljit_s32 add = mode32 ? E2K_ADDs : E2K_ADDd;
    sljit_s32 and = mode32 ? E2K_ANDs : E2K_ANDd;
    sljit_s32 or = mode32 ? E2K_ANDs : E2K_ANDd;
    sljit_s32 mode = mode32 ? WORD : DOUBLEWORD;
    sljit_s32 mode_store = mode;

    // TODO: make MOVE operations as the function
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
                            SLJIT_IMM, 0x3, SLJIT_IMM, init_value & 0xFFFFFFFF,
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
        signature_switcher2(compiler, add, dst, dstw,
                            SLJIT_IMM, 0, SLJIT_IMM, init_value,
                            mode_store | FORCE_LTS);
        break;
	default:
        SLJIT_ASSERT(0);
    }

    return const_;
}

SLJIT_API_FUNC_ATTRIBUTE
struct sljit_jump* sljit_emit_op_addr(struct sljit_compiler *compiler, sljit_s32 op,
	                                  sljit_s32 dst, sljit_sw dstw)
{
	struct sljit_jump *jump;

	CHECK_ERROR_PTR();
	CHECK_PTR(check_sljit_emit_op_addr(compiler, op, dst, dstw));
	ADJUST_LOCAL_OFFSET(dst, dstw);

	jump = (struct sljit_jump*)ensure_abuf(compiler, sizeof(struct sljit_jump));
	PTR_FAIL_IF(!jump);
	set_mov_addr(jump, compiler, 0);

    /* TODO:
        LTS: HS, ALS, LTS_hi, LTS_lo - 2.
    */
    jump->addr = compiler->size + 2*sizeof(sljit_u32);

    switch(GET_OPCODE(op)) {
    case SLJIT_MOV_ADDR:
        signature_switcher2(compiler, E2K_ADDd, dst, dstw, SLJIT_IMM, 0,
                            SLJIT_IMM, 0, DOUBLEWORD | FORCE_LTS);
		break;
    case SLJIT_MOV_ABS_ADDR:
        SLJIT_ASSERT(0);
		break;
    case SLJIT_ADD_ABS_ADDR:
        SLJIT_ASSERT(0);
		break;
	default:
        SLJIT_ASSERT(0);
    }

    return jump;
}

SLJIT_API_FUNC_ATTRIBUTE
void sljit_set_jump_addr(sljit_uw addr, sljit_uw new_target, sljit_sw executable_offset)
{
    void *start_addr, *end_addr;
    sljit_s32 new_target_hi = ((new_target >> 32) & 0xFFFFFFFF);
    sljit_s32 new_target_lo = (new_target & 0xFFFFFFFF);
    // TODO: Support U8
	//if (op == SLJIT_MOV) {
		start_addr = (void*)(addr);
        end_addr = start_addr + sizeof(sljit_uw);
		SLJIT_UPDATE_WX_FLAGS(start_addr, end_addr, 0);
	    SLJIT_MEMCPY((void*)addr,
                     &new_target_hi, sizeof(new_target_hi));
	    SLJIT_MEMCPY((void*)(addr+sizeof(new_target_hi)),
                     &new_target_lo, sizeof(new_target_lo));
		SLJIT_UPDATE_WX_FLAGS(start_addr, end_addr, 1);
        SLJIT_CACHE_FLUSH(start_addr, end_addr);
		return;
	//}
    return;
}

SLJIT_API_FUNC_ATTRIBUTE
void sljit_set_const(sljit_uw addr, sljit_s32 op, sljit_sw new_constant, sljit_sw executable_offset)
{
    void *start_addr, *end_addr;
    sljit_s32 mode32 = 0;
    sljit_s32 new_constant_hi = ((new_constant >> 32) & 0xFFFFFFFF);
    sljit_s32 new_constant_lo = (new_constant & 0xFFFFFFFF);
    sljit_s32 hi_enabled = 1;
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
        if ((op & 0xFF) == SLJIT_MOV32_U8) {
            mode32 = 1;
        }
        // TODO: Possible bug here because we dont know dst
        if (((new_constant >> 8) & 0x1) == 1) {
            new_constant = 0xFFFFFFFFFFFFFF00 | new_constant;
            if (mode32) {
                hi_enabled = 0;
            }
        } else {
            new_constant = new_constant & 0xFF;
            if (mode32) {
                hi_enabled = 0;
            }
        }
        new_constant_hi = ((new_constant >> 32) & 0xFFFFFFFF);
        new_constant_lo = (new_constant & 0xFFFFFFFF);
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
