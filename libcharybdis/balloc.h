/*
 *  ircd-ratbox: A slightly useful ircd.
 *  balloc.h: The ircd block allocator header.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
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
 *  $Id: balloc.h 1781 2006-07-30 18:07:38Z jilles $
 */

#ifndef INCLUDED_blalloc_h
#define INCLUDED_blalloc_h

#include "setup.h"
#include "tools.h"
#include "memory.h"
#include "ircd_defs.h"

#define CACHEFILE_HEAP_SIZE	32
#define CACHELINE_HEAP_SIZE	64


#ifdef NOBALLOC 
  	 
typedef struct BlockHeap BlockHeap; 	 
#define initBlockHeap() 	 
#define BlockHeapGarbageCollect(x) 	 
#define BlockHeapCreate(es, epb) ((BlockHeap*)(es)) 	 
#define BlockHeapDestroy(x) 	 
#define BlockHeapAlloc(x) MyMalloc((int)x) 	 
#define BlockHeapFree(x,y) MyFree(y) 	 
#define BlockHeapUsage(bh, bused, bfree, bmemusage) do { if (bused) (*(size_t *)bused) = 0; if (bfree) *((size_t *)bfree) = 0; if (bmemusage) *((size_t *)bmemusage) = 0; } while(0)
typedef struct MemBlock
{
	void *dummy;
} MemBlock;
 
#else

#undef DEBUG_BALLOC

#ifdef DEBUG_BALLOC
#define BALLOC_MAGIC 0x3d3a3c3d
#define BALLOC_FREE_MAGIC 0xafafafaf
#endif

/* status information for an allocated block in heap */
struct Block
{
	size_t alloc_size;
	struct Block *next;	/* Next in our chain of blocks */
	void *elems;		/* Points to allocated memory */
	dlink_list free_list;
	dlink_list used_list;
};
typedef struct Block Block;

struct MemBlock
{
#ifdef DEBUG_BALLOC
	unsigned long magic;
#endif
	dlink_node self;
	Block *block;		/* Which block we belong to */
};

typedef struct MemBlock MemBlock;

/* information for the root node of the heap */
struct BlockHeap
{
	dlink_node hlist;
	size_t elemSize;	/* Size of each element to be stored */
	unsigned long elemsPerBlock;	/* Number of elements per block */
	unsigned long blocksAllocated;	/* Number of blocks allocated */
	unsigned long freeElems;		/* Number of free elements */
	Block *base;		/* Pointer to first block */
};
typedef struct BlockHeap BlockHeap;

extern int BlockHeapFree(BlockHeap * bh, void *ptr);
extern void *BlockHeapAlloc(BlockHeap * bh);

extern BlockHeap *BlockHeapCreate(size_t elemsize, int elemsperblock);
extern int BlockHeapDestroy(BlockHeap * bh);

extern void initBlockHeap(void);
extern void BlockHeapUsage(BlockHeap * bh, size_t * bused, size_t * bfree, size_t * bmemusage);



#endif /* NOBALLOC */



#endif /* INCLUDED_blalloc_h */
