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

#include <stdinc.h>
#include <ratbox_lib.h>
#include <struct.h>
#include <ircd_defs.h>
#include <parse.h>
#include <dns.h>
#include <match.h>
#include <logger.h>
#include <s_conf.h>
#include <client.h>
#include <send.h>
#include <numeric.h>
#include <msg.h>

#define DNS_IDTABLE_SIZE 0x2000

#define DNS_HOST	((char)'H')
#define DNS_REVERSE	((char)'I')

static void submit_dns(const char, uint16_t id, int aftype, const char *addr);

static int start_authd(void);
static void parse_authd_reply(rb_helper * helper);
static void restart_authd_cb(rb_helper * helper);

static rb_helper *authd_helper;

struct dnsreq
{
	DNSCB callback;
	void *data;
};

static struct dnsreq querytable[DNS_IDTABLE_SIZE];

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

static inline void
check_authd(void)
{
	if(authd_helper == NULL)
		restart_authd();
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

void
cancel_lookup(uint16_t xid)
{
	querytable[xid].callback = NULL;
	querytable[xid].data = NULL;
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

	submit_dns(DNS_HOST, nid, aft, hostname);
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

	submit_dns(DNS_REVERSE, nid, aft, addr);
	return (nid);
}

static void
results_callback(const char *callid, const char *status, const char *aftype, const char *results)
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
	st = atoi(status);
	aft = atoi(aftype);
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


static char *resolver_path;

static int
start_authd(void)
{
	char fullpath[PATH_MAX + 1];
#ifdef _WIN32
	const char *suffix = ".exe";
#else
	const char *suffix = "";
#endif
	if(resolver_path == NULL)
	{
		snprintf(fullpath, sizeof(fullpath), "%s/authd%s", PKGLIBEXECDIR, suffix);

		if(access(fullpath, X_OK) == -1)
		{
			snprintf(fullpath, sizeof(fullpath), "%s/libexec/charybdis/authd%s",
				 ConfigFileEntry.dpath, suffix);
			if(access(fullpath, X_OK) == -1)
			{
				ilog(L_MAIN,
				     "Unable to execute authd in %s or %s/libexec/charybdis",
				     PKGLIBEXECDIR, ConfigFileEntry.dpath);
				sendto_realops_snomask(SNO_GENERAL, L_ALL,
						       "Unable to execute resolver in %s or %s/libexec/charybdis",
						       PKGLIBEXECDIR, ConfigFileEntry.dpath);
				return 1;
			}

		}

		resolver_path = rb_strdup(fullpath);
	}

	authd_helper = rb_helper_start("authd", resolver_path, parse_authd_reply, restart_authd_cb);

	if(authd_helper == NULL)
	{
		ilog(L_MAIN, "Unable to start authd helper: %s", strerror(errno));
		sendto_realops_snomask(SNO_GENERAL, L_ALL, "Unable to start authd helper: %s", strerror(errno));
		return 1;
	}
	ilog(L_MAIN, "resolver helper started");
	sendto_realops_snomask(SNO_GENERAL, L_ALL, "resolver helper started");
	rb_helper_run(authd_helper);
	return 0;
}

void
report_dns_servers(struct Client *source_p)
{
#if 0
	rb_dlink_node *ptr;
	RB_DLINK_FOREACH(ptr, nameservers.head)
	{
		sendto_one_numeric(source_p, RPL_STATSDEBUG, "A %s", (char *)ptr->data);
	}
#endif
}

static void
parse_authd_reply(rb_helper * helper)
{
	ssize_t len;
	int parc;
	char dnsBuf[READBUF_SIZE];

	char *parv[MAXPARA + 1];
	while((len = rb_helper_read(helper, dnsBuf, sizeof(dnsBuf))) > 0)
	{
		parc = rb_string_to_array(dnsBuf, parv, MAXPARA+1); 

		if(*parv[0] == 'R')
		{
			if(parc != 5)
			{
				ilog(L_MAIN, "authd sent a result with wrong number of arguments: got %d", parc);
				restart_authd();
				return;
			}
			results_callback(parv[1], parv[2], parv[3], parv[4]);
		}
		else
			return;
	}
}

static void
submit_dns(char type, uint16_t nid, int aftype, const char *addr)
{
	if(authd_helper == NULL)
	{
		handle_dns_failure(nid);
		return;
	}
	rb_helper_write(authd_helper, "%c %x %d %s", type, nid, aftype, addr);
}

void
init_authd(void)
{
	if(start_authd())
	{
		ilog(L_MAIN, "Unable to start authd helper: %s", strerror(errno));
		exit(0);
	}
}

static void
restart_authd_cb(rb_helper * helper)
{
	ilog(L_MAIN, "authd: restart_authd_cb called, authd died?");
	sendto_realops_snomask(SNO_GENERAL, L_ALL, "authd - restart_authd_cb called, authd died?");
	if(helper != NULL)
	{
		rb_helper_close(helper);
		authd_helper = NULL;
	}
	start_authd();
}

void
restart_authd(void)
{
	restart_authd_cb(authd_helper);
}

void
rehash_resolver(void)
{
	rb_helper_write(authd_helper, "R");
}

