/*
 *  ircd-ratbox: A slightly useful ircd
 *  reject.c: reject users with prejudice
 *
 *  Copyright (C) 2003 Aaron Sethman <androsyn@ratbox.org>
 *  Copyright (C) 2003-2005 ircd-ratbox development team
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
 *  $Id: reject.c 849 2006-02-15 01:33:43Z jilles $
 */

#include "stdinc.h"
#include "config.h"
#include "patricia.h"
#include "client.h"
#include "s_conf.h"
#include "event.h"
#include "tools.h"
#include "reject.h"
#include "s_stats.h"
#include "msg.h"

static patricia_tree_t *reject_tree;
dlink_list delay_exit;
static dlink_list reject_list;

struct reject_data
{
	dlink_node rnode;
	time_t time;
	unsigned int count;
};

static void
reject_exit(void *unused)
{
	struct Client *client_p;
	dlink_node *ptr, *ptr_next;

	DLINK_FOREACH_SAFE(ptr, ptr_next, delay_exit.head)
	{
		client_p = ptr->data;
	  	if(IsDead(client_p))
                	continue;

		/* this MUST be here, to prevent the possibility
		 * sendto_one() generates a write error, and then a client
		 * ends up on the dead_list and the abort_list --fl
		 *
		 * new disconnect notice stolen from ircu --nenolod
		 * no, this only happens when someone's IP has some
		 * ban on it and rejects them rather longer than the
		 * ircu message suggests --jilles
		 */
		if(!IsIOError(client_p))
			sendto_one(client_p, "ERROR :Closing Link: %s (*** Banned (cache))", client_p->host);

 	  	close_connection(client_p);
        	SetDead(client_p);
        	dlinkAddAlloc(client_p, &dead_list);
	}

        delay_exit.head = delay_exit.tail = NULL;
        delay_exit.length = 0;
}

static void
reject_expires(void *unused)
{
	dlink_node *ptr, *next;
	patricia_node_t *pnode;
	struct reject_data *rdata;
	
	DLINK_FOREACH_SAFE(ptr, next, reject_list.head)
	{
		pnode = ptr->data;
		rdata = pnode->data;		

		if(rdata->time + ConfigFileEntry.reject_duration > CurrentTime)
			continue;

		dlinkDelete(ptr, &reject_list);
		MyFree(rdata);
		patricia_remove(reject_tree, pnode);
	}
}

void
init_reject(void)
{
	reject_tree = New_Patricia(PATRICIA_BITS);
	eventAdd("reject_exit", reject_exit, NULL, DELAYED_EXIT_TIME);
	eventAdd("reject_expires", reject_expires, NULL, 60);
}


void
add_reject(struct Client *client_p)
{
	patricia_node_t *pnode;
	struct reject_data *rdata;

	/* Reject is disabled */
	if(ConfigFileEntry.reject_after_count == 0 || ConfigFileEntry.reject_ban_time == 0)
		return;

	if((pnode = match_ip(reject_tree, (struct sockaddr *)&client_p->localClient->ip)) != NULL)
	{
		rdata = pnode->data;
		rdata->time = CurrentTime;
		rdata->count++;
	}
	else
	{
		int bitlen = 32;
#ifdef IPV6
		if(client_p->localClient->ip.ss_family == AF_INET6)
			bitlen = 128;
#endif
		pnode = make_and_lookup_ip(reject_tree, (struct sockaddr *)&client_p->localClient->ip, bitlen);
		pnode->data = rdata = MyMalloc(sizeof(struct reject_data));
		dlinkAddTail(pnode, &rdata->rnode, &reject_list);
		rdata->time = CurrentTime;
		rdata->count = 1;
	}
}

int
check_reject(struct Client *client_p)
{
	patricia_node_t *pnode;
	struct reject_data *rdata;
	
	/* Reject is disabled */
	if(ConfigFileEntry.reject_after_count == 0 || ConfigFileEntry.reject_ban_time == 0 ||
	   ConfigFileEntry.reject_duration == 0)
		return 0;
		
	pnode = match_ip(reject_tree, (struct sockaddr *)&client_p->localClient->ip);
	if(pnode != NULL)
	{
		rdata = pnode->data;

		rdata->time = CurrentTime;
		if(rdata->count > ConfigFileEntry.reject_after_count)
		{
			ServerStats->is_rej++;
			SetReject(client_p);
			comm_setselect(client_p->localClient->fd, FDLIST_NONE, COMM_SELECT_WRITE | COMM_SELECT_READ, NULL, NULL, 0);
			SetClosing(client_p);
			dlinkMoveNode(&client_p->localClient->tnode, &unknown_list, &delay_exit);
			return 1;
		}
	}	
	/* Caller does what it wants */	
	return 0;
}

void 
flush_reject(void)
{
	dlink_node *ptr, *next;
	patricia_node_t *pnode;
	struct reject_data *rdata;
	
	DLINK_FOREACH_SAFE(ptr, next, reject_list.head)
	{
		pnode = ptr->data;
		rdata = pnode->data;
		dlinkDelete(ptr, &reject_list);
		MyFree(rdata);
		patricia_remove(reject_tree, pnode);
	}
}

int 
remove_reject(const char *ip)
{
	patricia_node_t *pnode;
	
	/* Reject is disabled */
	if(ConfigFileEntry.reject_after_count == 0 || ConfigFileEntry.reject_ban_time == 0 ||
	   ConfigFileEntry.reject_duration == 0)
		return -1;

	if((pnode = match_string(reject_tree, ip)) != NULL)
	{
		struct reject_data *rdata = pnode->data;
		dlinkDelete(&rdata->rnode, &reject_list);
		MyFree(rdata);
		patricia_remove(reject_tree, pnode);
		return 1;
	}
	return 0;
}

