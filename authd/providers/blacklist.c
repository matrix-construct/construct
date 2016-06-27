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
 */

/* Originally written for charybdis circa 2006 (by nenolod?).
 * Tweaked for authd. Some functions and structs renamed. Public/private
 * interfaces have been shifted around. Some code has been cleaned up too.
 * -- Elizafox 24 March 2016
 */

#include "authd.h"
#include "defaults.h"
#include "provider.h"
#include "notice.h"
#include "stdinc.h"
#include "dns.h"

#define SELF_PID (blacklist_provider.id)

typedef enum filter_t
{
	FILTER_ALL = 1,
	FILTER_LAST = 2,
} filter_t;

/* Blacklist accepted IP types */
#define IPTYPE_IPV4	1
#define IPTYPE_IPV6	2

/* A configured DNSBL */
struct blacklist
{
	char host[IRCD_RES_HOSTLEN + 1];
	char reason[BUFSIZE];		/* Reason template (ircd fills in the blanks) */
	uint8_t iptype;			/* IP types supported */
	rb_dlink_list filters;		/* Filters for queries */

	bool delete;			/* If true delete when no clients */
	int refcount;			/* When 0 and delete is set, remove this blacklist */
	unsigned int hits;

	time_t lastwarning;		/* Last warning about garbage replies sent */
};

/* A lookup in progress for a particular DNSBL for a particular client */
struct blacklist_lookup
{
	struct blacklist *bl;		/* Blacklist we're checking */
	struct auth_client *auth;	/* Client */
	struct dns_query *query;	/* DNS query pointer */

	rb_dlink_node node;
};

/* A blacklist filter */
struct blacklist_filter
{
	filter_t type;			/* Type of filter */
	char filter[HOSTIPLEN];		/* The filter itself */

	rb_dlink_node node;
};

/* Blacklist user data attached to auth_client instance */
struct blacklist_user
{
	rb_dlink_list queries;		/* Blacklist queries in flight */
};

/* public interfaces */
static void blacklists_destroy(void);

static bool blacklists_start(struct auth_client *);
static void blacklists_cancel(struct auth_client *);

/* private interfaces */
static void unref_blacklist(struct blacklist *);
static struct blacklist *new_blacklist(const char *, const char *, uint8_t, rb_dlink_list *);
static struct blacklist *find_blacklist(const char *);
static bool blacklist_check_reply(struct blacklist_lookup *, const char *);
static void blacklist_dns_callback(const char *, bool, query_type, void *);
static void initiate_blacklist_dnsquery(struct blacklist *, struct auth_client *);

/* Variables */
static rb_dlink_list blacklist_list = { NULL, NULL, 0 };
static int blacklist_timeout = BLACKLIST_TIMEOUT_DEFAULT;

/* private interfaces */

static void
unref_blacklist(struct blacklist *bl)
{
	rb_dlink_node *ptr, *nptr;

	bl->refcount--;
	if (bl->delete && bl->refcount <= 0)
	{
		RB_DLINK_FOREACH_SAFE(ptr, nptr, bl->filters.head)
		{
			rb_dlinkDelete(ptr, &bl->filters);
			rb_free(ptr);
		}

		rb_dlinkFindDestroy(bl, &blacklist_list);
		rb_free(bl);
	}
}

static struct blacklist *
new_blacklist(const char *name, const char *reason, uint8_t iptype, rb_dlink_list *filters)
{
	struct blacklist *bl;

	if (name == NULL || reason == NULL || iptype == 0)
		return NULL;

	if((bl = find_blacklist(name)) == NULL)
	{
		bl = rb_malloc(sizeof(struct blacklist));
		rb_dlinkAddAlloc(bl, &blacklist_list);
	}
	else
		bl->delete = false;

	rb_strlcpy(bl->host, name, IRCD_RES_HOSTLEN + 1);
	rb_strlcpy(bl->reason, reason, BUFSIZE);
	bl->iptype = iptype;

	rb_dlinkMoveList(filters, &bl->filters);

	bl->lastwarning = 0;

	return bl;
}

static struct blacklist *
find_blacklist(const char *name)
{
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, blacklist_list.head)
	{
		struct blacklist *bl = (struct blacklist *)ptr->data;

		if (!strcasecmp(bl->host, name))
			return bl;
	}

	return NULL;
}

