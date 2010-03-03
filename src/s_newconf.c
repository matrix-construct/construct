/*
 * ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 * s_newconf.c - code for dealing with conf stuff
 *
 * Copyright (C) 2004 Lee Hardy <lee@leeh.co.uk>
 * Copyright (C) 2004-2005 ircd-ratbox development team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1.Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * 2.Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3.The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
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
 * $Id: s_newconf.c 3508 2007-06-04 16:04:49Z jilles $
 */

#include "stdinc.h"
#include "ircd_defs.h"
#include "common.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "client.h"
#include "s_serv.h"
#include "send.h"
#include "hostmask.h"
#include "newconf.h"
#include "hash.h"
#include "irc_dictionary.h"

rb_dlink_list shared_conf_list;
rb_dlink_list cluster_conf_list;
rb_dlink_list oper_conf_list;
rb_dlink_list hubleaf_conf_list;
rb_dlink_list server_conf_list;
rb_dlink_list xline_conf_list;
rb_dlink_list resv_conf_list;	/* nicks only! */
rb_dlink_list nd_list;		/* nick delay */
rb_dlink_list tgchange_list;

rb_patricia_tree_t *tgchange_tree;

static rb_bh *nd_heap = NULL;

static void expire_temp_rxlines(void *unused);
static void expire_nd_entries(void *unused);

struct ev_entry *expire_nd_entries_ev = NULL;
struct ev_entry *expire_temp_rxlines_ev = NULL;

void
init_s_newconf(void)
{
	tgchange_tree = rb_new_patricia(PATRICIA_BITS);
	nd_heap = rb_bh_create(sizeof(struct nd_entry), ND_HEAP_SIZE, "nd_heap");
	expire_nd_entries_ev = rb_event_addish("expire_nd_entries", expire_nd_entries, NULL, 30);
	expire_temp_rxlines_ev = rb_event_addish("expire_temp_rxlines", expire_temp_rxlines, NULL, 60);
}

void
clear_s_newconf(void)
{
	struct server_conf *server_p;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, shared_conf_list.head)
	{
		/* ptr here is ptr->data->node */
		rb_dlinkDelete(ptr, &shared_conf_list);
		free_remote_conf(ptr->data);
	}

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, cluster_conf_list.head)
	{
		rb_dlinkDelete(ptr, &cluster_conf_list);
		free_remote_conf(ptr->data);
	}

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, hubleaf_conf_list.head)
	{
		rb_dlinkDelete(ptr, &hubleaf_conf_list);
		free_remote_conf(ptr->data);
	}

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, oper_conf_list.head)
	{
		free_oper_conf(ptr->data);
		rb_dlinkDestroy(ptr, &oper_conf_list);
	}

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, server_conf_list.head)
	{
		server_p = ptr->data;

		if(!server_p->servers)
		{
			rb_dlinkDelete(ptr, &server_conf_list);
			free_server_conf(ptr->data);
		}
		else
			server_p->flags |= SERVER_ILLEGAL;
	}
}

void
clear_s_newconf_bans(void)
{
	struct ConfItem *aconf;
	rb_dlink_node *ptr, *next_ptr;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, xline_conf_list.head)
	{
		aconf = ptr->data;

		if(aconf->hold)
			continue;

		free_conf(aconf);
		rb_dlinkDestroy(ptr, &xline_conf_list);
	}

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, resv_conf_list.head)
	{
		aconf = ptr->data;

		/* temporary resv */
		if(aconf->hold)
			continue;

		free_conf(aconf);
		rb_dlinkDestroy(ptr, &resv_conf_list);
	}

	clear_resv_hash();
}

struct remote_conf *
make_remote_conf(void)
{
	struct remote_conf *remote_p = rb_malloc(sizeof(struct remote_conf));
	return remote_p;
}

void
free_remote_conf(struct remote_conf *remote_p)
{
	s_assert(remote_p != NULL);
	if(remote_p == NULL)
		return;

	rb_free(remote_p->username);
	rb_free(remote_p->host);
	rb_free(remote_p->server);
	rb_free(remote_p);
}

int
find_shared_conf(const char *username, const char *host, 
		const char *server, int flags)
{
	struct remote_conf *shared_p;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, shared_conf_list.head)
	{
		shared_p = ptr->data;

		if(match(shared_p->username, username) &&
		   match(shared_p->host, host) &&
		   match(shared_p->server, server))
		{
			if(shared_p->flags & flags)
				return YES;
			else
				return NO;
		}
	}

	return NO;
}

