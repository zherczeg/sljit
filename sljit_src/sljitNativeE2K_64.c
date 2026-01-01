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

/* TODO:
1. Support >=v5
2. Support aas ops
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
// + sp
static const sljit_u8 reg_map[E2K_NREG_NUMBER + 2] = {
    // scratch
    0, 0, 1, 2, 3, 4, 5, 6, 7,          // ZERO, R0-R7, input args
    8, 9, 10, 11, 12, 13, 14, 15,       // R8-R15, call args, rotated
    16, 17, 18, 19, 20, 21, 22, 23,     // R16-R23
    24, 25, 26, 27, 28, 29, 30, 31,     // R24-R31
    // saved
    32, 33, 34, 35, 36, 37, 38, 39,     // S28-S14
    40, 41, 42, 43, 44, 45, 46, 47,     // S20-S13
    48, 49, 50, 51, 52, 53, 54, 55,     // S12-S5
    56, 57, 58, 59, 60,                 // S4-S0
    61, 62, 63                          // SP, TMP_REG1, TMP_REG2
};

// + zero reg
// + sp
static const sljit_u8 freg_map[E2K_NREG_NUMBER + 2] = {
    // scratch
    0, 0, 1, 2, 3, 4, 5, 6, 7,          // ZERO, R0-R7, input args
    8, 9, 10, 11, 12, 13, 14, 15,       // R8-R15, call args, rotated
    16, 17, 18, 19, 20, 21, 22, 23,     // R16-R23
    24, 25, 26, 27, 28, 29, 30, 31,     // R24-R31
    // saved
    32, 33, 34, 35, 36, 37, 38, 39,     // S28-S14
    40, 41, 42, 43, 44, 45, 46, 47,     // S20-S13
    48, 49, 50, 51, 52, 53, 54, 55,     // S12-S5
    56, 57, 58, 59, 60,                 // S4-S0
    61, 62, 63                          // SP, TMP_REG1, TMP_REG2
};


// label for ctpr
// 0 reg is empty reg, because ctpr starts with 1.
static const sljit_u8 ctpr_map[4] = {
    0, 11, 12, 13
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

enum {
    /* ------------------------------------------------- */
    E2K_ANDs,     E2K_ANDd,     E2K_ANDNs,    E2K_ANDNd,
    E2K_ORs,      E2K_ORd,      E2K_ORNs,     E2K_ORNd,
    /* ------------------------------------------------- */
    E2K_XORs,     E2K_XORd,     E2K_XORNs,    E2K_XORNd,
    E2K_SXT,      E2K_MERGEs,   E2K_MERGEd,
    /* ------------------------------------------------- */
    E2K_ADDs,     E2K_ADDd,     E2K_SUBs,     E2K_SUBd,
    E2K_SCLs,     E2K_SCLd,     E2K_SCRs,     E2K_SCRd,
    /* ------------------------------------------------- */
    E2K_SHLs,     E2K_SHLd,     E2K_SHRs,     E2K_SHRd,
    E2K_SARs,     E2K_SARd,     E2K_GETFs,    E2K_GETFd,
    /* ------------------------------------------------- */
    E2K_CMPOsb,   E2K_CMPBsb,   E2K_CMPEsb,   E2K_CMPBEsb,
    E2K_CMPSsb,   E2K_CMPPsb,   E2K_CMPLsb,   E2K_CMPLEsb,
    E2K_CMPOdb,   E2K_CMPBdb,   E2K_CMPEdb,   E2K_CMPBEdb,
    E2K_CMPSdb,   E2K_CMPPdb,   E2K_CMPLdb,   E2K_CMPLEdb,
    E2K_CMPANDEsb,E2K_CMPANDSsb,E2K_CMPANDPsb,E2K_CMPANDLEsb,
    E2K_CMPANDEdb,E2K_CMPANDSdb,E2K_CMPANDPdb,E2K_CMPANDLEdb,
    E2K_STb,      E2K_STh,      E2K_BITREVs,  E2K_STw,
    E2K_BITREVd,  E2K_STd,
    /* ------------------------------------------------- */
    E2K_FCMPEQsb, E2K_FCMPLTsb, E2K_FCMPLEsb, E2K_FCMPUODsb,
    E2K_FCMPNEQsb,E2K_FCMPNLTsb,E2K_FCMPNLEsb,
    E2K_FCMPEQdb, E2K_FCMPLTdb, E2K_FCMPLEdb, E2K_FCMPUODdb,
    E2K_FCMPNEQdb,E2K_FCMPNLTdb,E2K_FCMPNLEdb,
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
    E2K_UDIVs,    E2K_UDIVd,    E2K_SDIVs,    E2K_SDIVd,
    E2K_UDIVX,    E2K_UMODX,    E2K_SDIVX,    E2K_SMODX,
    /* ------------------------------------------------- */
    E2K_MOVTs,    E2K_MOVTd,    E2K_GETPL,
    E2K_LDb,      E2K_LZCNTs,   E2K_LDh,      E2K_LZCNTd,
    E2K_LDw,      E2K_POPCNTs,  E2K_LDd,      E2K_POPCNTd,
    /* ------------------------------------------------- */
    E2K_MULs,     E2K_MULd,     E2K_UMULX,    E2K_SMULX,
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
    { "ORs",       RES_ALS_012345, 0x04, 0,    0,   0,     E2K_ALOPF1},
    { "ORd",       RES_ALS_012345, 0x05, 0,    0,   0,     E2K_ALOPF1},
    { "ORNs",      RES_ALS_012345, 0x06, 0,    0,   0,     E2K_ALOPF1},
    { "ORNd",      RES_ALS_012345, 0x07, 0,    0,   0,     E2K_ALOPF1},
