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
#include <librb_config.h>
#include <rb_lib.h>

static void _rb_bh_fail(const char *reason, const char *file, int line) __attribute__((noreturn));

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

static rb_dlink_list *heap_lists;

#define rb_bh_fail(x) _rb_bh_fail(x, __FILE__, __LINE__)

static void
_rb_bh_fail(const char *reason, const char *file, int line)
{
	rb_lib_log("rb_heap_blockheap failure: %s (%s:%d)", reason, file, line);
	abort();
}

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
}

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
	bh->elemSize = elemsize;
	bh->elemsPerBlock = elemsperblock;
	if(desc != NULL)
		bh->desc = rb_strdup(desc);

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
	lrb_assert(bh != NULL);
	if(rb_unlikely(bh == NULL))
	{
		rb_bh_fail("Cannot allocate if bh == NULL");
	}

	return (rb_malloc(bh->elemSize));
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

	rb_free(ptr);
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
	if(bh == NULL)
		return (1);

	rb_dlinkDelete(&bh->hlist, heap_lists);
	rb_free(bh->desc);
	rb_free(bh);

	return (0);
}

void
rb_bh_usage(rb_bh *bh, size_t *bused, size_t *bfree, size_t *bmemusage, const char **desc)
{
	if(bused != NULL)
		*bused = 0;
	if(bfree != NULL)
		*bfree = 0;
	if(bmemusage != NULL)
		*bmemusage = 0;
	if(desc != NULL)
		*desc = "no blockheap";
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
