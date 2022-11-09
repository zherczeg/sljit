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
     Memory is allocated in continuous memory areas called chunks
     by prot_allocator_alloc_chunk()
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
#define PROT_ALLOCATOR_CHUNK_SIZE	(sljit_uw)0x10000

struct prot_allocator_chunk_header {
	void *executable;
};

/*
   prot_allocator_alloc_chunk / prot_allocator_free_chunk :
     * allocate executable system memory chunks
     * the size is always divisible by PROT_ALLOCATOR_CHUNK_SIZE
   SLJIT_ALLOCATOR_LOCK / SLJIT_ALLOCATOR_UNLOCK :
     * provided as part of sljitUtils
     * only the allocator requires this lock, sljit is fully thread safe
       as it only uses local variables
*/

#if (defined SLJIT_PROT_EXECUTABLE_ALLOCATOR && SLJIT_PROT_EXECUTABLE_ALLOCATOR)
#define SLJIT_ADD_EXEC_OFFSET(ptr, exec_offset) ((sljit_u8 *)(ptr) + (exec_offset))
#endif /* SLJIT_PROT_EXECUTABLE_ALLOCATOR */

#ifndef __NetBSD__
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#ifndef O_NOATIME
#define O_NOATIME 0
#endif

/* this is a linux extension available since kernel 3.11 */
#ifndef O_TMPFILE
#define O_TMPFILE 020200000
#endif

#ifndef _GNU_SOURCE
char *secure_getenv(const char *name);
int mkostemp(char *template, int flags);
#endif

static SLJIT_INLINE int prot_allocator_create_tempfile(void)
{
	int fd;
	char tmp_name[256];
	size_t tmp_name_len = 0;
	char *dir;
	struct stat st;
#if defined(SLJIT_SINGLE_THREADED) && SLJIT_SINGLE_THREADED
	mode_t mode;
#endif

#ifdef HAVE_MEMFD_CREATE
	/* this is a GNU extension, make sure to use -D_GNU_SOURCE */
	fd = memfd_create("sljit", MFD_CLOEXEC);
	if (fd != -1) {
		fchmod(fd, 0);
		return fd;
	}
#endif

	dir = secure_getenv("TMPDIR");

	if (dir) {
		tmp_name_len = strlen(dir);
		if (tmp_name_len > 0 && tmp_name_len < sizeof(tmp_name)) {
			if ((stat(dir, &st) == 0) && S_ISDIR(st.st_mode))
				strcpy(tmp_name, dir);
		}
	}

#ifdef P_tmpdir
	if (!tmp_name_len) {
		tmp_name_len = strlen(P_tmpdir);
		if (tmp_name_len > 0 && tmp_name_len < sizeof(tmp_name))
			strcpy(tmp_name, P_tmpdir);
	}
#endif
	if (!tmp_name_len) {
		strcpy(tmp_name, "/tmp");
		tmp_name_len = 4;
	}

	SLJIT_ASSERT(tmp_name_len > 0 && tmp_name_len < sizeof(tmp_name));

	if (tmp_name[tmp_name_len - 1] == '/')
		tmp_name[--tmp_name_len] = '\0';

#ifdef __linux__
	/*
	 * the previous trimming might had left an empty string if TMPDIR="/"
	 * so work around the problem below
	 */
	fd = open(tmp_name_len ? tmp_name : "/",
		O_TMPFILE | O_EXCL | O_RDWR | O_NOATIME | O_CLOEXEC, 0);
	if (fd != -1)
		return fd;
#endif

	if (tmp_name_len + 7 >= sizeof(tmp_name))
		return -1;

	strcpy(tmp_name + tmp_name_len, "/XXXXXX");
#if defined(SLJIT_SINGLE_THREADED) && SLJIT_SINGLE_THREADED
	mode = umask(0777);
#endif
	fd = mkostemp(tmp_name, O_CLOEXEC | O_NOATIME);
#if defined(SLJIT_SINGLE_THREADED) && SLJIT_SINGLE_THREADED
	umask(mode);
#else
	fchmod(fd, 0);
#endif

	if (fd == -1)
		return -1;

	if (unlink(tmp_name)) {
		close(fd);
		return -1;
	}

	return fd;
}

