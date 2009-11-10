/*
 *    Stack-less Just-In-Time compiler
 *    Copyright (c) Zoltan Herczeg
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 */

#ifndef _SLJIT_H_
#define _SLJIT_H_

// ------------------------------------------------------------------------
//  Stack-Less JIT compiler for multiple architectures (x86, ARM, PowerPC)
// ------------------------------------------------------------------------
//
// Short description
//  Advantages:
//    - The execution can be continued from any LIR instruction
//      In other words, jump into and out of the code is safe
//    - Both target of (conditional) jump and call instructions
//      and constants can be dynamically modified during runtime
//      - although it is not suggested to do it frequently
//      - very effective to cache an important value once
//    - A fixed stack space can be allocated for local variables
//    - The compiler is thread-safe
//  Disadvantages:
//    - Limited number of registers (only 6 integer registers, max 3
//      temporary and max 3 general, and only 4 floating point registers)
//  In practice:
//    - This approach is very effective for interpreters
//      - One of the general registers tipically points to a stack interface
//      - It can jump to any exception handler anytime (even for another
//        function. It is safe for SLJIT.)
//      - Fast paths can be modified runtime reflecting the changes of the
//        fastest execution path of the dynamic language
//      - SLJIT supports complex memory addressing modes
//      - mainly position independent code
//    - Optimizations (perhaps later)
//      - Only for basic blocks (when no labels inserted between LIR instructions)

#include "sljitConfig.h"

// ---------------------------------------------------------------------
//  Error codes
// ---------------------------------------------------------------------

// Indicates no error
#define SLJIT_NO_ERROR		0
// After the call of sljit_generate_code(), the error code of the compiler
// is set to this value to avoid future sljit calls (in debug mode at least).
// The complier should only be freed after sljit_generate_code()
#define SLJIT_CODE_GENERATED	1
// Cannot allocate non executable memory
#define SLJIT_MEMORY_ERROR	2
// Cannot allocate executable memory
// Only for sljit_generate_code()
#define SLJIT_EX_MEMORY_ERROR	3
// return value for SLJIT_CONFIG_UNSUPPORTED empty architecture
#define SLJIT_UNSUPPORTED	4

// ---------------------------------------------------------------------
//  Registers
// ---------------------------------------------------------------------

#define SLJIT_NO_REG		0

#define SLJIT_TEMPORARY_REG1	1
#define SLJIT_TEMPORARY_REG2	2
#define SLJIT_TEMPORARY_REG3	3

#define SLJIT_GENERAL_REG1	4
#define SLJIT_GENERAL_REG2	5
#define SLJIT_GENERAL_REG3	6

// Read-only register
// SLJIT_MEM2(SLJIT_LOCALS_REG, SLJIT_LOCALS_REG) is not supported
#define SLJIT_LOCALS_REG	7

#define SLJIT_NO_TMP_REGISTERS	3
#define SLJIT_NO_GEN_REGISTERS	3
#define SLJIT_NO_REGISTERS	7

// Return with machine word or double machine word

#define SLJIT_PREF_RET_REG	SLJIT_TEMPORARY_REG1
#define SLJIT_PREF_RET_HIREG	SLJIT_TEMPORARY_REG2

// x86 prefers temporary registers for special purposes. If not
// these registers are used, it costs a little performance drawback.
// It doesn't matter for other archs

#define SLJIT_PREF_SHIFT_REG	SLJIT_TEMPORARY_REG3

// ---------------------------------------------------------------------
//  Floating point registers
// ---------------------------------------------------------------------

// SLJIT_NO_REG is not valid for floating point,
// because there is an individual fpu compare instruction,
// and floating point operations are not used for (non-)zero testing

// Floating point operations are performed on double precision values

#define SLJIT_FLOAT_REG1	1
#define SLJIT_FLOAT_REG2	2
#define SLJIT_FLOAT_REG3	3
#define SLJIT_FLOAT_REG4	4

// ---------------------------------------------------------------------
//  Main structures and functions
// ---------------------------------------------------------------------

struct sljit_memory_fragment {
	struct sljit_memory_fragment *next;
	int used_size;
	sljit_ub memory[1];
};

