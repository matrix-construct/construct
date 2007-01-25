/*
 *  ircd-ratbox: A slightly useful ircd.
 *  s_gline.c: GLine global ban functions.
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
 *  $Id: s_gline.c 254 2005-09-21 23:35:12Z nenolod $
 */

#include "stdinc.h"
#include "tools.h"
#include "channel.h"
#include "client.h"
#include "common.h"
#include "config.h"
#include "irc_string.h"
#include "ircd.h"
#include "hostmask.h"
#include "numeric.h"
#include "commio.h"
#include "s_conf.h"
#include "scache.h"
#include "send.h"
#include "msg.h"
#include "s_serv.h"
#include "s_gline.h"
#include "hash.h"
#include "event.h"
#include "memory.h"

dlink_list glines;

static void expire_glines(void);
static void expire_pending_glines(void);

/* add_gline
 *
 * inputs       - pointer to struct ConfItem
 * output       - none
 * Side effects - links in given struct ConfItem into gline link list
 */
void
add_gline(struct ConfItem *aconf)
{
	dlinkAddTailAlloc(aconf, &glines);
	add_conf_by_address(aconf->host, CONF_GLINE, aconf->user, aconf);
}

/*
 * find_is_glined
 * inputs       - hostname
 *              - username
 * output       - pointer to struct ConfItem if user@host glined
 * side effects -
 */
struct ConfItem *
find_is_glined(const char *host, const char *user)
{
	dlink_node *gline_node;
	struct ConfItem *kill_ptr;

	DLINK_FOREACH(gline_node, glines.head)
	{
		kill_ptr = gline_node->data;
		if((kill_ptr->user && (!user || match(kill_ptr->user, user)))
		   && (kill_ptr->host && (!host || match(kill_ptr->host, host))))
		{
			return (kill_ptr);
		}
	}

	return (NULL);
}

/*
 * cleanup_glines
 *
 * inputs	- NONE
 * output	- NONE
 * side effects - expire gline lists
 *                This is an event started off in ircd.c
 */
void
cleanup_glines(void *unused)
{
	expire_glines();
	expire_pending_glines();
}

/*
 * expire_glines
 * 
 * inputs       - NONE
 * output       - NONE
 * side effects -
 *
 * Go through the gline list, expire any needed.
 */
static void
expire_glines()
{
	dlink_node *gline_node;
	dlink_node *next_node;
	struct ConfItem *kill_ptr;

	DLINK_FOREACH_SAFE(gline_node, next_node, glines.head)
	{
		kill_ptr = gline_node->data;

		/* these are in chronological order */
		if(kill_ptr->hold > CurrentTime)
			break;

		dlinkDestroy(gline_node, &glines);
		delete_one_address_conf(kill_ptr->host, kill_ptr);
	}
}

/*
 * expire_pending_glines
 * 
 * inputs       - NONE
 * output       - NONE
 * side effects -
 *
 * Go through the pending gline list, expire any that haven't had
 * enough "votes" in the time period allowed
 */
static void
expire_pending_glines()
{
	dlink_node *pending_node;
	dlink_node *next_node;
	struct gline_pending *glp_ptr;

	DLINK_FOREACH_SAFE(pending_node, next_node, pending_glines.head)
	{
		glp_ptr = pending_node->data;

		if(((glp_ptr->last_gline_time + GLINE_PENDING_EXPIRE) <=
		    CurrentTime) || find_is_glined(glp_ptr->host, glp_ptr->user))

		{
			MyFree(glp_ptr->reason1);
			MyFree(glp_ptr->reason2);
			MyFree(glp_ptr);
			dlinkDestroy(pending_node, &pending_glines);
		}
	}
}
