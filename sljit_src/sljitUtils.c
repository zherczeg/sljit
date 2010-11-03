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

// ------------------------------------------------------------------------
//  Locks
// ------------------------------------------------------------------------

#if defined(SLJIT_EXECUTABLE_ALLOCATOR) || defined(SLJIT_UTIL_GLOBAL_LOCK)

#ifdef _WIN32

#include "windows.h"

#ifdef SLJIT_EXECUTABLE_ALLOCATOR

static HANDLE allocator_mutex = 0;

static SLJIT_INLINE void allocator_grab_lock()
{
	// No idea what to do if an error occures. Static mutexes should never fail...
	if (!allocator_mutex)
		allocator_mutex = CreateMutex(NULL, TRUE, NULL);
	else
		WaitForSingleObject(allocator_mutex, INFINITE);
}

static SLJIT_INLINE void allocator_release_lock()
{
	ReleaseMutex(allocator_mutex);
}

#endif // SLJIT_EXECUTABLE_ALLOCATOR

#ifdef SLJIT_UTIL_GLOBAL_LOCK

static HANDLE global_mutex = 0;

void SLJIT_CALL sljit_grab_lock()
{
	// No idea what to do if an error occures. Static mutexes should never fail...
	if (!global_mutex)
		global_mutex = CreateMutex(NULL, TRUE, NULL);
	else
		WaitForSingleObject(global_mutex, INFINITE);
}

void SLJIT_CALL sljit_release_lock()
{
	ReleaseMutex(global_mutex);
}

#endif // SLJIT_UTIL_GLOBAL_LOCK

#else // _WIN32

#include "pthread.h"

#ifdef SLJIT_EXECUTABLE_ALLOCATOR

static pthread_mutex_t allocator_mutex = PTHREAD_MUTEX_INITIALIZER;

static SLJIT_INLINE void allocator_grab_lock()
{
	pthread_mutex_lock(&allocator_mutex);
}

static SLJIT_INLINE void allocator_release_lock()
{
	pthread_mutex_unlock(&allocator_mutex);
}

#endif // SLJIT_EXECUTABLE_ALLOCATOR

#ifdef SLJIT_UTIL_GLOBAL_LOCK

static pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;

void SLJIT_CALL sljit_grab_lock()
{
	pthread_mutex_lock(&global_mutex);
}

void SLJIT_CALL sljit_release_lock()
{
	pthread_mutex_unlock(&global_mutex);
}

#endif // SLJIT_UTIL_GLOBAL_LOCK

#endif // _WIN32

// ------------------------------------------------------------------------
//  Stack
// ------------------------------------------------------------------------




#endif
