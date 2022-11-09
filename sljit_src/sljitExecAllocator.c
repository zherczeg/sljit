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

#define NO_ALLOCATOR_WORKS 1

static void sljit_update_wx_flags(void *from, void *to, sljit_s32 enable_exec);
#define SLJIT_UPDATE_WX_FLAGS(from, to, enable_exec) \
	sljit_update_wx_flags(from, to, enable_exec)

#define GENERIC_ALLOCATOR_SELECTED 2
#include "sljitGenericExecAllocator.c"

#define WX_ALLOCATOR_SELECTED 3
#include "sljitWXExecAllocator.c"

#ifndef _WIN32
#include "sljitProtExecAllocator.c"
#define PROT_ALLOCATOR_SELECTED 4
#endif /* !_WIN32 */

#define SLJIT_ADD_EXEC_OFFSET(ptr, exec_offset) ((sljit_u8 *)(ptr) + (exec_offset))

static volatile int selected_allocator = 0;

static void sljit_update_wx_flags(void *from, void *to, sljit_s32 enable_exec)
{
#if defined(__APPLE__) && defined(MAP_JIT)
	if (selected_allocator == GENERIC_ALLOCATOR_SELECTED)
		generic_allocator_apple_update_wx_flags(enable_exec);
#endif /* __APPLE__ && MAP_JIT */

	if (selected_allocator == WX_ALLOCATOR_SELECTED)
		sljit_wx_allocator_update_wx_flags(from, to, enable_exec);
}

SLJIT_API_FUNC_ATTRIBUTE void* sljit_malloc_exec(sljit_uw size)
{
	void* result;

	SLJIT_ALLOCATOR_LOCK();

	switch (selected_allocator) {
	case 0:
		/* The same allocator should work multiple times. */
		selected_allocator = GENERIC_ALLOCATOR_SELECTED;
		result = sljit_generic_allocator_malloc_exec(size);
		if (result != NULL)
			break;

		selected_allocator = WX_ALLOCATOR_SELECTED;
		result = sljit_wx_allocator_malloc_exec(size);
		if (result != NULL)
			break;

#ifndef _WIN32
		selected_allocator = PROT_ALLOCATOR_SELECTED;
		result = sljit_prot_allocator_malloc_exec(size);
		if (result != NULL)
			break;
#endif /* !_WIN32 */

		selected_allocator = NO_ALLOCATOR_WORKS;
		result = NULL;
		break;

	case GENERIC_ALLOCATOR_SELECTED:
		result = sljit_generic_allocator_malloc_exec(size);
		break;

	case WX_ALLOCATOR_SELECTED:
		result = sljit_wx_allocator_malloc_exec(size);
		break;

#ifndef _WIN32
	case PROT_ALLOCATOR_SELECTED:
		result = sljit_prot_allocator_malloc_exec(size);
		break;
#endif /* !_WIN32 */

	default:
		result = NULL;
		break;
	}

	SLJIT_ALLOCATOR_UNLOCK();
	return result;
}

SLJIT_API_FUNC_ATTRIBUTE void sljit_free_exec(void* ptr)
{
	SLJIT_ALLOCATOR_LOCK();

	switch (selected_allocator) {
	case GENERIC_ALLOCATOR_SELECTED:
		sljit_generic_allocator_free_exec(ptr);
		break;

	case WX_ALLOCATOR_SELECTED:
		sljit_wx_allocator_free_exec(ptr);
		break;

#ifndef _WIN32
	case PROT_ALLOCATOR_SELECTED:
		sljit_prot_allocator_free_exec(ptr);
		break;
#endif /* !_WIN32 */

	default:
		break;
	}

	SLJIT_ALLOCATOR_UNLOCK();
}

SLJIT_API_FUNC_ATTRIBUTE void sljit_free_unused_memory_exec(void)
{
	SLJIT_ALLOCATOR_LOCK();

	switch (selected_allocator) {
	case GENERIC_ALLOCATOR_SELECTED:
		sljit_generic_allocator_free_unused_memory_exec();
		break;

#ifndef _WIN32
	case PROT_ALLOCATOR_SELECTED:
		sljit_prot_allocator_free_unused_memory_exec();
		break;
#endif /* !_WIN32 */

	default:
		break;
	}

	SLJIT_ALLOCATOR_UNLOCK();
}

SLJIT_API_FUNC_ATTRIBUTE sljit_sw sljit_exec_offset(void* ptr)
{
#ifndef _WIN32
	if (selected_allocator == PROT_ALLOCATOR_SELECTED)
		return sljit_prot_allocator_exec_offset(ptr);
#endif /* !_WIN32 */

	return 0;
}
