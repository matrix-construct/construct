/*
 * charybdis: An advanced ircd.
 * m_ban.c: Propagates network bans across servers.
 * 
 *  Copyright (C) 2010 Jilles Tjoelker
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

#include "stdinc.h"
#include "send.h"
#include "client.h"
#include "common.h"
#include "config.h"
#include "ircd.h"
#include "match.h"
#include "s_conf.h"
#include "msg.h"
#include "modules.h"
#include "hash.h"
#include "s_serv.h"
#include "operhash.h"
#include "reject.h"
#include "hostmask.h"

static int ms_ban(struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);

struct Message ban_msgtab = {
	"BAN", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_ignore, {ms_ban, 10}, {ms_ban, 10}, mg_ignore, mg_ignore}
};

mapi_clist_av1 ban_clist[] =  { &ban_msgtab, NULL };
DECLARE_MODULE_AV1(ban, NULL, NULL, ban_clist, NULL, NULL, "$Revision: 1349 $");

/* ms_ban()
 *
 * parv[1] - +/-
 * parv[2] - type
 * parv[3] - username mask or *
 * parv[4] - hostname mask
 * parv[5] - creation TS
 * parv[6] - duration (relative to creation)
 * parv[7] - lifetime (relative to creation)
 * parv[8] - oper or *
 * parv[9] - reason (possibly with |operreason)
 */
static int
ms_ban(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	rb_dlink_node *ptr;
	struct ConfItem *aconf;
	unsigned int ntype;
	const char *oper, *stype;
	time_t created, hold, lifetime;
	char *p;
	int act;

	if (strcmp(parv[1], "+") && strcmp(parv[1], "-"))
	{
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
				"Unknown BAN operation %s from %s",
				parv[1], source_p->name);
		return 0;
	}
	if (strlen(parv[2]) != 1)
	{
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
				"Unknown BAN type %s from %s",
				parv[2], source_p->name);
		return 0;
	}
	switch (parv[2][0])
	{
		case 'K':
			ntype = CONF_KILL;
			stype = "K-Line";
			break;
		default:
			sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
					"Unknown BAN type %s from %s",
					parv[2], source_p->name);
			return 0;
	}
	created = atol(parv[5]);
	hold = created + atoi(parv[6]);
	lifetime = created + atoi(parv[7]);
	if (!strcmp(parv[8], "*"))
		oper = IsServer(source_p) ? source_p->name : get_oper_name(source_p);
	else
		oper = parv[8];
	ptr = find_prop_ban(ntype, parv[3], parv[4]);
	if (ptr != NULL)
	{
		aconf = ptr->data;
		if (aconf->created >= created)
		{
			if (IsPerson(source_p))
				sendto_one_notice(source_p,
						":Your %s [%s%s%s] has been superseded",
						stype,
						aconf->user ? aconf->user : "",
						aconf->user ? "@" : "",
						aconf->host);
			return 0;
		}
		act = !(aconf->status & CONF_ILLEGAL) || !strcmp(parv[1], "+");
		if (lifetime > aconf->lifetime)
			aconf->lifetime = lifetime;
		/* already expired, hmm */
		if (aconf->lifetime <= rb_current_time())
			return 0;
		deactivate_conf(aconf, ptr);
		rb_free(aconf->user);
		aconf->user = NULL;
		rb_free(aconf->host);
		aconf->host = NULL;
		operhash_delete(aconf->info.oper);
		aconf->info.oper = NULL;
		rb_free(aconf->passwd);
		aconf->passwd = NULL;
		rb_free(aconf->spasswd);
		aconf->spasswd = NULL;
	}
	else
	{
		aconf = make_conf();
		aconf->status = CONF_ILLEGAL | ntype;
		aconf->lifetime = lifetime;
		rb_dlinkAddAlloc(aconf, &prop_bans);
		act = !strcmp(parv[1], "+");
	}
	aconf->flags &= ~CONF_FLAGS_MYOPER;
	aconf->flags |= CONF_FLAGS_TEMPORARY;
	aconf->user = ntype == CONF_KILL ? rb_strdup(parv[3]) : NULL;
	aconf->host = rb_strdup(parv[4]);
	aconf->info.oper = operhash_add(oper);
	aconf->created = created;
	aconf->hold = hold;
	p = strchr(parv[parc - 1], '|');
	if (p == NULL)
		aconf->passwd = rb_strdup(parv[parc - 1]);
	else
	{
		aconf->passwd = rb_strndup(parv[parc - 1], p - parv[parc - 1] + 1);
		aconf->spasswd = rb_strdup(p + 1);
	}
	if (!strcmp(parv[1], "+"))
	{
		/* Keep the notices in sync with modules/m_kline.c etc. */
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				       "%s added global %d min. %s%s%s for [%s%s%s] [%s]",
				       IsServer(source_p) ? source_p->name : get_oper_name(source_p),
				       (hold - rb_current_time()) / 60,
				       stype,
				       strcmp(parv[8], "*") ? " from " : "",
				       strcmp(parv[8], "*") ? parv[8] : "",
				       aconf->user ? aconf->user : "",
				       aconf->user ? "@" : "",
				       aconf->host,
				       parv[parc - 1]);
		aconf->status &= ~CONF_ILLEGAL;
		ilog(L_KLINE, "%s %s %d %s %s %s", parv[2],
				IsServer(source_p) ? source_p->name : get_oper_name(source_p),
				(hold - rb_current_time()) / 60,
				aconf->user, aconf->host,
				parv[parc - 1]);
	}
	else if (act)
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				"%s has removed the global %s for: [%s%s%s]%s%s",
				IsServer(source_p) ? source_p->name : get_oper_name(source_p),
				stype,
				aconf->user ? aconf->user : "",
				aconf->user ? "@" : "",
				aconf->host,
				strcmp(parv[8], "*") ? " on behalf of " : "",
				strcmp(parv[8], "*") ? parv[8] : "");
		ilog(L_KLINE, "U%s %s %s %s", parv[2],
				IsServer(source_p) ? source_p->name : get_oper_name(source_p),
				aconf->user, aconf->host);
	}
	switch (ntype)
	{
		case CONF_KILL:
			if (aconf->status & CONF_ILLEGAL)
				remove_reject_mask(aconf->user, aconf->host);
			else
			{
				add_conf_by_address(aconf->host, CONF_KILL, aconf->user, NULL, aconf);
				if(ConfigFileEntry.kline_delay ||
						(IsServer(source_p) &&
						 !HasSentEob(source_p)))
				{
					if(kline_queued == 0)
					{
						rb_event_addonce("check_klines", check_klines_event, NULL,
								 ConfigFileEntry.kline_delay);
						kline_queued = 1;
					}
				}
				else
					check_klines();
			}
			break;
	}
	sendto_server(NULL, NULL, CAP_BAN|CAP_TS6, NOCAPS,
			":%s BAN %s %s %s %s %s %s %s %s :%s",
			source_p->id,
			parv[1],
			parv[2],
			parv[3],
			parv[4],
			parv[5],
			parv[6],
			parv[7],
			parv[8],
			parv[parc - 1]);
	return 0;
}