/* --------------------------------------------------- */
    { "XORs",      RES_ALS_012345, 0x08, 0,    0,   0,     E2K_ALOPF1},
    { "XORd",      RES_ALS_012345, 0x09, 0,    0,   0,     E2K_ALOPF1},
    { "XORNs",     RES_ALS_012345, 0x0a, 0,    0,   0,     E2K_ALOPF1},
    { "XORNd",     RES_ALS_012345, 0x0b, 0,    0,   0,     E2K_ALOPF1},
    { "SXT",       RES_ALS_012345, 0x0c, 0,    0,   0,     E2K_ALOPF1},
    { "MERGEs",    RES_ALS_012345, 0x0e, 0,    0,   0,     E2K_ALOPF1},
    { "MERGEd",    RES_ALS_012345, 0x0f, 0,    0,   0,     E2K_ALOPF1},
/* --------------------------------------------------- */
    { "ADDs",      RES_ALS_012345, 0x10, 0,    0,   0,     E2K_ALOPF1},
    { "ADDd",      RES_ALS_012345, 0x11, 0,    0,   0,     E2K_ALOPF1},
    { "SUBs",      RES_ALS_012345, 0x12, 0,    0,   0,     E2K_ALOPF1},
    { "SUBd",      RES_ALS_012345, 0x13, 0,    0,   0,     E2K_ALOPF1},
    { "SCLs",      RES_ALS_012345, 0x14, 0,    0,   0,     E2K_ALOPF1},
    { "SCLd",      RES_ALS_012345, 0x15, 0,    0,   0,     E2K_ALOPF1},
    { "SCRs",      RES_ALS_012345, 0x16, 0,    0,   0,     E2K_ALOPF1},
    { "SCRd",      RES_ALS_012345, 0x17, 0,    0,   0,     E2K_ALOPF1},
/* --------------------------------------------------- */
    { "SHLs",      RES_ALS_012345, 0x18, 0,    0,   0,     E2K_ALOPF1},
    { "SHLd",      RES_ALS_012345, 0x19, 0,    0,   0,     E2K_ALOPF1},
    { "SHRs",      RES_ALS_012345, 0x1a, 0,    0,   0,     E2K_ALOPF1},
    { "SHRd",      RES_ALS_012345, 0x1b, 0,    0,   0,     E2K_ALOPF1},
    { "SARs",      RES_ALS_012345, 0x1c, 0,    0,   0,     E2K_ALOPF1},
    { "SARd",      RES_ALS_012345, 0x1d, 0,    0,   0,     E2K_ALOPF1},
    { "GETFs",     RES_ALS_012345, 0x1e, 0,    0,   0,     E2K_ALOPF1},
    { "GETFd",     RES_ALS_012345, 0x1f, 0,    0,   0,     E2K_ALOPF1},
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
    { "STb",       RES_ALS_25,     0x24, 0,    0,   0,     E2K_ALOPF3},
    { "STh",       RES_ALS_25,     0x25, 0,    0,   0,     E2K_ALOPF3},
    { "BITREVs",   RES_ALS_0134,   0x26, 0xc0, 0,   0,     E2K_ALOPF2},
    { "STw",       RES_ALS_25,     0x26, 0,    0,   0,     E2K_ALOPF3},
    { "BITREVd",   RES_ALS_0134,   0x27, 0xc0, 0,   0,     E2K_ALOPF2},
    { "STd",       RES_ALS_25,     0x27, 0,    0,   0,     E2K_ALOPF3},
