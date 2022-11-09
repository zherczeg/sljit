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
   This file contains a simple executable memory allocator

   It is assumed, that executable code blocks are usually medium (or sometimes
   large) memory blocks, and the allocator is not too frequently called (less
   optimized than other allocators). Thus, using it as a generic allocator is
   not suggested.

   How does it work:
     Memory is allocated in continuous memory areas called chunks by alloc_chunk()
     Chunk format:
     [ block ][ block ] ... [ block ][ block terminator ]

   All blocks and the block terminator is started with block_header. The block
   header contains the size of the previous and the next block. These sizes
   can also contain special values.
     Block size:
       0 - The block is a free_block, with a different size member.
       1 - The block is a block terminator.
       n - The block is used at the moment, and the value contains its size.
     Previous block size:
       0 - This is the first block of the memory chunk.
       n - The size of the previous block.

   Using these size values we can go forward or backward on the block chain.
   The unused blocks are stored in a chain list pointed by free_blocks. This
   list is useful if we need to find a suitable memory area when the allocator
   is called.

   When a block is freed, the new free block is connected to its adjacent free
   blocks if possible.

     [ free block ][ used block ][ free block ]
   and "used block" is freed, the three blocks are connected together:
     [           one big free block           ]
*/

/* --------------------------------------------------------------------- */
/*  System (OS) functions                                                */
/* --------------------------------------------------------------------- */

/* 64 KByte. */
#define GENERIC_ALLOCATOR_CHUNK_SIZE	(sljit_uw)0x10000u

/*
   alloc_chunk / free_chunk :
     * allocate executable system memory chunks
     * the size is always divisible by GENERIC_ALLOCATOR_CHUNK_SIZE
   SLJIT_ALLOCATOR_LOCK / SLJIT_ALLOCATOR_UNLOCK :
     * provided as part of sljitUtils
     * only the allocator requires this lock, sljit is fully thread safe
       as it only uses local variables
*/

#ifdef _WIN32

static SLJIT_INLINE void* generic_allocator_alloc_chunk(sljit_uw size)
{
	return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
}

static SLJIT_INLINE void generic_allocator_free_chunk(void *chunk, sljit_uw size)
{
	SLJIT_UNUSED_ARG(size);
	VirtualFree(chunk, 0, MEM_RELEASE);
}

#else /* POSIX */

#if defined(__APPLE__) && defined(MAP_JIT)
/*
   On macOS systems, returns MAP_JIT if it is defined _and_ we're running on a
   version where it's OK to have more than one JIT block or where MAP_JIT is
   required.
   On non-macOS systems, returns MAP_JIT if it is defined.
*/
#include <TargetConditionals.h>
#if TARGET_OS_OSX
#if defined SLJIT_CONFIG_X86 && SLJIT_CONFIG_X86
#ifdef MAP_ANON
#include <sys/utsname.h>
#include <stdlib.h>

#define GENERIC_ALLOCATOR_MAP_JIT	(generic_allocator_get_map_jit_flag())

static SLJIT_INLINE int generic_allocator_get_map_jit_flag()
{
	size_t page_size;
	void *ptr;
	struct utsname name;
	static int map_jit_flag = -1;

	if (map_jit_flag < 0) {
		map_jit_flag = 0;
		uname(&name);

		/* Kernel version for 10.14.0 (Mojave) or later */
		if (atoi(name.release) >= 18) {
			page_size = get_page_alignment() + 1;
			/* Only use MAP_JIT if a hardened runtime is used */
			ptr = mmap(NULL, page_size, PROT_WRITE | PROT_EXEC,
					MAP_PRIVATE | MAP_ANON, -1, 0);

			if (ptr != MAP_FAILED)
				munmap(ptr, page_size);
			else
				map_jit_flag = MAP_JIT;
		}
	}
	return map_jit_flag;
}
#endif /* MAP_ANON */
#else /* !SLJIT_CONFIG_X86 */
#if !(defined SLJIT_CONFIG_ARM && SLJIT_CONFIG_ARM)
#error "Unsupported architecture"
#endif /* SLJIT_CONFIG_ARM */
#include <AvailabilityMacros.h>
#include <pthread.h>

#define GENERIC_ALLOCATOR_MAP_JIT	(MAP_JIT)

#if (defined SLJIT_GENERIC_EXECUTABLE_ALLOCATOR && SLJIT_GENERIC_EXECUTABLE_ALLOCATOR)
#define SLJIT_UPDATE_WX_FLAGS(from, to, enable_exec) \
	generic_allocator_apple_update_wx_flags(enable_exec)
#endif /* SLJIT_GENERIC_EXECUTABLE_ALLOCATOR */

