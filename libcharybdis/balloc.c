/*
 *  ircd-ratbox: A slightly useful ircd.
 *  balloc.c: A block allocator.
 *
 * Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 * Copyright (C) 1996-2002 Hybrid Development Team
 * Copyright (C) 2002-2005 ircd-ratbox development team
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 *
 *  $Id: balloc.c 388 2005-12-07 16:34:40Z nenolod $
 */

/* 
 * About the block allocator
 *
 * Basically we have three ways of getting memory off of the operating
 * system. Below are this list of methods and the order of preference.
 *
 * 1. mmap() anonymous pages with the MMAP_ANON flag.
 * 2. mmap() via the /dev/zero trick.
 * 3. malloc() 
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
 * Of course it is up to the caller to make sure BlockHeapGarbageCollect() gets
 * called periodically to do this cleanup, otherwise you'll keep the memory in the
 * process.
 *
 *
 */

#include "stdinc.h"
#include "libcharybdis.h"

#define WE_ARE_MEMORY_C
#include "setup.h"
#include "balloc.h"
#ifndef NOBALLOC

#include "ircd_defs.h"		/* DEBUG_BLOCK_ALLOCATOR */
#include "ircd.h"
#include "memory.h"
#include "irc_string.h"
#include "tools.h"
#include "s_log.h"
#include "client.h"
#include "event.h"

#ifdef HAVE_MMAP		/* We've got mmap() that is good */
#include <sys/mman.h>
/* HP-UX sucks */
#ifdef MAP_ANONYMOUS
#ifndef MAP_ANON
#define MAP_ANON MAP_ANONYMOUS
#endif
#endif
#endif

static int newblock(BlockHeap * bh);
static int BlockHeapGarbageCollect(BlockHeap *);
static void block_heap_gc(void *unused);
static dlink_list heap_lists;

#if defined(HAVE_MMAP) && !defined(MAP_ANON)
static int zero_fd = -1;
#endif

#define blockheap_fail(x) _blockheap_fail(x, __FILE__, __LINE__)

static void
_blockheap_fail(const char *reason, const char *file, int line)
{
	libcharybdis_log("Blockheap failure: %s (%s:%d)", reason, file, line);
	abort();
}
                

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
	free(ptr);
#endif
}

#ifdef DEBUG_BALLOC
/* Check the list length the very slow way */
static unsigned long
slow_list_length(dlink_list *list)
{
	dlink_node *ptr;
	unsigned long count = 0;
    
	for (ptr = list->head; ptr; ptr = ptr->next)
	{
		count++;
		if(count > list->length * 2)
		{
			blockheap_fail("count > list->length * 2 - I give up");
		}
	}
	return count;
}

static void
bh_sanity_check_block(BlockHeap *bh, Block *block)
{
	unsigned long s_used, s_free;
	s_used = slow_list_length(&block->used_list);
	s_free = slow_list_length(&block->free_list);
	if(s_used != dlink_list_length(&block->used_list))
		blockheap_fail("used link count doesn't match head count");
	if(s_free != dlink_list_length(&block->free_list))
		blockheap_fail("free link count doesn't match head count");
	
	if(dlink_list_length(&block->used_list) + dlink_list_length(&block->free_list) != bh->elemsPerBlock)
		blockheap_fail("used_list + free_list != elemsPerBlock");
}

#if 0
/* See how confused we are */
static void
bh_sanity_check(BlockHeap *bh)
{
	Block *walker;
	unsigned long real_alloc = 0;
	unsigned long s_used, s_free;
	unsigned long blockcount = 0;
	unsigned long allocated;
	if(bh == NULL)
		blockheap_fail("Trying to sanity check a NULL block");		
	
	allocated = bh->blocksAllocated * bh->elemsPerBlock;
	
	for(walker = bh->base; walker != NULL; walker = walker->next)
	{
		blockcount++;
		s_used = slow_list_length(&walker->used_list);
		s_free = slow_list_length(&walker->free_list);
		
		if(s_used != dlink_list_length(&walker->used_list))
			blockheap_fail("used link count doesn't match head count");
		if(s_free != dlink_list_length(&walker->free_list))
			blockheap_fail("free link count doesn't match head count");
		
		if(dlink_list_length(&walker->used_list) + dlink_list_length(&walker->free_list) != bh->elemsPerBlock)
			blockheap_fail("used_list + free_list != elemsPerBlock");

		real_alloc += dlink_list_length(&walker->used_list);
		real_alloc += dlink_list_length(&walker->free_list);
	}

	if(allocated != real_alloc)
		blockheap_fail("block allocations don't match heap");

	if(bh->blocksAllocated != blockcount)
		blockheap_fail("blocksAllocated don't match blockcount");

	 
}

