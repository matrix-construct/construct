/*
 *  ircd-ratbox: A slightly useful ircd.
 *  memory.c: Memory utilities.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
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
 *  $Id: memory.c 388 2005-12-07 16:34:40Z nenolod $
 */


#define WE_ARE_MEMORY_C

#include "libcharybdis.h"

/*
 * MyMalloc - allocate memory, call outofmemory on failure
 */
void *
MyMalloc(size_t size)
{
	void *ret = calloc(1, size);
	if(ret == NULL)
		outofmemory();
	return ret;
}

/*
 * MyRealloc - reallocate memory, call outofmemory on failure
 */
void *
MyRealloc(void *x, size_t y)
{
	void *ret = realloc(x, y);

	if(ret == NULL)
		outofmemory();
	return ret;
}

/*
 * outofmemory()
 *
 * input        - NONE
 * output       - NONE
 * side effects - simply try to report there is a problem. Abort if it was called more than once
 */
void
outofmemory()
{
	static int was_here = 0;

	if(was_here)
		libcharybdis_die("Out of Memory!");

	was_here = 1;

	libcharybdis_restart("Aiee! Out of memory... >_<!");
}
