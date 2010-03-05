/*
 *  ircd-ratbox: A slightly useful ircd.
 *  balloc.c: A block allocator.
 *
 * Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 * Copyright (C) 1996-2002 Hybrid Development Team
 * Copyright (C) 2002-2006 ircd-ratbox development team
 *
 *  Below are the orignal headers from the old blalloc.c 
 *
 *  File:   blalloc.c
 *  Owner:  Wohali (Joan Touzet)
 *  
 *  Modified 2001/11/29 for mmap() support by Aaron Sethman <androsyn@ratbox.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 *  $Id: balloc.c 26100 2008-09-20 01:27:19Z androsyn $
 */

/* 
 * About the block allocator
 *
 * Basically we have three ways of getting memory off of the operating
 * system. Below are this list of methods and the order of preference.
 *
 * 1. mmap() anonymous pages with the MMAP_ANON flag.
 * 2. mmap() via the /dev/zero trick.
 * 3. HeapCreate/HeapAlloc (on win32) 
 * 4. malloc() 
 *
 * The advantages of 1 and 2 are this.  We can munmap() the pages which will
 * return the pages back to the operating system, thus reducing the size 
 * of the process as the memory is unused.  malloc() on many systems just keeps
 * a heap of memory to itself, which never gets given back to the OS, except on
 * exit.  This of course is bad, if say we have an event that causes us to allocate
 * say, 200MB of memory, while our normal memory consumption would be 15MB.  In the
 * malloc() case, the amount of memory allocated to our process never goes down, as
 * malloc() has it locked up in its heap.  With the mmap() method, we can munmap()
 * the block and return it back to the OS, thus causing our memory consumption to go
 * down after we no longer need it.
 * 
 *
 *
 */
#include <libratbox_config.h>
#include <ratbox_lib.h>

#ifndef NOBALLOC
#ifdef HAVE_MMAP		/* We've got mmap() that is good */
#include <sys/mman.h>
/* HP-UX sucks */
#ifdef MAP_ANONYMOUS
#ifndef MAP_ANON
#define MAP_ANON MAP_ANONYMOUS
#endif
#endif
#endif
#endif

static uintptr_t offset_pad;

/* status information for an allocated block in heap */
struct rb_heap_block
{
	size_t alloc_size;
	rb_dlink_node node;
	unsigned long free_count;
	void *elems;		/* Points to allocated memory */
};
typedef struct rb_heap_block rb_heap_block;

/* information for the root node of the heap */
struct rb_bh
{
	rb_dlink_node hlist;
	size_t elemSize;	/* Size of each element to be stored */
	unsigned long elemsPerBlock;	/* Number of elements per block */
	rb_dlink_list block_list;
	rb_dlink_list free_list;
	char *desc;
};

#ifndef NOBALLOC
static int newblock(rb_bh *bh);
static void rb_bh_gc_event(void *unused);
#endif /* !NOBALLOC */
static rb_dlink_list *heap_lists;

#if defined(WIN32)
static HANDLE block_heap;
#endif

#define rb_bh_fail(x) _rb_bh_fail(x, __FILE__, __LINE__)

static void
_rb_bh_fail(const char *reason, const char *file, int line)
{
	rb_lib_log("rb_heap_blockheap failure: %s (%s:%d)", reason, file, line);
	abort();
}

#ifndef NOBALLOC
/*
 * static inline void free_block(void *ptr, size_t size)
 *
 * Inputs: The block and its size
 * Output: None
 * Side Effects: Returns memory for the block back to the OS
 */
static inline void
free_block(void *ptr, size_t size)
{
#ifdef HAVE_MMAP
	munmap(ptr, size);
#else
#ifdef _WIN32
	HeapFree(block_heap, 0, ptr);
#else
	free(ptr);
#endif
#endif
}
#endif /* !NOBALLOC */

/*
 * void rb_init_bh(void)
 * 
 * Inputs: None
 * Outputs: None
 * Side Effects: Initializes the block heap
 */

void
rb_init_bh(void)
{
	heap_lists = rb_malloc(sizeof(rb_dlink_list));
	offset_pad = sizeof(void *);
	/* XXX if you get SIGBUS when trying to use a long long..here is where you need to
	 * fix your shit
	 */
#ifdef __sparc__
	if((offset_pad % __alignof__(long long)) != 0)
	{
		offset_pad += __alignof__(long long);
		offset_pad &= ~(__alignof__(long long) - 1);
	}
#endif

#ifndef NOBALLOC
#ifdef _WIN32
	block_heap = HeapCreate(HEAP_NO_SERIALIZE, 0, 0);
#endif
	rb_event_addish("rb_bh_gc_event", rb_bh_gc_event, NULL, 300);
#endif /* !NOBALLOC */
}

