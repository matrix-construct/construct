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
 *  $Id: reject.c 3456 2007-05-18 19:14:18Z jilles $
 */

#include "stdinc.h"
#include "config.h"
#include "client.h"
#include "s_conf.h"
#include "reject.h"
#include "s_stats.h"
#include "msg.h"
#include "hash.h"

static rb_patricia_tree_t *reject_tree;
rb_dlink_list delay_exit;
static rb_dlink_list reject_list;

static rb_patricia_tree_t *unknown_tree;

struct reject_data
{
	rb_dlink_node rnode;
	time_t time;
	unsigned int count;
	uint32_t mask_hashv;
};

static void
reject_exit(void *unused)
{
	struct Client *client_p;
	rb_dlink_node *ptr, *ptr_next;

	RB_DLINK_FOREACH_SAFE(ptr, ptr_next, delay_exit.head)
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
		{
			if(IsExUnknown(client_p))
				sendto_one(client_p, "ERROR :Closing Link: %s (*** Too many unknown connections)", client_p->host);
			else
				sendto_one(client_p, "ERROR :Closing Link: %s (*** Banned (cache))", client_p->host);
		}
 	  	close_connection(client_p);
        	SetDead(client_p);
        	rb_dlinkAddAlloc(client_p, &dead_list);
	}

        delay_exit.head = delay_exit.tail = NULL;
        delay_exit.length = 0;
}

static void
reject_expires(void *unused)
{
	rb_dlink_node *ptr, *next;
	rb_patricia_node_t *pnode;
	struct reject_data *rdata;
	
	RB_DLINK_FOREACH_SAFE(ptr, next, reject_list.head)
	{
		pnode = ptr->data;
		rdata = pnode->data;		

		if(rdata->time + ConfigFileEntry.reject_duration > rb_current_time())
			continue;

		rb_dlinkDelete(ptr, &reject_list);
		rb_free(rdata);
		rb_patricia_remove(reject_tree, pnode);
	}
}

void
init_reject(void)
{
	reject_tree = rb_new_patricia(PATRICIA_BITS);
	unknown_tree = rb_new_patricia(PATRICIA_BITS);
	rb_event_add("reject_exit", reject_exit, NULL, DELAYED_EXIT_TIME);
	rb_event_add("reject_expires", reject_expires, NULL, 60);
}


void
add_reject(struct Client *client_p, const char *mask1, const char *mask2)
{
	rb_patricia_node_t *pnode;
	struct reject_data *rdata;
	uint32_t hashv;

	/* Reject is disabled */
	if(ConfigFileEntry.reject_after_count == 0 || ConfigFileEntry.reject_ban_time == 0)
		return;

	hashv = 0;
	if (mask1 != NULL)
		hashv ^= fnv_hash_upper(mask1, 32);
	if (mask2 != NULL)
		hashv ^= fnv_hash_upper(mask2, 32);

	if((pnode = rb_match_ip(reject_tree, (struct sockaddr *)&client_p->localClient->ip)) != NULL)
	{
		rdata = pnode->data;
		rdata->time = rb_current_time();
		rdata->count++;
	}
	else
	{
		int bitlen = 32;
#ifdef RB_IPV6
		if(client_p->localClient->ip.ss_family == AF_INET6)
			bitlen = 128;
#endif
		pnode = make_and_lookup_ip(reject_tree, (struct sockaddr *)&client_p->localClient->ip, bitlen);
		pnode->data = rdata = rb_malloc(sizeof(struct reject_data));
		rb_dlinkAddTail(pnode, &rdata->rnode, &reject_list);
		rdata->time = rb_current_time();
		rdata->count = 1;
	}
	rdata->mask_hashv = hashv;
}