/* --------------------------------------------------- */
    // TODO: FXCMP
    { "FCMPEQsb",  RES_ALS_0134,   0x2e, 0x0,  0,   0,     E2K_ALOPF11},
    { "FCMPLTsb",  RES_ALS_0134,   0x2e, 0x1,  0,   0,     E2K_ALOPF11},
    { "FCMPLEsb",  RES_ALS_0134,   0x2e, 0x2,  0,   0,     E2K_ALOPF11},
    { "FCMPUODsb", RES_ALS_0134,   0x2e, 0x3,  0,   0,     E2K_ALOPF11},
    { "FCMPNEQsb", RES_ALS_0134,   0x2e, 0x4,  0,   0,     E2K_ALOPF11},
    { "FCMPNLTsb", RES_ALS_0134,   0x2e, 0x5,  0,   0,     E2K_ALOPF11},
    { "FCMPNLEsb", RES_ALS_0134,   0x2e, 0x6,  0,   0,     E2K_ALOPF11},
    { "FCMPEQdb",  RES_ALS_0134,   0x2f, 0x0,  0,   0,     E2K_ALOPF11},
    { "FCMPLTdb",  RES_ALS_0134,   0x2f, 0x1,  0,   0,     E2K_ALOPF11},
    { "FCMPLEdb",  RES_ALS_0134,   0x2f, 0x2,  0,   0,     E2K_ALOPF11},
    { "FCMPUODdb", RES_ALS_0134,   0x2f, 0x3,  0,   0,     E2K_ALOPF11},
    { "FCMPNEQdb", RES_ALS_0134,   0x2f, 0x4,  0,   0,     E2K_ALOPF11},
    { "FCMPNLTdb", RES_ALS_0134,   0x2f, 0x5,  0,   0,     E2K_ALOPF11},
    { "FCMPNLEdb", RES_ALS_0134,   0x2f, 0x6,  0,   0,     E2K_ALOPF11},
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
    // TODO: Skip FX ops
    { "UDIVs",    RES_ALS_25,      0x40, 0,    0x01, 0xc0, E2K_ALOPF1},
    { "UDIVd",    RES_ALS_25,      0x41, 0,    0x01, 0xc0, E2K_ALOPF1},
    { "SDIVs",    RES_ALS_25,      0x42, 0,    0x01, 0xc0, E2K_ALOPF1},
    { "SDIVd",    RES_ALS_25,      0x43, 0,    0x01, 0xc0, E2K_ALOPF1},
    { "UDIVX",    RES_ALS_25,      0x44, 0,    0x01, 0xc0, E2K_ALOPF1},
    { "UMODX",    RES_ALS_25,      0x45, 0,    0x01, 0xc0, E2K_ALOPF1},
    { "SDIVX",    RES_ALS_25,      0x46, 0,    0x01, 0xc0, E2K_ALOPF1},
    { "SMODX",    RES_ALS_25,      0x47, 0,    0x01, 0xc0, E2K_ALOPF1},
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
/* --------------------------------------------------- */
    { "FDIVs",    RES_ALS5,        0x48, 0,    0x01, 0xc0, E2K_ALOPF11},
    { "FDIVd",    RES_ALS5,        0x49, 0,    0x01, 0xc0, E2K_ALOPF11},
/* --------------------------------------------------- */
    { "FSQRTs",   RES_ALS5,        0x4c, 0,    0x01, 0xc0, E2K_ALOPF12},
    { "FSQRTId",  RES_ALS5,        0x4d, 0,    0x01, 0xc0, E2K_ALOPF12},
    { "FSQRTTd",  RES_ALS5,        0x51, 0,    0x01, 0xc0, E2K_ALOPF12},
/* --------------------------------------------------- */
    { "GETSP",    RES_ALS0,        0x58, 0xec, 0x01, 0xc0, E2K_ALOPF2},
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
    { "ALOPF21",    FMT_SRC1 | FMT_SRC2 },
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
                           sljit_u32 src1)
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
  emit_cds(compiler, bundle, 0, als, pred, inv, p)
#define emit_mrgc(compiler, bundle, als, pred, inv) \
  emit_cds(compiler, bundle, 2, als, pred, inv)

#define ISRC1(src1) emit_src1(compiler, bundle, E2K_CONST,    src1)
#define RSRC1(src1) emit_src1(compiler, bundle, E2K_REG,      src1)
#define ISRC2(src2) emit_src2(compiler, bundle, E2K_CONST,    src2)
#define RSRC2(src2) emit_src2(compiler, bundle, E2K_REG,      src2)
#define RSRC3(src3) emit_src3(compiler, bundle, E2K_REG,      src3)
#define RDST(dst)   emit_dst (compiler, bundle, E2K_REG,      dst)
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
#define emit_alopf3_rr(compiler, bundle, spec, op, src1, src2, src3) \
  emit_alopf3(compiler, bundle, spec, op, RSRC1(src1), RSRC2(src2), RSRC3(src3))
#define emit_alopf3_ir(compiler, bundle, spec, op, src1, src2, src3) \
  emit_alopf3(compiler, bundle, spec, op, ISRC1(src1), RSRC2(src2), RSRC3(src3))
#define emit_alopf3_ri(compiler, bundle, spec, op, src1, src2, src3) \
  emit_alopf3(compiler, bundle, spec, op, RSRC1(src1), ISRC2(src2), RSRC3(src3))
