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

SLJIT_API_FUNC_ATTRIBUTE const char* sljit_get_platform_name(void)
{
    return "e2k" SLJIT_CPUINFO;
}

#define TMP_REG1 (SLJIT_NUMBER_OF_REGISTERS)
#define TMP_REG2 (SLJIT_NUMBER_OF_REGISTERS + 1)

// help
#define FIELD(WORD, BIT) (WORD >> (BIT - 1))
#define FIELD_BIT(WORD, BIT) ((WORD >> (BIT - 1)) & 0x1)

// TODO
#define SLJIT_E2K_ALES_SUPPORT      0
#define SLJIT_E2K_AAS_SUPPORT       0
#define SLJIT_E2K_LOOP_MODE_SUPPORT 0

// QP terms
#define E2K_REG_WINDOW_SIZE  32
#define E2K_ROTATED_REG_BASE 4
#define E2K_ROTATED_REG_SIZE 4
#define E2K_ARG_REG_BASE     4

// Word terms
#define E2K_ROTATED_REG_BASE_32 (E2K_ROTATED_REG_BASE * 2)
#define E2K_ROTATED_REG_SIZE_32 (E2K_ROTATED_REG_SIZE * 2)
#define E2K_ARG_REG_BASE_32     (E2K_ARG_REG_BASE * 2)

/*
    v4 compatible with v6 latencies.
*/

/*
    All E2K registers ids.
    Number registers.
    Predicates.
    Control.
*/
#define E2K_NREG_ID_START 1
#define E2K_NREG_NUMBER SLJIT_NUMBER_OF_REGISTERS \
                        + SLJIT_NUMBER_OF_TEMPORARY_REGISTERS
#define E2K_PRED_ID_START E2K_NREG_ID_START + E2K_NREG_NUMBER
#define E2K_PRED_NUMBER 4
#define E2K_CTPR_ID_START E2K_PRED_ID_START + E2K_PRED_NUMBER
#define E2K_CTPR_NUMBER 3
#define E2K_REG_MAX E2K_CTPR_ID_START + E2K_CTPR_NUMBER

#define E2K_PRED(i) E2K_PRED_ID_START + i
#define E2K_CTPR(i) E2K_CTPR_ID_START + i

// Max 4 args
#define E2K_REG_SP 4

/*
    64 regs only cause of ENTER_GET_REGS has mask 0xff
*/

// + zero reg
static const sljit_u8 reg_map[E2K_NREG_NUMBER + 1] = {
    // scratch
    0, 0, 1, 2, 3, 4, 5, 6, 7,          // ZERO, R0-R7, input args
    8, 9, 10, 11, 12, 13, 14, 15,       // R8-R15, call args, rotated
    16, 17, 18, 19, 20, 21, 22, 23,     // R16-R23
    24, 25, 26, 27, 28, 29, 30, 31,     // R24-R31
    // saved
    32, 33, 34, 35, 36, 37, 38, 39,     // S29-S15
    40, 41, 42, 43, 44, 45, 46, 47,     // S21-S14
    48, 49, 50, 51, 52, 53, 54, 55,     // S13-S6
    56, 57, 58, 59, 60, 61,             // S5-S0
    62, 63                              // TMP_REG1, TMP_REG2
};

// + zero reg
static const sljit_u8 freg_map[E2K_NREG_NUMBER + 1] = {
    // scratch
    0, 0, 1, 2, 3, 4, 5, 6, 7,          // ZERO, R0-R7, input args
    8, 9, 10, 11, 12, 13, 14, 15,       // R8-R15, call args, rotated
    16, 17, 18, 19, 20, 21, 22, 23,     // R16-R23
    24, 25, 26, 27, 28, 29, 30, 31,     // R24-R31
    // saved
    32, 33, 34, 35, 36, 37, 38, 39,     // S29-S15
    40, 41, 42, 43, 44, 45, 46, 47,     // S21-S14
    48, 49, 50, 51, 52, 53, 54, 55,     // S13-S6
    56, 57, 58, 59, 60, 61,             // S5-S0
    62, 63                              // TMP_REG1, TMP_REG2
};

// Luajit
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

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

typedef struct {
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
    E2K_REG_B =     0x200,
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
} E2kOp;

enum {
    /* ------------------------------------------------- */
    E2K_ANDS,     E2K_ANDD,     E2K_ANDNS,    E2K_ANDND,
    E2K_ORS,      E2K_ORD,      E2K_ORNS,     E2K_ORND,
    /* ------------------------------------------------- */
    E2K_XORS,     E2K_XORD,     E2K_XORNS,    E2K_XORND,
    E2K_SXT,      E2K_MERGES,   E2K_MERGED,
    /* ------------------------------------------------- */
    E2K_ADDS,     E2K_ADDD,     E2K_SUBS,     E2K_SUBD,
    E2K_SCLS,     E2K_SCLD,     E2K_SCRS,     E2K_SCRD,
    /* ------------------------------------------------- */
    E2K_SHLS,     E2K_SHLD,     E2K_SHRS,     E2K_SHRD,
    E2K_SARS,     E2K_SARD,     E2K_GETFS,    E2K_GETFD,
    /* ------------------------------------------------- */
    E2K_CMPBSB,   E2K_CMPESB,   E2K_CMPBESB,  E2K_CMPLSB,
    E2K_CMPLESB,  E2K_CMPBDB,   E2K_CMPEDB,   E2K_CMPBEDB,
    E2K_CMPLDB,   E2K_CMPLEDB,  E2K_CMPANDESB,
    E2K_STB,      E2K_STH,      E2K_STW,      E2K_STD,
    /* ------------------------------------------------- */
    E2K_FCMPEQSB, E2K_FCMPLTSB, E2K_FCMPLESB, E2K_FCMPNLTSB,
    E2K_FCMPNLESB,E2K_FCMPEQDB, E2K_FCMPLTDB, E2K_FCMPLEDB,
    E2K_FCMPNLTDB,E2K_FCMPNLEDB,
    /* ------------------------------------------------- */
    E2K_FADDS,    E2K_FADDD,    E2K_FSUBS,    E2K_FSUBD,
    E2K_FMINS,    E2K_FMIND,    E2K_FMAXS,    E2K_FMAXD,
    /* ------------------------------------------------- */
    E2K_FMULS,    E2K_FMULD,    E2K_FSTOISTR, E2K_ISTOFS,
    E2K_FDTOIDTR, E2K_IDTOFD,   E2K_FSTOIDTR, E2K_ISTOFD,
    E2K_FSTOFD,   E2K_FDTOISTR, E2K_IDTOFS,   E2K_FDTOFS,
    /* ------------------------------------------------- */
    E2K_MOVTD,    E2K_LDB,      E2K_LDH,      E2K_LDW,
    E2K_LDD,
    /* ------------------------------------------------- */
    E2K_MULS,     E2K_MULD,     E2K_UMULX,    E2K_SMULX,
    E2K_UMULHd,   E2K_SMULHd,
    /* ------------------------------------------------- */
    E2K_UDIVX,    E2K_UMODX,    E2K_SDIVX,    E2K_SMODX,
    E2K_SDIVd,    E2K_UDIVd,
    E2K_FDIVD,    E2K_FSQRTID,
    /* ------------------------------------------------- */
    E2K_FSQRTTD,
    /* ------------------------------------------------- */
    E2K_GETSP,
    /* ------------------------------------------------- */
    E2K_FDTOIFD,
    /* ------------------------------------------------- */
    E2K_PSHUFB
    /* ------------------------------------------------- */
};