int
check_reject(struct Client *client_p)
{
	rb_patricia_node_t *pnode;
	struct reject_data *rdata;
	
	/* Reject is disabled */
	if(ConfigFileEntry.reject_after_count == 0 || ConfigFileEntry.reject_ban_time == 0 ||
	   ConfigFileEntry.reject_duration == 0)
		return 0;
		
	pnode = rb_match_ip(reject_tree, (struct sockaddr *)&client_p->localClient->ip);
	if(pnode != NULL)
	{
		rdata = pnode->data;

		rdata->time = rb_current_time();
		if(rdata->count > ConfigFileEntry.reject_after_count)
		{
			ServerStats.is_rej++;
			SetReject(client_p);
			rb_setselect(client_p->localClient->F, RB_SELECT_WRITE | RB_SELECT_READ, NULL, NULL);
			SetClosing(client_p);
			rb_dlinkMoveNode(&client_p->localClient->tnode, &unknown_list, &delay_exit);
			return 1;
		}
	}	
	/* Caller does what it wants */	
	return 0;
}

void 
flush_reject(void)
{
	rb_dlink_node *ptr, *next;
	rb_patricia_node_t *pnode;
	struct reject_data *rdata;
	
	RB_DLINK_FOREACH_SAFE(ptr, next, reject_list.head)
	{
		pnode = ptr->data;
		rdata = pnode->data;
		rb_dlinkDelete(ptr, &reject_list);
		rb_free(rdata);
		rb_patricia_remove(reject_tree, pnode);
	}
}

int 
remove_reject_ip(const char *ip)
{
	rb_patricia_node_t *pnode;
	
	/* Reject is disabled */
	if(ConfigFileEntry.reject_after_count == 0 || ConfigFileEntry.reject_ban_time == 0 ||
	   ConfigFileEntry.reject_duration == 0)
		return -1;

	if((pnode = rb_match_string(reject_tree, ip)) != NULL)
	{
		struct reject_data *rdata = pnode->data;
		rb_dlinkDelete(&rdata->rnode, &reject_list);
		rb_free(rdata);
		rb_patricia_remove(reject_tree, pnode);
		return 1;
	}
	return 0;
}

int
remove_reject_mask(const char *mask1, const char *mask2)
{
	rb_dlink_node *ptr, *next;
	rb_patricia_node_t *pnode;
	struct reject_data *rdata;
	uint32_t hashv;
	int n = 0;
	
	hashv = 0;
	if (mask1 != NULL)
		hashv ^= fnv_hash_upper(mask1, 32);
	if (mask2 != NULL)
		hashv ^= fnv_hash_upper(mask2, 32);
	RB_DLINK_FOREACH_SAFE(ptr, next, reject_list.head)
	{
		pnode = ptr->data;
		rdata = pnode->data;
		if (rdata->mask_hashv == hashv)
		{
			rb_dlinkDelete(ptr, &reject_list);
			rb_free(rdata);
			rb_patricia_remove(reject_tree, pnode);
			n++;
		}
	}
	return n;
}


int
add_unknown_ip(struct Client *client_p)
{
	rb_patricia_node_t *pnode;

	if((pnode = rb_match_ip(unknown_tree, (struct sockaddr *)&client_p->localClient->ip)) == NULL)
	{
		int bitlen = 32;
#ifdef RB_IPV6
		if(client_p->localClient->ip.ss_family == AF_INET6)
			bitlen = 128;
#endif
		pnode = make_and_lookup_ip(unknown_tree, (struct sockaddr *)&client_p->localClient->ip, bitlen);
		pnode->data = (void *)0;
	}

	if((unsigned long)pnode->data >= ConfigFileEntry.max_unknown_ip)
	{
		SetExUnknown(client_p);
		SetReject(client_p);
		rb_setselect(client_p->localClient->F, RB_SELECT_WRITE | RB_SELECT_READ, NULL, NULL);
		SetClosing(client_p);
		rb_dlinkMoveNode(&client_p->localClient->tnode, &unknown_list, &delay_exit);
		return 1;
	}

	pnode->data = (void *)((unsigned long)pnode->data + 1);

	return 0;
}

void
del_unknown_ip(struct Client *client_p)
{
	rb_patricia_node_t *pnode;

	if((pnode = rb_match_ip(unknown_tree, (struct sockaddr *)&client_p->localClient->ip)) != NULL)
	{
		pnode->data = (void *)((unsigned long)pnode->data - 1);
		if((unsigned long)pnode->data <= 0)
		{
			rb_patricia_remove(unknown_tree, pnode);
		}
	}
	/* this can happen due to m_webirc.c's manipulations, for example */
}
