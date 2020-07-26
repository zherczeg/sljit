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

/*
   This file contains a simple W^X executable memory allocator for POSIX systems.

   It allocates a separate block for each code block which may waste a lot of memory.
*/

#include <sys/mman.h>

#ifdef __NetBSD__
#if defined(PROT_MPROTECT)
#define SLJIT_PROT_WX PROT_MPROTECT(PROT_EXEC)
#else
#define SLJIT_PROT_WX 0
#endif /* PROT_MPROTECT */
#elif defined(__FreeBSD__)
#if defined(PROT_MAX)
#define SLJIT_PROT_WX PROT_MAX(PROT_READ | PROT_WRITE | PROT_EXEC)
#else
#define SLJIT_PROT_WX 0
#endif
#else /* POSIX */
#define SLJIT_PROT_WX 0
#endif /* NetBSD */

SLJIT_API_FUNC_ATTRIBUTE void* sljit_malloc_exec(sljit_uw size)
{
	sljit_uw* ptr;
	sljit_uw page_mask = (sljit_uw)get_page_alignment();

	size = (size + sizeof(sljit_uw) + page_mask) & ~page_mask;
	ptr = (sljit_uw*)mmap(NULL, size, PROT_READ | PROT_WRITE | SLJIT_PROT_WX,
				MAP_PRIVATE | MAP_ANON, -1, 0);

	if (ptr == MAP_FAILED) {
		return NULL;
	}

	*ptr = size;
	return ptr + 1;
}

#undef SLJIT_PROT_WX

SLJIT_API_FUNC_ATTRIBUTE void sljit_free_exec(void* ptr)
{
	sljit_uw *start_ptr = ((sljit_uw*)ptr) - 1;
	munmap(start_ptr, *start_ptr);
}

#define SLJIT_UPDATE_WX_FLAGS(from, to, enable_exec) \
	sljit_update_wx_flags((from), (to), (enable_exec))

static void sljit_update_wx_flags(void *from, void *to, sljit_s32 enable_exec)
{
	sljit_uw page_mask = (sljit_uw)get_page_alignment();

	sljit_uw start = (sljit_uw)from;
	sljit_uw end = (sljit_uw)to;
	int prot = PROT_READ | (enable_exec ? PROT_EXEC : PROT_WRITE);

	SLJIT_ASSERT (start < end);

	start &= ~page_mask;
	end = (end + page_mask) & ~page_mask;

	mprotect((void*)start, end - start, prot);
}

SLJIT_API_FUNC_ATTRIBUTE void sljit_free_unused_memory_exec(void)
{
	/* This allocator does not keep unused memory for future allocations. */
}