static void
bh_sanity_check_all(void *unused)
{
        dlink_node *ptr;
	DLINK_FOREACH(ptr, heap_lists.head)
	{
		bh_sanity_check(ptr->data);
	}				
}
#endif

#endif



/*
 * void initBlockHeap(void)
 * 
 * Inputs: None
 * Outputs: None
 * Side Effects: Initializes the block heap
 */

void
initBlockHeap(void)
{
#if defined(HAVE_MMAP) && !defined(MAP_ANON)
	zero_fd = open("/dev/zero", O_RDWR);

	if(zero_fd < 0)
		blockheap_fail("Failed opening /dev/zero");
	comm_socket(zero_fd, FD_FILE, "Anonymous mmap()");
#endif
	eventAddIsh("block_heap_gc", block_heap_gc, NULL, 30);
}

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
	ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, zero_fd, 0);
#endif
	if(ptr == MAP_FAILED)
	{
		ptr = NULL;
	}
#else
	ptr = malloc(size);
#endif
	return (ptr);
}


static void
block_heap_gc(void *unused)
{
	dlink_node *ptr;
	DLINK_FOREACH(ptr, heap_lists.head)
	{
		BlockHeapGarbageCollect(ptr->data);
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
newblock(BlockHeap * bh)
{
	MemBlock *newblk;
	Block *b;
	unsigned long i;
	void *offset;

	/* Setup the initial data structure. */
	b = (Block *) calloc(1, sizeof(Block));
	if(b == NULL)
	{
		return (1);
	}
	b->free_list.head = b->free_list.tail = NULL;
	b->used_list.head = b->used_list.tail = NULL;
	b->next = bh->base;

	b->alloc_size = (bh->elemsPerBlock + 1) * (bh->elemSize + sizeof(MemBlock));

	b->elems = get_block(b->alloc_size);
	if(b->elems == NULL)
	{
		return (1);
	}
	offset = b->elems;
	/* Setup our blocks now */
	for (i = 0; i < bh->elemsPerBlock; i++)
	{
		void *data;
		newblk = (void *) offset;
		newblk->block = b;
#ifdef DEBUG_BALLOC
		newblk->magic = BALLOC_MAGIC;
#endif
		data = (void *) ((size_t) offset + sizeof(MemBlock));
		newblk->block = b;
		dlinkAdd(data, &newblk->self, &b->free_list);
		offset = (unsigned char *) ((unsigned char *) offset +
					    bh->elemSize + sizeof(MemBlock));
	}

	++bh->blocksAllocated;
	bh->freeElems += bh->elemsPerBlock;
	bh->base = b;

	return (0);
}


/* ************************************************************************ */
/* FUNCTION DOCUMENTATION:                                                  */
/*    BlockHeapCreate                                                       */
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
/*   Pointer to new BlockHeap, or NULL if unsuccessful                      */
/* ************************************************************************ */
BlockHeap *
BlockHeapCreate(size_t elemsize, int elemsperblock)
{
	BlockHeap *bh;
	s_assert(elemsize > 0 && elemsperblock > 0);

	/* Catch idiotic requests up front */
	if((elemsize <= 0) || (elemsperblock <= 0))
	{
		blockheap_fail("Attempting to BlockHeapCreate idiotic sizes");
	}

	/* Allocate our new BlockHeap */
	bh = (BlockHeap *) calloc(1, sizeof(BlockHeap));
	if(bh == NULL)
	{
		blockheap_fail("Attempt to calloc() failed");
		outofmemory();	/* die.. out of memory */
	}

	if((elemsize % sizeof(void *)) != 0)
	{
		/* Pad to even pointer boundary */
		elemsize += sizeof(void *);
		elemsize &= ~(sizeof(void *) - 1);
	}

	bh->elemSize = elemsize;
	bh->elemsPerBlock = elemsperblock;
	bh->blocksAllocated = 0;
	bh->freeElems = 0;
	bh->base = NULL;

	/* Be sure our malloc was successful */
	if(newblock(bh))
	{
		if(bh != NULL)
			free(bh);
		libcharybdis_restart("Aiee! -- newblock() failed!!!");
	}

	if(bh == NULL)
	{
		blockheap_fail("bh == NULL when it shouldn't be");
	}
	dlinkAdd(bh, &bh->hlist, &heap_lists);
	return (bh);
}

/* ************************************************************************ */
/* FUNCTION DOCUMENTATION:                                                  */
/*    BlockHeapAlloc                                                        */
/* Description:                                                             */
/*    Returns a pointer to a struct within our BlockHeap that's free for    */
/*    the taking.                                                           */
/* Parameters:                                                              */
/*    bh (IN):  Pointer to the Blockheap.                                   */
/* Returns:                                                                 */
/*    Pointer to a structure (void *), or NULL if unsuccessful.             */
/* ************************************************************************ */

void *
BlockHeapAlloc(BlockHeap * bh)
{
	Block *walker;
	dlink_node *new_node;

	s_assert(bh != NULL);
	if(bh == NULL)
	{
		blockheap_fail("Cannot allocate if bh == NULL");
	}

	if(bh->freeElems == 0)
	{
		/* Allocate new block and assign */
		/* newblock returns 1 if unsuccessful, 0 if not */

		if(newblock(bh))
		{
			/* That didn't work..try to garbage collect */
			BlockHeapGarbageCollect(bh);
			if(bh->freeElems == 0)
			{
				libcharybdis_restart("newblock() failed and garbage collection didn't help");
			}
		}
	}

	for (walker = bh->base; walker != NULL; walker = walker->next)
	{
		if(dlink_list_length(&walker->free_list) > 0)
		{
#ifdef DEBUG_BALLOC
			bh_sanity_check_block(bh, walker);
#endif
			bh->freeElems--;
			new_node = walker->free_list.head;
			dlinkMoveNode(new_node, &walker->free_list, &walker->used_list);
			s_assert(new_node->data != NULL);
			if(new_node->data == NULL)
				blockheap_fail("new_node->data is NULL and that shouldn't happen!!!");
			memset(new_node->data, 0, bh->elemSize);
#ifdef DEBUG_BALLOC
			do
			{
				struct MemBlock *memblock = (void *) ((size_t) new_node->data - sizeof(MemBlock));
				if(memblock->magic == BALLOC_FREE_MAGIC)
					memblock->magic = BALLOC_MAGIC;
			
			} while(0);
			bh_sanity_check_block(bh, walker);
#endif
			return (new_node->data);
		}
	}
	blockheap_fail("BlockHeapAlloc failed, giving up");
	return NULL;
}


/* ************************************************************************ */
/* FUNCTION DOCUMENTATION:                                                  */
/*    BlockHeapFree                                                         */
/* Description:                                                             */
/*    Returns an element to the free pool, does not free()                  */
/* Parameters:                                                              */
/*    bh (IN): Pointer to BlockHeap containing element                      */
/*    ptr (in):  Pointer to element to be "freed"                           */
/* Returns:                                                                 */
/*    0 if successful, 1 if element not contained within BlockHeap.         */
/* ************************************************************************ */
int
BlockHeapFree(BlockHeap * bh, void *ptr)
{
	Block *block;
	struct MemBlock *memblock;

	s_assert(bh != NULL);
	s_assert(ptr != NULL);

	if(bh == NULL)
	{
		libcharybdis_restart("balloc.c:BlockHeapFree() bh == NULL");
		return (1);
	}

	if(ptr == NULL)
	{
		libcharybdis_restart("balloc.BlockHeapFree() ptr == NULL");
		return (1);
	}

	memblock = (void *) ((size_t) ptr - sizeof(MemBlock));
#ifdef DEBUG_BALLOC
	if(memblock->magic == BALLOC_FREE_MAGIC)
	{
		blockheap_fail("double free of a block");
		outofmemory();
	} else 
	if(memblock->magic != BALLOC_MAGIC)
	{
		blockheap_fail("memblock->magic != BALLOC_MAGIC");
		outofmemory();
	}
#endif
	s_assert(memblock->block != NULL);
	if(memblock->block == NULL)
	{
		blockheap_fail("memblock->block == NULL, not a valid block?");
		outofmemory();
	}

	block = memblock->block;
#ifdef DEBUG_BALLOC
	bh_sanity_check_block(bh, block);
#endif
	bh->freeElems++;
	mem_frob(ptr, bh->elemSize);
	dlinkMoveNode(&memblock->self, &block->used_list, &block->free_list);
#ifdef DEBUG_BALLOC
	bh_sanity_check_block(bh, block);
#endif
	return (0);
}

/* ************************************************************************ */
/* FUNCTION DOCUMENTATION:                                                  */
/*    BlockHeapGarbageCollect                                               */
/* Description:                                                             */
/*    Performs garbage collection on the block heap.  Any blocks that are   */
/*    completely unallocated are removed from the heap.  Garbage collection */
/*    will never remove the root node of the heap.                          */
/* Parameters:                                                              */
/*    bh (IN):  Pointer to the BlockHeap to be cleaned up                   */
/* Returns:                                                                 */
/*   0 if successful, 1 if bh == NULL                                       */
/* ************************************************************************ */
static int
BlockHeapGarbageCollect(BlockHeap * bh)
{
	Block *walker, *last;
	if(bh == NULL)
	{
		return (1);
	}

	if(bh->freeElems < bh->elemsPerBlock || bh->blocksAllocated == 1)
	{
		/* There couldn't possibly be an entire free block.  Return. */
		return (0);
	}

	last = NULL;
	walker = bh->base;

	while (walker != NULL)
	{
		if((dlink_list_length(&walker->free_list) == bh->elemsPerBlock) != 0)
		{
			free_block(walker->elems, walker->alloc_size);
			if(last != NULL)
			{
				last->next = walker->next;
				if(walker != NULL)
					free(walker);
				walker = last->next;
			}
			else
			{
				bh->base = walker->next;
				if(walker != NULL)
					free(walker);
				walker = bh->base;
			}
			bh->blocksAllocated--;
			bh->freeElems -= bh->elemsPerBlock;
		}
		else
		{
			last = walker;
			walker = walker->next;
		}
	}
	return (0);
}

/* ************************************************************************ */
/* FUNCTION DOCUMENTATION:                                                  */
/*    BlockHeapDestroy                                                      */
/* Description:                                                             */
/*    Completely free()s a BlockHeap.  Use for cleanup.                     */
/* Parameters:                                                              */
/*    bh (IN):  Pointer to the BlockHeap to be destroyed.                   */
/* Returns:                                                                 */
/*   0 if successful, 1 if bh == NULL                                       */
/* ************************************************************************ */
int
BlockHeapDestroy(BlockHeap * bh)
{
	Block *walker, *next;

	if(bh == NULL)
	{
		return (1);
	}

	for (walker = bh->base; walker != NULL; walker = next)
	{
		next = walker->next;
		free_block(walker->elems, walker->alloc_size);
		if(walker != NULL)
			free(walker);
	}
	dlinkDelete(&bh->hlist, &heap_lists);
	free(bh);
	return (0);
}

void
BlockHeapUsage(BlockHeap * bh, size_t * bused, size_t * bfree, size_t * bmemusage)
{
	size_t used;
	size_t freem;
	size_t memusage;
	if(bh == NULL)
	{
		return;
	}

	freem = bh->freeElems;
	used = (bh->blocksAllocated * bh->elemsPerBlock) - bh->freeElems;
	memusage = used * (bh->elemSize + sizeof(MemBlock));

	if(bused != NULL)
		*bused = used;
	if(bfree != NULL)
		*bfree = freem;
	if(bmemusage != NULL)
		*bmemusage = memusage;
}

#endif /* NOBALLOC */