static SLJIT_INLINE void generic_allocator_apple_update_wx_flags(sljit_s32 enable_exec)
{
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 110000
	pthread_jit_write_protect_np(enable_exec);
#else
#error "Must target Big Sur or newer"
#endif /* BigSur */
}
#endif /* SLJIT_CONFIG_X86 */
#else /* !TARGET_OS_OSX */
#define GENERIC_ALLOCATOR_MAP_JIT	(MAP_JIT)
#endif /* TARGET_OS_OSX */
#endif /* __APPLE__ && MAP_JIT */

#ifndef GENERIC_ALLOCATOR_MAP_JIT
#define GENERIC_ALLOCATOR_MAP_JIT	(0)
#endif /* !GENERIC_ALLOCATOR_MAP_JIT */

static SLJIT_INLINE void* generic_allocator_alloc_chunk(sljit_uw size)
{
	void *retval;
	int prot = PROT_READ | PROT_WRITE | PROT_EXEC;
	int flags = MAP_PRIVATE;
	int fd = -1;

#ifdef PROT_MAX
	prot |= PROT_MAX(prot);
#endif

#ifdef MAP_ANON
	flags |= MAP_ANON | GENERIC_ALLOCATOR_MAP_JIT;
#else /* !MAP_ANON */
	if (SLJIT_UNLIKELY((dev_zero < 0) && open_dev_zero()))
		return NULL;

	fd = dev_zero;
#endif /* MAP_ANON */

	retval = mmap(NULL, size, prot, flags, fd, 0);
	if (retval == MAP_FAILED)
		return NULL;

#ifdef __FreeBSD__
        /* HardenedBSD's mmap lies, so check permissions again */
	if (mprotect(retval, size, PROT_READ | PROT_WRITE | PROT_EXEC) < 0) {
		munmap(retval, size);
		return NULL;
	}
#endif /* FreeBSD */

	SLJIT_UPDATE_WX_FLAGS(retval, (sljit_u8*)retval + size, 0);
	return retval;
}

static SLJIT_INLINE void generic_allocator_free_chunk(void *chunk, sljit_uw size)
{
	munmap(chunk, size);
}

#endif /* _WIN32 */

/* --------------------------------------------------------------------- */
/*  Common functions                                                     */
/* --------------------------------------------------------------------- */

#define GENERIC_ALLOCATOR_CHUNK_MASK	(~(GENERIC_ALLOCATOR_CHUNK_SIZE - 1))

struct generic_allocator_block_header {
	sljit_uw size;
	sljit_uw prev_size;
};

struct generic_allocator_free_block {
	struct generic_allocator_block_header header;
	struct generic_allocator_free_block *next;
	struct generic_allocator_free_block *prev;
	sljit_uw size;
};

#define GENERIC_ALLOCATOR_AS_BLOCK_HEADER(base, offset) \
	((struct generic_allocator_block_header*)(((sljit_u8*)base) + offset))
#define GENERIC_ALLOCATOR_AS_FREE_BLOCK(base, offset) \
	((struct generic_allocator_free_block*)(((sljit_u8*)base) + offset))
#define GENERIC_ALLOCATOR_MEM_START(base) \
	((void*)(((sljit_u8*)base) + sizeof(struct generic_allocator_block_header)))
#define GENERIC_ALLOCATOR_ALIGN_SIZE(size) \
	(((size) + sizeof(struct generic_allocator_block_header) + 7u) & ~(sljit_uw)7)

static struct generic_allocator_free_block* generic_allocator_free_blocks;
static sljit_uw generic_allocator_allocated_size;
static sljit_uw generic_allocator_total_size;

static SLJIT_INLINE void generic_allocator_insert_free_block(struct generic_allocator_free_block *free_block, sljit_uw size)
{
	free_block->header.size = 0;
	free_block->size = size;

	free_block->next = generic_allocator_free_blocks;
	free_block->prev = NULL;
	if (generic_allocator_free_blocks)
		generic_allocator_free_blocks->prev = free_block;
	generic_allocator_free_blocks = free_block;
}

static SLJIT_INLINE void generic_allocator_remove_free_block(struct generic_allocator_free_block *free_block)
{
	if (free_block->next)
		free_block->next->prev = free_block->prev;

	if (free_block->prev)
		free_block->prev->next = free_block->next;
	else {
		SLJIT_ASSERT(generic_allocator_free_blocks == free_block);
		generic_allocator_free_blocks = free_block->next;
	}
}