static inline bool
blacklist_check_reply(struct blacklist_lookup *bllookup, const char *ipaddr)
{
	struct blacklist *bl = bllookup->bl;
	const char *lastoctet;
	rb_dlink_node *ptr;

	/* No filters and entry found - thus positive match */
	if (!rb_dlink_list_length(&bl->filters))
		return true;

	/* Below will prolly have to change if IPv6 address replies are sent back */
	if ((lastoctet = strrchr(ipaddr, '.')) == NULL || *(++lastoctet) == '\0')
		goto blwarn;

	RB_DLINK_FOREACH(ptr, bl->filters.head)
	{
		struct blacklist_filter *filter = ptr->data;
		const char *cmpstr;

		if (filter->type == FILTER_ALL)
			cmpstr = ipaddr;
		else if (filter->type == FILTER_LAST)
			cmpstr = lastoctet;
		else
		{
			warn_opers(L_CRIT, "Blacklist: Unknown blacklist filter type (host %s): %d",
					bl->host, filter->type);
			exit(EX_PROVIDER_ERROR);
		}

		if (strcmp(cmpstr, filter->filter) == 0)
			/* Match! */
			return true;
	}

	return false;
blwarn:
	if (bl->lastwarning + 3600 < rb_current_time())
	{
		warn_opers(L_WARN, "Garbage/undecipherable reply received from blacklist %s (reply %s)",
				bl->host, ipaddr);
		bl->lastwarning = rb_current_time();
	}

	return false;
}

static void
blacklist_dns_callback(const char *result, bool status, query_type type, void *data)
{
	struct blacklist_lookup *bllookup = (struct blacklist_lookup *)data;
	struct blacklist_user *bluser;
	struct blacklist *bl;
	struct auth_client *auth;

	lrb_assert(bllookup != NULL);
	lrb_assert(bllookup->auth != NULL);

	bl = bllookup->bl;
	auth = bllookup->auth;

	if((bluser = get_provider_data(auth, SELF_PID)) == NULL)
		return;

	if (result != NULL && status && blacklist_check_reply(bllookup, result))
	{
		/* Match found, so proceed no further */
		bl->hits++;
		blacklists_cancel(auth);
		reject_client(auth, SELF_PID, bl->host, bl->reason);
		return;
	}

	unref_blacklist(bl);
	cancel_query(bllookup->query);	/* Ignore future responses */
	rb_dlinkDelete(&bllookup->node, &bluser->queries);
	rb_free(bllookup);

	if(!rb_dlink_list_length(&bluser->queries))
	{
		/* Done here */
		notice_client(auth->cid, "*** IP not found in DNS blacklist%s",
				rb_dlink_list_length(&blacklist_list) > 1 ? "s" : "");
		rb_free(bluser);
		set_provider_data(auth, SELF_PID, NULL);
		set_provider_timeout_absolute(auth, SELF_PID, 0);
		provider_done(auth, SELF_PID);

		auth_client_unref(auth);
	}
}

static void
initiate_blacklist_dnsquery(struct blacklist *bl, struct auth_client *auth)
{
	struct blacklist_lookup *bllookup = rb_malloc(sizeof(struct blacklist_lookup));
	struct blacklist_user *bluser = get_provider_data(auth, SELF_PID);
	char buf[IRCD_RES_HOSTLEN + 1];
	int aftype;

	bllookup->bl = bl;
	bllookup->auth = auth;

	aftype = GET_SS_FAMILY(&auth->c_addr);
	if((aftype == AF_INET && (bl->iptype & IPTYPE_IPV4) == 0) ||
		(aftype == AF_INET6 && (bl->iptype & IPTYPE_IPV6) == 0))
		/* Incorrect blacklist type for this IP... */
	{
		rb_free(bllookup);
		return;
	}

	build_rdns(buf, sizeof(buf), &auth->c_addr, bl->host);

	bllookup->query = lookup_ip(buf, AF_INET, blacklist_dns_callback, bllookup);

	rb_dlinkAdd(bllookup, &bllookup->node, &bluser->queries);
	bl->refcount++;
}