static const E2kOp e2kop[] = {
/*  name        resources       opc   opce  opc2 opce2 */
    { "ANDs",     RES_ALS_012345, 0x00, 0,    0,    0    },
    { "ANDd",     RES_ALS_012345, 0x01, 0,    0,    0    },
    { "ANDNs",    RES_ALS_012345, 0x02, 0,    0,    0    },
    { "ANDNd",    RES_ALS_012345, 0x03, 0,    0,    0    },
    { "ORs",      RES_ALS_012345, 0x04, 0,    0,    0    },
    { "ORd",      RES_ALS_012345, 0x05, 0,    0,    0    },
    { "ORNs",     RES_ALS_012345, 0x06, 0,    0,    0    },
    { "ORNd",     RES_ALS_012345, 0x07, 0,    0,    0    },
/* --------------------------------------------------- */
    { "XORs",     RES_ALS_012345, 0x08, 0,    0,    0    },
    { "XORd",     RES_ALS_012345, 0x09, 0,    0,    0    },
    { "XORNs",    RES_ALS_012345, 0x0a, 0,    0,    0    },
    { "XORNd",    RES_ALS_012345, 0x0b, 0,    0,    0    },
    { "SXT",      RES_ALS_012345, 0x0c, 0,    0,    0    },
    { "MERGEs",   RES_ALS_012345, 0x0e, 0,    0,    0    },
    { "MERGEd",   RES_ALS_012345, 0x0f, 0,    0,    0    },
/* --------------------------------------------------- */
    { "ADDs",     RES_ALS_012345, 0x10, 0,    0,    0    },
    { "ADDd",     RES_ALS_012345, 0x11, 0,    0,    0    },
    { "SUBs",     RES_ALS_012345, 0x12, 0,    0,    0    },
    { "SUBd",     RES_ALS_012345, 0x13, 0,    0,    0    },
    { "SCLs",     RES_ALS_012345, 0x14, 0,    0,    0    },
    { "SCLd",     RES_ALS_012345, 0x15, 0,    0,    0    },
    { "SCRs",     RES_ALS_012345, 0x16, 0,    0,    0    },
    { "SCRd",     RES_ALS_012345, 0x17, 0,    0,    0    },
/* --------------------------------------------------- */
    { "SHLs",     RES_ALS_012345, 0x18, 0,    0,    0    },
    { "SHLd",     RES_ALS_012345, 0x19, 0,    0,    0    },
    { "SHRs",     RES_ALS_012345, 0x1a, 0,    0,    0    },
    { "SHRd",     RES_ALS_012345, 0x1b, 0,    0,    0    },
    { "SARs",     RES_ALS_012345, 0x1c, 0,    0,    0    },
    { "SARd",     RES_ALS_012345, 0x1d, 0,    0,    0    },
    { "GETFs",    RES_ALS_012345, 0x1e, 0,    0,    0    },
    { "GETFd",    RES_ALS_012345, 0x1f, 0,    0,    0    },
/* --------------------------------------------------- */
    { "CMPBsb",   RES_ALS_0134,   0x20, 0x1,  0,    0    },
    { "CMPEsb",   RES_ALS_0134,   0x20, 0x2,  0,    0    },
    { "CMPBEsb",  RES_ALS_0134,   0x20, 0x3,  0,    0    },
    { "CMPLsb",   RES_ALS_0134,   0x20, 0x6,  0,    0    },
    { "CMPLEsb",  RES_ALS_0134,   0x20, 0x7,  0,    0    },
    { "CMPBdb",   RES_ALS_0134,   0x21, 0x1,  0,    0    },
    { "CMPEdb",   RES_ALS_0134,   0x21, 0x2,  0,    0    },
    { "CMPBEdb",  RES_ALS_0134,   0x21, 0x3,  0,    0    },
    { "CMPLdb",   RES_ALS_0134,   0x21, 0x6,  0,    0    },
    { "CMPLEdb",  RES_ALS_0134,   0x21, 0x7,  0,    0    },
    { "CMPANDEsb",RES_ALS_0134,   0x22, 0x2,  0,    0    },
    { "STb",      RES_ALS_25,     0x24, 0,    0,    0    },
    { "STh",      RES_ALS_25,     0x25, 0,    0,    0    },
    { "STw",      RES_ALS_25,     0x26, 0,    0,    0    },
    { "STd",      RES_ALS_25,     0x27, 0,    0,    0    },
/* --------------------------------------------------- */
    { "FCMPEQsb", RES_ALS_0134,   0x2e, 0x0,  0,    0    },
    { "FCMPLTsb", RES_ALS_0134,   0x2e, 0x1,  0,    0    },
    { "FCMPLEsb", RES_ALS_0134,   0x2e, 0x2,  0,    0    },
    { "FCMPNLTsb",RES_ALS_0134,   0x2e, 0x5,  0,    0    },
    { "FCMPNLEsb",RES_ALS_0134,   0x2e, 0x6,  0,    0    },
    { "FCMPEQdb", RES_ALS_0134,   0x2f, 0x0,  0,    0    },
    { "FCMPLTdb", RES_ALS_0134,   0x2f, 0x1,  0,    0    },
    { "FCMPLEdb", RES_ALS_0134,   0x2f, 0x2,  0,    0    },
    { "FCMPNLTdb",RES_ALS_0134,   0x2f, 0x5,  0,    0    },
    { "FCMPNLEdb",RES_ALS_0134,   0x2f, 0x6,  0,    0    },
/* --------------------------------------------------- */
    { "FADDs",    RES_ALS_0134,   0x30, 0,    0,    0    },
    { "FADDd",    RES_ALS_0134,   0x31, 0,    0,    0    },
    { "FSUBs",    RES_ALS_0134,   0x32, 0,    0,    0    },
    { "FSUBd",    RES_ALS_0134,   0x33, 0,    0,    0    },
    { "FMINs",    RES_ALS_0134,   0x34, 0,    0,    0    },
    { "FMINd",    RES_ALS_0134,   0x35, 0,    0,    0    },
    { "FMAXs",    RES_ALS_0134,   0x36, 0,    0,    0    },
    { "FMAXd",    RES_ALS_0134,   0x37, 0,    0,    0    },
/* --------------------------------------------------- */
    { "FMULs",    RES_ALS_0134,   0x38, 0,    0,    0    },
    { "FMULd",    RES_ALS_0134,   0x39, 0,    0,    0    },
    { "FSTOIStr", RES_ALS_0134,   0x3c, 0xc2, 0,    0    },
    { "ISTOFS",   RES_ALS_0134,   0x3c, 0xc4, 0,    0    },
    { "FDTOIDtr", RES_ALS_0134,   0x3d, 0xc2, 0,    0    },
    { "IDTOFD",   RES_ALS_0134,   0x3d, 0xc4, 0,    0    },
    { "FSTOIDtr", RES_ALS_0134,   0x3e, 0xc2, 0,    0    },
    { "ISTOFD",   RES_ALS_0134,   0x3e, 0xc4, 0,    0    },
    { "FSTOFD",   RES_ALS_0134,   0x3e, 0xc6, 0,    0    },
    { "FDTOIStr", RES_ALS_0134,   0x3f, 0xc2, 0,    0    },
    { "IDTOFS",   RES_ALS_0134,   0x3f, 0xc4, 0,    0    },
    { "FDTOFS",   RES_ALS_0134,   0x3f, 0xc6, 0,    0    },
/* --------------------------------------------------- */
    { "MOVTd",    RES_ALS0,       0x61, 0xc0, 0,    0    },
    { "LDb",      RES_ALS_0235,   0x64, 0,    0,    0    },
    { "LDh",      RES_ALS_0235,   0x65, 0,    0,    0    },
    { "LDw",      RES_ALS_0235,   0x66, 0,    0,    0    },
    { "LDd",      RES_ALS_0235,   0x67, 0,    0,    0    },
/* --------------------------------------------------- */
    { "MULs",     RES_ALS_0134,   0x20, 0,    0x01, 0xc0 },
    { "MULd",     RES_ALS_0134,   0x21, 0,    0x01, 0xc0 },
    { "UMULX",    RES_ALS_0134,   0x22, 0,    0x01, 0xc0 },
    { "SMULX",    RES_ALS_0134,   0x23, 0,    0x01, 0xc0 },
    { "UMULHd",   RES_ALS_0134,   0x70, 0,    0x01, 0xc0 },
    { "SMULHd",   RES_ALS_0134,   0x71, 0,    0x01, 0xc0 },
/* --------------------------------------------------- */
    { "UDIVX",    RES_ALS_25,     0x44, 0,    0x01, 0xc0 },
    { "UMODX",    RES_ALS_25,     0x45, 0,    0x01, 0xc0 },
    { "SDIVX",    RES_ALS_25,     0x46, 0,    0x01, 0xc0 },
    { "SMODX",    RES_ALS_25,     0x47, 0,    0x01, 0xc0 },
    { "SDIVd",    RES_ALS_25,     0x43, 0,    0x01, 0xc0 },
    { "UDIVd",    RES_ALS_25,     0x41, 0,    0x01, 0xc0 },
    { "FDIVd",    RES_ALS5,       0x49, 0,    0x01, 0xc0 },
    { "FSQRTId",  RES_ALS5,       0x4d, 0xc0, 0x01, 0xc0 },
/* --------------------------------------------------- */
    { "FSQRTTd",  RES_ALS5,       0x51, 0,    0x01, 0xc0 },
/* --------------------------------------------------- */
    { "GETSP",    RES_ALS0,       0x58, 0xec, 0x01, 0xc0 },
/* --------------------------------------------------- */
    { "FDTOIFd",  RES_ALS_0134,   0x6d, 0,    0x01, 0xc0 },
/* --------------------------------------------------- */
    { "PSHUFB",   RES_ALS_0134,   0x4d, 0,    0x0f, 0    },
/* --------------------------------------------------- */
};