#ifndef NOBALLOC
/*
 * static inline void *get_block(size_t size)
 * 
 * Input: Size of block to allocate
 * Output: Pointer to new block
 * Side Effects: None
 */
static inline void *
get_block(size_t size)
{
	void *ptr;
#ifdef HAVE_MMAP
#ifdef MAP_ANON
	ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
#else
	int zero_fd;
	zero_fd = open("/dev/zero", O_RDWR);
	if(zero_fd < 0)
		rb_bh_fail("Failed opening /dev/zero");
	ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, zero_fd, 0);
	close(zero_fd);
#endif /* MAP_ANON */
	if(ptr == MAP_FAILED)
		ptr = NULL;
#else
#ifdef _WIN32
	ptr = HeapAlloc(block_heap, 0, size);
#else
	ptr = malloc(size);
#endif
#endif
	return (ptr);
}


static void
rb_bh_gc_event(void *unused)
{
	rb_dlink_node *ptr;
	RB_DLINK_FOREACH(ptr, heap_lists->head)
	{
		rb_bh_gc(ptr->data);
	}
}

/* ************************************************************************ */
/* FUNCTION DOCUMENTATION:                                                  */
/*    newblock                                                              */
/* Description:                                                             */
/*    Allocates a new block for addition to a blockheap                     */
/* Parameters:                                                              */
/*    bh (IN): Pointer to parent blockheap.                                 */
/* Returns:                                                                 */
/*    0 if successful, 1 if not                                             */
/* ************************************************************************ */

static int
newblock(rb_bh *bh)
{
	rb_heap_block *b;
	unsigned long i;
	uintptr_t offset;
	rb_dlink_node *node;
	/* Setup the initial data structure. */
	b = rb_malloc(sizeof(rb_heap_block));

	b->alloc_size = bh->elemsPerBlock * bh->elemSize;

	b->elems = get_block(b->alloc_size);
	if(rb_unlikely(b->elems == NULL))
	{
		return (1);
	}
	offset = (uintptr_t)b->elems;
	/* Setup our blocks now */
	for(i = 0; i < bh->elemsPerBlock; i++, offset += bh->elemSize)
	{
		*((void **)offset) = b;
		node = (void *)(offset + offset_pad);
		rb_dlinkAdd((void *)offset, node, &bh->free_list);
	}
	rb_dlinkAdd(b, &b->node, &bh->block_list);
	b->free_count = bh->elemsPerBlock;
	return (0);
}
#endif /* !NOBALLOC */

/* ************************************************************************ */
/* FUNCTION DOCUMENTATION:                                                  */
/*    rb_bh_create                                                       */
/* Description:                                                             */
/*   Creates a new blockheap from which smaller blocks can be allocated.    */
/*   Intended to be used instead of multiple calls to malloc() when         */
/*   performance is an issue.                                               */
/* Parameters:                                                              */
/*   elemsize (IN):  Size of the basic element to be stored                 */
/*   elemsperblock (IN):  Number of elements to be stored in a single block */
/*         of memory.  When the blockheap runs out of free memory, it will  */
/*         allocate elemsize * elemsperblock more.                          */
/* Returns:                                                                 */
/*   Pointer to new rb_bh, or NULL if unsuccessful                      */
/* ************************************************************************ */
rb_bh *
rb_bh_create(size_t elemsize, int elemsperblock, const char *desc)
{
	rb_bh *bh;
	lrb_assert(elemsize > 0 && elemsperblock > 0);
	lrb_assert(elemsize >= sizeof(rb_dlink_node));

	/* Catch idiotic requests up front */
	if((elemsize == 0) || (elemsperblock <= 0))
	{
		rb_bh_fail("Attempting to rb_bh_create idiotic sizes");
	}

	if(elemsize < sizeof(rb_dlink_node))
		rb_bh_fail("Attempt to rb_bh_create smaller than sizeof(rb_dlink_node)");

	/* Allocate our new rb_bh */
	bh = rb_malloc(sizeof(rb_bh));
#ifndef NOBALLOC
	elemsize += offset_pad;
	if((elemsize % sizeof(void *)) != 0)
	{
		/* Pad to even pointer boundary */
		elemsize += sizeof(void *);
		elemsize &= ~(sizeof(void *) - 1);
	}
#endif

	bh->elemSize = elemsize;
	bh->elemsPerBlock = elemsperblock;
	if(desc != NULL)
		bh->desc = rb_strdup(desc);

#ifndef NOBALLOC
	/* Be sure our malloc was successful */
	if(newblock(bh))
	{
		if(bh != NULL)
			free(bh);
		rb_lib_log("newblock() failed");
		rb_outofmemory();	/* die.. out of memory */
	}
#endif /* !NOBALLOC */

	if(bh == NULL)
	{
		rb_bh_fail("bh == NULL when it shouldn't be");
	}
	rb_dlinkAdd(bh, &bh->hlist, heap_lists);
	return (bh);
}

