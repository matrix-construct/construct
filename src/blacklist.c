/*
 * charybdis: A slightly useful ircd.
 * blacklist.c: Manages DNS blacklist entries and lookups
 *
 * Copyright (C) 2006-2011 charybdis development team
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
		/* only accept 127.x.y.z as a listing */
		if (reply->addr.ss_family == AF_INET &&
				!memcmp(&((struct sockaddr_in *)&reply->addr)->sin_addr, "\177", 1))
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

static void initiate_blacklist_dnsquery(struct Blacklist *blptr, struct Client *client_p)
{
	struct BlacklistClient *blcptr = rb_malloc(sizeof(struct BlacklistClient));
	char buf[IRCD_RES_HOSTLEN + 1];
	uint8_t *ip;

	blcptr->blacklist = blptr;
	blcptr->client_p = client_p;

	blcptr->dns_query.ptr = blcptr;
	blcptr->dns_query.callback = blacklist_dns_callback;

	if ((client_p->localClient->ip.ss_family == AF_INET) && blptr->ipv4)
	{
		ip = (uint8_t *)&((struct sockaddr_in *)&client_p->localClient->ip)->sin_addr.s_addr;

		/* becomes 2.0.0.127.torbl.ahbl.org or whatever */
		rb_snprintf(buf, sizeof buf, "%d.%d.%d.%d.%s",
			    (unsigned int) ip[3],
			    (unsigned int) ip[2],
			    (unsigned int) ip[1],
			    (unsigned int) ip[0],
			    blptr->host);
	}
	/* IPv6 is supported now. --Elizabeth */
	else if ((client_p->localClient->ip.ss_family == AF_INET6) && blptr->ipv6)
	{
		/* Breaks it into ip[0] = 0x00, ip[1] = 0x00... ip[16] = 0x01 for localhost
		 * I wish there was a uint4_t for C, this would make the below cleaner, but... */
		ip = (uint8_t *)&((struct sockaddr_in6 *)&client_p->localClient->ip)->sin6_addr.s6_addr;
		char *bufptr = buf;
		int i;

		/* The below will give us something like
		 * 1.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.foobl.invalid
		 */

		/* Going backwards */
		for (i = 15; i >= 0; i--)
		{
			/* Get upper and lower nibbles (yes this works fine on
			 * both big and little endian) */
			uint8_t hi = (ip[i] >> 4) & 0x0F;
			uint8_t lo = ip[i] & 0x0F;

			/* One part... (why 5? rb_snprintf adds \0) */
			rb_snprintf(bufptr, 5, "%1x.%1x.",
				    (unsigned int) lo, /* Remember, backwards */
				    (unsigned int) hi);

			/* Lurch forward to next position */
			bufptr += 4;
		}

		/* Tack host on */
		strcpy(bufptr, blptr->host);
	}
	/* This shouldn't happen... */
	else
		return;

	gethost_byname_type(buf, &blcptr->dns_query, T_A);

	rb_dlinkAdd(blcptr, &blcptr->node, &client_p->preClient->dnsbl_queries);
	blptr->refcount++;
}

/* public interfaces */
struct Blacklist *new_blacklist(char *name, char *reject_reason, int ipv4, int ipv6)
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
	rb_strlcpy(blptr->host, name, IRCD_RES_HOSTLEN + 1);
	rb_strlcpy(blptr->reject_reason, reject_reason, IRCD_BUFSIZE);
	blptr->ipv4 = ipv4;
	blptr->ipv6 = ipv6;
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