static inline void
lookup_all_blacklists(struct auth_client *auth)
{
	struct blacklist_user *bluser = get_provider_data(auth, SELF_PID);
	rb_dlink_node *ptr;

	notice_client(auth->cid, "*** Checking your IP against DNS blacklist%s",
			rb_dlink_list_length(&blacklist_list) > 1 ? "s" : "");

	RB_DLINK_FOREACH(ptr, blacklist_list.head)
	{
		struct blacklist *bl = (struct blacklist *)ptr->data;

		if (!bl->delete)
			initiate_blacklist_dnsquery(bl, auth);
	}

	set_provider_timeout_relative(auth, SELF_PID, blacklist_timeout);
}

static inline void
delete_blacklist(struct blacklist *bl)
{
	if (bl->refcount > 0)
		bl->delete = true;
	else
	{
		rb_dlinkFindDestroy(bl, &blacklist_list);
		rb_free(bl);
	}
}

static void
delete_all_blacklists(void)
{
	rb_dlink_node *ptr, *nptr;

	RB_DLINK_FOREACH_SAFE(ptr, nptr, blacklist_list.head)
	{
		delete_blacklist(ptr->data);
	}
}

/* public interfaces */
static bool
blacklists_start(struct auth_client *auth)
{
	uint32_t rdns_pid, ident_pid;

	lrb_assert(get_provider_data(auth, SELF_PID) == NULL);

	if(!rb_dlink_list_length(&blacklist_list))
		/* Nothing to do... */
		return true;

	auth_client_ref(auth);

	set_provider_data(auth, SELF_PID, rb_malloc(sizeof(struct blacklist_user)));

	if((!get_provider_id("rdns", &rdns_pid) || is_provider_done(auth, rdns_pid)) &&
		(!get_provider_id("ident", &ident_pid) || is_provider_done(auth, ident_pid)))
	{
		/* Start the lookup if ident and rdns are finished, or not loaded. */
		lookup_all_blacklists(auth);
	}

	set_provider_running(auth, SELF_PID);
	return true;
}

/* This is called every time a provider is completed as long as we are marked not done */
static void
blacklists_initiate(struct auth_client *auth, uint32_t provider)
{
	struct blacklist_user *bluser = get_provider_data(auth, SELF_PID);
	uint32_t rdns_pid, ident_pid;

	lrb_assert(provider != SELF_PID);
	lrb_assert(!is_provider_done(auth, SELF_PID));
	lrb_assert(rb_dlink_list_length(&blacklist_list) > 0);

	if(bluser == NULL || rb_dlink_list_length(&bluser->queries))
		/* Nothing to do */
		return;
	else if((!get_provider_id("rdns", &rdns_pid) || is_provider_done(auth, rdns_pid)) &&
		(!get_provider_id("ident", &ident_pid) || is_provider_done(auth, ident_pid)))
		/* Don't start until ident and rdns are finished (or not loaded) */
		return;
	else
		lookup_all_blacklists(auth);
}

static void
blacklists_generic_cancel(struct auth_client *auth, const char *message)
{
	rb_dlink_node *ptr, *nptr;
	struct blacklist_user *bluser = get_provider_data(auth, SELF_PID);

	if(bluser == NULL)
		return;

	if(rb_dlink_list_length(&bluser->queries))
	{
		notice_client(auth->cid, message);

		RB_DLINK_FOREACH_SAFE(ptr, nptr, bluser->queries.head)
		{
			struct blacklist_lookup *bllookup = ptr->data;

			cancel_query(bllookup->query);
			unref_blacklist(bllookup->bl);

			rb_dlinkDelete(&bllookup->node, &bluser->queries);
			rb_free(bllookup);
		}
	}

	rb_free(bluser);
	set_provider_data(auth, SELF_PID, NULL);
	set_provider_timeout_absolute(auth, SELF_PID, 0);
	provider_done(auth, SELF_PID);

	auth_client_unref(auth);
}

static void
blacklists_cancel(struct auth_client *auth)
{
	blacklists_generic_cancel(auth, "*** Aborting DNS blacklist checks");
}

static void
blacklists_timeout(struct auth_client *auth)
{
	blacklists_generic_cancel(auth, "*** No response from DNS blacklists");
}