/* ************************************************************************ */
/* FUNCTION DOCUMENTATION:                                                  */
/*    rb_bh_alloc                                                        */
/* Description:                                                             */
/*    Returns a pointer to a struct within our rb_bh that's free for    */
/*    the taking.                                                           */
/* Parameters:                                                              */
/*    bh (IN):  Pointer to the Blockheap.                                   */
/* Returns:                                                                 */
/*    Pointer to a structure (void *), or NULL if unsuccessful.             */
/* ************************************************************************ */

void *
rb_bh_alloc(rb_bh *bh)
{
#ifndef NOBALLOC
	rb_dlink_node *new_node;
	rb_heap_block **block;
	void *ptr;
#endif
	lrb_assert(bh != NULL);
	if(rb_unlikely(bh == NULL))
	{
		rb_bh_fail("Cannot allocate if bh == NULL");
	}

#ifdef NOBALLOC
	return (rb_malloc(bh->elemSize));
#else
	if(bh->free_list.head == NULL)
	{
		/* Allocate new block and assign */
		/* newblock returns 1 if unsuccessful, 0 if not */

		if(rb_unlikely(newblock(bh)))
		{
			rb_lib_log("newblock() failed");
			rb_outofmemory();	/* Well that didn't work either...bail */
		}
		if(bh->free_list.head == NULL)
		{
			rb_lib_log("out of memory after newblock()...");
			rb_outofmemory();
		}
	}

	new_node = bh->free_list.head;
	block = (rb_heap_block **) new_node->data;
	ptr = (void *)((uintptr_t)new_node->data + (uintptr_t)offset_pad);
	rb_dlinkDelete(new_node, &bh->free_list);
	(*block)->free_count--;
	memset(ptr, 0, bh->elemSize - offset_pad);
	return (ptr);
#endif
}


/* ************************************************************************ */
/* FUNCTION DOCUMENTATION:                                                  */
/*    rb_bh_free                                                          */
/* Description:                                                             */
/*    Returns an element to the free pool, does not free()                  */
/* Parameters:                                                              */
/*    bh (IN): Pointer to rb_bh containing element                        */
/*    ptr (in):  Pointer to element to be "freed"                           */
/* Returns:                                                                 */
/*    0 if successful, 1 if element not contained within rb_bh.           */
/* ************************************************************************ */
int
rb_bh_free(rb_bh *bh, void *ptr)
{
#ifndef NOBALLOC
	rb_heap_block *block;
	void *data;
#endif
	lrb_assert(bh != NULL);
	lrb_assert(ptr != NULL);

	if(rb_unlikely(bh == NULL))
	{
		rb_lib_log("balloc.c:rb_bhFree() bh == NULL");
		return (1);
	}

	if(rb_unlikely(ptr == NULL))
	{
		rb_lib_log("balloc.rb_bhFree() ptr == NULL");
		return (1);
	}

#ifdef NOBALLOC
	rb_free(ptr);
#else
	data = (void *)((uintptr_t)ptr - (uintptr_t)offset_pad);
	block = *(rb_heap_block **) data;
	/* XXX */
	if(rb_unlikely
	   (!((uintptr_t)ptr >= (uintptr_t)block->elems
	      && (uintptr_t)ptr < (uintptr_t)block->elems + (uintptr_t)block->alloc_size)))
	{
		rb_bh_fail("rb_bh_free() bogus pointer");
	}
	block->free_count++;

	rb_dlinkAdd(data, (rb_dlink_node *)ptr, &bh->free_list);
#endif /* !NOBALLOC */
	return (0);
}


