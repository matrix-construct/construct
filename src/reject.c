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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 *  $Id: reject.c 25119 2008-03-13 16:57:05Z androsyn $
 */

#include "stdinc.h"
#include "client.h"
#include "s_conf.h"
#include "reject.h"
#include "s_stats.h"
#include "ircd.h"
#include "send.h"
#include "numeric.h"
#include "parse.h"
#include "hostmask.h"
#include "match.h"
#include "hash.h"

static rb_patricia_tree_t *reject_tree;
static rb_dlink_list delay_exit;
static rb_dlink_list reject_list;
static rb_dlink_list throttle_list;
static rb_patricia_tree_t *throttle_tree;
static void throttle_expires(void *unused);


typedef struct _reject_data
{
	rb_dlink_node rnode;
	time_t time;
	unsigned int count;
	uint32_t mask_hashv;
} reject_t;

typedef struct _delay_data
{
	rb_dlink_node node;
	rb_fde_t *F;
} delay_t;

typedef struct _throttle
{
	rb_dlink_node node;
	time_t last;
	int count;
} throttle_t;

unsigned long
delay_exit_length(void)
{
	return rb_dlink_list_length(&delay_exit);
}

static void
reject_exit(void *unused)
{
	rb_dlink_node *ptr, *ptr_next;
	delay_t *ddata;
	static const char *errbuf = "ERROR :Closing Link: (*** Banned (cache))\r\n";
	
	RB_DLINK_FOREACH_SAFE(ptr, ptr_next, delay_exit.head)
	{
		ddata = ptr->data;

		rb_write(ddata->F, errbuf, strlen(errbuf));		
		rb_close(ddata->F);
		rb_free(ddata);
	}

	delay_exit.head = delay_exit.tail = NULL;
	delay_exit.length = 0;
}

static void
reject_expires(void *unused)
{
	rb_dlink_node *ptr, *next;
	rb_patricia_node_t *pnode;
	reject_t *rdata;
	
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
	throttle_tree = rb_new_patricia(PATRICIA_BITS);
	rb_event_add("reject_exit", reject_exit, NULL, DELAYED_EXIT_TIME);
	rb_event_add("reject_expires", reject_expires, NULL, 60);
	rb_event_add("throttle_expires", throttle_expires, NULL, 10);
}

unsigned long
throttle_size(void)
{
	unsigned long count;
	rb_dlink_node *ptr;
	rb_patricia_node_t *pnode;
	throttle_t *t;

	count = 0;
	RB_DLINK_FOREACH(ptr, throttle_list.head)
	{
		pnode = ptr->data;
		t = pnode->data;
		if (t->count > ConfigFileEntry.throttle_count)
			count++;
	}

	return count;
}

void
add_reject(struct Client *client_p, const char *mask1, const char *mask2)
{
	rb_patricia_node_t *pnode;
	reject_t *rdata;
	uint32_t hashv;

	/* Reject is disabled */
	if(ConfigFileEntry.reject_after_count == 0 || ConfigFileEntry.reject_duration == 0)
		return;

	hashv = 0;
	if (mask1 != NULL)
		hashv ^= fnv_hash_upper((const unsigned char *)mask1, 32);
	if (mask2 != NULL)
		hashv ^= fnv_hash_upper((const unsigned char *)mask2, 32);

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
		if(GET_SS_FAMILY(&client_p->localClient->ip) == AF_INET6)
			bitlen = 128;
#endif
		pnode = make_and_lookup_ip(reject_tree, (struct sockaddr *)&client_p->localClient->ip, bitlen);
		pnode->data = rdata = rb_malloc(sizeof(reject_t));
		rb_dlinkAddTail(pnode, &rdata->rnode, &reject_list);
		rdata->time = rb_current_time();
		rdata->count = 1;
	}
	rdata->mask_hashv = hashv;
}

int
check_reject(rb_fde_t *F, struct sockaddr *addr)
{
	rb_patricia_node_t *pnode;
	reject_t *rdata;
	delay_t *ddata;
	/* Reject is disabled */
	if(ConfigFileEntry.reject_after_count == 0 || ConfigFileEntry.reject_duration == 0)
		return 0;
		
	pnode = rb_match_ip(reject_tree, addr);
	if(pnode != NULL)
	{
		rdata = pnode->data;

		rdata->time = rb_current_time();
		if(rdata->count > (unsigned long)ConfigFileEntry.reject_after_count)
		{
			ddata = rb_malloc(sizeof(delay_t));
			ServerStats.is_rej++;
			rb_setselect(F, RB_SELECT_WRITE | RB_SELECT_READ, NULL, NULL);
			ddata->F = F;
			rb_dlinkAdd(ddata, &ddata->node, &delay_exit);
			return 1;
		}
	}	
	/* Caller does what it wants */	
	return 0;
}

