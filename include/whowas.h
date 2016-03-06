/*
 *  ircd-ratbox: A slightly useful ircd.
 *  whowas.h: Header for the whowas functions.
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
 */
#ifndef INCLUDED_whowas_h
#define INCLUDED_whowas_h

#include "ircd_defs.h"
#include "client.h"

#include "setup.h"

struct User;
struct Client;

/*
  lets speed this up...
  also removed away information. *tough*
  - Dianora
 */
struct Whowas
{
	struct whowas_top *wtop;
	rb_dlink_node wnode;		/* for the wtop linked list */
	rb_dlink_node cnode;		/* node for online clients */
	rb_dlink_node whowas_node;	/* node for the whowas linked list */
	char name[NICKLEN + 1];
	char username[USERLEN + 1];
	char hostname[HOSTLEN + 1];
	char sockhost[HOSTIPLEN + 1];
	char realname[REALLEN + 1];
	char suser[NICKLEN + 1];
	unsigned char flags;
	const char *servername;
	time_t logoff;
	struct Client *online;	/* Pointer to new nickname for chasing or NULL */
};

/* Flags */
#define WHOWAS_IP_SPOOFING	0x1
#define WHOWAS_DYNSPOOF		0x2

/*
** initwhowas
*/
extern void whowas_init(void);

/*
** add_history
**      Add the currently defined name of the client to history.
**      usually called before changing to a new name (nick).
**      Client must be a fully registered user (specifically,
**      the user structure must have been allocated).
*/
void whowas_add_history(struct Client *, int);

/*
** off_history
**      This must be called when the client structure is about to
**      be released. History mechanism keeps pointers to client
**      structures and it must know when they cease to exist. This
**      also implicitly calls AddHistory.
*/
void whowas_off_history(struct Client *);

/*
** get_history
**      Return the current client that was using the given
**      nickname within the timelimit. Returns NULL, if no
**      one found...
*/
struct Client *whowas_get_history(const char *, time_t);
					/* Nick name */
					/* Time limit in seconds */

rb_dlink_list *whowas_get_list(const char *name);
void whowas_set_size(int whowas_length);
void whowas_memory_usage(size_t *count, size_t *memused);

#endif /* INCLUDED_whowas_h */
