/*
 *  dns.c: An interface to the resolver module in authd
 *  (based somewhat on ircd-ratbox dns.c)
 *
 *  Copyright (C) 2005 Aaron Sethman <androsyn@ratbox.org>
 *  Copyright (C) 2005-2012 ircd-ratbox development team
 *  Copyright (C) 2016 William Pitcock <nenolod@dereferenced.org>
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
 */

#include "stdinc.h"
#include "rb_lib.h"
#include "client.h"
#include "ircd_defs.h"
#include "parse.h"
#include "dns.h"
#include "match.h"
#include "logger.h"
#include "s_conf.h"
#include "client.h"
#include "send.h"
#include "numeric.h"
#include "msg.h"
#include "hash.h"

#define DNS_HOST_IPV4		((char)'4')
#define DNS_HOST_IPV6		((char)'6')
#define DNS_REVERSE_IPV4	((char)'R')
#define DNS_REVERSE_IPV6	((char)'S')

static void submit_dns(uint32_t uid, char type, const char *addr);
static void submit_dns_stat(uint32_t uid);

struct dnsreq
{
	DNSCB callback;
	void *data;
};

struct dnsstatreq
{
	DNSLISTCB callback;
	void *data;
};

/* These serve as a form of sparse array */
static rb_dictionary *query_dict;
static rb_dictionary *stat_dict;

rb_dlink_list nameservers;

static uint32_t query_id = 0;
static uint32_t stat_id = 0;


static inline uint32_t
assign_id(uint32_t *id)
{
	if(++(*id) == 0)
		*id = 1;

	return *id;
}

static void
handle_dns_failure(uint32_t xid)
{
	struct dnsreq *req = rb_dictionary_retrieve(query_dict, RB_UINT_TO_POINTER(xid));
	s_assert(req);

	if(req->callback == NULL)
		return;

	req->callback("FAILED", 0, 0, req->data);
	req->callback = NULL;
	req->data = NULL;
}

static void
handle_dns_stat_failure(uint32_t xid)
{
	struct dnsstatreq *req = rb_dictionary_retrieve(stat_dict, RB_UINT_TO_POINTER(xid));
	s_assert(req);

	if(req->callback == NULL)
		return;

	req->callback(1, NULL, 2, req->data);
	req->callback = NULL;
	req->data = NULL;
}


void
cancel_lookup(uint32_t xid)
{
	struct dnsreq *req = rb_dictionary_retrieve(query_dict, RB_UINT_TO_POINTER(xid));
	s_assert(req);
	req->callback = NULL;
	req->data = NULL;
}

void
cancel_dns_stats(uint32_t xid)
{
	struct dnsstatreq *req = rb_dictionary_retrieve(stat_dict, RB_UINT_TO_POINTER(xid));
	s_assert(req);
	req->callback = NULL;
	req->data = NULL;
}


uint32_t
lookup_hostname(const char *hostname, int aftype, DNSCB callback, void *data)
{
	struct dnsreq *req = rb_malloc(sizeof(struct dnsreq));
	int aft;
	uint32_t rid = assign_id(&query_id);

	check_authd();

	rb_dictionary_add(query_dict, RB_UINT_TO_POINTER(rid), req);

	req->callback = callback;
	req->data = data;

#ifdef RB_IPV6
	if(aftype == AF_INET6)
		aft = 6;
	else
#endif
		aft = 4;

	submit_dns(rid, aft == 4 ? DNS_HOST_IPV4 : DNS_HOST_IPV6, hostname);
	return (rid);
}

uint32_t
lookup_ip(const char *addr, int aftype, DNSCB callback, void *data)
{
	struct dnsreq *req = rb_malloc(sizeof(struct dnsreq));
	int aft;
	uint32_t rid = assign_id(&query_id);

	check_authd();

	rb_dictionary_add(query_dict, RB_UINT_TO_POINTER(rid), req);

	req->callback = callback;
	req->data = data;

#ifdef RB_IPV6
	if(aftype == AF_INET6)
		aft = 6;
	else
#endif
		aft = 4;

	submit_dns(rid, aft == 4 ? DNS_REVERSE_IPV4 : DNS_REVERSE_IPV6, addr);
	return (rid);
}

