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

// ---------------------------------------------------------------------
//  Stack-Less JIT compiler for multiple platforms (x86, ARM, powerPC)
// ---------------------------------------------------------------------
//
// Short description of the general approach
//  Advantages:
//    - The execution can be continued from any LIR instruction
//      In other words, jump into and out of the code is safe
//    - Target of (conditional) jump and call instructions can be
//      dynamically modified during the execution of the code
//    - Constants can be modified during the execution of the code
//  Disadvantages:
//    - Limited number of registers (only 6, max 3 temporary and max 3 general)
//    - Variables cannot be stored temporarily on the stack
//  In practice:
//    - This approach is very effective for interpreters
//      - One of the general registers tipically points to a stack interface
//      - It can jump to any exception handler anytime (even for another
//        function. It is safe for SLJIT.)
//      - Fast paths can be modified runtime reflecting the fastest execution
//        way of the dynamic language
//      - SLJIT supports complex memory addressing modes
//    - Optimizations (later)
//      - Only for basic blocks

// ---------------------------------------------------------------------
//  Configuration
// ---------------------------------------------------------------------

// Architecture selection
#define SLJIT_CONFIG_X86_32
//#define SLJIT_CONFIG_ARM

// General libraries
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define SLJIT_MALLOC(size) malloc(size)
#define SLJIT_FREE(ptr) free(ptr)

#define SLJIT_MEMMOVE(dest, src, len) memmove(dest, src, len)

// Debug
#define SLJIT_DEBUG

// Verbose operations
#define SLJIT_VERBOSE

#define INLINE __inline

// Byte structure
typedef unsigned char sljit_ub;
typedef char sljit_b;
// Machine word structure. Can encapsulate a pointer.
//   32 bit for 32 bit machines
//   64 bit for 64 bit machines
typedef unsigned int sljit_uw;
typedef int sljit_w;

// Machine independent types
typedef unsigned short sljit_u16b;
typedef short sljit_16b;
typedef unsigned int sljit_u32b;
typedef int sljit_32b;

#ifdef SLJIT_DEBUG
#define SLJIT_ASSERT(x) \
	do { \
		if (!(x)) { \
			printf("Assertion failed at " __FILE__ ":%d\n", __LINE__); \
			*((int*)0) = 0; \
		} \
	} while (0)
#define SLJIT_ASSERT_IMPOSSIBLE() \
	do { \
		printf("Should never been reached " __FILE__ ":%d\n", __LINE__); \
		*((int*)0) = 0; \
	} while (0)
#else
#define SLJIT_ASSERT(x) \
	do { } while (0)
#define SLJIT_ASSERT_IMPOSSIBLE() \
	do { } while (0)
#endif

// ABI (Application Binary Interface) types
#ifdef __GNUC__
#define SLJIT_CDECL __attribute__ ((cdecl))
#else
#define SLJIT_CDECL __cdecl
#endif

#ifdef __GNUC__
#define SLJIT_FASTCALL __attribute__ ((fastcall))
#else
#define SLJIT_FASTCALL __fastcall
#endif

// ---------------------------------------------------------------------
//  Error codes
// ---------------------------------------------------------------------

#define SLJIT_NO_ERROR		0
#define SLJIT_CODE_GENERATED	1
#define SLJIT_MEMORY_ERROR	2

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

#define SLJIT_NO_TMP_REGISTERS	3
#define SLJIT_NO_GEN_REGISTERS	3
#define SLJIT_NO_REGISTERS	6

// Return with machine word or double machine word

#define SLJIT_PREF_RET_REG	SLJIT_TEMPORARY_REG1
#define SLJIT_PREF_RET_HIREG	SLJIT_TEMPORARY_REG3

// x86 prefers temporary registers for special purposes. If not these
// registers are used, it costs a little performance drawback. It doeasn't
// matter for other archs

#define SLJIT_PREF_MUL_BASE	SLJIT_TEMPORARY_REG1
#define SLJIT_PREF_MUL_WITH	SLJIT_TEMPORARY_REG3
#define SLJIT_PREF_SHIFT_REG	SLJIT_TEMPORARY_REG2

// ---------------------------------------------------------------------
//  Main functions
// ---------------------------------------------------------------------

struct sljit_memory_fragment {
	struct sljit_memory_fragment *next;
	int used_size;
	sljit_ub memory[1];
};

struct sljit_label {
	struct sljit_label *next;
	sljit_uw addr;
	int size;
};

struct sljit_jump {
	struct sljit_jump *next;
	int flags;
	sljit_uw addr;
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
	int size;

#ifdef SLJIT_VERBOSE
	FILE* verbose;
#endif
};

// Creates an sljit compiler.
// Returns NULL if failed
struct sljit_compiler* sljit_create_compiler(void);
// Free everything except the codes
void sljit_free_compiler(struct sljit_compiler *compiler);