/* -- Bundling helpers ---------------------------------------------------- */

static SLJIT_INLINE sljit_uw check_resource(struct sljit_compiler *compiler,
                                            E2kBundle *bundle,
                                            sljit_uw mask)
{
    SLJIT_UNUSED_ARG(compiler);
    sljit_uw res = bundle->res & mask;
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
    sljit_uw syl = check_resource(compiler, bundle, mask);
    syl = syl >> shift;
    int idx = 0;
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

    if (val < E2K_CTPR_ID_START)
        return E2K_REG_R;
    else if (val < E2K_PRED_ID_START)
        return E2K_REG_PRED;
    else if (val < E2K_REG_MAX)
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

    // from small to big
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

/* -- Bundling ------------------------------------------------------------ */

static SLJIT_INLINE void emit_bundle_setup(struct sljit_compiler *compiler,
                                           E2kBundle *bundle)
{
    SLJIT_UNUSED_ARG(compiler);
    SLJIT_ZEROMEM(bundle, sizeof(E2kBundle));
    bundle->res = RES_INIT;
    bundle->f1 = 1; /* HS itself */
}

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
    int half_pad = bundle->f3 & 0x1;
    sljit_u8 f3 = (bundle->f3 >> 1) + half_pad;
    sljit_u8 f4 = bundle->f4 + (hs_cds >> 1);
    sljit_u8 lng = f1 + f2 + f3 + f4;
    sljit_u8 hs_lng = (lng + 1) & -2;

    E2kHS hs;
    hs.i = 0;
    hs.fields.mdl = (sljit_u32)(f1 + f2 - 1);
    hs.fields.lng = (sljit_u32)(hs_lng >> 1) - 1;
    hs.fields.nop = (sljit_u32)bundle->nop;
    hs.fields.x_s_sw = hs_x_s_sw;
    hs.fields.c = hs_c;
    hs.fields.cds = hs_cds >> 1;
    hs.fields.ales = hs_ales;
    hs.fields.als = hs_als;

    sljit_u32 *inst = (sljit_u32*)ensure_buf(compiler, sizeof(sljit_u32)*((hs_lng+1)/2));
    sljit_u16 *inst16;

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
    if (hs_c & 0x01) {
        E2kCopf2 cs0 = { bundle->cs[0] };
        cs0.fields.disp = cs0.fields.disp + (hs_lng >> 1);
        *inst++ = cs0.i;
    }
    /* --- f1 ---------------------------------------------------------------*/

    /* --- f2 ---------------------------------------------------------------*/
    /* ALES[2-5], 16-bit */
    inst16 = (sljit_u16 *)inst;
    if (hs_ales & 0x04) *inst16++ = bundle->ales[2];
    if (hs_ales & 0x20) *inst16++ = bundle->ales[5];
    if (half_pad) *inst16++ = 0; /* Align */
    inst = (sljit_u32*)inst16;
    /* CS1, 32-bit */
    if (hs_c & 0x02) *inst++ = bundle->cs[1];
    /* --- f2 ---------------------------------------------------------------*/

    /* --- f3 ---------------------------------------------------------------*/
    /* ALES[0-1,3-4], 16-bit */
    inst16 = (sljit_u16 *)inst;
    if (hs_ales & 0x01) *inst16++ = bundle->ales[0];
    if (hs_ales & 0x02) *inst16++ = bundle->ales[1];
    if (hs_ales & 0x08) *inst16++ = bundle->ales[3];
    if (hs_ales & 0x10) *inst16++ = bundle->ales[4];
    if (half_pad) *inst16++ = 0;    /* Align */
    inst = (sljit_u32*)inst16;

    if (hs_lng != lng) *inst++ = 0; /* Align */
    /* --- f3 ---------------------------------------------------------------*/

    /* --- f4 ---------------------------------------------------------------*/
    /* LTS[3-0], 32-bit, 16-bit half syls not implemented */
    int used_lts = ((~bundle->res & RES_LTS_ALL) >> RES_LTS_SHIFT);
    for (int i = 4; i > 0; i--) {
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
                           sljit_uw src1)
{
    if (type == E2K_REG) {
        switch (get_reg_type(compiler, bundle, src1)) {
        case E2K_REG_R:
            return reg_map[src1] | 0x80;
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

static sljit_u32 emit_src2(struct sljit_compiler *compiler,
                          E2kBundle *bundle,
                          E2kOpT type,
                          sljit_sw src2)
{
    if (type == E2K_REG) {
        switch (get_reg_type(compiler, bundle, src2)) {
        case E2K_REG_R:
            return reg_map[src2] | 0x80;
        default:
            SLJIT_ASSERT(0);
            return 0;
        }
    } else {
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
        case E2K_REG_CTPR:
            return (dst - E2K_CTPR_ID_START) | 0xd0;
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
        return (pred - E2K_PRED_ID_START);
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
    emit_bundle_finalize(compiler, bundle);
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
    emit_bundle_finalize(compiler, bundle);
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
    emit_bundle_finalize(compiler, bundle);
    return als_idx;
}

static void emit_alef1(struct sljit_compiler *compiler, E2kBundle *bundle,
                       sljit_u32 op, sljit_u32 src3, int als_idx)
{
    /* No combined operations on 2 and 5 channels */
    int res = 1 << (RES_ALES_SHIFT + als_idx);
    check_resource(compiler, bundle, res);
    if (!(res & RES_ALES_25)) {
        E2kAlef1 syl;
        syl.i = 0;
        syl.fields.src3 = (sljit_u16)src3;
        syl.fields.opc2 = e2kop[op].opc2;
        bundle->ales[als_idx] = syl.i;
        bundle->f3++;
    }
}

static void emit_alef2(struct sljit_compiler *compiler, E2kBundle *bundle,
                       sljit_u32 op, int als_idx)
{
    /* No combined operations on 2 and 5 channels */
    int res = 1 << (RES_ALES_SHIFT + als_idx);
    check_resource(compiler, bundle, res);
    if (!(res & RES_ALES_25)) {
        E2kAlef2 syl;
        syl.i = 0;
        syl.fields.opce2 = e2kop[op].opce2;
        syl.fields.opc2 = e2kop[op].opc2;
        bundle->ales[als_idx] = syl.i;
        bundle->f3++;
    }
}

static int emit_alopf12(struct sljit_compiler *compiler, E2kBundle *bundle,
                        sljit_u32 spec, sljit_u32 op,
                        sljit_u32 src2, sljit_u32 dst)
{
    int als_idx = emit_alopf2(compiler, bundle, spec, op, src2, dst);
    emit_alef2(compiler, bundle, op, als_idx);
    emit_bundle_finalize(compiler, bundle);
    return als_idx;
}

static int emit_alopf21(struct sljit_compiler *compiler, E2kBundle *bundle,
                        sljit_u32 spec, sljit_u32 op,
                        sljit_u32 src1, sljit_u32 src2, sljit_u32 src3, sljit_u32 dst)
{
    int als_idx = emit_alopf1(compiler, bundle, spec, op, src1, src2, dst);
    emit_alef1(compiler, bundle, op, src3, als_idx);
    emit_bundle_finalize(compiler, bundle);
    return als_idx;
}

static int emit_alopf11(struct sljit_compiler* compiler, E2kBundle *bundle,
                        sljit_u32 spec, sljit_u32 op,
                        sljit_u32 src1, sljit_u32 src2, sljit_u32 dst)
{
    int als_idx = emit_alopf1(compiler, bundle, spec, op, src1, src2, dst);
    emit_alef2(compiler, bundle, op, als_idx);
    emit_bundle_finalize(compiler, bundle);
    return als_idx;
}

static int emit_alopf22(struct sljit_compiler *compiler, E2kBundle *bundle,
                        sljit_u32 spec, sljit_u32 op,
                        sljit_u32 src2, sljit_u32 dst)
{
    int als_idx = emit_alopf2(compiler, bundle, spec, op, src2, dst);
    emit_alef2(compiler, bundle, op, als_idx);
    emit_bundle_finalize(compiler, bundle);
    return als_idx;
}

static void emit_cds(struct sljit_compiler *compiler, E2kBundle *bundle,
                     sljit_u32 opc, sljit_u32 als, sljit_u32 pred, sljit_u32 inverted)
{
    int cds_idx = get_sylidx(compiler, bundle, RES_CDS_ALL, RES_CDS_SHIFT);
    E2kCDS syl;
    syl.i = 0;
    syl.fields.pred = (pred - E2K_PRED_ID_START) | 0x60;
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
    emit_bundle_finalize(compiler, bundle);
}

static void emit_copf1(struct sljit_compiler *compiler, E2kBundle *bundle,
                       sljit_u32 opc, sljit_u32 ctpr, sljit_uw param)
{
    check_resource(compiler, bundle, RES_CS0);
    E2kCopf1 syl;
    syl.i = 0;
    syl.fields.param = param;
    syl.fields.opc = opc;
    if (ctpr) /* RID_CTPRX is nonnull */
      syl.fields.ctpr = ctpr - E2K_CTPR_ID_START;

    bundle->cs[0] = syl.i;
    bundle->f1++;
    emit_bundle_finalize(compiler, bundle);
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
    emit_bundle_finalize(compiler, bundle);
}

#define emit_rlp(compiler, bundle, als, pred, inv) \
  emit_cds(compiler, bundle, 0, als, pred, inv, p)
#define emit_mrgc(compiler, bundle, als, pred, inv) \
  emit_cds(compiler, bundle, 2, als, pred, inv)

#define ISRC1(src1) emit_src1(compiler, bundle, E2K_CONST, src1)
#define RSRC1(src1) emit_src1(compiler, bundle, E2K_REG, src1)
#define ISRC2(src2) emit_src2(compiler, bundle, E2K_CONST, src2)
#define RSRC2(src2) emit_src2(compiler, bundle, E2K_REG, src2)
#define RSRC3(src3) emit_src3(compiler, bundle, E2K_REG, src3)
#define RDST(dst)   emit_dst(compiler, bundle, E2K_REG, dst)
#define PDST(pred)  emit_pdst(compiler, bundle, E2K_REG_PRED, pred)

// ALOPF1
#define emit_alopf1_rr(compiler, bundle, spec, op, src1, src2, dst) \
  emit_alopf1(compiler, bundle, spec, op, RSRC1(src1), RSRC2(src2), RDST(dst))
#define emit_alopf1_ir(compiler, bundle, spec, op, src1, src2, dst) \
  emit_alopf1(compiler, bundle, spec, op, ISRC1(src1), RSRC2(src2), RDST(dst))
#define emit_alopf1_ri(compiler, bundle, spec, op, src1, src2, dst) \
  emit_alopf1(compiler, bundle, spec, op, RSRC1(src1), ISRC2(src2), RDST(dst))
#define emit_alopf1_ii(compiler, bundle, spec, op, src1, src2, dst) \
  emit_alopf1(compiler, bundle, spec, op, ISRC1(src1), ISRC2(src2), RDST(dst))

// ALOPF2
#define emit_alopf2_i(compiler, bundle, spec, op, src2, dst) \
  emit_alopf2(compiler, bundle, spec, op, ISRC2(src2), RDST(dst))
#define emit_alopf2_r(compiler, bundle, spec, op, src2, dst) \
  emit_alopf2(compiler, bundle, spec, op, RSRC2(src2), RDST(dst))

// ALOPF3
#define emit_alopf3_ri(compiler, bundle, spec, op, src1, src2, src3) \
  emit_alopf3(compiler, bundle, spec, op, RSRC1(src1), ISRC2(src2), RSRC3(src3))

// ALOPF7
#define emit_alopf7_rr(compiler, bundle, spec, op, src1, src2, pred) \
  emit_alopf7(compiler, bundle, spec, op, RSRC1(src1), RSRC2(src2), PDST(pred))
#define emit_alopf7_ri(compiler, bundle, spec, op, src1, src2, pred) \
  emit_alopf7(compiler, bundle, spec, op, RSRC1(src1), ISRC2(src2), PDST(pred))

// ALOPF12
#define emit_alopf12_i(compiler, bundle, spec, op, src2, dst) \
  emit_alopf12(compiler, bundle, spec, op, ISRC2(src2), RDST(dst))
#define emit_alopf12_r(compiler, bundle, spec, op, src2, dst) \
  emit_alopf12(compiler, bundle, spec, op, RSRC2(src2), RDST(dst))

// ALOPF11
#define emit_alopf11_rr(compiler, bundle, spec, op, src1, src2, dst) \
  emit_alopf11(compiler, bundle, spec, op, RSRC1(src1), RSRC2(src2), RDST(dst))
#define emit_alopf11_ir(compiler, bundle, spec, op, src1, src2, dst) \
  emit_alopf11(compiler, bundle, spec, op, ISRC1(src1), RSRC2(src2), RDST(dst))
#define emit_alopf11_ri(compiler, bundle, spec, op, src1, src2, dst) \
  emit_alopf11(compiler, bundle, spec, op, RSRC1(src1), ISRC2(src2), RDST(dst))
#define emit_alopf11_ii(compiler, bundle, spec, op, src1, src2, dst) \
  emit_alopf11(compiler, bundle, spec, op, ISRC1(src1), ISRC2(src2), RDST(dst))

// ALOPF21
#define emit_alopf21_rrr(compiler, bundle, spec, op, src1, src2, src3, dst) \
  emit_alopf21(compiler, bundle, spec, op, RSRC1(src1), RSRC2(src2), RSRC3(src3), RDST(dst))

/* ???
#define emit_snapno(compiler, bundle, snapno) \
  emit_alopf1(compiler, bundle, 0, E2K_ADDD, emit_src1(compiler, bundle, E2K_CONST, 0), \
              emit_lts(compiler, bundle, E2K_CONST32, (sljit_uw)snapno) | 0xd8, \
              emit_dst(compiler, bundle, E2K_REG, RID_TMP))
*/

#define emit_nop(compiler, bundle, nops) \
  bundle->nop = nops

/* -- Emit loads/stores --------------------------------------------------- */

/* ???
#define dispofs(as, k) \
  ((intptr_t)((uintptr_t)(k) - (uintptr_t)J2GG(as->J)->dispatch))
*/

/* Prefer rematerialization of BASE/L from global_State over spills. */
/* ???
#define emit_canremat(ref)  ((ref) <= REF_BASE)
*/

/* Load a 64 bit constant into a GPR. */
/* ???
static void emit_loadu64(ASMState *as, sljit_u32 r, sljit_uw u64)
{
  emit_alopf1_ii(as, 0, E2K_ADDD, 0, u64, r, &as->mcp);
}

static void emit_loadk64(ASMState *as, sljit_u32 r, IRIns *ir)
{
  const sljit_uw *k = &ir_k64(ir)->u64;
  emit_loadu64(as, r, *k);
}
*/

/*
static void emit_ldd(struct sljit_compiler *compiler, E2kBundle *bundle,
                     sljit_u32 dest, void *addr)
{
  emit_alopf1_ri(compiler, bundle, 0, E2K_LDD, addr,
                 0, dest, &as->mcp);
}

static void emit_std(ASMState *as, sljit_u32 src, void *addr)
{
  emit_alopf3_ri(as, 0, E2K_STD, RID_DISPATCH, (intptr_t)dispofs(as, addr),
                 src, &as->mcp);
}
*/

/* Load a constant address into a GPR. */
//#define emit_loada(as, r, addr)   emit_loadu64(as, (r), u64ptr((addr)))
/* Load a 32 bit constant into a GPR. */
//#define emit_loadi(as, r, i)      emit_loadu64(as, (r), (intptr_t)i)

/* Get/set global_State fields. */
//#define emit_getgl(as, r, field) emit_ldd(as, r, (void *)&J2G(as->J)->field)
//#define emit_setgl(as, r, field) emit_std(as, r, (void *)&J2G(as->J)->field)

/* Trace number is determined from per-trace exit stubs. */
//#define emit_setvmstate(as, i) UNUSED(i)

/* -- Emit control-flow instructions -------------------------------------- */
static void emit_ct(struct sljit_compiler *compiler, E2kBundle *bundle,
                    sljit_u32 ctpr, sljit_u32 pred, int inverted)
{
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

static void emit_call(struct sljit_compiler *compiler, E2kBundle *bundle,
                      sljit_u32 ctpr, sljit_u32 pred,
                      int inverted, int wbs)
{
    emit_ct(compiler, bundle, ctpr, pred, inverted);
    check_resource(compiler, bundle, RES_CS1);
    E2kC1f1 syl;
    syl.i = 0;
    syl.fields.opc = E2K_CALL;
    syl.fields.params = wbs;
    bundle->cs[1] = syl.i;
    bundle->f2++;
    emit_bundle_finalize(compiler, bundle);
}

/* TODO
static void emit_prepcall(struct sljit_compiler *compiler, E2kBundle *bundle,
                          sljit_u32 ctpr, ASMFunction target)
{
    // check 28 bit disp
    if (((((uintptr_t)target ^ (uintptr_t)as->mcp) >> 3) >> 28) == 0) {
        ptrdiff_t disp = (ptrdiff_t)((void *) target - (void *)as->mcp);
        emit_copf2(as, E2K_DISP, ctpr, disp, p);
    } else { // Target out of range; need indirect call
        emit_alopf2_i(as, 0, E2K_MOVTD, (intptr_t)target, ctpr, p);
    }
}
*/


static void emit_return(struct sljit_compiler *compiler, E2kBundle *bundle,
                        sljit_u32 pred, int inverted)
{
    emit_copf1(compiler, bundle, E2K_RETURN, E2K_CTPR(3), 0);
    emit_nop(compiler, bundle, 4);
    emit_ct(compiler, bundle, E2K_CTPR(3), pred, inverted);
}

static void emit_ibranch(struct sljit_compiler *compiler, E2kBundle *bundle,
                         sljit_uw disp, sljit_u32 pred,
                         int inverted)
{
    emit_copf2(compiler, bundle, E2K_IBRANCH, E2K_CTPR(1), disp);
    emit_ct(compiler, bundle, pred, inverted, 0);
}

static void emit_jmp(struct sljit_compiler *compiler, E2kBundle *bundle,
                     sljit_u32 *target)
{
    emit_ibranch(compiler, bundle,
                 (sljit_sw)((void *)target - (void *)compiler->buf),
                 0, 0);
}

static void emit_umulhd_muld(struct sljit_compiler* compiler, E2kBundle *bundle)
{
    // mul SLJIT_R0 and SLJIT_R1
    // Result into R1:R0

    // 0. Move R1 into TMP
    emit_alopf11_ri(compiler, bundle, 0, E2K_ADDD, SLJIT_R1, 0, TMP_REG1);
    // 1. smulhd for high
    emit_alopf11_rr(compiler, bundle, 0, E2K_UMULHd, SLJIT_R0, SLJIT_R1, SLJIT_R1);
    // 2. muld for low
    emit_alopf11_rr(compiler, bundle, 0, E2K_MULD, SLJIT_R0, TMP_REG1, SLJIT_R0);
}

static void emit_smulhd_muld(struct sljit_compiler* compiler, E2kBundle *bundle)
{
    // mul SLJIT_R0 and SLJIT_R1
    // Result into R1:R0

    // 0. Move R1 into TMP
    emit_alopf11_ri(compiler, bundle, 0, E2K_ADDD, SLJIT_R1, 0, TMP_REG1);
    // 1. smulhd for high
    emit_alopf11_rr(compiler, bundle, 0, E2K_SMULHd, SLJIT_R0, SLJIT_R1, SLJIT_R1);
    // 2. muld for low
    emit_alopf11_rr(compiler, bundle, 0, E2K_MULD, SLJIT_R0, TMP_REG1, SLJIT_R0);
}

static void emit_udivd_umodx(struct sljit_compiler* compiler, E2kBundle *bundle)
{
    // Div R0 by the R1
    // Result -> R0
    // Reminder -> R1
    // 0. Move R1 into TMP
    emit_alopf11_ri(compiler, bundle, 0, E2K_ADDD, SLJIT_R0, 0, TMP_REG1);
    // Result -> R0
    emit_alopf1_rr(compiler, bundle, 0, E2K_UDIVd, SLJIT_R0, SLJIT_R1, SLJIT_R0);
    // Reminder -> R1
    emit_alopf1_rr(compiler, bundle, 0, E2K_UMODX, TMP_REG1, SLJIT_R1, SLJIT_R1);
}

static void emit_sdivd_smodx(struct sljit_compiler* compiler, E2kBundle *bundle)
{
    // Div R0 by the R1
    // Result -> R0
    // Reminder -> R1
    // 0. Move R1 into TMP
    emit_alopf11_ri(compiler, bundle, 0, E2K_ADDD, SLJIT_R0, 0, TMP_REG1);
    // Result -> R0
    emit_alopf1_rr(compiler, bundle, 0, E2K_SDIVd, SLJIT_R0, SLJIT_R1, SLJIT_R0);
    // Reminder -> R1
    emit_alopf1_rr(compiler, bundle, 0, E2K_SMODX, TMP_REG1, SLJIT_R1, SLJIT_R1);
}

static void emit_udivs(struct sljit_compiler* compiler, E2kBundle *bundle)
{
    // Div R0 by R1
    // Result -> R0
    emit_alopf1_rr(compiler, bundle, 0, E2K_UDIVd, SLJIT_R0, SLJIT_R1, SLJIT_R0);
}

static void emit_sdivs(struct sljit_compiler* compiler, E2kBundle *bundle)
{
    // Div R0 by R1
    // Result -> R0
    emit_alopf1_rr(compiler, bundle, 0, E2K_SDIVd, SLJIT_R0, SLJIT_R1, SLJIT_R0);
}

/* -- Emit generic operations --------------------------------------------- */

/* argument extension */
static void emit_ext(struct sljit_compiler *compiler, E2kBundle *bundle,
                     sljit_u32 dst, sljit_u32 src, int ext) {
    emit_alopf1_ir(compiler, bundle, 0, E2K_SXT, ext, src, dst);
}

/* Generic move between two regs. */
static void emit_movrr(struct sljit_compiler *compiler, E2kBundle *bundle,
                       sljit_u32 dst, sljit_u32 src)
{
    emit_alopf1_ri(compiler, bundle, 0, E2K_ADDD, src, 0, dst);
}

// TODO
/* Generic load of register with base and (small) offset address. */
/*
static void emit_loadofs(ASMState *as, IRIns *ir, sljit_u32 r, sljit_u32 base, int32_t ofs)
{
    emit_alopf1_ri(as, 0, irt_is64(ir->t) ? E2K_LDD : E2K_LDW,
                   base, ofs, r, &as->mcp);
}
*/

// TODO
/* Generic store of register with base and (small) offset address. */
/*
static void emit_storeofs(ASMState *as, IRIns *ir, sljit_u32 r, sljit_u32 base, int32_t ofs)
{
    emit_alopf3_ri(as, 0, irt_is64(ir->t) ? E2K_STD : E2K_STW,
                   base, ofs, r, &as->mcp);
}
*/

/* Add offset to pointer. */
static void emit_addptr(struct sljit_compiler *compiler, E2kBundle *bundle,
                        sljit_u32 r, sljit_u32 ofs)
{
    if (ofs) {
        emit_alopf1_ri(compiler, bundle, 0, E2K_ADDD, r, ofs, r);
    }
}

/* Get additional stack space. */
static void emit_spsub(struct sljit_compiler *compiler, E2kBundle *bundle,
                       sljit_s32 ofs)
{
    if (ofs) {
        emit_alopf12_i(compiler, bundle, 0, E2K_GETSP, -ofs, E2K_REG_SP);
    }
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
// Luajit

SLJIT_API_FUNC_ATTRIBUTE void* sljit_generate_code(struct sljit_compiler *compiler, sljit_s32 options, void *exec_allocator_data)
{
    return NULL;
}

// TODO:
SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_has_cpu_feature(sljit_s32 feature_type)
{
	switch (feature_type) {
	case SLJIT_HAS_FPU:
    // case SLJIT_HAS_VIRTUAL_REGISTERS:
    // case SLJIT_HAS_ZERO_REGISTER:
	// case SLJIT_HAS_CLZ:
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

    sljit_u32 stack_size = (saveds * 8) + local_size;
    SLJIT_ASSERT(stack_size < 0x7FFFFFFF); // 2**32 - 1
    compiler->local_size = stack_size;
    // ------------------------------------------------------------------------

    E2kBundle _bundle;
    E2kBundle *bundle = &_bundle;
    emit_bundle_setup(compiler, bundle);

    /* -- setwd, setbn ---------------*/
    check_resource(compiler, bundle, RES_CS1);
    E2kSETR setr;
    setr.fields.rbs    = E2K_ROTATED_REG_BASE;
    setr.fields.rsz    = E2K_ROTATED_REG_SIZE;
    setr.fields.rcur   = 0x0;
    setr.fields.psz    = 0x0;
    setr.fields.x      = 0x0;
    setr.fields.setbn  = 0x1;
    setr.fields.setbp  = 0x0;
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
    /* -- setwd, setbn ---------------*/

    sljit_u32 src2 = -stack_size;
    emit_alopf2_i(compiler, bundle, 0, E2K_GETSP, src2, reg_map[E2K_REG_SP]);

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

    E2kBundle _bundle;
    E2kBundle *bundle = &_bundle;
    emit_bundle_setup(compiler, bundle);
    emit_return(compiler, bundle, E2K_CTPR(1), 0);

    return SLJIT_SUCCESS;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_return_to(struct sljit_compiler *compiler,
                               sljit_s32 src, sljit_sw srcw)
{
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_op0(struct sljit_compiler *compiler, sljit_s32 op)
{
	CHECK_ERROR();
	CHECK(check_sljit_emit_op0(compiler, op));

    E2kBundle _bundle;
    E2kBundle *bundle = &_bundle;
    emit_bundle_setup(compiler, bundle);

	op = GET_OPCODE(op);
	switch (op) {
	case SLJIT_BREAKPOINT:
    case SLJIT_NOP:
		emit_nop(compiler, bundle, 1);
		break;
	case SLJIT_LMUL_UW:
        emit_umulhd_muld(compiler, bundle);
        break;
	case SLJIT_LMUL_SW:
        emit_smulhd_muld(compiler, bundle);
        break;
	case SLJIT_DIVMOD_UW:
        emit_udivd_umodx(compiler, bundle);
	case SLJIT_DIVMOD_SW:
        emit_sdivd_smodx(compiler, bundle);
	case SLJIT_DIV_UW:
        emit_udivs(compiler, bundle);
	case SLJIT_DIV_SW:
        emit_sdivs(compiler, bundle);
	case SLJIT_MEMORY_BARRIER:
		return SLJIT_ERR_UNSUPPORTED;
	case SLJIT_ENDBR:
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
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_op2(struct sljit_compiler *compiler, sljit_s32 op,
	                     sljit_s32 dst, sljit_sw dstw,
	                     sljit_s32 src1, sljit_sw src1w,
	                     sljit_s32 src2, sljit_sw src2w)
{
    return SLJIT_ERR_UNSUPPORTED;
}


SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_op2u(struct sljit_compiler *compiler, sljit_s32 op,
	                      sljit_s32 src1, sljit_sw src1w,
	                      sljit_s32 src2, sljit_sw src2w)
{
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE sljit_s32 sljit_emit_op2r(struct sljit_compiler *compiler, sljit_s32 op,
	sljit_s32 dst_reg,
	sljit_s32 src1, sljit_sw src1w,
	sljit_s32 src2, sljit_sw src2w)
{
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_shift_into(struct sljit_compiler *compiler, sljit_s32 op,
	                            sljit_s32 dst_reg,
	                            sljit_s32 src1_reg,
	                            sljit_s32 src2_reg,
	                            sljit_s32 src3, sljit_sw src3w)
{
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_op2_shift(struct sljit_compiler *compiler, sljit_s32 op,
	                           sljit_s32 dst, sljit_sw dstw,
	                           sljit_s32 src1, sljit_sw src1w,
	                           sljit_s32 src2, sljit_sw src2w,
	                           sljit_sw shift_arg)
{
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_op_src(struct sljit_compiler *compiler, sljit_s32 op,
                        	sljit_s32 src, sljit_sw srcw)
{
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_op_dst(struct sljit_compiler *compiler, sljit_s32 op,
	                        sljit_s32 dst, sljit_sw dstw)
{
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_get_register_index(sljit_s32 type, sljit_s32 reg)
{
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_op_custom(struct sljit_compiler *compiler,
	                           void *instruction, sljit_u32 size)
{
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_fop1(struct sljit_compiler *compiler, sljit_s32 op,
	                      sljit_s32 dst, sljit_sw dstw,
	                      sljit_s32 src, sljit_sw srcw)
{
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_fop2(struct sljit_compiler *compiler, sljit_s32 op,
	                      sljit_s32 dst, sljit_sw dstw,
	                      sljit_s32 src1, sljit_sw src1w,
	                      sljit_s32 src2, sljit_sw src2w)
{
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_fset32(struct sljit_compiler *compiler,
	                        sljit_s32 freg, sljit_f32 value)
{
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_fset64(struct sljit_compiler *compiler,
	                        sljit_s32 freg, sljit_f64 value)
{
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_fcopy(struct sljit_compiler *compiler, sljit_s32 op,
	                       sljit_s32 freg, sljit_s32 reg)
{
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE
struct sljit_label* sljit_emit_label(struct sljit_compiler *compiler)
{
    return NULL;
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
    return NULL;
}

SLJIT_API_FUNC_ATTRIBUTE
struct sljit_jump* sljit_emit_call(struct sljit_compiler *compiler, sljit_s32 type,
	                               sljit_s32 arg_types)
{
    return NULL;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_ijump(struct sljit_compiler *compiler, sljit_s32 type,
                           sljit_s32 src, sljit_sw srcw)
{
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_icall(struct sljit_compiler *compiler, sljit_s32 type,
	                       sljit_s32 arg_types,
	                       sljit_s32 src, sljit_sw srcw)
{
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_op_flags(struct sljit_compiler *compiler, sljit_s32 op,
	                          sljit_s32 dst, sljit_sw dstw,
	                          sljit_s32 type)
{
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_select(struct sljit_compiler *compiler, sljit_s32 type,
	                        sljit_s32 dst_reg,
	                        sljit_s32 src1, sljit_sw src1w,
	                        sljit_s32 src2_reg)
{
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_fselect(struct sljit_compiler *compiler, sljit_s32 type,
	                         sljit_s32 dst_freg,
	                         sljit_s32 src1, sljit_sw src1w,
	                         sljit_s32 src2_freg)
{
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_mem(struct sljit_compiler *compiler, sljit_s32 type,
	                     sljit_s32 reg,
	                     sljit_s32 mem, sljit_sw memw)
{
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_atomic_load(struct sljit_compiler *compiler, sljit_s32 op,
	                             sljit_s32 dst_reg,
	                             sljit_s32 mem_reg)
{
    return SLJIT_ERR_UNSUPPORTED;
}


SLJIT_API_FUNC_ATTRIBUTE
sljit_s32 sljit_emit_atomic_store(struct sljit_compiler *compiler, sljit_s32 op,
	                              sljit_s32 src_reg,
	                              sljit_s32 mem_reg,
	                              sljit_s32 temp_reg)
{
    return SLJIT_ERR_UNSUPPORTED;
}

SLJIT_API_FUNC_ATTRIBUTE
struct sljit_const* sljit_emit_const(struct sljit_compiler *compiler, sljit_s32 op,
	                                 sljit_s32 dst, sljit_sw dstw,
	                                 sljit_sw init_value)
{
    return NULL;
}

SLJIT_API_FUNC_ATTRIBUTE
struct sljit_jump* sljit_emit_op_addr(struct sljit_compiler *compiler, sljit_s32 op,
	                                  sljit_s32 dst, sljit_sw dstw)
{
    return NULL;
}

SLJIT_API_FUNC_ATTRIBUTE
void sljit_set_jump_addr(sljit_uw addr, sljit_uw new_target, sljit_sw executable_offset)
{
    return;
}

SLJIT_API_FUNC_ATTRIBUTE
void sljit_set_const(sljit_uw addr, sljit_s32 op, sljit_sw new_constant, sljit_sw executable_offset)
{
    return;
}