int
is_reject_ip(struct sockaddr *addr)
{
	rb_patricia_node_t *pnode;
	reject_t *rdata;
	int duration;

	/* Reject is disabled */
	if(ConfigFileEntry.reject_after_count == 0 || ConfigFileEntry.reject_duration == 0)
		return 0;
		
	pnode = rb_match_ip(reject_tree, addr);
	if(pnode != NULL)
	{
		rdata = pnode->data;

		if(rdata->count > (unsigned long)ConfigFileEntry.reject_after_count)
		{
			duration = rdata->time + ConfigFileEntry.reject_duration - rb_current_time();
			return duration > 0 ? duration : 1;
		}
	}	
	return 0;
}

void 
flush_reject(void)
{
	rb_dlink_node *ptr, *next;
	rb_patricia_node_t *pnode;
	reject_t *rdata;
	
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
	if(ConfigFileEntry.reject_after_count == 0 || ConfigFileEntry.reject_duration == 0)
		return -1;

	if((pnode = rb_match_string(reject_tree, ip)) != NULL)
	{
		reject_t *rdata = pnode->data;
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
	reject_t *rdata;
	uint32_t hashv;
	int n = 0;
	
	hashv = 0;
	if (mask1 != NULL)
		hashv ^= fnv_hash_upper((const unsigned char *)mask1, 32);
	if (mask2 != NULL)
		hashv ^= fnv_hash_upper((const unsigned char *)mask2, 32);
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
throttle_add(struct sockaddr *addr)
{
	throttle_t *t;
	rb_patricia_node_t *pnode;

	if((pnode = rb_match_ip(throttle_tree, addr)) != NULL)
	{
		t = pnode->data;

		if(t->count > ConfigFileEntry.throttle_count)
		{
			ServerStats.is_thr++;
			return 1;
		}
		/* Stop penalizing them after they've been throttled */
		t->last = rb_current_time();
		t->count++;

	} else {
		int bitlen = 32;
#ifdef RB_IPV6
		if(GET_SS_FAMILY(addr) == AF_INET6)
			bitlen = 128;
#endif
		t = rb_malloc(sizeof(throttle_t));	
		t->last = rb_current_time();
		t->count = 1;
		pnode = make_and_lookup_ip(throttle_tree, addr, bitlen);
		pnode->data = t;
		rb_dlinkAdd(pnode, &t->node, &throttle_list); 
	}	
	return 0;
}

int
is_throttle_ip(struct sockaddr *addr)
{
	throttle_t *t;
	rb_patricia_node_t *pnode;
	int duration;

	if((pnode = rb_match_ip(throttle_tree, addr)) != NULL)
	{
		t = pnode->data;
		if(t->count > ConfigFileEntry.throttle_count)
		{
			duration = t->last + ConfigFileEntry.throttle_duration - rb_current_time();
			return duration > 0 ? duration : 1;
		}
	}
	return 0;
}

void 
flush_throttle(void)
{
	rb_dlink_node *ptr, *next;
	rb_patricia_node_t *pnode;
	throttle_t *t;
	
	RB_DLINK_FOREACH_SAFE(ptr, next, throttle_list.head)
	{
		pnode = ptr->data;
		t = pnode->data;		

		rb_dlinkDelete(ptr, &throttle_list);
		rb_free(t);
		rb_patricia_remove(throttle_tree, pnode);
	}
}

static void
throttle_expires(void *unused)
{
	rb_dlink_node *ptr, *next;
	rb_patricia_node_t *pnode;
	throttle_t *t;
	
	RB_DLINK_FOREACH_SAFE(ptr, next, throttle_list.head)
	{
		pnode = ptr->data;
		t = pnode->data;		

		if(t->last + ConfigFileEntry.throttle_duration > rb_current_time())
			continue;

		rb_dlinkDelete(ptr, &throttle_list);
		rb_free(t);
		rb_patricia_remove(throttle_tree, pnode);
	}
}

