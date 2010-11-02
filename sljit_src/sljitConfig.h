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
//  Architecture
// ---------------------------------------------------------------------

// Architecture selection
// #define SLJIT_CONFIG_X86_32
// #define SLJIT_CONFIG_X86_64
// #define SLJIT_CONFIG_ARM_V5
// #define SLJIT_CONFIG_ARM_V7
// #define SLJIT_CONFIG_ARM_THUMB2
// #define SLJIT_CONFIG_PPC_32
// #define SLJIT_CONFIG_PPC_64
// #define SLJIT_CONFIG_MIPS_32

// #define SLJIT_CONFIG_AUTO

// ---------------------------------------------------------------------
//  Utilities
// ---------------------------------------------------------------------

// All user pointers are initialized to NULL
// User pointers are allowed for sljit_label
#define SLJIT_UTIL_LABEL_USER
// User pointers are allowed for sljit_jump
#define SLJIT_UTIL_JUMP_USER
// User pointers are allowed for sljit_const
#define SLJIT_UTIL_CONST_USER

// ---------------------------------------------------------------------
//  Configuration
// ---------------------------------------------------------------------

// If SLJIT_STD_MACROS_DEFINED is not defined, the application should
// define SLJIT_MALLOC, SLJIT_FREE, SLJIT_MEMMOVE, and NULL
// #define SLJIT_STD_MACROS_DEFINED

// Executable code allocation
// If SLJIT_EXECUTABLE_ALLOCATOR is not defined, the application should
// define both SLJIT_MALLOC_EXEC and SLJIT_FREE_EXEC
#define SLJIT_EXECUTABLE_ALLOCATOR

// Debug checks (assertions, etc)
#define SLJIT_DEBUG

// Verbose operations
#define SLJIT_VERBOSE

// If SLJIT_HAVE_LIKELY is not defined, the application should
// define both SLJIT_LIKELY and SLJIT_UNLIKELY
// #define SLJIT_HAVE_LIKELY

// If SLJIT_C_DEFINES is not defined, the application should
// define SLJIT_INLINE and SLJIT_CONST
// #define SLJIT_C_DEFINES

// If SLJIT_HAVE_CACHE_FLUSH is not defined, the application should
// define SLJIT_CACHE_FLUSH
// #define SLJIT_HAVE_CACHE_FLUSH

// See the beginning of sljitConfigInternal.h

#endif
