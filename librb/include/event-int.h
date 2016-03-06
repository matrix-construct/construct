/*
 *  ircd-ratbox: A slightly useful ircd.
 *  event-int.h: internal structs for events
 *
 *  Copyright (C) 2007 ircd-ratbox development team
 *  Copyright (C) 2007 Aaron Sethman <androsyn@ratbox.org>
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
 *  $Id: event-int.h 26272 2008-12-10 05:55:10Z androsyn $
 */

struct ev_entry
{
	rb_dlink_node node;
	EVH *func;
	void *arg;
	char *name;
	time_t frequency;
	time_t when;
	time_t next;
	void *data;
	void *comm_ptr;
};
void rb_event_io_register_all(void);
