/*
 * charybdis: A slightly useful ircd.
 * blacklist.c: Manages DNS blacklist entries and lookups
 *
 * Copyright (C) 2006-2008 charybdis development team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: blacklist.c 2743 2006-11-10 15:15:00Z jilles $
 */

#include "stdinc.h"
#include "client.h"
#include "res.h"
#include "numeric.h"
#include "reject.h"
#include "s_conf.h"
#include "s_user.h"
#include "blacklist.h"

rb_dlink_list blacklist_list = { NULL, NULL, 0 };

/* private interfaces */
static struct Blacklist *find_blacklist(char *name)
{
	rb_dlink_node *nptr;

	RB_DLINK_FOREACH(nptr, blacklist_list.head)
	{
		struct Blacklist *blptr = (struct Blacklist *) nptr->data;

		if (!irccmp(blptr->host, name))
			return blptr;
	}

	return NULL;
}

static void blacklist_dns_callback(void *vptr, struct DNSReply *reply)
{
	struct BlacklistClient *blcptr = (struct BlacklistClient *) vptr;
	int listed = 0;

	if (blcptr == NULL || blcptr->client_p == NULL)
		return;

	if (blcptr->client_p->preClient == NULL)
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				"blacklist_dns_callback(): blcptr->client_p->preClient (%s) is NULL", get_client_name(blcptr->client_p, HIDE_IP));
		rb_free(blcptr);
		return;
	}

	if (reply != NULL)
	{
		/* only accept 127.0.0.x as a listing */
		if (reply->addr.ss_family == AF_INET &&
				!memcmp(&((struct sockaddr_in *)&reply->addr)->sin_addr, "\177\0\0", 3))
			listed = TRUE;
		else if (blcptr->blacklist->lastwarning + 3600 < rb_current_time())
		{
			sendto_realops_snomask(SNO_GENERAL, L_ALL,
					"Garbage reply from blacklist %s",
					blcptr->blacklist->host);
			blcptr->blacklist->lastwarning = rb_current_time();
		}
	}

	/* they have a blacklist entry for this client */
	if (listed && blcptr->client_p->preClient->dnsbl_listed == NULL)
	{
		blcptr->client_p->preClient->dnsbl_listed = blcptr->blacklist;
		/* reference to blacklist moves from blcptr to client_p->preClient... */
	}
	else
		unref_blacklist(blcptr->blacklist);

	rb_dlinkDelete(&blcptr->node, &blcptr->client_p->preClient->dnsbl_queries);

	/* yes, it can probably happen... */
	if (rb_dlink_list_length(&blcptr->client_p->preClient->dnsbl_queries) == 0 && blcptr->client_p->flags & FLAGS_SENTUSER && !EmptyString(blcptr->client_p->name))
	{
		char buf[USERLEN + 1];
		rb_strlcpy(buf, blcptr->client_p->username, sizeof buf);
		register_local_user(blcptr->client_p, blcptr->client_p, buf);
	}

	rb_free(blcptr);
}

/* XXX: no IPv6 implementation, not to concerned right now though. */
static void initiate_blacklist_dnsquery(struct Blacklist *blptr, struct Client *client_p)
{
	struct BlacklistClient *blcptr = rb_malloc(sizeof(struct BlacklistClient));
	char buf[IRCD_BUFSIZE];
	int ip[4];

	blcptr->blacklist = blptr;
	blcptr->client_p = client_p;

	blcptr->dns_query.ptr = blcptr;
	blcptr->dns_query.callback = blacklist_dns_callback;

	/* XXX: yes I know this is bad, I don't really care right now */
	sscanf(client_p->sockhost, "%d.%d.%d.%d", &ip[3], &ip[2], &ip[1], &ip[0]);

	/* becomes 2.0.0.127.torbl.ahbl.org or whatever */
	rb_snprintf(buf, IRCD_BUFSIZE, "%d.%d.%d.%d.%s", ip[0], ip[1], ip[2], ip[3], blptr->host);

	gethost_byname_type(buf, &blcptr->dns_query, T_A);

	rb_dlinkAdd(blcptr, &blcptr->node, &client_p->preClient->dnsbl_queries);
	blptr->refcount++;
}

/* public interfaces */
struct Blacklist *new_blacklist(char *name, char *reject_reason)
{
	struct Blacklist *blptr;

	if (name == NULL || reject_reason == NULL)
		return NULL;

	blptr = find_blacklist(name);
	if (blptr == NULL)
	{
		blptr = rb_malloc(sizeof(struct Blacklist));
		rb_dlinkAddAlloc(blptr, &blacklist_list);
	}
	else
		blptr->status &= ~CONF_ILLEGAL;
	rb_strlcpy(blptr->host, name, HOSTLEN);
	rb_strlcpy(blptr->reject_reason, reject_reason, IRCD_BUFSIZE);
	blptr->lastwarning = 0;

	return blptr;
}

void unref_blacklist(struct Blacklist *blptr)
{
	blptr->refcount--;
	if (blptr->status & CONF_ILLEGAL && blptr->refcount <= 0)
	{
		rb_dlinkFindDestroy(blptr, &blacklist_list);
		rb_free(blptr);
	}
}

void lookup_blacklists(struct Client *client_p)
{
	rb_dlink_node *nptr;

	/* We don't do IPv6 right now, sorry! */
	if (client_p->localClient->ip.ss_family == AF_INET6)
		return;

	RB_DLINK_FOREACH(nptr, blacklist_list.head)
	{
		struct Blacklist *blptr = (struct Blacklist *) nptr->data;

		if (!(blptr->status & CONF_ILLEGAL))
			initiate_blacklist_dnsquery(blptr, client_p);
	}
}

void abort_blacklist_queries(struct Client *client_p)
{
	rb_dlink_node *ptr, *next_ptr;
	struct BlacklistClient *blcptr;

	if (client_p->preClient == NULL)
		return;
	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, client_p->preClient->dnsbl_queries.head)
	{
		blcptr = ptr->data;
		rb_dlinkDelete(&blcptr->node, &client_p->preClient->dnsbl_queries);
		unref_blacklist(blcptr->blacklist);
		delete_resolver_queries(&blcptr->dns_query);
		rb_free(blcptr);
	}
}

void destroy_blacklists(void)
{
	rb_dlink_node *ptr, *next_ptr;
	struct Blacklist *blptr;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, blacklist_list.head)
	{
		blptr = ptr->data;
		blptr->hits = 0; /* keep it simple and consistent */
		if (blptr->refcount > 0)
			blptr->status |= CONF_ILLEGAL;
		else
		{
			rb_free(ptr->data);
			rb_dlinkDestroy(ptr, &blacklist_list);
		}
	}
}