struct sljit_label {
	struct sljit_label *next;
	sljit_uw addr;
	// The maximum size difference
	sljit_uw size;
};

struct sljit_jump {
	struct sljit_jump *next;
	sljit_uw addr;
	int flags;
	union {
		sljit_uw target;
		struct sljit_label* label;
	};
};

struct sljit_const {
	struct sljit_const *next;
	sljit_uw addr;
};

struct sljit_compiler {
	int error;

	struct sljit_label *labels;
	struct sljit_jump *jumps;
	struct sljit_const *consts;
	struct sljit_label *last_label;
	struct sljit_jump *last_jump;
	struct sljit_const *last_const;

	struct sljit_memory_fragment *buf;
	struct sljit_memory_fragment *abuf;

	// Used general registers
	int general;
	// Local stack size
	int local_size;
	// Code size
	sljit_uw size;

#ifdef SLJIT_CONFIG_X86_32
	int args;
#endif

#ifdef SLJIT_CONFIG_X86_64
	sljit_uw addrs;
	union {
		sljit_uw *addr_ptr;
		int mode32;
	};
#endif

#ifdef SLJIT_CONFIG_ARM
	// Constant pool handling
	sljit_uw *cpool;
	sljit_ub *cpool_unique;
	sljit_uw cpool_diff;
	sljit_uw cpool_fill;
	// General fields
	sljit_uw patches;
	sljit_uw last_type;
	sljit_uw last_ins;
	sljit_uw last_imm;
	// Temporary fields
	sljit_uw shift_imm;
	int cache_arg;
	sljit_w cache_argw;
#endif

#ifdef SLJIT_CONFIG_PPC_32
	sljit_w imm;
	int cache_arg;
	sljit_w cache_argw;
#endif

#ifdef SLJIT_CONFIG_PPC_64
	sljit_w imm;
	int cache_arg;
	sljit_w cache_argw;
#endif

#ifdef SLJIT_VERBOSE
	FILE* verbose;
#endif
};

// Creates an sljit compiler.
// Returns NULL if failed
struct sljit_compiler* sljit_create_compiler(void);
// Free everything except the codes
void sljit_free_compiler(struct sljit_compiler *compiler);

#define sljit_get_compiler_error(compiler)	(compiler->error)

#ifdef SLJIT_VERBOSE
// NULL = no verbose
void sljit_compiler_verbose(struct sljit_compiler *compiler, FILE* verbose);
#endif

void* sljit_generate_code(struct sljit_compiler *compiler);
void sljit_free_code(void* code);

// Instruction generation. Returns with error code

// Entry instruction. The instruction has "args" number of arguments
// and will use the first "general" number of general registers.
// The arguments are loaded into the general registers (arg1 to general_reg1, ...).
// Therefore, "args" must be less or equal than "general".
// local_size extra stack space is allocated for the jit code,
// which can accessed through SLJIT_LOCALS_REG (use it as a base register)
// Note: multiple calls of this function overwrites the previous call

int sljit_emit_enter(struct sljit_compiler *compiler, int args, int general, int local_size);

// sljit_emit_return uses variables which are initialized by sljit_emit_enter.
// Sometimes you want to return from a jit code, which entry point is in another jit code.
// sljit_fake_enter does not emit any instruction, just initialize those variables
// Note: multiple calls of this function overwrites the previous call

void sljit_fake_enter(struct sljit_compiler *compiler, int args, int general, int local_size);

// Return from jit. Return with SLJIT_NO_REG or a register
int sljit_emit_return(struct sljit_compiler *compiler, int reg);

// Source and destination values for arithmetical instructions
//  imm           - a simple immediate value (cannot be used as a destination)
//  reg           - any of the registers (immediate argument unused)
//  [imm]         - absolute immediate memory address
//  [reg+imm]     - indirect memory address
//  [reg+reg+imm] - two level indirect addressing

