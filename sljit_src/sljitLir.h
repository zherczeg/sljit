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
//    - Target of (conditional) jump and call instructions can be
//      dynamically modified during the execution of the code
//    - Constants can be modified during runtime (after code generation)
//    - A fixed stack space can be allocated for local variables
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
//      - mainly position independent code except for far jumps
//        (Is it worth to change it into real PIC in the future?)
//    - Optimizations (perhaps later)
//      - Only for basic blocks (no labels between LIR instructions)

// ---------------------------------------------------------------------
//  Configuration
// ---------------------------------------------------------------------

// Architecture selection (comment out one here, use -D preprocessor
//   option, or define SLJIT_CONFIG_AUTO)
//#define SLJIT_CONFIG_X86_32
//#define SLJIT_CONFIG_X86_64
//#define SLJIT_CONFIG_ARM
//#define SLJIT_CONFIG_PPC_32
//#define SLJIT_CONFIG_PPC_64

// Auto select option (requires compiler support)
#ifdef SLJIT_CONFIG_AUTO
#ifndef WIN32

#if defined(__i386__) || defined(__i386)
#define SLJIT_CONFIG_X86_32
#elif defined(__x86_64__)
#define SLJIT_CONFIG_X86_64
#elif defined(__arm__) || defined(__ARM__)
#define SLJIT_CONFIG_ARM
#elif (__ppc64__) || (__powerpc64__)
#define SLJIT_CONFIG_PPC_64
#elif defined(__ppc__) || defined(__powerpc__)
#define SLJIT_CONFIG_PPC_32
#else
#error "Unsupported machine"
#endif

#else // ifndef WIN32

#if defined(_M_X64)
#define SLJIT_CONFIG_X86_64
#elif defined(_ARM_)
#define SLJIT_CONFIG_ARM
#else
#define SLJIT_CONFIG_X86_32
#endif

#endif // ifndef WIN32
#endif // ifdef SLJIT_CONFIG_AUTO

// General libraries
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// General allocation
#define SLJIT_MALLOC(size) malloc(size)
#define SLJIT_FREE(ptr) free(ptr)

// Executable code allocation
#if !defined(SLJIT_CONFIG_X86_64) && !defined(SLJIT_CONFIG_PPC_64)

#define SLJIT_MALLOC_EXEC(size) malloc(size)
#define SLJIT_FREE_EXEC(ptr) free(ptr)

#else

// We use mmap on x86-64, but that is not OS independent standard C function
void* sljit_malloc_exec(int size);
#define SLJIT_MALLOC_EXEC(size) sljit_malloc_exec(size)
void sljit_free_exec(void* ptr);
#define SLJIT_FREE_EXEC(ptr) sljit_free_exec(ptr)

#endif

#define SLJIT_MEMMOVE(dest, src, len) memmove(dest, src, len)

// Debug checks (assertions, etc)
#define SLJIT_DEBUG

// Verbose operations
#define SLJIT_VERBOSE

// Inline functions
#define INLINE __inline

// Byte type
typedef unsigned char sljit_ub;
typedef char sljit_b;

// Machine word type. Can encapsulate a pointer.
//   32 bit for 32 bit machines
//   64 bit for 64 bit machines
#if !defined(SLJIT_CONFIG_X86_64) && !defined(SLJIT_CONFIG_PPC_64)
#define SLJIT_32BIT_ARCHITECTURE	1
typedef unsigned int sljit_uw;
typedef int sljit_w;
#else
#define SLJIT_64BIT_ARCHITECTURE	1
typedef unsigned long int sljit_uw;
typedef long int sljit_w;
#endif

// ABI (Application Binary Interface) types
#ifdef SLJIT_CONFIG_X86_32

#ifdef __GNUC__
#define SLJIT_CALL __attribute__ ((stdcall))
#else
#define SLJIT_CALL __stdcall
#endif

#else // Other architectures

#define SLJIT_CALL

#endif

#if defined(SLJIT_CONFIG_PPC_64)
// It seems ppc64 compilers use an indirect addressing for functions
// I don't know why... It just makes things complicated
#define SLJIT_INDIRECT_CALL
#endif

#ifdef SLJIT_CONFIG_ARM
	// Just call __ARM_NR_cacheflush on Linux
#define SLJIT_CACHE_FLUSH(from, to) \
	__clear_cache((char*)(from), (char*)(to))
#else
	// Not required to implement on archs with unified caches
#define SLJIT_CACHE_FLUSH(from, to)
#endif

// Feel free to overwrite these assert defines
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

