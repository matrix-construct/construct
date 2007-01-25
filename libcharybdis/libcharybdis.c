/*
 *  charybdis: A slightly useful ircd.
 *  libcharybdis.c: library entrypoint
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
 *  Copyright (C) 2005 William Pitcock and Jilles Tjoelker
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
 *  $Id: libcharybdis.c 386 2005-12-07 16:21:24Z nenolod $
 */

#include "stdinc.h"
#include "libcharybdis.h"

static void (*log_callback)(const char *str) = NULL;
static void (*restart_callback)(const char *str) = NULL;
static void (*die_callback)(const char *str) = NULL;
static char errbuf[BUFSIZE * 2];

void libcharybdis_log(const char *str, ...)
{
	va_list args;

	if (log_callback == NULL)
		return;

	va_start(args, str);
	ircvsnprintf(errbuf, BUFSIZE * 2, str, args);
	va_end(args);

	log_callback(errbuf);
}

void libcharybdis_restart(const char *str, ...)
{
	va_list args;

	if (restart_callback == NULL)
		return;

	va_start(args, str);
	ircvsnprintf(errbuf, BUFSIZE * 2, str, args);
	va_end(args);

	restart_callback(errbuf);
}

void libcharybdis_die(const char *str, ...)
{
	va_list args;

	if (die_callback == NULL)
		return;

	va_start(args, str);
	ircvsnprintf(errbuf, BUFSIZE * 2, str, args);
	va_end(args);

	die_callback(errbuf);
}

void libcharybdis_init(void (*log_cb)(const char *str),
	void (*restart_cb)(const char *str), void (*die_cb)(const char *str))
{
	log_callback = log_cb;
	restart_callback = restart_cb;
	die_callback = die_cb;

	fdlist_init();
	init_netio();
	eventInit();
	initBlockHeap();
	init_dlink_nodes();
	linebuf_init();
}