void
propagate_generic(struct Client *source_p, const char *command,
		const char *target, int cap, const char *format, ...)
{
	char buffer[BUFSIZE];
	va_list args;

	va_start(args, format);
	rb_vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	sendto_match_servs(source_p, target, cap, NOCAPS,
			"%s %s %s",
			command, target, buffer);
	sendto_match_servs(source_p, target, CAP_ENCAP, cap,
			"ENCAP %s %s %s",
			target, command, buffer);
}
			
void
cluster_generic(struct Client *source_p, const char *command,
		int cltype, int cap, const char *format, ...)
{
	char buffer[BUFSIZE];
	struct remote_conf *shared_p;
	va_list args;
	rb_dlink_node *ptr;

	va_start(args, format);
	rb_vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	RB_DLINK_FOREACH(ptr, cluster_conf_list.head)
	{
		shared_p = ptr->data;

		if(!(shared_p->flags & cltype))
			continue;

		sendto_match_servs(source_p, shared_p->server, cap, NOCAPS,
				"%s %s %s",
				command, shared_p->server, buffer);
		sendto_match_servs(source_p, shared_p->server, CAP_ENCAP, cap,
				"ENCAP %s %s %s",
				shared_p->server, command, buffer);
	}
}

struct oper_conf *
make_oper_conf(void)
{
	struct oper_conf *oper_p = rb_malloc(sizeof(struct oper_conf));
	return oper_p;
}

void
free_oper_conf(struct oper_conf *oper_p)
{
	s_assert(oper_p != NULL);
	if(oper_p == NULL)
		return;

	rb_free(oper_p->username);
	rb_free(oper_p->host);
	rb_free(oper_p->name);

	if(oper_p->passwd)
	{
		memset(oper_p->passwd, 0, strlen(oper_p->passwd));
		rb_free(oper_p->passwd);
	}

#ifdef HAVE_LIBCRYPTO
	rb_free(oper_p->rsa_pubkey_file);

	if(oper_p->rsa_pubkey)
		RSA_free(oper_p->rsa_pubkey);
#endif

	rb_free(oper_p);
}

struct oper_conf *
find_oper_conf(const char *username, const char *host, const char *locip, const char *name)
{
	struct oper_conf *oper_p;
	struct rb_sockaddr_storage ip, cip;
	char addr[HOSTLEN+1];
	int bits, cbits;
	rb_dlink_node *ptr;

	parse_netmask(locip, (struct sockaddr *)&cip, &cbits);

	RB_DLINK_FOREACH(ptr, oper_conf_list.head)
	{
		oper_p = ptr->data;

		/* name/username doesnt match.. */
		if(irccmp(oper_p->name, name) || !match(oper_p->username, username))
			continue;

		rb_strlcpy(addr, oper_p->host, sizeof(addr));

		if(parse_netmask(addr, (struct sockaddr *)&ip, &bits) != HM_HOST)
		{
			if(ip.ss_family == cip.ss_family &&
			   comp_with_mask_sock((struct sockaddr *)&ip, (struct sockaddr *)&cip, bits))
				return oper_p;
		}

		/* we have to compare against the host as well, because its
		 * valid to set a spoof to an IP, which if we only compare
		 * in ip form to sockhost will not necessarily match --anfl
		 */
		if(match(oper_p->host, host))
			return oper_p;
	}

	return NULL;
}

struct server_conf *
make_server_conf(void)
{
	struct server_conf *server_p = rb_malloc(sizeof(struct server_conf));
	server_p->aftype = AF_INET;
	return server_p;
}

void
free_server_conf(struct server_conf *server_p)
{
	s_assert(server_p != NULL);
	if(server_p == NULL)
		return;

	if(!EmptyString(server_p->passwd))
	{
		memset(server_p->passwd, 0, strlen(server_p->passwd));
		rb_free(server_p->passwd);
	}

	if(!EmptyString(server_p->spasswd))
	{
		memset(server_p->spasswd, 0, strlen(server_p->spasswd));
		rb_free(server_p->spasswd);
	}

	rb_free(server_p->name);
	rb_free(server_p->host);
	rb_free(server_p->class_name);
	rb_free(server_p);
}

void
add_server_conf(struct server_conf *server_p)
{
	if(EmptyString(server_p->class_name))
	{
		server_p->class_name = rb_strdup("default");
		server_p->class = default_class;
		return;
	}

	server_p->class = find_class(server_p->class_name);

	if(server_p->class == default_class)
	{
		conf_report_error("Warning connect::class invalid for %s",
				server_p->name);

		rb_free(server_p->class_name);
		server_p->class_name = rb_strdup("default");
	}

	if(strchr(server_p->host, '*') || strchr(server_p->host, '?'))
		return;
}