static void
blacklists_destroy(void)
{
	rb_dictionary_iter iter;
	struct auth_client *auth;

	RB_DICTIONARY_FOREACH(auth, &iter, auth_clients)
	{
		blacklists_cancel(auth);
		/* auth is now invalid as we have no reference */
	}

	delete_all_blacklists();
}

static void
add_conf_blacklist(const char *key, int parc, const char **parv)
{
	rb_dlink_list filters = { NULL, NULL, 0 };
	char *tmp, *elemlist = rb_strdup(parv[2]);
	uint8_t iptype;

	if(*elemlist == '*')
		goto end;

	for(char *elem = rb_strtok_r(elemlist, ",", &tmp); elem; elem = rb_strtok_r(NULL, ",", &tmp))
	{
		struct blacklist_filter *filter = rb_malloc(sizeof(struct blacklist_filter));
		int dot_c = 0;
		filter_t type = FILTER_LAST;

		/* Check blacklist filter type and for validity */
		for(char *c = elem; *c != '\0'; c++)
		{
			if(*c == '.')
			{
				if(++dot_c > 3)
				{
					warn_opers(L_CRIT, "Blacklist: addr_conf_blacklist got a bad filter (too many octets)");
					exit(EX_PROVIDER_ERROR);
				}

				type = FILTER_ALL;
			}
			else if(!isdigit(*c))
			{
				warn_opers(L_CRIT, "Blacklist: addr_conf_blacklist got a bad filter (invalid character in blacklist filter: %c)",
						*c);
				exit(EX_PROVIDER_ERROR);
			}
		}

		if(dot_c > 0 && dot_c < 3)
		{
			warn_opers(L_CRIT, "Blacklist: addr_conf_blacklist got a bad filter (insufficient octets)");
			exit(EX_PROVIDER_ERROR);
		}

		filter->type = type;
		rb_strlcpy(filter->filter, elem, sizeof(filter->filter));
		rb_dlinkAdd(filter, &filter->node, &filters);
	}

end:
	rb_free(elemlist);

	iptype = atoi(parv[1]) & 0x3;
	if(new_blacklist(parv[0], parv[3], iptype, &filters) == NULL)
	{
		warn_opers(L_CRIT, "Blacklist: addr_conf_blacklist got a malformed blacklist");
		exit(EX_PROVIDER_ERROR);
	}
}

static void
del_conf_blacklist(const char *key, int parc, const char **parv)
{
	struct blacklist *bl = find_blacklist(parv[0]);
	if(bl == NULL)
	{
		/* Not fatal for now... */
		warn_opers(L_WARN, "Blacklist: tried to remove nonexistent blacklist %s", parv[0]);
		return;
	}

	delete_blacklist(bl);
}

static void
del_conf_blacklist_all(const char *key, int parc, const char **parv)
{
	delete_all_blacklists();
}

static void
add_conf_blacklist_timeout(const char *key, int parc, const char **parv)
{
	int timeout = atoi(parv[0]);

	if(timeout < 0)
	{
		warn_opers(L_CRIT, "Blacklist: blacklist timeout < 0 (value: %d)", timeout);
		exit(EX_PROVIDER_ERROR);
	}

	blacklist_timeout = timeout;
}

#if 0
static void
blacklist_stats(uint32_t rid, char letter)
{
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, blacklist_list.head)
	{
		struct blacklist *bl = ptr->data;

		if(bl->delete)
			continue;

		stats_result(rid, letter, "%s %hhu %u", bl->host, bl->iptype, bl->hits);
	}

	stats_done(rid, letter);
}
#endif

struct auth_opts_handler blacklist_options[] =
{
	{ "rbl", 4, add_conf_blacklist },
	{ "rbl_del", 1, del_conf_blacklist },
	{ "rbl_del_all", 0, del_conf_blacklist_all },
	{ "rbl_timeout", 1, add_conf_blacklist_timeout },
	{ NULL, 0, NULL },
};

struct auth_provider blacklist_provider =
{
	.name = "blacklist",
	.letter = 'B',
	.destroy = blacklists_destroy,
	.start = blacklists_start,
	.cancel = blacklists_cancel,
	.timeout = blacklists_timeout,
	.completed = blacklists_initiate,
	.opt_handlers = blacklist_options,
	/* .stats_handler = { 'B', blacklist_stats }, */
};