#ifdef SLJIT_VERBOSE
// NULL = no verbose
void sljit_compiler_verbose(struct sljit_compiler *compiler, FILE* verbose);
#endif

void* sljit_generate_code(struct sljit_compiler *compiler);
void sljit_free_code(void* code);

// Instruction generation. Returns with error code

// Entry instruction. The instruction has "args" number of arguments
// and will use the first "general" number of general registers.
// Therefore, "args" must be less or equal than "general"

#define CALL_TYPE_CDECL		0
#define CALL_TYPE_FASTCALL	1

int sljit_emit_enter(struct sljit_compiler *compiler, int type, int args, int general);

// Return from jit. Return with NO_REG or a register
int sljit_emit_return(struct sljit_compiler *compiler, int reg);

// Source and destination values for arithmetical instructions
//  imm           - a simple immediate value (cannot be destination)
//  reg           - any of the registers (immediate argument unused)
//  [imm]         - absolute immediate memory address
//  [reg+imm]     - indirect memory address
//  [reg+reg+imm] - two level indirect addressing

#define SLJIT_LOAD_FLAG		0x100
#define SLJIT_IMM_FLAG		0x200

// Helper defines
// Register output: simply the name of the register
// For destination, you can use SLJIT_NO_REG as well
#define SLJIT_IMM		SLJIT_IMM_FLAG
#define SLJIT_MEM0()		SLJIT_LOAD_FLAG
#define SLJIT_MEM1(r1)		SLJIT_LOAD_FLAG | (r1)
#define SLJIT_MEM2(r1, r2)	SLJIT_LOAD_FLAG | (r1) | ((r2) << 4)

// Most of the following instructions are also set the CPU status-flags
// Common flags for all architectures (x86, ARM, PPC)
//  - carry flag
//  - overflow flag
//  - zero flag
//  - negative/positive flag (depends on arc)
// Note:
//  - instructions which are not set status flags are marked with asterisk (*)
//  - no perf penalty for setting the CPU status flags
//  - you cannot postpone conditional jump instructions depending on
//    one of these instructions

// * - CPU flags not set
#define SLJIT_MOV			0
// * - CPU flags not set
#define SLJIT_NOT			1
#define SLJIT_NEG			2

int sljit_emit_op1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw);

#define SLJIT_ADD			0
#define SLJIT_ADDC			1
#define SLJIT_SUB			2
#define SLJIT_SUBC			3
#define SLJIT_MUL			4
#define SLJIT_AND			5
#define SLJIT_OR			6
#define SLJIT_XOR			7
#define SLJIT_SHL			8
#define SLJIT_LSHR			9
#define SLJIT_ASHR			10

int sljit_emit_op2(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src1, sljit_w src1w,
	int src2, sljit_w src2w);

// Label and jump instructions

struct sljit_label* sljit_emit_label(struct sljit_compiler *compiler);

// Invert conditional instruction: xor (^) with 0x1
#define SLJIT_C_EQUAL			0
#define SLJIT_C_NOT_EQUAL		1
#define SLJIT_C_LESS			2
#define SLJIT_C_NOT_LESS		3
#define SLJIT_C_GREATER			4
#define SLJIT_C_NOT_GREATER		5
#define SLJIT_C_SIG_LESS		6
#define SLJIT_C_SIG_NOT_LESS		7
#define SLJIT_C_SIG_GREATER		8
#define SLJIT_C_SIG_NOT_GREATER	9

#define SLJIT_C_CARRY			10
#define SLJIT_C_NOT_CARRY		11
#define SLJIT_C_ZERO			12
#define SLJIT_C_NOT_ZERO		13
#define SLJIT_C_OVERFLOW		14
#define SLJIT_C_NOT_OVERFLOW		15

#define SLJIT_JUMP			16

// The target can be redefined later
#define SLJIT_LONG_JUMP			0x100

struct sljit_jump* sljit_emit_jump(struct sljit_compiler *compiler, int type);
void sljit_set_label(struct sljit_jump *jump, struct sljit_label* label);
void sljit_set_target(struct sljit_jump *jump, sljit_uw target);

int sljit_emit_cond_set(struct sljit_compiler *compiler, int dst, sljit_w dstw, int type);
struct sljit_const* sljit_emit_const(struct sljit_compiler *compiler, int dst, sljit_w dstw, sljit_w constant);

// After the code generation the address for label, jump and const instructions
// are computed. Since these structures are freed sljit_free_compiler, the
// addresses must be preserved by the user program elsewere
#define sljit_get_label_addr(label)	(label->addr)
#define sljit_get_jump_addr(jump)	(jump->addr)
#define sljit_get_const_addr(const_)	(const_->addr)

// Only the addresses are required to rewrite the code
void sljit_set_jump_addr(sljit_uw addr, sljit_uw new_addr);
void sljit_set_const(sljit_uw addr, sljit_w constant);

#endif

