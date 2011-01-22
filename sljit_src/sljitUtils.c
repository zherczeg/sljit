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

static SLJIT_INLINE void allocator_grab_lock(void)
{
	// No idea what to do if an error occures. Static mutexes should never fail...
	if (!allocator_mutex)
		allocator_mutex = CreateMutex(NULL, TRUE, NULL);
	else
		WaitForSingleObject(allocator_mutex, INFINITE);
}

static SLJIT_INLINE void allocator_release_lock(void)
{
	ReleaseMutex(allocator_mutex);
}

#endif // SLJIT_EXECUTABLE_ALLOCATOR

#ifdef SLJIT_UTIL_GLOBAL_LOCK

static HANDLE global_mutex = 0;

void SLJIT_CALL sljit_grab_lock(void)
{
	// No idea what to do if an error occures. Static mutexes should never fail...
	if (!global_mutex)
		global_mutex = CreateMutex(NULL, TRUE, NULL);
	else
		WaitForSingleObject(global_mutex, INFINITE);
}

void SLJIT_CALL sljit_release_lock(void)
{
	ReleaseMutex(global_mutex);
}

#endif // SLJIT_UTIL_GLOBAL_LOCK

#else // _WIN32

#include "pthread.h"

#ifdef SLJIT_EXECUTABLE_ALLOCATOR

static pthread_mutex_t allocator_mutex = PTHREAD_MUTEX_INITIALIZER;

static SLJIT_INLINE void allocator_grab_lock(void)
{
	pthread_mutex_lock(&allocator_mutex);
}

static SLJIT_INLINE void allocator_release_lock(void)
{
	pthread_mutex_unlock(&allocator_mutex);
}

#endif // SLJIT_EXECUTABLE_ALLOCATOR

#ifdef SLJIT_UTIL_GLOBAL_LOCK

static pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;

void SLJIT_CALL sljit_grab_lock(void)
{
	pthread_mutex_lock(&global_mutex);
}

void SLJIT_CALL sljit_release_lock(void)
{
	pthread_mutex_unlock(&global_mutex);
}

#endif // SLJIT_UTIL_GLOBAL_LOCK

#endif // _WIN32

// ------------------------------------------------------------------------
//  Stack
// ------------------------------------------------------------------------

#ifdef SLJIT_UTIL_STACK

#ifdef _WIN32
#include "windows.h"
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

// Planning to make it even more clever in the future
static sljit_w sljit_page_align = 0;

#ifdef _WIN32
static SLJIT_INLINE sljit_w get_win32_page_size(void)
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	return si.dwPageSize;
}
#endif

struct sljit_stack* SLJIT_CALL sljit_allocate_stack(sljit_w limit, sljit_w max_limit)
{
	struct sljit_stack* stack;

	if (limit > max_limit)
		return NULL;

#ifdef _WIN32
	if (!sljit_page_align)
		sljit_page_align = get_win32_page_size();
#else
	if (!sljit_page_align) {
		sljit_page_align = sysconf(_SC_PAGESIZE) - 1;
		// Should never happen
		if (sljit_page_align < 0)
			sljit_page_align = 4095;
	}
#endif

	// Align limit and max_limit
	max_limit = (max_limit + sljit_page_align) & ~sljit_page_align;

	stack = (struct sljit_stack*)SLJIT_MALLOC(sizeof(struct sljit_stack));
	if (!stack)
		return NULL;

#ifdef _WIN32
	stack->base = (sljit_w)VirtualAlloc(0, max_limit, MEM_RESERVE, PAGE_READWRITE);
	if (!stack->base) {
		SLJIT_FREE(stack);
		return NULL;
	}
	stack->limit = stack->base;
	stack->max_limit = stack->base + max_limit;
	if (sljit_stack_resize(stack, stack->base + limit)) {
		sljit_free_stack(stack);
		return NULL;
	}
#else
	stack->base = (sljit_w)mmap(0, max_limit, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	if (stack->base == (sljit_w)MAP_FAILED) {
		SLJIT_FREE(stack);
		return NULL;
	}
	stack->limit = stack->base + limit;
	stack->max_limit = stack->base + max_limit;
#endif
	stack->top = stack->base;
	return stack;
}

#undef PAGE_ALIGN

void SLJIT_CALL sljit_free_stack(struct sljit_stack* stack)
{
#ifdef _WIN32
	VirtualFree((void*)stack->base, 0, MEM_RELEASE);
#else
	munmap((void*)stack->base, stack->max_limit - stack->base);
#endif
	SLJIT_FREE(stack);
}

sljit_w SLJIT_CALL sljit_stack_resize(struct sljit_stack* stack, sljit_w new_limit)
{
	sljit_w aligned_old_limit;
	sljit_w aligned_new_limit;

	if ((new_limit > stack->max_limit) || (new_limit < stack->base))
		return -1;
#ifdef _WIN32
	aligned_new_limit = (new_limit + sljit_page_align) & ~sljit_page_align;
	aligned_old_limit = (stack->limit + sljit_page_align) & ~sljit_page_align;
	if (aligned_new_limit != aligned_old_limit) {
		if (aligned_new_limit > aligned_old_limit) {
			if (!VirtualAlloc((void*)aligned_old_limit, aligned_new_limit - aligned_old_limit, MEM_COMMIT, PAGE_READWRITE))
				return -1;
		}
		else {
			if (!VirtualFree((void*)aligned_new_limit, aligned_old_limit - aligned_new_limit, MEM_DECOMMIT))
				return -1;
		}
	}
	stack->limit = new_limit;
	return 0;
#else
	if (new_limit >= stack->limit) {
		stack->limit = new_limit;
		return 0;
	}
	aligned_new_limit = (new_limit + sljit_page_align) & ~sljit_page_align;
	aligned_old_limit = (stack->limit + sljit_page_align) & ~sljit_page_align;
	if (aligned_new_limit < aligned_old_limit)
		madvise((void*)aligned_new_limit, aligned_old_limit - aligned_new_limit, MADV_DONTNEED);
	stack->limit = new_limit;
	return 0;
#endif
}

#endif // SLJIT_UTIL_STACK

#endif
