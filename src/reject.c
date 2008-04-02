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
#include "struct.h"
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

static rb_patricia_tree_t *global_tree;
static rb_patricia_tree_t *reject_tree;
static rb_patricia_tree_t *dline_tree;
static rb_patricia_tree_t *eline_tree;
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

typedef struct _global_data
{
	int count;
} global_t;


static rb_patricia_node_t *
add_ipline(struct ConfItem *aconf, rb_patricia_tree_t *tree, struct sockaddr *addr, int cidr)
{
	rb_patricia_node_t *pnode;
	pnode = make_and_lookup_ip(tree, addr, cidr);
	if(pnode == NULL)
		return NULL;
	aconf->pnode = pnode;
	pnode->data = aconf;
	return (pnode);
}

int 
add_dline(struct ConfItem *aconf)
{
	struct rb_sockaddr_storage st;
	int bitlen;
	if(parse_netmask(aconf->host, (struct sockaddr *)&st, &bitlen) == HM_HOST)
		return 0;

	if(add_ipline(aconf, dline_tree, (struct sockaddr *)&st, bitlen) != NULL)
		return 1;
	return 0;
}

int
add_eline(struct ConfItem *aconf)
{
	struct rb_sockaddr_storage st;
	int bitlen;
	if(parse_netmask(aconf->host, (struct sockaddr *)&st, &bitlen) == HM_HOST)
		return 0;

	if(add_ipline(aconf, eline_tree, (struct sockaddr *)&st, bitlen) != NULL)
		return 1;
	return 0;
}

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
	dline_tree = rb_new_patricia(PATRICIA_BITS);
	eline_tree = rb_new_patricia(PATRICIA_BITS);
	throttle_tree = rb_new_patricia(PATRICIA_BITS);
	global_tree =  rb_new_patricia(PATRICIA_BITS);
	rb_event_add("reject_exit", reject_exit, NULL, DELAYED_EXIT_TIME);
	rb_event_add("reject_expires", reject_expires, NULL, 60);
	rb_event_add("throttle_expires", throttle_expires, NULL, 10);
}


