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

#define DNS_IDTABLE_SIZE 0x2000
#define DNS_STATTABLE_SIZE 0x10

#define DNS_HOST_IPV4		((char)'4')
#define DNS_HOST_IPV6		((char)'6')
#define DNS_REVERSE_IPV4	((char)'R')
#define DNS_REVERSE_IPV6	((char)'S')

static void submit_dns(uint16_t uid, char type, const char *addr);
static void submit_dns_stat(uint16_t uid);

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

static struct dnsreq querytable[DNS_IDTABLE_SIZE];
static struct dnsstatreq stattable[DNS_STATTABLE_SIZE];
rb_dlink_list nameservers;


static uint16_t
assign_dns_id(void)
{
	static uint16_t id = 1;
	int loopcnt = 0;
	while(1)
	{
		if(++loopcnt > DNS_IDTABLE_SIZE)
			return 0;
		if(id < DNS_IDTABLE_SIZE - 1 || id == 0)
			id++;
		else
			id = 1;
		if(querytable[id].callback == NULL)
			break;
	}
	return (id);
}

static uint8_t
assign_dns_stat_id(void)
{
	static uint8_t id = 1;
	int loopcnt = 0;
	while(1)
	{
		if(++loopcnt > DNS_STATTABLE_SIZE)
			return 0;
		if(id < DNS_STATTABLE_SIZE - 1 || id == 0)
			id++;
		else
			id = 1;
		if(stattable[id].callback == NULL)
			break;
	}
	return (id);
}

static void
handle_dns_failure(uint16_t xid)
{
	struct dnsreq *req;

	req = &querytable[xid];
	if(req->callback == NULL)
		return;

	req->callback("FAILED", 0, 0, req->data);
	req->callback = NULL;
	req->data = NULL;
}

static void
handle_dns_stat_failure(uint8_t xid)
{
	struct dnsstatreq *req;
	const char *err[] = { "Unknown failure" };

	req = &stattable[xid];
	if(req->callback == NULL)
		return;

	req->callback(1, err, 2, req->data);

	req->callback = NULL;
	req->data = NULL;
}

void
cancel_lookup(uint16_t xid)
{
	querytable[xid].callback = NULL;
	querytable[xid].data = NULL;
}

void
cancel_dns_stats(uint16_t xid)
{
	stattable[xid].callback = NULL;
	stattable[xid].data = NULL;
}

uint16_t
lookup_hostname(const char *hostname, int aftype, DNSCB callback, void *data)
{
	struct dnsreq *req;
	int aft;
	uint16_t nid;
	check_authd();
	nid = assign_dns_id();
	if((nid = assign_dns_id()) == 0)
		return 0;

	req = &querytable[nid];

	req->callback = callback;
	req->data = data;

#ifdef RB_IPV6
	if(aftype == AF_INET6)
		aft = 6;
	else
#endif
		aft = 4;

	submit_dns(nid, aft == 4 ? DNS_HOST_IPV4 : DNS_HOST_IPV6, hostname);
	return (nid);
}

uint16_t
lookup_ip(const char *addr, int aftype, DNSCB callback, void *data)
{
	struct dnsreq *req;
	int aft;
	uint16_t nid;
	check_authd();

	if((nid = assign_dns_id()) == 0)
		return 0;

	req = &querytable[nid];

	req->callback = callback;
	req->data = data;

#ifdef RB_IPV6
	if(aftype == AF_INET6)
		aft = 6;
	else
#endif
		aft = 4;

	submit_dns(nid, aft == 4 ? DNS_REVERSE_IPV4 : DNS_REVERSE_IPV6, addr);
	return (nid);
}

uint8_t
get_nameservers(DNSLISTCB callback, void *data)
{
	struct dnsstatreq *req;
	uint8_t nid;
	check_authd();

	if((nid = assign_dns_stat_id()) == 0)
		return 0;

	req = &stattable[nid];
	req->callback = callback;
	req->data = data;

	submit_dns_stat(nid);
	return (nid);
}

void
dns_results_callback(const char *callid, const char *status, const char *type, const char *results)
{
	struct dnsreq *req;
	uint16_t nid;
	int st;
	int aft;
	long lnid = strtol(callid, NULL, 16);

	if(lnid > DNS_IDTABLE_SIZE || lnid == 0)
		return;
	nid = (uint16_t)lnid;
	req = &querytable[nid];
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
	req->callback = NULL;
	req->data = NULL;
}

void
dns_stats_results_callback(const char *callid, const char *status, int resc, const char *resv[])
{
	struct dnsstatreq *req;
	uint8_t nid;
	int st, i;
	long lnid = strtol(callid, NULL, 16);

	if(lnid > DNS_STATTABLE_SIZE || lnid == 0)
		return;
	nid = (uint8_t)lnid;
	req = &stattable[nid];

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
	req->callback(resc, resv, st, stattable[nid].data);

	req->data = NULL;
	req->callback = NULL;
}

static void
get_nameservers_cb(int resc, const char *resv[], int status, void *data)
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
		iwarn(L_MAIN, "Error getting DNS servers: %s", error);
	}
}

void
init_nameserver_cache(void)
{
	(void)get_nameservers(get_nameservers_cb, NULL);
}

static void
submit_dns(uint16_t nid, char type, const char *addr)
{
	if(authd_helper == NULL)
	{
		handle_dns_failure(nid);
		return;
	}
	rb_helper_write(authd_helper, "D %x %c %s", nid, type, addr);
}

static void
submit_dns_stat(uint16_t nid)
{
	if(authd_helper == NULL)
	{
		handle_dns_stat_failure(nid);
		return;
	}
	rb_helper_write(authd_helper, "S %x D", nid);
}