// IMPORATNT NOTE: memory access MUST be naturally aligned.
//   length | alignment
// ---------+-----------
//   byte   | 1 byte (not aligned)
//   half   | 2 byte (real_address & 0x1 == 0)
//   int    | 4 byte (real_address & 0x3 == 0)
//  sljit_w | 4 byte if SLJIT_32BIT_ARCHITECTURE defined
//          | 8 byte if SLJIT_64BIT_ARCHITECTURE defined
// This is a strict requirement for embedded systems.

// Note: different architectures have different addressing limitations
//  So sljit may generate several instructions if we use other addressing modes
// x86: [reg+reg+imm] supported, -2^31 <= imm <= 2^31. Write-back not supported
// arm: [reg+imm] [reg+reg] supported, -4095 <= imm <= 4095 for words and
//      unsigned bytes. -255 <= imm <= 255 for other types. Write-back supported
// ppc: [reg+imm], [reg+reg] supported -65535 <= imm <= 65535. Some immediates
//      must be divisible by 4. Write-back supported

// Register output: simply the name of the register
// For destination, you can use SLJIT_NO_REG as well
#define SLJIT_MEM_FLAG		0x100
#define SLJIT_MEM0()		(SLJIT_MEM_FLAG)
#define SLJIT_MEM1(r1)		(SLJIT_MEM_FLAG | (r1))
#define SLJIT_MEM2(r1, r2)	(SLJIT_MEM_FLAG | (r1) | ((r2) << 4))
#define SLJIT_IMM		0x200

// It may sound suprising, but the default int size on 64bit CPUs is still
// 32 bit. The 64 bit registers are mostly used only for memory addressing.
// This flag can be combined with the op argument of sljit_emit_op1 and
// sljit_emit_op2. It does NOT have any effect on 32bit CPUs or the addressing
// mode on 64 bit CPUs (SLJIT_MEMx macros)
#define SLJIT_INT_OP		0x100

// Common CPU status flags for all architectures (x86, ARM, PPC)
//  - carry flag
//  - overflow flag
//  - zero flag
//  - negative/positive flag (depends on arc)

// By default, the instructions may, or may not set the CPU status flags.
// Forcing to set status flags can be done with the following flags:

// Set Equal (Zero) status flag
#define SLJIT_SET_E			0x0200
// Set signed status flags
#define SLJIT_SET_S			0x0400
// Set unsgined status flags
#define SLJIT_SET_U			0x0800
// Set signed overflow
#define SLJIT_SET_O			0x1000
// Set carry (kinda unsigned overflow, but behaves differently on various cpus)
#define SLJIT_SET_C			0x2000

// Notes:
//   - CPU flags are NEVER set for MOV instructions
//   - you cannot postpone conditional jump instructions depending on
//     one of these instructions except if the next instruction is a MOV
//   - flag combinations: '|' 'logical or' and '^' 'both cannot be specified together'

// Notes for MOV instructions:
// U = Mov with update (post form). If source or destination defined as SLJIT_MEM1(r1)
//     or SLJIT_MEM2(r1, r2), r1 is increased by the sum of r2 and the constant argument
// UB = unsigned byte (8 bit)
// SB = signed byte (8 bit)
// UH = unsgined half (16 bit)
// SH = unsgined half (16 bit)

// Flags: -
#define SLJIT_MOV			0
// Flags: -
#define SLJIT_MOV_UB			1
// Flags: -
#define SLJIT_MOV_SB			2
// Flags: -
#define SLJIT_MOV_UH			3
// Flags: -
#define SLJIT_MOV_SH			4
// Flags: -
#define SLJIT_MOV_UI			5
// Flags: -
#define SLJIT_MOV_SI			6
// Flags: -
#define SLJIT_MOVU			7
// Flags: -
#define SLJIT_MOVU_UB			8
// Flags: -
#define SLJIT_MOVU_SB			9
// Flags: -
#define SLJIT_MOVU_UH			10
// Flags: -
#define SLJIT_MOVU_SH			11
// Flags: -
#define SLJIT_MOVU_UI			12
// Flags: -
#define SLJIT_MOVU_SI			13
// Flags: E
#define SLJIT_NOT			14
// Flags: E | O
#define SLJIT_NEG			15

int sljit_emit_op1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw);