struct server_conf *
find_server_conf(const char *name)
{
	struct server_conf *server_p;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, server_conf_list.head)
	{
		server_p = ptr->data;

		if(ServerConfIllegal(server_p))
			continue;

		if(match(name, server_p->name))
			return server_p;
	}

	return NULL;
}

void
attach_server_conf(struct Client *client_p, struct server_conf *server_p)
{
	/* already have an attached conf */
	if(client_p->localClient->att_sconf)
	{
		/* short circuit this special case :) */
		if(client_p->localClient->att_sconf == server_p)
			return;

		detach_server_conf(client_p);
	}

	CurrUsers(server_p->class)++;

	client_p->localClient->att_sconf = server_p;
	server_p->servers++;
}

void
detach_server_conf(struct Client *client_p)
{
	struct server_conf *server_p = client_p->localClient->att_sconf;

	if(server_p == NULL)
		return;

	client_p->localClient->att_sconf = NULL;
	server_p->servers--;
	CurrUsers(server_p->class)--;

	if(ServerConfIllegal(server_p) && !server_p->servers)
	{
		/* the class this one is using may need destroying too */
		if(MaxUsers(server_p->class) < 0 && CurrUsers(server_p->class) <= 0)
			free_class(server_p->class);

		rb_dlinkDelete(&server_p->node, &server_conf_list);
		free_server_conf(server_p);
	}
}

void
set_server_conf_autoconn(struct Client *source_p, const char *name, int newval)
{
	struct server_conf *server_p;

	if((server_p = find_server_conf(name)) != NULL)
	{
		if(newval)
			server_p->flags |= SERVER_AUTOCONN;
		else
			server_p->flags &= ~SERVER_AUTOCONN;

		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				"%s has changed AUTOCONN for %s to %i",
				get_oper_name(source_p), name, newval);
	}
	else
		sendto_one_notice(source_p, ":Can't find %s", name);
}

void
disable_server_conf_autoconn(const char *name)
{
	struct server_conf *server_p;

	server_p = find_server_conf(name);
	if(server_p != NULL && server_p->flags & SERVER_AUTOCONN)
	{
		server_p->flags &= ~SERVER_AUTOCONN;

		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				"Disabling AUTOCONN for %s because of error",
				name);
		ilog(L_SERVER, "Disabling AUTOCONN for %s because of error",
				name);
	}
}

struct ConfItem *
find_xline(const char *gecos, int counter)
{
	struct ConfItem *aconf;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, xline_conf_list.head)
	{
		aconf = ptr->data;

		if(match_esc(aconf->host, gecos))
		{
			if(counter)
				aconf->port++;
			return aconf;
		}
	}

	return NULL;
}

struct ConfItem *
find_xline_mask(const char *gecos)
{
	struct ConfItem *aconf;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, xline_conf_list.head)
	{
		aconf = ptr->data;

		if(!irccmp(aconf->host, gecos))
			return aconf;
	}

	return NULL;
}

struct ConfItem *
find_nick_resv(const char *name)
{
	struct ConfItem *aconf;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, resv_conf_list.head)
	{
		aconf = ptr->data;

		if(match_esc(aconf->host, name))
		{
			aconf->port++;
			return aconf;
		}
	}

	return NULL;
}

struct ConfItem *
find_nick_resv_mask(const char *name)
{
	struct ConfItem *aconf;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, resv_conf_list.head)
	{
		aconf = ptr->data;

		if(!irccmp(aconf->host, name))
			return aconf;
	}

	return NULL;
}

/* clean_resv_nick()
 *
 * inputs	- nick
 * outputs	- 1 if nick is vaild resv, 0 otherwise
 * side effects -
 */
int
clean_resv_nick(const char *nick)
{
	char tmpch;
	int as = 0;
	int q = 0;
	int ch = 0;

	if(*nick == '-' || IsDigit(*nick))
		return 0;

	while ((tmpch = *nick++))
	{
		if(tmpch == '?' || tmpch == '@' || tmpch == '#')
			q++;
		else if(tmpch == '*')
			as++;
		else if(IsNickChar(tmpch))
			ch++;
		else
			return 0;
	}

	if(!ch && as)
		return 0;

	return 1;
}

/* valid_wild_card_simple()
 *
 * inputs	- "thing" to test
 * outputs	- 1 if enough wildcards, else 0
 * side effects -
 */
