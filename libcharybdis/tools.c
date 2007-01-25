/*
 *  ircd-ratbox: A slightly useful ircd.
 *  tools.c: Various functions needed here and there.
 *
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
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
 *  $Id: tools.c 1110 2006-03-29 22:55:25Z nenolod $
 *
 *  Here is the original header:
 *
 *  Useful stuff, ripped from places ..
 *  adrian chadd <adrian@creative.net.au>
 *
 * When you update these functions make sure you update the ones in tools.h
 * as well!!!
 */

#include "stdinc.h"
#define TOOLS_C
#include "tools.h"
#include "balloc.h"
#include "s_user.h"

#ifndef NDEBUG
/*
 * frob some memory. debugging time.
 * -- adrian
 */
void
mem_frob(void *data, int len)
{
	unsigned long x = 0xdeadbeef;
	unsigned char *b = (unsigned char *)&x;
	int i;
	char *cdata = data;
	for (i = 0; i < len; i++)
	{
		*cdata = b[i % 4];
		cdata++;
	}
}
#endif

/*
 * init_dlink_nodes
 *
 */
extern BlockHeap *dnode_heap;
void
init_dlink_nodes(void)
{
	dnode_heap = BlockHeapCreate(sizeof(dlink_node), DNODE_HEAP_SIZE);
	if(dnode_heap == NULL)
		outofmemory();
}

/*
 * make_dlink_node
 *
 * inputs	- NONE
 * output	- pointer to new dlink_node
 * side effects	- NONE
 */
dlink_node *
make_dlink_node(void)
{
	return(BlockHeapAlloc(dnode_heap));
}

/*
 * free_dlink_node
 *
 * inputs	- pointer to dlink_node
 * output	- NONE
 * side effects	- free given dlink_node 
 */
void
free_dlink_node(dlink_node * ptr)
{
	assert(ptr != NULL);

	BlockHeapFree(dnode_heap, ptr);
}

/*
 * find_umode_slot
 *
 * inputs       - NONE
 * outputs      - an available umode bitmask or
 *                0 if no umodes are available
 * side effects - NONE
 */
unsigned int
find_umode_slot(void)
{
	unsigned int all_umodes = 0, my_umode = 0, i;

	for (i = 0; i < 128; i++)
		all_umodes |= user_modes[i];

	for (my_umode = 1; my_umode && (all_umodes & my_umode);
		my_umode <<= 1);

	return my_umode;
}