// Flags: E | O | C
#define SLJIT_ADD			16
// Flags: E | O | C
#define SLJIT_ADDC			17
// Flags: E | O | C | S | U
#define SLJIT_SUB			18
// Flags: E | O | C | S | U
#define SLJIT_SUBC			19
// Note: integer mul
// Flags: E ^ O
#define SLJIT_MUL			20
// Flags: E
#define SLJIT_AND			21
// Flags: E
#define SLJIT_OR			22
// Flags: E
#define SLJIT_XOR			23
// Flags: E
#define SLJIT_SHL			24
// Flags: E
#define SLJIT_LSHR			25
// Flags: E
#define SLJIT_ASHR			26

int sljit_emit_op2(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w);

int sljit_is_fpu_available(void);

// Note: dst is the left and src is the right operand for SLJIT_FCMP
// Flags: E | U (looks like cpus prefer this way...)
#define SLJIT_FCMP			27
// Flags: -
#define SLJIT_FMOV			28
// Flags: -
#define SLJIT_FNEG			29
// Flags: -
#define SLJIT_FABS			30

int sljit_emit_fop1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw);

// Flags: -
#define SLJIT_FADD			31
// Flags: -
#define SLJIT_FSUB			32
// Flags: -
#define SLJIT_FMUL			33
// Flags: -
#define SLJIT_FDIV			34

int sljit_emit_fop2(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w);

// Label and jump instructions

struct sljit_label* sljit_emit_label(struct sljit_compiler *compiler);

// Invert conditional instruction: xor (^) with 0x1
#define SLJIT_C_EQUAL			0
#define SLJIT_C_ZERO			0
#define SLJIT_C_NOT_EQUAL		1
#define SLJIT_C_NOT_ZERO		1

#define SLJIT_C_LESS			2
#define SLJIT_C_NOT_LESS		3
#define SLJIT_C_GREATER			4
#define SLJIT_C_NOT_GREATER		5
#define SLJIT_C_SIG_LESS		6
#define SLJIT_C_SIG_NOT_LESS		7
#define SLJIT_C_SIG_GREATER		8
#define SLJIT_C_SIG_NOT_GREATER		9

#define SLJIT_C_OVERFLOW		10
#define SLJIT_C_NOT_OVERFLOW		11

#define SLJIT_JUMP			12
#define SLJIT_CALL0			13
#define SLJIT_CALL1			14
#define SLJIT_CALL2			15
#define SLJIT_CALL3			16

// The target may be redefined during runtime
#define SLJIT_LONG_JUMP			0x100

struct sljit_jump* sljit_emit_jump(struct sljit_compiler *compiler, int type);

void sljit_set_label(struct sljit_jump *jump, struct sljit_label* label);
// Only for jumps defined with SLJIT_LONG_JUMP flag
// Use sljit_emit_ijump for fixed jumps
void sljit_set_target(struct sljit_jump *jump, sljit_uw target);

// Call function or jump anywhere. Both direct and indirect form
//  type must be between SLJIT_JUMP and SLJIT_CALL3
//  Direct form: set src to SLJIT_IMM() and srcw to the address
//  Indirect form: any other valid addressing mode
int sljit_emit_ijump(struct sljit_compiler *compiler, int type, int src, sljit_w srcw);

int sljit_emit_cond_set(struct sljit_compiler *compiler, int dst, sljit_w dstw, int type);
struct sljit_const* sljit_emit_const(struct sljit_compiler *compiler, int dst, sljit_w dstw, sljit_w initval);

// After the code generation the address for label, jump and const instructions
// are computed. Since these structures are freed sljit_free_compiler, the
// addresses must be preserved by the user program elsewere
#define sljit_get_label_addr(label)	((label)->addr)
#define sljit_get_jump_addr(jump)	((jump)->addr)
#define sljit_get_const_addr(const_)	((const_)->addr)

// Only the addresses are required to rewrite the code
void sljit_set_jump_addr(sljit_uw addr, sljit_uw new_addr);
void sljit_set_const(sljit_uw addr, sljit_w new_constant);

// Portble helper function to get an offset of a member
#define SLJIT_OFFSETOF(base, member) 	((sljit_w)(&((base*)0x10)->member) - 0x10)

#endif