#if (defined SLJIT_GENERIC_EXECUTABLE_ALLOCATOR && SLJIT_GENERIC_EXECUTABLE_ALLOCATOR)
SLJIT_API_FUNC_ATTRIBUTE void* sljit_malloc_exec(sljit_uw size)
#else /* !SLJIT_GENERIC_EXECUTABLE_ALLOCATOR */
static void* sljit_generic_allocator_malloc_exec(sljit_uw size)
#endif /* SLJIT_GENERIC_EXECUTABLE_ALLOCATOR */
{
	struct generic_allocator_block_header *header;
	struct generic_allocator_block_header *next_header;
	struct generic_allocator_free_block *free_block;
	sljit_uw chunk_size;

#if (defined SLJIT_GENERIC_EXECUTABLE_ALLOCATOR && SLJIT_GENERIC_EXECUTABLE_ALLOCATOR)
	SLJIT_ALLOCATOR_LOCK();
#endif /* SLJIT_GENERIC_EXECUTABLE_ALLOCATOR */
	if (size < (64 - sizeof(struct generic_allocator_block_header)))
		size = (64 - sizeof(struct generic_allocator_block_header));
	size = GENERIC_ALLOCATOR_ALIGN_SIZE(size);

	free_block = generic_allocator_free_blocks;
	while (free_block) {
		if (free_block->size >= size) {
			chunk_size = free_block->size;
			SLJIT_UPDATE_WX_FLAGS(NULL, NULL, 0);
			if (chunk_size > size + 64) {
				/* We just cut a block from the end of the free block. */
				chunk_size -= size;
				free_block->size = chunk_size;
				header = GENERIC_ALLOCATOR_AS_BLOCK_HEADER(free_block, chunk_size);
				header->prev_size = chunk_size;
				GENERIC_ALLOCATOR_AS_BLOCK_HEADER(header, size)->prev_size = size;
			}
			else {
				generic_allocator_remove_free_block(free_block);
				header = (struct generic_allocator_block_header*)free_block;
				size = chunk_size;
			}
			generic_allocator_allocated_size += size;
			header->size = size;
#if (defined SLJIT_GENERIC_EXECUTABLE_ALLOCATOR && SLJIT_GENERIC_EXECUTABLE_ALLOCATOR)
			SLJIT_ALLOCATOR_UNLOCK();
#endif /* SLJIT_GENERIC_EXECUTABLE_ALLOCATOR */
			return GENERIC_ALLOCATOR_MEM_START(header);
		}
		free_block = free_block->next;
	}

	chunk_size = (size + sizeof(struct generic_allocator_block_header) + GENERIC_ALLOCATOR_CHUNK_SIZE - 1) & GENERIC_ALLOCATOR_CHUNK_MASK;
	header = (struct generic_allocator_block_header*)generic_allocator_alloc_chunk(chunk_size);
	if (!header) {
#if (defined SLJIT_GENERIC_EXECUTABLE_ALLOCATOR && SLJIT_GENERIC_EXECUTABLE_ALLOCATOR)
		SLJIT_ALLOCATOR_UNLOCK();
#endif /* SLJIT_GENERIC_EXECUTABLE_ALLOCATOR */
		return NULL;
	}

	chunk_size -= sizeof(struct generic_allocator_block_header);
	generic_allocator_total_size += chunk_size;

	header->prev_size = 0;
	if (chunk_size > size + 64) {
		/* Cut the allocated space into a free and a used block. */
		generic_allocator_allocated_size += size;
		header->size = size;
		chunk_size -= size;

		free_block = GENERIC_ALLOCATOR_AS_FREE_BLOCK(header, size);
		free_block->header.prev_size = size;
		generic_allocator_insert_free_block(free_block, chunk_size);
		next_header = GENERIC_ALLOCATOR_AS_BLOCK_HEADER(free_block, chunk_size);
	}
	else {
		/* All space belongs to this allocation. */
		generic_allocator_allocated_size += chunk_size;
		header->size = chunk_size;
		next_header = GENERIC_ALLOCATOR_AS_BLOCK_HEADER(header, chunk_size);
	}
	next_header->size = 1;
	next_header->prev_size = chunk_size;
#if (defined SLJIT_GENERIC_EXECUTABLE_ALLOCATOR && SLJIT_GENERIC_EXECUTABLE_ALLOCATOR)
	SLJIT_ALLOCATOR_UNLOCK();
#endif /* SLJIT_GENERIC_EXECUTABLE_ALLOCATOR */
	return GENERIC_ALLOCATOR_MEM_START(header);
}

