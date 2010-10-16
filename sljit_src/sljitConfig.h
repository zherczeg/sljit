/*
 *    Stack-less Just-In-Time compiler
 *
 *    Copyright 2009-2010 Zoltan Herczeg (hzmester@freemail.hu). All rights reserved.
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

#ifndef _SLJIT_CONFIG_H_
#define _SLJIT_CONFIG_H_

// ---------------------------------------------------------------------
//  Configuration
// ---------------------------------------------------------------------

// Architecture selection (comment out one here, use -D preprocessor
//   option, or define SLJIT_CONFIG_AUTO)
//#define SLJIT_CONFIG_X86_32
//#define SLJIT_CONFIG_X86_64
//#define SLJIT_CONFIG_ARM_V5
//#define SLJIT_CONFIG_ARM_V7
//#define SLJIT_CONFIG_ARM_THUMB2
//#define SLJIT_CONFIG_PPC_32
//#define SLJIT_CONFIG_PPC_64
//#define SLJIT_CONFIG_MIPS_32

// Auto select option (requires compiler support)
#ifdef SLJIT_CONFIG_AUTO
#ifndef _WIN32

#if defined(__i386__) || defined(__i386)
#define SLJIT_CONFIG_X86_32
#elif defined(__x86_64__)
#define SLJIT_CONFIG_X86_64
#elif defined(__arm__) || defined(__ARM__)
#if defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__)
#define SLJIT_CONFIG_ARM_V7
#elif defined(__ARM_ARCH_7__)
#define SLJIT_CONFIG_ARM_THUMB2
#else
#define SLJIT_CONFIG_ARM_V5
#endif
#elif (__ppc64__) || (__powerpc64__)
#define SLJIT_CONFIG_PPC_64
#elif defined(__ppc__) || defined(__powerpc__)
#define SLJIT_CONFIG_PPC_32
#elif (__mips__)
#define SLJIT_CONFIG_MIPS_32
#else
/* Unsupported machine */
#define SLJIT_CONFIG_UNSUPPORTED
#endif

#else // ifndef _WIN32

#if defined(_M_X64)
#define SLJIT_CONFIG_X86_64
#elif defined(_ARM_)
#define SLJIT_CONFIG_ARM_V5
#else
#define SLJIT_CONFIG_X86_32
#endif

#endif // ifndef WIN32
#endif // ifdef SLJIT_CONFIG_AUTO

// General libraries
// SLJIT is designed to be independent from them as possible
// In release (SLJIT_DEBUG not defined) mode only
// the following macros are depending from them
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// General allocation
#define SLJIT_MALLOC(size) malloc(size)
#define SLJIT_FREE(ptr) free(ptr)

// Executable code allocation
// If SLJIT_EXECUTABLE_ALLOCATOR is not defined, the application should
// define both SLJIT_MALLOC_EXEC and SLJIT_FREE_EXEC
#define SLJIT_EXECUTABLE_ALLOCATOR

#define SLJIT_MEMMOVE(dest, src, len) memmove(dest, src, len)

// Debug checks (assertions, etc)
#define SLJIT_DEBUG

// Verbose operations
#define SLJIT_VERBOSE

// Compiler specific features. Disable them if they are not supported

#ifdef __GNUC__
#define SLJIT_LIKELY(x)		__builtin_expect((x), 1)
#define SLJIT_UNLIKELY(x)	__builtin_expect((x), 0)
#else
#define SLJIT_LIKELY(x)		(x)
#define SLJIT_UNLIKELY(x)	(x)
#endif

// Inline functions
#define SLJIT_INLINE __inline
// Const variables
#define SLJIT_CONST const

// Byte type
typedef unsigned char sljit_ub;
typedef char sljit_b;

// Machine word type. Can encapsulate a pointer.
//   32 bit for 32 bit machines
//   64 bit for 64 bit machines
#if !defined(SLJIT_CONFIG_X86_64) && !defined(SLJIT_CONFIG_PPC_64)
#define SLJIT_32BIT_ARCHITECTURE	1
#define SLJIT_WORD_SHIFT		2
typedef unsigned int sljit_uw;
typedef int sljit_w;
#else
#define SLJIT_64BIT_ARCHITECTURE	1
#define SLJIT_WORD_SHIFT		3
#ifdef _WIN32
typedef unsigned __int64 sljit_uw;
typedef __int64 sljit_w;
#else
typedef unsigned long int sljit_uw;
typedef long int sljit_w;
#endif
#endif

// Double precision
#define SLJIT_FLOAT_SHIFT		3

#if defined(SLJIT_CONFIG_PPC_32) || defined(SLJIT_CONFIG_PPC_64)
#define SLJIT_BIG_ENDIAN		1
#else
#define SLJIT_LITTLE_ENDIAN		1
#endif

// ABI (Application Binary Interface) types
#ifdef SLJIT_CONFIG_X86_32

#ifdef __GNUC__
#define SLJIT_CALL __attribute__ ((fastcall))
#define SLJIT_X86_32_FASTCALL
#elif defined(_WIN32)
#define SLJIT_CALL __fastcall
#define SLJIT_X86_32_FASTCALL
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

#if defined(SLJIT_CONFIG_X86_32) || defined(SLJIT_CONFIG_X86_64)
// Turn on SSE2 support on x86 (operating on doubles)
// (If you want better performance than legacy fpu instructions)
#define SLJIT_SSE2

#ifdef SLJIT_CONFIG_X86_32
// Auto detect SSE2 support using CPUID.
// On 64 bit x86 cpus, sse2 must be present
#define SLJIT_SSE2_AUTO
#endif

#endif /* defined(SLJIT_CONFIG_X86_32) || defined(SLJIT_CONFIG_X86_64) */

#if !defined(SLJIT_CONFIG_X86_32) && !defined(SLJIT_CONFIG_X86_64)
	// Just call __ARM_NR_cacheflush on Linux
#define SLJIT_CACHE_FLUSH(from, to) \
	__clear_cache((char*)(from), (char*)(to))
#else
	// Not required to implement on archs with unified caches
#define SLJIT_CACHE_FLUSH(from, to)
#endif

#ifdef SLJIT_EXECUTABLE_ALLOCATOR
void* sljit_malloc_exec(sljit_w size);
void sljit_free_exec(void* ptr);
#define SLJIT_MALLOC_EXEC(size) sljit_malloc_exec(size)
#define SLJIT_FREE_EXEC(ptr) sljit_free_exec(ptr)
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
#define SLJIT_ASSERT_STOP() \
	do { \
		printf("Should never been reached " __FILE__ ":%d\n", __LINE__); \
		*((int*)0) = 0; \
	} while (0)
#else
#define SLJIT_ASSERT(x) \
	do { } while (0)
#define SLJIT_ASSERT_STOP() \
	do { } while (0)
#endif

#endif