int
valid_wild_card_simple(const char *data)
{
	const char *p;
	char tmpch;
	int nonwild = 0;
	int wild = 0;

	/* check the string for minimum number of nonwildcard chars */
	p = data;

	while((tmpch = *p++))
	{
		/* found an escape, p points to the char after it, so skip
		 * that and move on.
		 */
		if(tmpch == '\\' && *p)
		{
			p++;
			if(++nonwild >= ConfigFileEntry.min_nonwildcard_simple)
				return 1;
		}
		else if(!IsMWildChar(tmpch))
		{
			/* if we have enough nonwildchars, return */
			if(++nonwild >= ConfigFileEntry.min_nonwildcard_simple)
				return 1;
		}
		else
			wild++;
	}

	/* strings without wilds are also ok */
	return wild == 0;
}

time_t
valid_temp_time(const char *p)
{
	time_t result = 0;

	while(*p)
	{
		if(IsDigit(*p))
		{
			result *= 10;
			result += ((*p) & 0xF);
			p++;
		}
		else
			return -1;
	}

	if(result > (60 * 24 * 7 * 52))
		result = (60 * 24 * 7 * 52);

	return(result * 60);
}

/* Propagated bans are expired elsewhere. */
static void
expire_temp_rxlines(void *unused)
{
	struct ConfItem *aconf;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;
	int i;

	HASH_WALK_SAFE(i, R_MAX, ptr, next_ptr, resvTable)
	{
		aconf = ptr->data;

		if(aconf->lifetime != 0)
			continue;
		if(aconf->hold && aconf->hold <= rb_current_time())
		{
			if(ConfigFileEntry.tkline_expire_notices)
				sendto_realops_snomask(SNO_GENERAL, L_ALL,
						"Temporary RESV for [%s] expired",
						aconf->host);

			free_conf(aconf);
			rb_dlinkDestroy(ptr, &resvTable[i]);
		}
	}
	HASH_WALK_END

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, resv_conf_list.head)
	{
		aconf = ptr->data;

		if(aconf->lifetime != 0)
			continue;
		if(aconf->hold && aconf->hold <= rb_current_time())
		{
			if(ConfigFileEntry.tkline_expire_notices)
				sendto_realops_snomask(SNO_GENERAL, L_ALL,
						"Temporary RESV for [%s] expired",
						aconf->host);
			free_conf(aconf);
			rb_dlinkDestroy(ptr, &resv_conf_list);
		}
	}

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, xline_conf_list.head)
	{
		aconf = ptr->data;

		if(aconf->lifetime != 0)
			continue;
		if(aconf->hold && aconf->hold <= rb_current_time())
		{
			if(ConfigFileEntry.tkline_expire_notices)
				sendto_realops_snomask(SNO_GENERAL, L_ALL,
						"Temporary X-line for [%s] expired",
						aconf->host);
			free_conf(aconf);
			rb_dlinkDestroy(ptr, &xline_conf_list);
		}
	}
}

unsigned long
get_nd_count(void)
{
	return(rb_dlink_list_length(&nd_list));
}

void
add_nd_entry(const char *name)
{
	struct nd_entry *nd;

	if(irc_dictionary_find(nd_dict, name) != NULL)
		return;

	nd = rb_bh_alloc(nd_heap);
	
	rb_strlcpy(nd->name, name, sizeof(nd->name));
	nd->expire = rb_current_time() + ConfigFileEntry.nick_delay;

	/* this list is ordered */
	rb_dlinkAddTail(nd, &nd->lnode, &nd_list);

	irc_dictionary_add(nd_dict, nd->name, nd);
}

void
free_nd_entry(struct nd_entry *nd)
{
	irc_dictionary_delete(nd_dict, nd->name);

	rb_dlinkDelete(&nd->lnode, &nd_list);
	rb_bh_free(nd_heap, nd);
}

void
expire_nd_entries(void *unused)
{
	struct nd_entry *nd;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, nd_list.head)
	{
		nd = ptr->data;

		/* this list is ordered - we can stop when we hit the first
		 * entry that doesnt expire..
		 */
		if(nd->expire > rb_current_time())
			return;

		free_nd_entry(nd);
	}
}

void
add_tgchange(const char *host)
{
	tgchange *target;
	rb_patricia_node_t *pnode;

	if(find_tgchange(host))
		return;

	target = rb_malloc(sizeof(tgchange));
	pnode = make_and_lookup(tgchange_tree, host);

	pnode->data = target;
	target->pnode = pnode;

	target->ip = rb_strdup(host);
	target->expiry = rb_current_time() + (60*60*12);

	rb_dlinkAdd(target, &target->node, &tgchange_list);
}

tgchange *
find_tgchange(const char *host)
{
	rb_patricia_node_t *pnode;

	if((pnode = rb_match_exact_string(tgchange_tree, host)))
		return pnode->data;

	return NULL;
}