static SLJIT_INLINE struct prot_allocator_chunk_header* prot_allocator_alloc_chunk(sljit_uw size)
{
	struct prot_allocator_chunk_header *retval;
	int fd;

	fd = prot_allocator_create_tempfile();
	if (fd == -1)
		return NULL;

	if (ftruncate(fd, (off_t)size)) {
		close(fd);
		return NULL;
	}

	retval = (struct prot_allocator_chunk_header *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if (retval == MAP_FAILED) {
		close(fd);
		return NULL;
	}

	retval->executable = mmap(NULL, size, PROT_READ | PROT_EXEC, MAP_SHARED, fd, 0);

	if (retval->executable == MAP_FAILED) {
		munmap((void *)retval, size);
		close(fd);
		return NULL;
	}

	close(fd);
	return retval;
}
#else
/*
 * MAP_REMAPDUP is a NetBSD extension available sinde 8.0, make sure to
 * adjust your feature macros (ex: -D_NETBSD_SOURCE) as needed
 */
static SLJIT_INLINE struct prot_allocator_chunk_header* prot_allocator_alloc_chunk(sljit_uw size)
{
	struct prot_allocator_chunk_header *retval;

	retval = (struct prot_allocator_chunk_header *)mmap(NULL, size,
			PROT_READ | PROT_WRITE | PROT_MPROTECT(PROT_EXEC),
			MAP_ANON | MAP_SHARED, -1, 0);

	if (retval == MAP_FAILED)
		return NULL;

	retval->executable = mremap(retval, size, NULL, size, MAP_REMAPDUP);
	if (retval->executable == MAP_FAILED) {
		munmap((void *)retval, size);
		return NULL;
	}

	if (mprotect(retval->executable, size, PROT_READ | PROT_EXEC) == -1) {
		munmap(retval->executable, size);
		munmap((void *)retval, size);
		return NULL;
	}

	return retval;
}
#endif /* NetBSD */

static SLJIT_INLINE void prot_allocator_free_chunk(void *chunk, sljit_uw size)
{
	struct prot_allocator_chunk_header *header = ((struct prot_allocator_chunk_header *)chunk) - 1;

	munmap(header->executable, size);
	munmap((void *)header, size);
}

/* --------------------------------------------------------------------- */
/*  Common functions                                                     */
/* --------------------------------------------------------------------- */

#define PROT_ALLOCATOR_CHUNK_MASK	(~(PROT_ALLOCATOR_CHUNK_SIZE - 1))

struct prot_allocator_block_header {
	sljit_uw size;
	sljit_uw prev_size;
	sljit_sw executable_offset;
};

struct prot_allocator_free_block {
	struct prot_allocator_block_header header;
	struct prot_allocator_free_block *next;
	struct prot_allocator_free_block *prev;
	sljit_uw size;
};

#define PROT_ALLOCATOR_AS_BLOCK_HEADER(base, offset) \
	((struct prot_allocator_block_header*)(((sljit_u8*)base) + offset))
#define PROT_ALLOCATOR_AS_FREE_BLOCK(base, offset) \
	((struct prot_allocator_free_block*)(((sljit_u8*)base) + offset))
#define PROT_ALLOCATOR_MEM_START(base) \
	((void*)((base) + 1))
#define PROT_ALLOCATOR_ALIGN_SIZE(size) \
	(((size) + sizeof(struct prot_allocator_block_header) + 7u) & ~(sljit_uw)7)

static struct prot_allocator_free_block* prot_allocator_free_blocks;
static sljit_uw prot_allocator_allocated_size;
static sljit_uw prot_allocator_total_size;

static SLJIT_INLINE void prot_allocator_insert_free_block(struct prot_allocator_free_block *free_block, sljit_uw size)
{
	free_block->header.size = 0;
	free_block->size = size;

	free_block->next = prot_allocator_free_blocks;
	free_block->prev = NULL;
	if (prot_allocator_free_blocks)
		prot_allocator_free_blocks->prev = free_block;
	prot_allocator_free_blocks = free_block;
}

static SLJIT_INLINE void prot_allocator_remove_free_block(struct prot_allocator_free_block *free_block)
{
	if (free_block->next)
		free_block->next->prev = free_block->prev;

	if (free_block->prev)
		free_block->prev->next = free_block->next;
	else {
		SLJIT_ASSERT(prot_allocator_free_blocks == free_block);
		prot_allocator_free_blocks = free_block->next;
	}
}

#if (defined SLJIT_PROT_EXECUTABLE_ALLOCATOR && SLJIT_PROT_EXECUTABLE_ALLOCATOR)
SLJIT_API_FUNC_ATTRIBUTE void* sljit_malloc_exec(sljit_uw size)
#else /* !SLJIT_PROT_EXECUTABLE_ALLOCATOR */
static void* sljit_prot_allocator_malloc_exec(sljit_uw size)
#endif /* SLJIT_PROT_EXECUTABLE_ALLOCATOR */
{
	struct prot_allocator_chunk_header *chunk_header;
	struct prot_allocator_block_header *header;
	struct prot_allocator_block_header *next_header;
	struct prot_allocator_free_block *free_block;
	sljit_uw chunk_size;
	sljit_sw executable_offset;

#if (defined SLJIT_PROT_EXECUTABLE_ALLOCATOR && SLJIT_PROT_EXECUTABLE_ALLOCATOR)
	SLJIT_ALLOCATOR_LOCK();
#endif /* SLJIT_PROT_EXECUTABLE_ALLOCATOR */
	if (size < (64 - sizeof(struct prot_allocator_block_header)))
		size = (64 - sizeof(struct prot_allocator_block_header));
	size = PROT_ALLOCATOR_ALIGN_SIZE(size);

	free_block = prot_allocator_free_blocks;
	while (free_block) {
		if (free_block->size >= size) {
			chunk_size = free_block->size;
			if (chunk_size > size + 64) {
				/* We just cut a block from the end of the free block. */
				chunk_size -= size;
				free_block->size = chunk_size;
				header = PROT_ALLOCATOR_AS_BLOCK_HEADER(free_block, chunk_size);
				header->prev_size = chunk_size;
				header->executable_offset = free_block->header.executable_offset;
				PROT_ALLOCATOR_AS_BLOCK_HEADER(header, size)->prev_size = size;
			}
			else {
				prot_allocator_remove_free_block(free_block);
				header = (struct prot_allocator_block_header*)free_block;
				size = chunk_size;
			}
			prot_allocator_allocated_size += size;
			header->size = size;
#if (defined SLJIT_PROT_EXECUTABLE_ALLOCATOR && SLJIT_PROT_EXECUTABLE_ALLOCATOR)
			SLJIT_ALLOCATOR_UNLOCK();
#endif /* SLJIT_PROT_EXECUTABLE_ALLOCATOR */
			return PROT_ALLOCATOR_MEM_START(header);
		}
		free_block = free_block->next;
	}

	chunk_size = sizeof(struct prot_allocator_chunk_header) + sizeof(struct prot_allocator_block_header);
	chunk_size = (chunk_size + size + PROT_ALLOCATOR_CHUNK_SIZE - 1) & PROT_ALLOCATOR_CHUNK_MASK;

	chunk_header = prot_allocator_alloc_chunk(chunk_size);
	if (!chunk_header) {
#if (defined SLJIT_PROT_EXECUTABLE_ALLOCATOR && SLJIT_PROT_EXECUTABLE_ALLOCATOR)
		SLJIT_ALLOCATOR_UNLOCK();
#endif /* SLJIT_PROT_EXECUTABLE_ALLOCATOR */
		return NULL;
	}

	executable_offset = (sljit_sw)((sljit_u8*)chunk_header->executable - (sljit_u8*)chunk_header);

	chunk_size -= sizeof(struct prot_allocator_chunk_header) + sizeof(struct prot_allocator_block_header);
	prot_allocator_total_size += chunk_size;

	header = (struct prot_allocator_block_header *)(chunk_header + 1);

	header->prev_size = 0;
	header->executable_offset = executable_offset;
	if (chunk_size > size + 64) {
		/* Cut the allocated space into a free and a used block. */
		prot_allocator_allocated_size += size;
		header->size = size;
		chunk_size -= size;

		free_block = PROT_ALLOCATOR_AS_FREE_BLOCK(header, size);
		free_block->header.prev_size = size;
		free_block->header.executable_offset = executable_offset;
		prot_allocator_insert_free_block(free_block, chunk_size);
		next_header = PROT_ALLOCATOR_AS_BLOCK_HEADER(free_block, chunk_size);
	}
	else {
		/* All space belongs to this allocation. */
		prot_allocator_allocated_size += chunk_size;
		header->size = chunk_size;
		next_header = PROT_ALLOCATOR_AS_BLOCK_HEADER(header, chunk_size);
	}
	next_header->size = 1;
	next_header->prev_size = chunk_size;
	next_header->executable_offset = executable_offset;
#if (defined SLJIT_PROT_EXECUTABLE_ALLOCATOR && SLJIT_PROT_EXECUTABLE_ALLOCATOR)
	SLJIT_ALLOCATOR_UNLOCK();
#endif /* SLJIT_PROT_EXECUTABLE_ALLOCATOR */
	return PROT_ALLOCATOR_MEM_START(header);
}

#if (defined SLJIT_PROT_EXECUTABLE_ALLOCATOR && SLJIT_PROT_EXECUTABLE_ALLOCATOR)
SLJIT_API_FUNC_ATTRIBUTE void sljit_free_exec(void* ptr)
#else /* !SLJIT_PROT_EXECUTABLE_ALLOCATOR */
static void sljit_prot_allocator_free_exec(void* ptr)
#endif /* SLJIT_PROT_EXECUTABLE_ALLOCATOR */
{
	struct prot_allocator_block_header *header;
	struct prot_allocator_free_block* free_block;

#if (defined SLJIT_PROT_EXECUTABLE_ALLOCATOR && SLJIT_PROT_EXECUTABLE_ALLOCATOR)
	SLJIT_ALLOCATOR_LOCK();
#endif /* SLJIT_PROT_EXECUTABLE_ALLOCATOR */
	header = PROT_ALLOCATOR_AS_BLOCK_HEADER(ptr, -(sljit_sw)sizeof(struct prot_allocator_block_header));
	header = PROT_ALLOCATOR_AS_BLOCK_HEADER(header, -header->executable_offset);
	prot_allocator_allocated_size -= header->size;

	/* Connecting free blocks together if possible. */

	/* If header->prev_size == 0, free_block will equal to header.
	   In this case, free_block->header.size will be > 0. */
	free_block = PROT_ALLOCATOR_AS_FREE_BLOCK(header, -(sljit_sw)header->prev_size);
	if (SLJIT_UNLIKELY(!free_block->header.size)) {
		free_block->size += header->size;
		header = PROT_ALLOCATOR_AS_BLOCK_HEADER(free_block, free_block->size);
		header->prev_size = free_block->size;
	}
	else {
		free_block = (struct prot_allocator_free_block*)header;
		prot_allocator_insert_free_block(free_block, header->size);
	}

	header = PROT_ALLOCATOR_AS_BLOCK_HEADER(free_block, free_block->size);
	if (SLJIT_UNLIKELY(!header->size)) {
		free_block->size += ((struct prot_allocator_free_block*)header)->size;
		prot_allocator_remove_free_block((struct prot_allocator_free_block*)header);
		header = PROT_ALLOCATOR_AS_BLOCK_HEADER(free_block, free_block->size);
		header->prev_size = free_block->size;
	}

	/* The whole chunk is free. */
	if (SLJIT_UNLIKELY(!free_block->header.prev_size && header->size == 1)) {
		/* If this block is freed, we still have (prot_allocator_allocated_size / 2) free space. */
		if (prot_allocator_total_size - free_block->size > (prot_allocator_allocated_size * 3 / 2)) {
			prot_allocator_total_size -= free_block->size;
			prot_allocator_remove_free_block(free_block);
			prot_allocator_free_chunk(free_block, free_block->size +
				sizeof(struct prot_allocator_chunk_header) +
				sizeof(struct prot_allocator_block_header));
		}
	}

#if (defined SLJIT_PROT_EXECUTABLE_ALLOCATOR && SLJIT_PROT_EXECUTABLE_ALLOCATOR)
	SLJIT_ALLOCATOR_UNLOCK();
#endif /* SLJIT_PROT_EXECUTABLE_ALLOCATOR */
}

#if (defined SLJIT_PROT_EXECUTABLE_ALLOCATOR && SLJIT_PROT_EXECUTABLE_ALLOCATOR)
SLJIT_API_FUNC_ATTRIBUTE void sljit_free_unused_memory_exec(void)
#else /* !SLJIT_PROT_EXECUTABLE_ALLOCATOR */
static void sljit_prot_allocator_free_unused_memory_exec(void)
#endif /* SLJIT_PROT_EXECUTABLE_ALLOCATOR */
{
	struct prot_allocator_free_block* free_block;
	struct prot_allocator_free_block* next_free_block;

#if (defined SLJIT_PROT_EXECUTABLE_ALLOCATOR && SLJIT_PROT_EXECUTABLE_ALLOCATOR)
	SLJIT_ALLOCATOR_LOCK();
#endif /* SLJIT_PROT_EXECUTABLE_ALLOCATOR */

	free_block = prot_allocator_free_blocks;
	while (free_block) {
		next_free_block = free_block->next;
		if (!free_block->header.prev_size && 
				PROT_ALLOCATOR_AS_BLOCK_HEADER(free_block, free_block->size)->size == 1) {
			prot_allocator_total_size -= free_block->size;
			prot_allocator_remove_free_block(free_block);
			prot_allocator_free_chunk(free_block, free_block->size +
				sizeof(struct prot_allocator_chunk_header) +
				sizeof(struct prot_allocator_block_header));
		}
		free_block = next_free_block;
	}

	SLJIT_ASSERT((prot_allocator_total_size && prot_allocator_free_blocks) || (!prot_allocator_total_size && !prot_allocator_free_blocks));
#if (defined SLJIT_PROT_EXECUTABLE_ALLOCATOR && SLJIT_PROT_EXECUTABLE_ALLOCATOR)
	SLJIT_ALLOCATOR_UNLOCK();
#endif /* SLJIT_PROT_EXECUTABLE_ALLOCATOR */
}

#if (defined SLJIT_PROT_EXECUTABLE_ALLOCATOR && SLJIT_PROT_EXECUTABLE_ALLOCATOR)
SLJIT_API_FUNC_ATTRIBUTE sljit_sw sljit_exec_offset(void* ptr)
#else /* !SLJIT_PROT_EXECUTABLE_ALLOCATOR */
static sljit_sw sljit_prot_allocator_exec_offset(void* ptr)
#endif /* SLJIT_PROT_EXECUTABLE_ALLOCATOR */
{
	return ((struct prot_allocator_block_header *)(ptr))[-1].executable_offset;
}