void
add_reject(struct Client *client_p)
{
	rb_patricia_node_t *pnode;
	reject_t *rdata;

	/* Reject is disabled */
	if(ConfigFileEntry.reject_after_count == 0 || ConfigFileEntry.reject_duration == 0)
		return;

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
remove_reject(const char *ip)
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

static void
delete_ipline(struct ConfItem *aconf, rb_patricia_tree_t *t)
{
	rb_patricia_remove(t, aconf->pnode);
	if(!aconf->clients)
	{
		free_conf(aconf);
	}
}

static struct ConfItem *
find_ipline(rb_patricia_tree_t *t, struct sockaddr *addr)
{
	rb_patricia_node_t *pnode;
	pnode = rb_match_ip(t, addr);
	if(pnode != NULL)
		return (struct ConfItem *) pnode->data;
	return NULL;
}

static struct ConfItem *
find_ipline_exact(rb_patricia_tree_t *t, struct sockaddr *addr, unsigned int bitlen)
{
	rb_patricia_node_t *pnode;
	pnode = rb_match_ip_exact(t, addr, bitlen);
	if(pnode != NULL)
		return (struct ConfItem *) pnode->data;
	return NULL;
}


struct ConfItem *
find_dline(struct sockaddr *addr)
{
	struct ConfItem *aconf;
	aconf = find_ipline(eline_tree, addr);
	if(aconf != NULL)
	{
		return aconf;
	}
	return (find_ipline(dline_tree, addr));
}

struct ConfItem *
find_dline_exact(struct sockaddr *addr, unsigned int bitlen)
{
	return find_ipline_exact(dline_tree, addr, bitlen);
}

void
remove_dline(struct ConfItem *aconf)
{
	delete_ipline(aconf, dline_tree);
}

void
report_dlines(struct Client *source_p)
{
	rb_patricia_node_t *pnode;
	struct ConfItem *aconf;
	const char *host, *pass, *user, *oper_reason;
	RB_PATRICIA_WALK(dline_tree->head, pnode)
	{
		aconf = pnode->data;
		if(aconf->flags & CONF_FLAGS_TEMPORARY)
			RB_PATRICIA_WALK_BREAK;
		get_printable_kline(source_p, aconf, &host, &pass, &user, &oper_reason);
		sendto_one_numeric(source_p, RPL_STATSDLINE,
                            		     form_str (RPL_STATSDLINE),
                                             'D', host, pass,
                                             oper_reason ? "|" : "",
                                             oper_reason ? oper_reason : "");
	}
	RB_PATRICIA_WALK_END;
}

void
report_tdlines(struct Client *source_p)
{
	rb_patricia_node_t *pnode;
	struct ConfItem *aconf;
	const char *host, *pass, *user, *oper_reason;
	RB_PATRICIA_WALK(dline_tree->head, pnode)
	{
		aconf = pnode->data;
		if(!(aconf->flags & CONF_FLAGS_TEMPORARY))
			RB_PATRICIA_WALK_BREAK;
		get_printable_kline(source_p, aconf, &host, &pass, &user, &oper_reason);
		sendto_one_numeric(source_p, RPL_STATSDLINE,
                            		     form_str (RPL_STATSDLINE),
                                             'd', host, pass,
                                             oper_reason ? "|" : "",
                                             oper_reason ? oper_reason : "");
	}
	RB_PATRICIA_WALK_END;
}

void
report_elines(struct Client *source_p)
{
	rb_patricia_node_t *pnode;
	struct ConfItem *aconf;
	int port;
	const char *name, *host, *pass, *user, *classname;
	RB_PATRICIA_WALK(eline_tree->head, pnode)
	{
		aconf = pnode->data;
		get_printable_conf(aconf, &name, &host, &pass, &user, &port, &classname);
		sendto_one_numeric(source_p, RPL_STATSDLINE,
                            		     form_str (RPL_STATSDLINE),
                                             'e', host, pass,
                                             "", "");
	}
	RB_PATRICIA_WALK_END;
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
			return 1;			

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

static int 
get_global_count(struct sockaddr *addr)
{
	rb_patricia_node_t *pnode;
	global_t *glb;
	
	if((pnode = rb_match_ip(global_tree, addr)))
	{
		glb = pnode->data;
		return glb->count;
	}
	return 0;		
}

static int
inc_global_ip(struct sockaddr *addr, int bitlen)
{
	rb_patricia_node_t *pnode;
	global_t *glb;


	if((pnode = rb_match_ip(global_tree, addr)))
	{
		glb = pnode->data;
	} 
	else
	{
		pnode = make_and_lookup_ip(global_tree, addr, bitlen);
		glb = rb_malloc(sizeof(global_t));
		pnode->data = glb;
	}
	glb->count++;
	return glb->count;
}

static void
dec_global_ip(struct sockaddr *addr)
{
	rb_patricia_node_t *pnode;
	global_t *glb;
	
	if((pnode = rb_match_ip(global_tree, addr)))
	{
		glb = pnode->data;
		glb->count--;
		if(glb->count == 0)
		{
			rb_free(glb);
			rb_patricia_remove(global_tree, pnode);
			return;
		}
	}
}

int
inc_global_cidr_count(struct Client *client_p)
{
	struct rb_sockaddr_storage ip;
	struct sockaddr *addr;
	int bitlen;

	if(!MyClient(client_p))
	{
		if(EmptyString(client_p->sockhost) || !strcmp(client_p->sockhost, "0"))
			return -1; 
		if(!rb_inet_pton_sock(client_p->sockhost, (struct sockaddr *)&ip))
			return -1;
		addr = (struct sockaddr *)&ip;
	} else
		addr = (struct sockaddr *)&client_p->localClient->ip;
#ifdef RB_IPV6	
	if(GET_SS_FAMILY(addr) == AF_INET6)
	{
		bitlen = ConfigFileEntry.global_cidr_ipv6_bitlen;
	} else
#endif
		bitlen = ConfigFileEntry.global_cidr_ipv4_bitlen;

	return inc_global_ip(addr, bitlen);
}

void
dec_global_cidr_count(struct Client *client_p)
{
	struct rb_sockaddr_storage ip;
	struct sockaddr *addr;
	if(!MyClient(client_p))
	{
		if(EmptyString(client_p->sockhost) || !strcmp(client_p->sockhost, "0"))
			return;
		if(!rb_inet_pton_sock(client_p->sockhost, (struct sockaddr *)&ip))
			return;
		addr = (struct sockaddr *)&ip;
	} else
		addr = (struct sockaddr *)&client_p->localClient->ip;
	
	dec_global_ip(addr);
}

int
check_global_cidr_count(struct Client *client_p)
{
	struct rb_sockaddr_storage ip;
	struct sockaddr *addr;
	int count, max;
	if(!MyClient(client_p))
	{
		if(EmptyString(client_p->sockhost) || !strcmp(client_p->sockhost, "0"))
			return -1;
		if(!rb_inet_pton_sock(client_p->sockhost, (struct sockaddr *)&ip))
			return -1;
		addr = (struct sockaddr *)&ip;
	} else 
		addr = (struct sockaddr *)&client_p->localClient->ip;
	count = get_global_count(addr);
#ifdef RB_IPV6
	if(GET_SS_FAMILY(addr) == AF_INET6)
		max = ConfigFileEntry.global_cidr_ipv6_count;
	else
#endif
		max = ConfigFileEntry.global_cidr_ipv4_count;
	if(count >= max)
		return 1;
	return 0;
}

static void
clear_cidr_tree(void *data)
{
	rb_free(data);	
}

void
rehash_global_cidr_tree(void)
{
	struct Client *client_p;
	rb_dlink_node *ptr;
	rb_clear_patricia(global_tree, clear_cidr_tree);
	RB_DLINK_FOREACH(ptr, global_client_list.head)
	{
		client_p = ptr->data;
		if(IsMe(client_p) && IsServer(client_p))
			continue;
		inc_global_cidr_count(client_p);	
	}
	return;
}