#define emit_alopf3_ii(compiler, bundle, spec, op, src1, src2, src3) \
  emit_alopf3(compiler, bundle, spec, op, ISRC1(src1), ISRC2(src2), RSRC3(src3))

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

    emit_ct(compiler, ctpr, pred, inverted);
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
                    sljit_sw src3, sljit_sw dst)
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
                                   SLJIT_EXTRACT_REG(dst));
                    break;
                case REG_SRC1:
                    emit_alopf1_ri(compiler, bundle, 0, opcode,
                                   SLJIT_EXTRACT_REG(src1),
                                   src2,
                                   SLJIT_EXTRACT_REG(dst));
                    break;
                case REG_SRC2:
                    emit_alopf1_ir(compiler, bundle, 0, opcode,
                                   src1,
                                   SLJIT_EXTRACT_REG(src2),
                                   SLJIT_EXTRACT_REG(dst));
                    break;
                case REG_ZERO:
                    emit_alopf1_ii(compiler, bundle, 0, opcode,
                                   src1,
                                   src2,
                                   SLJIT_EXTRACT_REG(dst));
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
                                  SLJIT_EXTRACT_REG(dst));
                    break;
                case REG_ZERO:
                    emit_alopf2_i(compiler, bundle, 0, opcode,
                                  SLJIT_EXTRACT_REG(src2),
                                  SLJIT_EXTRACT_REG(dst));
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
                                   SLJIT_EXTRACT_REG(src3));
                    break;
                case REG_SRC1 | REG_SRC3:
                    emit_alopf3_ri(compiler, bundle, 0, opcode,
                                   SLJIT_EXTRACT_REG(src1),
                                   src2,
                                   SLJIT_EXTRACT_REG(src3));
                    break;
                case REG_SRC2 | REG_SRC3:
                    emit_alopf3_ir(compiler, bundle, 0, opcode,
                                   src1,
                                   SLJIT_EXTRACT_REG(src2),
                                   SLJIT_EXTRACT_REG(src3));
                    break;
                case REG_SRC3:
                    emit_alopf3_ii(compiler, bundle, 0, opcode,
                                   src1,
                                   src2,
                                   SLJIT_EXTRACT_REG(src3));
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
                                   SLJIT_EXTRACT_REG(dst));
                    break;
                case REG_SRC1:
                    emit_alopf11_ri(compiler, bundle, 0, opcode,
                                   SLJIT_EXTRACT_REG(src1),
                                   src2,
                                   SLJIT_EXTRACT_REG(dst));
                    break;
                case REG_SRC2:
                    emit_alopf11_ir(compiler, bundle, 0, opcode,
                                   src1,
                                   SLJIT_EXTRACT_REG(src2),
                                   SLJIT_EXTRACT_REG(dst));
                    break;
                case REG_ZERO:
                    emit_alopf11_ii(compiler, bundle, 0, opcode,
                                   src1,
                                   src2,
                                   SLJIT_EXTRACT_REG(dst));
                    break;
            }
            break;
        case E2K_ALOPF12:
            switch (src_type)
            {
                case REG_SRC2:
                    emit_alopf12_r(compiler, bundle, 0, opcode,
                                   SLJIT_EXTRACT_REG(src2),
                                   dst);
                    break;
                case REG_ZERO:
                    emit_alopf12_i(compiler, bundle, 0, opcode,
                                   src2,
                                   dst);
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
                                   SLJIT_EXTRACT_REG(dst));
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
                         sljit_sw src3, sljit_sw dst)
{
    BUNDLE_SETUP();
    emit_op(compiler, bundle, opcode, reg_imm_mask, src1, src2, src3, dst);
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

/* -- Emit generic operations --------------------------------------------- */

// Load into reg only
static void emit_load(struct sljit_compiler *compiler,
                      sljit_s32 dst, sljit_uw dstw,
                      sljit_s32 src, sljit_uw srcw) {
    if (!SLJIT_IS_MEM(src)) {
        SLJIT_ASSERT(0);
    }
    if (!SLJIT_IS_REG(dst)) {
        SLJIT_ASSERT(0);
    }

    BUNDLE_SETUP();

    if (SLJIT_IS_MEM0(src)) {
        emit_nop(compiler, bundle, 4);
        emit_alopf1_ii(compiler, bundle, 0, E2K_LDd, 0x0, srcw, dst);
    } else if (SLJIT_IS_MEM1(src)) {
        emit_nop(compiler, bundle, 4);
        emit_alopf1_ri(compiler, bundle, 0, E2K_LDd,
                       SLJIT_EXTRACT_REG(src), 0x0, dst);
    } else if (SLJIT_IS_MEM2(src)) {
        emit_nop(compiler, bundle, 4);
        emit_alopf1_rr(compiler, bundle, 0,
                       E2K_LDd, SLJIT_EXTRACT_REG(src),
                       SLJIT_EXTRACT_SECOND_REG(src), dst);
    }

    emit_bundle_finalize(compiler, bundle);
}

// Store reg only
static void emit_store(struct sljit_compiler *compiler,
                       sljit_s32 dst, sljit_uw dstw,
                       sljit_s32 src, sljit_uw srcw) {
    if (!SLJIT_IS_MEM(dst)) {
        SLJIT_ASSERT(0);
    }
    if (!SLJIT_IS_REG(src)) {
        SLJIT_ASSERT(0);
    }

    BUNDLE_SETUP();

    if (SLJIT_IS_MEM0(dst)) {
        emit_nop(compiler, bundle, 4);
        emit_alopf3_ii(compiler, bundle, 0, E2K_STd, 0x0, dstw, src);
    } else if (SLJIT_IS_MEM1(dst)) {
        emit_nop(compiler, bundle, 4);
        emit_alopf3_ri(compiler, bundle, 0, E2K_STd,
                       SLJIT_EXTRACT_REG(dst), 0x0, src);
    } else if (SLJIT_IS_MEM2(dst)) {
        emit_nop(compiler, bundle, 4);
        emit_alopf3_rr(compiler, bundle, 0,
                       E2K_STd, SLJIT_EXTRACT_REG(dst),
                       SLJIT_EXTRACT_SECOND_REG(dst), src);
    }

    emit_bundle_finalize(compiler, bundle);
}

// Analyze input types (const, mem, reg) and call correct op emit.
static
void signature_switcher1(struct sljit_compiler *compiler,
                         sljit_u16 op,
                         sljit_s32 dst, sljit_sw dstw,
                         sljit_s32 src, sljit_sw srcw)
{
    if (SLJIT_IS_REG(dst)) {
        if (SLJIT_IS_REG(src)) {
            emit_op_full(compiler, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                         0, src, 0, dst);
        } else if (SLJIT_IS_IMM(src)) {
            emit_op_full(compiler, op, REG_SRC1 | REG_SRC3,
                         0, srcw, 0, dst);
        } else if (SLJIT_IS_MEM(src)) {
            emit_load(compiler, TMP_REG1, 0, src, srcw);
            emit_op_full(compiler, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                         0, TMP_REG1, 0, dst);
        } else {
            SLJIT_ASSERT(0);
        }
    } else if (SLJIT_IS_MEM(dst)) {
        if (SLJIT_IS_REG(src)) {
            emit_op_full(compiler, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                         0, src, 0, TMP_REG1);
        } else if (SLJIT_IS_IMM(src)) {
            emit_op_full(compiler, op, REG_SRC1 | REG_SRC3,
                         0, srcw, 0, TMP_REG1);
        } else if (SLJIT_IS_MEM(src)) {
            emit_load(compiler, TMP_REG1, 0, src, srcw);
            emit_op_full(compiler, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                         0, TMP_REG1, 0, TMP_REG1);
        } else {
            SLJIT_ASSERT(0);
        }
        emit_store(compiler, dst, dstw, TMP_REG1, 0);
    } else {
        SLJIT_ASSERT(0);
    }
}

// Analyze input types (const, mem, reg) and call correct op emit.
// TODO: src1 as imm and mem is banned.
static
void signature_switcher2(struct sljit_compiler *compiler,
                         sljit_u16 op,
                         sljit_s32 dst, sljit_sw dstw,
                         sljit_s32 src1, sljit_sw src1w,
                         sljit_s32 src2, sljit_sw src2w)
{
    if (SLJIT_IS_REG(dst)) {
        if (SLJIT_IS_REG(src2)) {
            if (SLJIT_IS_REG(src1)) {
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                             src1, src2, 0, dst);
            } else if (SLJIT_IS_IMM(src1)) {
                // src1 does not support 32 and 64 imm
                emit_op_full(compiler, E2K_ADDd, REG_ZERO,
                             0, src1w, 0, TMP_REG1);
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                             TMP_REG1, src2, 0, dst);
            } else if (SLJIT_IS_MEM(src1)) {
                emit_load(compiler, TMP_REG1, 0, src1, src1w);
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                             TMP_REG1, src2, 0, dst);
            } else {
                SLJIT_ASSERT(0);
            }
        } else if (SLJIT_IS_IMM(src2)) {
            if (SLJIT_IS_REG(src1)) {
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC3,
                             src1, src2w, 0, dst);
            } else if (SLJIT_IS_IMM(src1)) {
                // src1 does not support 32 and 64 imm
                emit_op_full(compiler, E2K_ADDd, REG_ZERO,
                             0, src1w, 0, TMP_REG1);
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC3,
                             TMP_REG1, src2w, 0, dst);
            } else if (SLJIT_IS_MEM(src1)) {
                emit_load(compiler, TMP_REG1, 0, src1, src1w);
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC3,
                             TMP_REG1, src2w, 0, dst);
            } else {
                SLJIT_ASSERT(0);
            }
        } else if (SLJIT_IS_MEM(src2)) {
            emit_load(compiler, TMP_REG1, 0, src2, src2w);
            if (SLJIT_IS_REG(src1)) {
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                             src1, TMP_REG1, 0, dst);
            } else if (SLJIT_IS_IMM(src1)) {
                // src1 does not support 32 and 64 imm
                emit_op_full(compiler, E2K_ADDd, REG_ZERO,
                             0, src1w, 0, TMP_REG2);
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                             TMP_REG2, TMP_REG1, 0, TMP_REG1);
            } else if (SLJIT_IS_MEM(src1)) {
                emit_load(compiler, TMP_REG2, 0, src1, src1w);
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                             TMP_REG2, TMP_REG1, 0, TMP_REG1);
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
                             src1, src2, 0, TMP_REG1);
            } else if (SLJIT_IS_IMM(src1)) {
                // src1 does not support 32 and 64 imm
                emit_op_full(compiler, E2K_ADDd, REG_ZERO,
                             0, src1w, 0, TMP_REG1);
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                             TMP_REG1, src2, 0, TMP_REG1);
            } else if (SLJIT_IS_MEM(src1)) {
                emit_load(compiler, TMP_REG1, 0, src1, src1w);
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                             TMP_REG1, src2, 0, TMP_REG1);
            } else {
                SLJIT_ASSERT(0);
            }
        } else if (SLJIT_IS_IMM(src2)) {
            if (SLJIT_IS_REG(src1)) {
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC3,
                             src1, src2w, 0, TMP_REG1);
            } else if (SLJIT_IS_IMM(src1)) {
                // src1 does not support 32 and 64 imm
                emit_op_full(compiler, E2K_ADDd, REG_ZERO,
                             0, src1w, 0, TMP_REG1);
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC3,
                             TMP_REG1, src2w, 0, TMP_REG1);
            } else if (SLJIT_IS_MEM(src1)) {
                emit_load(compiler, TMP_REG1, 0, src1, src1w);
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC3,
                             TMP_REG1, src2w, 0, TMP_REG1);
            } else {
                SLJIT_ASSERT(0);
            }
        } else if (SLJIT_IS_MEM(src2)) {
            emit_load(compiler, TMP_REG1, 0, src2, src2w);
            if (SLJIT_IS_REG(src1)) {
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                             src1, TMP_REG1, 0, TMP_REG1);
            } else if (SLJIT_IS_IMM(src1)) {
                // src1 does not support 32 and 64 imm
                emit_op_full(compiler, E2K_ADDd, REG_ZERO,
                             0, src1w, 0, TMP_REG2);
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                             TMP_REG2, TMP_REG1, 0, TMP_REG1);
            } else if (SLJIT_IS_MEM(src1)) {
                emit_load(compiler, TMP_REG2, 0, src1, src1w);
                emit_op_full(compiler, op, REG_SRC1 | REG_SRC2 | REG_SRC3,
                             TMP_REG2, TMP_REG1, 0, TMP_REG1);
            } else {
                SLJIT_ASSERT(0);
            }
        } else {
            SLJIT_ASSERT(0);
        }
        emit_store(compiler, dst, dstw, TMP_REG1, 0);
    } else {
        SLJIT_ASSERT(0);
    }
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
// Luajit

