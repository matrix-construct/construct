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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 *  $Id: rb_memory.c 26092 2008-09-19 15:13:52Z androsyn $
 */
#include <libratbox_config.h>
#include <ratbox_lib.h>

void
rb_outofmemory(void)
{
	static int was_here = 0;

	if(was_here)
		abort();

	was_here = 1;

	rb_lib_log("Out of memory: restarting server...");
	rb_lib_restart("Out of Memory");
}
