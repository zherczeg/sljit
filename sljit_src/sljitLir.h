/*
 *    Stack-less Just-In-Time compiler
 *
 *    Copyright 2009-2010 Zoltan Herczeg. All rights reserved.
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
//    - Limited number of registers (only 6+4 integer registers, max 3+2
//      temporary and max 3+2 general, and 4 floating point registers)
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
#define SLJIT_SUCCESS			0
// After the call of sljit_generate_code(), the error code of the compiler
// is set to this value to avoid future sljit calls (in debug mode at least).
// The complier should be freed after sljit_generate_code()
#define SLJIT_ERR_COMPILED		1
// Cannot allocate non executable memory
#define SLJIT_ERR_ALLOC_FAILED		2
// Cannot allocate executable memory
// Only for sljit_generate_code()
#define SLJIT_ERR_EX_ALLOC_FAILED	3
// return value for SLJIT_CONFIG_UNSUPPORTED empty architecture
#define SLJIT_ERR_UNSUPPORTED		4

// ---------------------------------------------------------------------
//  Registers
// ---------------------------------------------------------------------

#define SLJIT_UNUSED		0

#define SLJIT_TEMPORARY_REG1	1
#define SLJIT_TEMPORARY_REG2	2
#define SLJIT_TEMPORARY_REG3	3
// Note: Extra Registers cannot be used for memory addressing
// Note: on x86-32, these registers are emulated (using stack loads & stores)
#define SLJIT_TEMPORARY_EREG1	4
#define SLJIT_TEMPORARY_EREG2	5

#define SLJIT_GENERAL_REG1	6
#define SLJIT_GENERAL_REG2	7
#define SLJIT_GENERAL_REG3	8
// Note: Extra Registers cannot be used for memory addressing
// Note: on x86-32, these registers are emulated (using stack loads & stores)
#define SLJIT_GENERAL_EREG1	9
#define SLJIT_GENERAL_EREG2	10

// Read-only register
// SLJIT_MEM2(SLJIT_LOCALS_REG, SLJIT_LOCALS_REG) is not supported
#define SLJIT_LOCALS_REG	11

// Number of registers
#define SLJIT_NO_TMP_REGISTERS	5
#define SLJIT_NO_GEN_REGISTERS	5
#define SLJIT_NO_REGISTERS	11

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

// SLJIT_UNUSED is not valid for floating point,
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

	// Used local registers
	int temporaries;
	// Used general registers
	int generals;
	// Local stack size
	int local_size;
	// Code size
	sljit_uw size;

#ifdef SLJIT_CONFIG_X86_32
	sljit_w args;
	sljit_w temporaries_start;
	sljit_w generals_start;
#endif

#ifdef SLJIT_CONFIG_X86_64
	int mode32;
#endif

#if defined(SLJIT_CONFIG_ARM_V5) || defined(SLJIT_CONFIG_ARM_V7)
	// Constant pool handling
	sljit_uw *cpool;
	sljit_ub *cpool_unique;
	sljit_uw cpool_diff;
	sljit_uw cpool_fill;
	// General fields
	// Contains pointer, "ldr pc, [...]" pairs
	sljit_uw patches;
	// Temporary fields
	sljit_uw shift_imm;
	int cache_arg;
	sljit_w cache_argw;
#endif

#ifdef SLJIT_CONFIG_ARM_THUMB2
	int cache_arg;
	sljit_w cache_argw;
#endif

#if defined(SLJIT_CONFIG_PPC_32) || defined(SLJIT_CONFIG_PPC_64)
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

int sljit_emit_enter(struct sljit_compiler *compiler, int args, int temporaries, int generals, int local_size);

// sljit_emit_return uses variables which are initialized by sljit_emit_enter.
// Sometimes you want to return from a jit code, which entry point is in another jit code.
// sljit_fake_enter does not emit any instruction, just initialize those variables
// Note: multiple calls of this function overwrites the previous call

void sljit_fake_enter(struct sljit_compiler *compiler, int args, int temporaries, int generals, int local_size);

// Return from jit. See below the possible values for src and srcw
int sljit_emit_return(struct sljit_compiler *compiler, int src, sljit_w srcw);

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
// For destination, you can use SLJIT_UNUSED as well
#define SLJIT_MEM		0x100
#define SLJIT_MEM0()		(SLJIT_MEM)
#define SLJIT_MEM1(r1)		(SLJIT_MEM | (r1))
#define SLJIT_MEM2(r1, r2)	(SLJIT_MEM | (r1) | ((r2) << 4))
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

// Set Equal (Zero) status flag (E)
#define SLJIT_SET_E			0x0200
// Set signed status flag (S)
#define SLJIT_SET_S			0x0400
// Set unsgined status flag (U)
#define SLJIT_SET_U			0x0800
// Set signed overflow flag (O)
#define SLJIT_SET_O			0x1000
// Set carry flag (C)
// (kinda unsigned overflow, but behaves differently on various cpus)
#define SLJIT_SET_C			0x2000

// Notes:
//   - CPU flags are NEVER set for MOV instructions
//   - you cannot postpone conditional jump instructions depending on
//     one of these instructions except if the next instruction is a MOV
//   - flag combinations: '|' means 'logical or'

// Flags: -
// Debugger instruction is not supported by all architectures
#define SLJIT_DEBUGGER			0

int sljit_emit_op0(struct sljit_compiler *compiler, int op);

// Notes for MOV instructions:
// U = Mov with update (post form). If source or destination defined as SLJIT_MEM1(r1)
//     or SLJIT_MEM2(r1, r2), r1 is increased by the sum of r2 and the constant argument
// UB = unsigned byte (8 bit)
// SB = signed byte (8 bit)
// UH = unsgined half (16 bit)
// SH = unsgined half (16 bit)

// Flags: -
#define SLJIT_MOV			1
// Flags: -
#define SLJIT_MOV_UB			2
// Flags: -
#define SLJIT_MOV_SB			3
// Flags: -
#define SLJIT_MOV_UH			4
// Flags: -
#define SLJIT_MOV_SH			5
// Flags: -
#define SLJIT_MOV_UI			6
// Flags: -
#define SLJIT_MOV_SI			7
// Flags: -
#define SLJIT_MOVU			8
// Flags: -
#define SLJIT_MOVU_UB			9
// Flags: -
#define SLJIT_MOVU_SB			10
// Flags: -
#define SLJIT_MOVU_UH			11
// Flags: -
#define SLJIT_MOVU_SH			12
// Flags: -
#define SLJIT_MOVU_UI			13
// Flags: -
#define SLJIT_MOVU_SI			14
// Flags: E
#define SLJIT_NOT			15
// Flags: E | O
#define SLJIT_NEG			16

int sljit_emit_op1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw);

// Flags: E | O | C
#define SLJIT_ADD			17
// Flags: E | O | C
#define SLJIT_ADDC			18
// Flags: E | O | C | S | U
#define SLJIT_SUB			19
// Flags: E | O | C | S | U
#define SLJIT_SUBC			20
// Note: integer mul
// Flags: -
#define SLJIT_MUL			21
// Flags: E
#define SLJIT_AND			22
// Flags: E
#define SLJIT_OR			23
// Flags: E
#define SLJIT_XOR			24
// Flags: E
#define SLJIT_SHL			25
// Flags: E
#define SLJIT_LSHR			26
// Flags: E
#define SLJIT_ASHR			27

int sljit_emit_op2(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w);

int sljit_is_fpu_available(void);

// Note: dst is the left and src is the right operand for SLJIT_FCMP
// Flags: E | U (looks like cpus prefer this way...)
#define SLJIT_FCMP			28
// Flags: -
#define SLJIT_FMOV			29
// Flags: -
#define SLJIT_FNEG			30
// Flags: -
#define SLJIT_FABS			31

int sljit_emit_fop1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw);

// Flags: -
#define SLJIT_FADD			32
// Flags: -
#define SLJIT_FSUB			33
// Flags: -
#define SLJIT_FMUL			34
// Flags: -
#define SLJIT_FDIV			35

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

// The target can be changed during runtime (see: sljit_set_jump_addr)
#define SLJIT_REWRITABLE_JUMP		0x100

// Emit a jump instruction. The destination is not set, only the type of the jump.
//  type must be between SLJIT_C_EQUAL and SLJIT_CALL3
struct sljit_jump* sljit_emit_jump(struct sljit_compiler *compiler, int type);

// Set the destination of the jump to this label
void sljit_set_label(struct sljit_jump *jump, struct sljit_label* label);
// Only for jumps defined with SLJIT_REWRITABLE_JUMP flag
// Note: Use sljit_emit_ijump for fixed jumps
void sljit_set_target(struct sljit_jump *jump, sljit_uw target);

// Call function or jump anywhere. Both direct and indirect form
//  type must be between SLJIT_JUMP and SLJIT_CALL3
//  Direct form: set src to SLJIT_IMM() and srcw to the address
//  Indirect form: any other valid addressing mode
int sljit_emit_ijump(struct sljit_compiler *compiler, int type, int src, sljit_w srcw);

// Set dst to 1 if condition is fulfilled, 0 otherwise
//  type must be between SLJIT_C_EQUAL and SLJIT_C_NOT_OVERFLOW
int sljit_emit_cond_set(struct sljit_compiler *compiler, int dst, sljit_w dstw, int type);

// The constant can be changed runtime (see: sljit_set_const)
struct sljit_const* sljit_emit_const(struct sljit_compiler *compiler, int dst, sljit_w dstw, sljit_w initval);

// After the code generation the address for label, jump and const instructions
// are computed. Since these structures are freed sljit_free_compiler, the
// addresses must be preserved by the user program elsewere
#define sljit_get_label_addr(label)	((label)->addr)
#define sljit_get_jump_addr(jump)	((jump)->addr)
#define sljit_get_const_addr(const_)	((const_)->addr)

// Only the address is required to rewrite the code
void sljit_set_jump_addr(sljit_uw addr, sljit_uw new_addr);
void sljit_set_const(sljit_uw addr, sljit_w new_constant);

// Portble helper function to get an offset of a member
#define SLJIT_OFFSETOF(base, member) 	((sljit_w)(&((base*)0x10)->member) - 0x10)

// Get the entry address of a given function
#ifndef SLJIT_INDIRECT_CALL
#define SLJIT_FUNC_OFFSET(func_name)	((sljit_w)func_name)
#else
#define SLJIT_FUNC_OFFSET(func_name)	((sljit_w)*(void**)func_name)
#endif

#endif