static uint32_t
get_nameservers(DNSLISTCB callback, void *data)
{
	struct dnsstatreq *req = rb_malloc(sizeof(struct dnsstatreq));
	uint32_t qid = assign_id(&stat_id);

	check_authd();

	rb_dictionary_add(stat_dict, RB_UINT_TO_POINTER(qid), req);

	req->callback = callback;
	req->data = data;

	submit_dns_stat(qid);
	return (qid);
}


void
dns_results_callback(const char *callid, const char *status, const char *type, const char *results)
{
	struct dnsreq *req;
	uint32_t rid;
	int st;
	int aft;
	long lrid = strtol(callid, NULL, 16);

	if(lrid > UINT32_MAX)
		return;

	rid = (uint32_t)lrid;
	req = rb_dictionary_retrieve(query_dict, RB_UINT_TO_POINTER(rid));
	if(req == NULL)
		return;

	st = (*status == 'O');
	aft = *type == '6' || *type == 'S' ? 6 : 4;
	if(req->callback == NULL)
	{
		/* got cancelled..oh well */
		req->data = NULL;
		return;
	}
#ifdef RB_IPV6
	if(aft == 6)
		aft = AF_INET6;
	else
#endif
		aft = AF_INET;

	req->callback(results, st, aft, req->data);

	rb_free(req);
	rb_dictionary_delete(query_dict, RB_UINT_TO_POINTER(rid));
}

void
dns_stats_results_callback(const char *callid, const char *status, int resc, const char *resv[])
{
	struct dnsstatreq *req;
	uint32_t qid;
	int st;
	long lqid = strtol(callid, NULL, 16);

	if(lqid > UINT32_MAX)
		return;

	qid = (uint32_t)lqid;
	req = rb_dictionary_retrieve(stat_dict, RB_UINT_TO_POINTER(qid));

	s_assert(req);

	if(req->callback == NULL)
	{
		req->data = NULL;
		return;
	}

	switch(*status)
	{
	case 'Y':
		st = 0;
		break;
	case 'X':
		/* Error */
		st = 1;
		break;
	default:
		/* Shouldn't happen... */
		return;
	}

	/* Query complete */
	req->callback(resc, resv, st, req->data);

	rb_free(req);
	rb_dictionary_delete(stat_dict, RB_UINT_TO_POINTER(qid));
}

static void
stats_results_callback(int resc, const char *resv[], int status, void *data)
{
	if(status == 0)
	{
		rb_dlink_node *n, *tn;

		RB_DLINK_FOREACH_SAFE(n, tn, nameservers.head)
		{
			/* Clean up old nameservers */
			rb_free(n->data);
			rb_dlinkDestroy(n, &nameservers);
		}

		for(int i = 0; i < resc; i++)
			rb_dlinkAddAlloc(rb_strdup(resv[i]), &nameservers);
	}
	else
	{
		const char *error = resc ? resv[resc] : "Unknown error";
		iwarn("Error getting DNS servers: %s", error);
	}
}


void
init_dns(void)
{
	query_dict = rb_dictionary_create("dns queries", rb_uint32cmp);
	stat_dict = rb_dictionary_create("dns stat queries", rb_uint32cmp);
	(void)get_nameservers(stats_results_callback, NULL);
}

void
reload_nameservers(void)
{
	check_authd();
	rb_helper_write(authd_helper, "R D");
	(void)get_nameservers(stats_results_callback, NULL);
}


static void
submit_dns(uint32_t nid, char type, const char *addr)
{
	if(authd_helper == NULL)
	{
		handle_dns_failure(nid);
		return;
	}
	rb_helper_write(authd_helper, "D %x %c %s", nid, type, addr);
}

static void
submit_dns_stat(uint32_t nid)
{
	if(authd_helper == NULL)
	{
		handle_dns_stat_failure(nid);
		return;
	}
	rb_helper_write(authd_helper, "S %x D", nid);
}