#define SLJIT_PREF_MUL_DST	SLJIT_TEMPORARY_REG1
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
	sljit_uw cache_arg;
	sljit_uw cache_argw;
#endif

#ifdef SLJIT_CONFIG_PPC_64
	int mode32;
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

int sljit_emit_enter(struct sljit_compiler *compiler, int args, int general, int local_size);

// sljit_emit_return uses variables which are initialized by sljit_emit_enter.
// Sometimes you want to return from a jit code, which entry point is in another jit code.
// sljit_fake_enter does not emit any instruction, just initialize those variables

void sljit_fake_enter(struct sljit_compiler *compiler, int args, int general, int local_size);

// Return from jit. Return with NO_REG or a register
int sljit_emit_return(struct sljit_compiler *compiler, int reg);

// Source and destination values for arithmetical instructions
//  imm           - a simple immediate value (cannot be used as a destination)
//  reg           - any of the registers (immediate argument unused)
//  [imm]         - absolute immediate memory address
//  [reg+imm]     - indirect memory address
//  [reg+reg+imm] - two level indirect addressing

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
#define SLJIT_INT_OPERATION		0x100

// Common CPU status flags for all architectures (x86, ARM, PPC)
//  - carry flag
//  - overflow flag
//  - zero flag
//  - negative/positive flag (depends on arc)

// By default, the instructions may, or may not set the CPU status flags.
// Using this option, we can force the compiler to generate instructions,
// which also set the CPU status flags. Omit this option if you do not
// want to branch after the instruction, since the compiler may generate
// faster code
#define SLJIT_SET_FLAGS			0x200

// Notes:
//  - you cannot postpone conditional jump instructions depending on
//    one of these instructions except if the next instruction is a mov*
//  - the carry flag set/reset depends on arch
//    i.e: x86 sub operation sets carry, if substraction overflows, while
//    arm sub sets carry, if the substraction does NOT overflow. I suggest
//    to use carry mainly for addc and subc operations

// CPU flags are NEVER set for MOV instructions
// U = Mov with update. If source or destination uses the form of
// [reg + (expr)] the reg is increased by (expr)
// UB = unsigned byte (8 bit)
// SB = signed byte (8 bit)
// UH = unsgined half (16 bit)
// SH = unsgined half (16 bit)
#define SLJIT_MOV			0
#define SLJIT_MOV_UB			1
#define SLJIT_MOV_SB			2
#define SLJIT_MOV_UH			3
#define SLJIT_MOV_SH			4
#define SLJIT_MOV_UI			5
#define SLJIT_MOV_SI			6
#define SLJIT_MOVU			7
#define SLJIT_MOVU_UB			8
#define SLJIT_MOVU_SB			9
#define SLJIT_MOVU_UH			10
#define SLJIT_MOVU_SH			11
#define SLJIT_MOVU_UI			12
#define SLJIT_MOVU_SI			13
#define SLJIT_NOT			14
#define SLJIT_NEG			15

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

int sljit_is_fpu_available(void);

// dst is source in case of FCMP
// SLJIT_FCMP is not yet implemented !
#define SLJIT_FCMP			0
#define SLJIT_FMOV			1
#define SLJIT_FNEG			2
#define SLJIT_FABS			3

int sljit_emit_fop1(struct sljit_compiler *compiler, int op,
	int dst, sljit_w dstw,
	int src, sljit_w srcw);

#define SLJIT_FADD			0
#define SLJIT_FSUB			1
#define SLJIT_FMUL			2
#define SLJIT_FDIV			3

int sljit_emit_fop2(struct sljit_compiler *compiler, int op,
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
#define SLJIT_C_SIG_NOT_GREATER		9

#define SLJIT_C_CARRY			10
#define SLJIT_C_NOT_CARRY		11
#define SLJIT_C_ZERO			12
#define SLJIT_C_NOT_ZERO		13
#define SLJIT_C_OVERFLOW		14
#define SLJIT_C_NOT_OVERFLOW		15

#define SLJIT_JUMP			16
#define SLJIT_CALL0			17
#define SLJIT_CALL1			18
#define SLJIT_CALL2			19
#define SLJIT_CALL3			20

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
#define sljit_get_label_addr(label)	(label->addr)
#define sljit_get_jump_addr(jump)	(jump->addr)
#define sljit_get_const_addr(const_)	(const_->addr)

// Only the addresses are required to rewrite the code
void sljit_set_jump_addr(sljit_uw addr, sljit_uw new_addr);
void sljit_set_const(sljit_uw addr, sljit_w constant);

#endif