#if (defined SLJIT_GENERIC_EXECUTABLE_ALLOCATOR && SLJIT_GENERIC_EXECUTABLE_ALLOCATOR)
SLJIT_API_FUNC_ATTRIBUTE void sljit_free_exec(void* ptr)
#else /* !SLJIT_GENERIC_EXECUTABLE_ALLOCATOR */
static void sljit_generic_allocator_free_exec(void* ptr)
#endif /* SLJIT_GENERIC_EXECUTABLE_ALLOCATOR */
{
	struct generic_allocator_block_header *header;
	struct generic_allocator_free_block* free_block;

#if (defined SLJIT_GENERIC_EXECUTABLE_ALLOCATOR && SLJIT_GENERIC_EXECUTABLE_ALLOCATOR)
	SLJIT_ALLOCATOR_LOCK();
#endif /* SLJIT_GENERIC_EXECUTABLE_ALLOCATOR */
	header = GENERIC_ALLOCATOR_AS_BLOCK_HEADER(ptr, -(sljit_sw)sizeof(struct generic_allocator_block_header));
	generic_allocator_allocated_size -= header->size;

	/* Connecting free blocks together if possible. */
	SLJIT_UPDATE_WX_FLAGS(NULL, NULL, 0);

	/* If header->prev_size == 0, free_block will equal to header.
	   In this case, free_block->header.size will be > 0. */
	free_block = GENERIC_ALLOCATOR_AS_FREE_BLOCK(header, -(sljit_sw)header->prev_size);
	if (SLJIT_UNLIKELY(!free_block->header.size)) {
		free_block->size += header->size;
		header = GENERIC_ALLOCATOR_AS_BLOCK_HEADER(free_block, free_block->size);
		header->prev_size = free_block->size;
	}
	else {
		free_block = (struct generic_allocator_free_block*)header;
		generic_allocator_insert_free_block(free_block, header->size);
	}

	header = GENERIC_ALLOCATOR_AS_BLOCK_HEADER(free_block, free_block->size);
	if (SLJIT_UNLIKELY(!header->size)) {
		free_block->size += ((struct generic_allocator_free_block*)header)->size;
		generic_allocator_remove_free_block((struct generic_allocator_free_block*)header);
		header = GENERIC_ALLOCATOR_AS_BLOCK_HEADER(free_block, free_block->size);
		header->prev_size = free_block->size;
	}

	/* The whole chunk is free. */
	if (SLJIT_UNLIKELY(!free_block->header.prev_size && header->size == 1)) {
		/* If this block is freed, we still have (generic_allocator_allocated_size / 2) free space. */
		if (generic_allocator_total_size - free_block->size > (generic_allocator_allocated_size * 3 / 2)) {
			generic_allocator_total_size -= free_block->size;
			generic_allocator_remove_free_block(free_block);
			generic_allocator_free_chunk(free_block, free_block->size + sizeof(struct generic_allocator_block_header));
		}
	}

	SLJIT_UPDATE_WX_FLAGS(NULL, NULL, 1);
#if (defined SLJIT_GENERIC_EXECUTABLE_ALLOCATOR && SLJIT_GENERIC_EXECUTABLE_ALLOCATOR)
	SLJIT_ALLOCATOR_UNLOCK();
#endif /* SLJIT_GENERIC_EXECUTABLE_ALLOCATOR */
}

#if (defined SLJIT_GENERIC_EXECUTABLE_ALLOCATOR && SLJIT_GENERIC_EXECUTABLE_ALLOCATOR)
SLJIT_API_FUNC_ATTRIBUTE void sljit_free_unused_memory_exec(void)
#else /* !SLJIT_GENERIC_EXECUTABLE_ALLOCATOR */
static void sljit_generic_allocator_free_unused_memory_exec(void)
#endif /* SLJIT_GENERIC_EXECUTABLE_ALLOCATOR */
{
	struct generic_allocator_free_block* free_block;
	struct generic_allocator_free_block* next_free_block;

#if (defined SLJIT_GENERIC_EXECUTABLE_ALLOCATOR && SLJIT_GENERIC_EXECUTABLE_ALLOCATOR)
	SLJIT_ALLOCATOR_LOCK();
#endif /* SLJIT_GENERIC_EXECUTABLE_ALLOCATOR */
	SLJIT_UPDATE_WX_FLAGS(NULL, NULL, 0);

	free_block = generic_allocator_free_blocks;
	while (free_block) {
		next_free_block = free_block->next;
		if (!free_block->header.prev_size &&
				GENERIC_ALLOCATOR_AS_BLOCK_HEADER(free_block, free_block->size)->size == 1) {
			generic_allocator_total_size -= free_block->size;
			generic_allocator_remove_free_block(free_block);
			generic_allocator_free_chunk(free_block, free_block->size + sizeof(struct generic_allocator_block_header));
		}
		free_block = next_free_block;
	}

	SLJIT_ASSERT((generic_allocator_total_size && generic_allocator_free_blocks) || (!generic_allocator_total_size && !generic_allocator_free_blocks));
	SLJIT_UPDATE_WX_FLAGS(NULL, NULL, 1);
#if (defined SLJIT_GENERIC_EXECUTABLE_ALLOCATOR && SLJIT_GENERIC_EXECUTABLE_ALLOCATOR)
	SLJIT_ALLOCATOR_UNLOCK();
#endif /* SLJIT_GENERIC_EXECUTABLE_ALLOCATOR */
}
