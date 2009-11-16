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

#ifndef _SLJIT_CONFIG_H_
#define _SLJIT_CONFIG_H_

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
/* Unsupported machine */
#define SLJIT_CONFIG_UNSUPPORTED
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
#if !defined(SLJIT_CONFIG_X86_64) && !defined(SLJIT_CONFIG_PPC_64)

#define SLJIT_MALLOC_EXEC(size) malloc(size)
#define SLJIT_FREE_EXEC(ptr) free(ptr)

#else

// We use mmap on x86-64 and PPC-64, but that
// is not OS independent standard C function
#define SLJIT_MALLOC_EXEC(size) sljit_malloc_exec(size)
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

#if defined(SLJIT_CONFIG_PPC_32) || defined(SLJIT_CONFIG_PPC_64)
#define SLJIT_BIG_ENDIAN		1
#else
#define SLJIT_LITTLE_ENDIAN		1
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

#ifdef SLJIT_CONFIG_ARM
	// Just call __ARM_NR_cacheflush on Linux
#define SLJIT_CACHE_FLUSH(from, to) \
	__clear_cache((char*)(from), (char*)(to))
#else
	// TODO: PPC/PPC-64 requires an implemetation
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