SLJIT_API_FUNC_ATTRIBUTE
void* sljit_generate_code(struct sljit_compiler *compiler, sljit_s32 options,
                          void *exec_allocator_data)
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

    sljit_u32 stack_size = (saveds * 8) + local_size;
    SLJIT_ASSERT(stack_size < 0x7FFFFFFF); // 2**32 - 1
    compiler->local_size = stack_size;

    // TODO: Add SLJIT_SP for stack pointer register
    // TODO: Think about new reg map
    // ------------------------------------------------------------------------

    BUNDLE_SETUP();

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
    emit_bundle_finalize(compiler, bundle);

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

    emit_return(compiler, E2K_CTPR(1), 0);

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

	op = GET_OPCODE(op);
	switch (op) {
	case SLJIT_BREAKPOINT:
    case SLJIT_NOP:
		emit_nop_full(compiler, 1);
		break;
	case SLJIT_LMUL_UW:
	case SLJIT_LMUL_SW:
        // mul SLJIT_R0 and SLJIT_R1
        // Result into R1:R0
        emit_op_full(compiler, E2K_ADDd, REG_SRC1 | REG_SRC2 | REG_SRC3,
                     SLJIT_R1, 0, 0, TMP_REG1);
        if (op == SLJIT_LMUL_UW) {
            emit_op_full(compiler, E2K_UMULHd, REG_SRC1 | REG_SRC2 | REG_SRC3,
                         SLJIT_R0, SLJIT_R1, 0, SLJIT_R1);
        } else {
            emit_op_full(compiler, E2K_SMULHd, REG_SRC1 | REG_SRC2 | REG_SRC3,
                         SLJIT_R0, SLJIT_R1, 0, SLJIT_R1);
        }
        emit_op_full(compiler, E2K_MULd, REG_SRC1 | REG_SRC2 | REG_SRC3,
                     SLJIT_R0, TMP_REG1, 0, SLJIT_R0);
        break;
	case SLJIT_DIVMOD_UW:
	case SLJIT_DIVMOD_SW:
        // Div R0 by the R1
        // Result -> R0
        // Reminder -> R1
        emit_op_full(compiler, E2K_ADDd, REG_SRC1 | REG_SRC2 | REG_SRC3, SLJIT_R0, 0, 0, TMP_REG1);
        if (op == SLJIT_DIVMOD_UW) {
            emit_op_full(compiler, E2K_UDIVd, REG_SRC1 | REG_SRC2 | REG_SRC3,
                         SLJIT_R0, SLJIT_R1, 0, SLJIT_R0);
            emit_op_full(compiler, E2K_UMODX, REG_SRC1 | REG_SRC2 | REG_SRC3,
                         TMP_REG1, SLJIT_R1, 0, SLJIT_R1);
        } else {
            emit_op_full(compiler, E2K_SDIVd, REG_SRC1 | REG_SRC2 | REG_SRC3,
                         SLJIT_R0, SLJIT_R1, 0, SLJIT_R0);
            emit_op_full(compiler, E2K_SMODX, REG_SRC1 | REG_SRC2 | REG_SRC3,
                         TMP_REG1, SLJIT_R1, 0, SLJIT_R1);
        }
	case SLJIT_DIV_UW:
	case SLJIT_DIV_SW:
        // Div R0 by R1
        // Result -> R0
        if (op == SLJIT_DIV_UW) {
            emit_op_full(compiler, E2K_UDIVd, REG_SRC1 | REG_SRC2 | REG_SRC3,
                         SLJIT_R0, SLJIT_R1, 0, SLJIT_R0);
        } else {
            emit_op_full(compiler, E2K_SDIVd, REG_SRC1 | REG_SRC2 | REG_SRC3,
                         SLJIT_R0, SLJIT_R1, 0, SLJIT_R0);
        }
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

    /*
        Formats.
        dst|src: reg, SLJIT_MEM0(addr), SLJIT_MEM1(r), SLJIT_MEM2(r, r), SLJIT_IMM
    */
	switch (GET_OPCODE(op)) {
    // TODO: Support other formats
	case SLJIT_MOV:
	case SLJIT_MOV_U32:
	case SLJIT_MOV_S32:
	case SLJIT_MOV32:
	case SLJIT_MOV_P:
	case SLJIT_MOV_U8:
	case SLJIT_MOV_S8:
	case SLJIT_MOV_U16:
	case SLJIT_MOV_S16:
        signature_switcher1(compiler, E2K_ADDd, dst, dstw, src, srcw);
        break;
	case SLJIT_CLZ:
        signature_switcher1(compiler, E2K_LZCNTd, dst, dstw, src, srcw);
        break;
	case SLJIT_CTZ:
        signature_switcher1(compiler, E2K_BITREVd, TMP_REG1, dstw, src, srcw);
        signature_switcher1(compiler, E2K_LZCNTd, dst, dstw, TMP_REG1, srcw);
        break;
	case SLJIT_REV:
	case SLJIT_REV_U32:
	case SLJIT_REV_S32:
	case SLJIT_REV_U16:
	case SLJIT_REV_S16:
        signature_switcher1(compiler, E2K_BITREVd, dst, dstw, src, srcw);
        break;
    default:
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

	switch (GET_OPCODE(op)) {
    // TODO: Think about carry
	case SLJIT_ADD:
	case SLJIT_ADD32:
	case SLJIT_ADDC:
	case SLJIT_ADDC32:
        signature_switcher2(compiler, E2K_ADDd, dst, dstw, src1, src1w, src2, src2w);
        break;
    // TODO: Think about carry
	case SLJIT_SUB:
	case SLJIT_SUB32:
	case SLJIT_SUBC:
	case SLJIT_SUBC32:
        signature_switcher2(compiler, E2K_SUBd, dst, dstw, src1, src1w, src2, src2w);
        break;
	case SLJIT_MUL:
	case SLJIT_MUL32:
        signature_switcher2(compiler, E2K_MULd, dst, dstw, src1, src1w, src2, src2w);
        break;
	case SLJIT_AND:
	case SLJIT_AND32:
        signature_switcher2(compiler, E2K_ANDd, dst, dstw, src1, src1w, src2, src2w);
        break;
	case SLJIT_OR:
	case SLJIT_OR32:
        signature_switcher2(compiler, E2K_ORd, dst, dstw, src1, src1w, src2, src2w);
        break;
	case SLJIT_XOR:
	case SLJIT_XOR32:
        signature_switcher2(compiler, E2K_XORd, dst, dstw, src1, src1w, src2, src2w);
        break;
        break;
	case SLJIT_SHL:
	case SLJIT_SHL32:
        signature_switcher2(compiler, E2K_SHLd, dst, dstw, src1, src1w, src2, src2w);
        break;
    // TODO: Understand what to do
	case SLJIT_MSHL:
	case SLJIT_MSHL32:
        signature_switcher2(compiler, E2K_SHLd, dst, dstw, src1, src1w, src2, src2w);
        break;
	case SLJIT_LSHR:
	case SLJIT_LSHR32:
        signature_switcher2(compiler, E2K_SHRd, dst, dstw, src1, src1w, src2, src2w);
        break;
    // TODO: Understand what to do
	case SLJIT_MLSHR:
	case SLJIT_MLSHR32:
        signature_switcher2(compiler, E2K_SHRd, dst, dstw, src1, src1w, src2, src2w);
        break;
	case SLJIT_ASHR:
	case SLJIT_ASHR32:
        signature_switcher2(compiler, E2K_SARd, dst, dstw, src1, src1w, src2, src2w);
        break;
    // TODO: Understand what to do
	case SLJIT_MASHR:
	case SLJIT_MASHR32:
        signature_switcher2(compiler, E2K_SARd, dst, dstw, src1, src1w, src2, src2w);
        break;
	case SLJIT_ROTL:
	case SLJIT_ROTL32:
        signature_switcher2(compiler, E2K_SCLd, dst, dstw, src1, src1w, src2, src2w);
        break;
	case SLJIT_ROTR:
	case SLJIT_ROTR32:
        signature_switcher2(compiler, E2K_SCRd, dst, dstw, src1, src1w, src2, src2w);
        break;
    default:
        return SLJIT_ERR_UNSUPPORTED;
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
	return sljit_emit_op2(compiler, op, TMP_REG1, 0, src1, src1w, src2, src2w);
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
	CHECK_ERROR();
	CHECK(check_sljit_emit_fop1(compiler, op, dst, dstw, src, srcw));

	switch (GET_OPCODE(op)) {
    case SLJIT_MOV_F64:
        signature_switcher1(compiler, E2K_FADDd, dst, dstw, src, srcw);
        break;
    case SLJIT_MOV_F32:
        signature_switcher1(compiler, E2K_FADDs, dst, dstw, src, srcw);
        break;
    case SLJIT_CONV_F64_FROM_F32:
        signature_switcher1(compiler, E2K_FSTOFD, dst, dstw, src, srcw);
        break;
    case SLJIT_CONV_F32_FROM_F64:
        signature_switcher1(compiler, E2K_FDTOFS, dst, dstw, src, srcw);
        break;
    case SLJIT_CONV_SW_FROM_F64:
        signature_switcher1(compiler, E2K_FDTOID, dst, dstw, src, srcw);
        break;
    case SLJIT_CONV_SW_FROM_F32:
        signature_switcher1(compiler, E2K_FSTOID, dst, dstw, src, srcw);
        break;
    case SLJIT_CONV_S32_FROM_F64:
        signature_switcher1(compiler, E2K_FDTOIS, dst, dstw, src, srcw);
        break;
    case SLJIT_CONV_S32_FROM_F32:
        signature_switcher1(compiler, E2K_FSTOIS, dst, dstw, src, srcw);
        break;
    case SLJIT_CONV_F64_FROM_SW:
        signature_switcher1(compiler, E2K_IDTOFD, dst, dstw, src, srcw);
        break;
    case SLJIT_CONV_F32_FROM_SW:
        signature_switcher1(compiler, E2K_IDTOFS, dst, dstw, src, srcw);
        break;
    case SLJIT_CONV_F64_FROM_S32:
        signature_switcher1(compiler, E2K_ISTOFD, dst, dstw, src, srcw);
        break;
    case SLJIT_CONV_F32_FROM_S32:
        signature_switcher1(compiler, E2K_ISTOFS, dst, dstw, src, srcw);
        break;
    // TODO:
    case SLJIT_CONV_F64_FROM_UW:
    case SLJIT_CONV_F32_FROM_UW:
    case SLJIT_CONV_F64_FROM_U32:
    case SLJIT_CONV_F32_FROM_U32:
        return SLJIT_ERR_UNSUPPORTED;
    // TODO:
    case SLJIT_CMP_F64:
    case SLJIT_CMP_F32:
        return SLJIT_ERR_UNSUPPORTED;
    case SLJIT_NEG_F64:
        break;
    case SLJIT_NEG_F32:
        break;
    case SLJIT_ABS_F64:
        signature_switcher2(compiler, E2K_XORd, dst, dstw, 0, 0x8000000000000000, src, srcw);
        break;
    case SLJIT_ABS_F32:
        signature_switcher2(compiler, E2K_XORs, dst, dstw, 0, 0x80000000, src, srcw);
        break;
    default:
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

	switch (GET_OPCODE(op)) {
    case SLJIT_ADD_F64:
        signature_switcher2(compiler, E2K_FADDd, dst, dstw, src1, src1w, src2, src2w);
        break;
    case SLJIT_ADD_F32:
        signature_switcher2(compiler, E2K_FADDs, dst, dstw, src1, src1w, src2, src2w);
        break;
    case SLJIT_SUB_F64:
        signature_switcher2(compiler, E2K_FSUBd, dst, dstw, src1, src1w, src2, src2w);
        break;
    case SLJIT_SUB_F32:
        signature_switcher2(compiler, E2K_FSUBs, dst, dstw, src1, src1w, src2, src2w);
        break;
    case SLJIT_MUL_F64:
        signature_switcher2(compiler, E2K_FMULd, dst, dstw, src1, src1w, src2, src2w);
        break;
    case SLJIT_MUL_F32:
        signature_switcher2(compiler, E2K_FMULs, dst, dstw, src1, src1w, src2, src2w);
        break;
    case SLJIT_DIV_F64:
        signature_switcher2(compiler, E2K_FDIVd, dst, dstw, src1, src1w, src2, src2w);
        break;
    case SLJIT_DIV_F32:
        signature_switcher2(compiler, E2K_FDIVs, dst, dstw, src1, src1w, src2, src2w);
        break;
    default:
        return SLJIT_ERR_UNSUPPORTED;
	}

    return SLJIT_SUCCESS;
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
    struct sljit_jump *jump;
    sljit_u32 *inst;

	CHECK_ERROR();
	CHECK(check_sljit_emit_ijump(compiler, type, src, srcw));

    // Make possible to read variable from callee function
	ADJUST_LOCAL_OFFSET(src, srcw);

    // direct form, maybe IMM only
    if (SLJIT_IS_IMM(src)) {
		jump = (struct sljit_jump*)ensure_abuf(compiler, sizeof(struct sljit_jump));
		FAIL_IF_NULL(jump);
		set_jump(jump, compiler, (sljit_u32)JUMP_ADDR);
		jump->u.target = (sljit_uw)srcw;

		jump->addr = compiler->size;
        // TODO: Now MOVTd only, but for perf we want disp in the future.
        // We should calc jump len and replace movtd by disp.
        // TMP_REG2 as mark.
        BUNDLE_SETUP();
        emit_alopf2_r(compiler, bundle, 1, E2K_MOVTd, TMP_REG2, E2K_CTPR(1));
        emit_nop(compiler, bundle, 8);
        emit_bundle_finalize(compiler, bundle);
        emit_ct(compiler, E2K_CTPR(1), 0, 0);
    // indirect form
    } else if (SLJIT_IS_MEM(src)) {
        emit_load(compiler, TMP_REG1, 0, src, srcw);
        BUNDLE_SETUP();
        emit_alopf2_r(compiler, bundle, 1, E2K_MOVTd, TMP_REG1, E2K_CTPR(1));
        emit_nop(compiler, bundle, 8);
        emit_bundle_finalize(compiler, bundle);
        emit_ct(compiler, E2K_CTPR(1), 0, 0);
    // indirect form
    } else if (SLJIT_IS_REG(src)) {
        BUNDLE_SETUP();
        emit_alopf2_r(compiler, bundle, 1, E2K_MOVTd, src, E2K_CTPR(1));
        emit_nop(compiler, bundle, 8);
        emit_bundle_finalize(compiler, bundle);
        emit_ct(compiler, E2K_CTPR(1), 0, 0);
    } else {
        SLJIT_ASSERT(0);
    }

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