/* ************************************************************************ */
/* FUNCTION DOCUMENTATION:                                                  */
/*    rb_bhDestroy                                                      */
/* Description:                                                             */
/*    Completely free()s a rb_bh.  Use for cleanup.                     */
/* Parameters:                                                              */
/*    bh (IN):  Pointer to the rb_bh to be destroyed.                   */
/* Returns:                                                                 */
/*   0 if successful, 1 if bh == NULL                                       */
/* ************************************************************************ */
int
rb_bh_destroy(rb_bh *bh)
{
#ifndef NOBALLOC
	rb_dlink_node *ptr, *next;
	rb_heap_block *b;
#endif
	if(bh == NULL)
		return (1);

#ifndef NOBALLOC
	RB_DLINK_FOREACH_SAFE(ptr, next, bh->block_list.head)
	{
		b = ptr->data;
		free_block(b->elems, b->alloc_size);
		rb_free(b);
	}
#endif /* !NOBALLOC */

	rb_dlinkDelete(&bh->hlist, heap_lists);
	rb_free(bh->desc);
	rb_free(bh);

	return (0);
}

void
rb_bh_usage(rb_bh *bh, size_t *bused, size_t *bfree, size_t *bmemusage, const char **desc)
{
#ifndef NOBALLOC
	size_t used, freem, memusage;

	if(bh == NULL)
	{
		return;
	}

	freem = rb_dlink_list_length(&bh->free_list);
	used = (rb_dlink_list_length(&bh->block_list) * bh->elemsPerBlock) - freem;
	memusage = used * bh->elemSize;
	if(bused != NULL)
		*bused = used;
	if(bfree != NULL)
		*bfree = freem;
	if(bmemusage != NULL)
		*bmemusage = memusage;
	if(desc != NULL)
		*desc = bh->desc;
#else
	if(bused != NULL)
		*bused = 0;
	if(bfree != NULL)
		*bfree = 0;
	if(bmemusage != NULL)
		*bmemusage = 0;
	if(desc != NULL)
		*desc = "no blockheap";
#endif
}

void
rb_bh_usage_all(rb_bh_usage_cb *cb, void *data)
{
	rb_dlink_node *ptr;
	rb_bh *bh;
	size_t used, freem, memusage, heapalloc;
	static const char *unnamed = "(unnamed_heap)";
	const char *desc = unnamed;

	if(cb == NULL)
		return;

	RB_DLINK_FOREACH(ptr, heap_lists->head)
	{
		bh = (rb_bh *)ptr->data;
		freem = rb_dlink_list_length(&bh->free_list);
		used = (rb_dlink_list_length(&bh->block_list) * bh->elemsPerBlock) - freem;
		memusage = used * bh->elemSize;
		heapalloc = (freem + used) * bh->elemSize;
		if(bh->desc != NULL)
			desc = bh->desc;
		cb(used, freem, memusage, heapalloc, desc, data);
	}
	return;
}

void
rb_bh_total_usage(size_t *total_alloc, size_t *total_used)
{
	rb_dlink_node *ptr;
	size_t total_memory = 0, used_memory = 0, used, freem;
	rb_bh *bh;

	RB_DLINK_FOREACH(ptr, heap_lists->head)
	{
		bh = (rb_bh *)ptr->data;
		freem = rb_dlink_list_length(&bh->free_list);
		used = (rb_dlink_list_length(&bh->block_list) * bh->elemsPerBlock) - freem;
		used_memory += used * bh->elemSize;
		total_memory += (freem + used) * bh->elemSize;
	}

	if(total_alloc != NULL)
		*total_alloc = total_memory;
	if(total_used != NULL)
		*total_used = used_memory;
}

#ifndef NOBALLOC
int
rb_bh_gc(rb_bh *bh)
{
	rb_heap_block *b;
	rb_dlink_node *ptr, *next;
	unsigned long i;
	uintptr_t offset;

	if(bh == NULL)
	{
		/* somebody is smoking some craq..(probably lee, but don't tell him that) */
		return (1);
	}

	if((rb_dlink_list_length(&bh->free_list) < bh->elemsPerBlock)
	   || rb_dlink_list_length(&bh->block_list) == 1)
	{
		/* There couldn't possibly be an entire free block.  Return. */
		return (0);
	}

	RB_DLINK_FOREACH_SAFE(ptr, next, bh->block_list.head)
	{
		b = ptr->data;
		if(rb_dlink_list_length(&bh->block_list) == 1)
			return (0);

		if(b->free_count == bh->elemsPerBlock)
		{
			/* i'm seriously going to hell for this.. */

			offset = (uintptr_t)b->elems;
			for(i = 0; i < bh->elemsPerBlock; i++, offset += (uintptr_t)bh->elemSize)
			{
				rb_dlinkDelete((rb_dlink_node *)(offset + offset_pad),
					       &bh->free_list);
			}
			rb_dlinkDelete(&b->node, &bh->block_list);
			free_block(b->elems, b->alloc_size);
			rb_free(b);
		}

	}
	return (0);
}
#endif /* !NOBALLOC */
