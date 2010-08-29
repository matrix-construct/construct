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
#include "channel.h"
#include "client.h"
#include "common.h"
#include "config.h"
#include "ircd.h"
#include "match.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "msg.h"
#include "modules.h"
#include "hash.h"
#include "s_serv.h"
#include "operhash.h"
#include "reject.h"
#include "hostmask.h"

static int m_ban(struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);
static int ms_ban(struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);

struct Message ban_msgtab = {
	"BAN", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_ban, 0}, {ms_ban, 9}, {ms_ban, 9}, mg_ignore, {m_ban, 0}}
};

mapi_clist_av1 ban_clist[] =  { &ban_msgtab, NULL };
DECLARE_MODULE_AV1(ban, NULL, NULL, ban_clist, NULL, NULL, "$Revision: 1349 $");

static int
m_ban(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	sendto_one_notice(source_p, ":The BAN command is not user-accessible.");
	sendto_one_notice(source_p, ":To ban a user from a channel, see /QUOTE HELP CMODE");
	if (IsOper(source_p))
		sendto_one_notice(source_p, ":To ban a user from a server or from the network, see /QUOTE HELP KLINE");
	return 0;
}

/* ms_ban()
 *
 * parv[1] - type
 * parv[2] - username mask or *
 * parv[3] - hostname mask
 * parv[4] - creation TS
 * parv[5] - duration (relative to creation)
 * parv[6] - lifetime (relative to creation)
 * parv[7] - oper or *
 * parv[8] - reason (possibly with |operreason)
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
	int valid;

	if (strlen(parv[1]) != 1)
	{
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
				"Unknown BAN type %s from %s",
				parv[1], source_p->name);
		return 0;
	}
	switch (parv[1][0])
	{
		case 'K':
			ntype = CONF_KILL;
			stype = "K-Line";
			break;
		case 'X':
			ntype = CONF_XLINE;
			stype = "X-Line";
			break;
		case 'R':
			ntype = IsChannelName(parv[3]) ? CONF_RESV_CHANNEL :
				CONF_RESV_NICK;
			stype = "RESV";
			break;
		default:
			sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
					"Unknown BAN type %s from %s",
					parv[1], source_p->name);
			return 0;
	}
	created = atol(parv[4]);
	hold = created + atoi(parv[5]);
	lifetime = created + atoi(parv[6]);
	if (!strcmp(parv[7], "*"))
		oper = IsServer(source_p) ? source_p->name : get_oper_name(source_p);
	else
		oper = parv[7];
	ptr = find_prop_ban(ntype, parv[2], parv[3]);
	if (ptr != NULL)
	{
		/* We already know about this ban mask. */
		aconf = ptr->data;
		if (aconf->created > created ||
				(aconf->created == created &&
				 aconf->lifetime >= lifetime))
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
		/* act indicates if something happened (from the oper's
		 * point of view). This is the case if the ban was
		 * previously active (not deleted) or if the new ban
		 * is not a removal and not already expired.
		 */
		act = !(aconf->status & CONF_ILLEGAL) || (hold != created &&
				hold > rb_current_time());
		if (lifetime > aconf->lifetime)
			aconf->lifetime = lifetime;
		/* already expired, hmm */
		if (aconf->lifetime <= rb_current_time())
			return 0;
		/* Deactivate, it will be reactivated later if appropriate. */
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
		/* New ban mask. */
		aconf = make_conf();
		aconf->status = CONF_ILLEGAL | ntype;
		aconf->lifetime = lifetime;
		rb_dlinkAddAlloc(aconf, &prop_bans);
		act = hold != created && hold > rb_current_time();
	}
	aconf->flags &= ~CONF_FLAGS_MYOPER;
	aconf->flags |= CONF_FLAGS_TEMPORARY;
	aconf->user = ntype == CONF_KILL ? rb_strdup(parv[2]) : NULL;
	aconf->host = rb_strdup(parv[3]);
	aconf->info.oper = operhash_add(oper);
	aconf->created = created;
	aconf->hold = hold;
	if (ntype != CONF_KILL || (p = strchr(parv[parc - 1], '|')) == NULL)
		aconf->passwd = rb_strdup(parv[parc - 1]);
	else
	{
		aconf->passwd = rb_strndup(parv[parc - 1], p - parv[parc - 1] + 1);
		aconf->spasswd = rb_strdup(p + 1);
	}
	/* The ban is fully filled in and in the prop_bans list
	 * but still deactivated. Now determine if it should be activated
	 * and send the server notices.
	 */
	/* We only reject *@* and the like here.
	 * Otherwise malformed bans are fairly harmless and can be removed.
	 */
	switch (ntype)
	{
		case CONF_KILL:
			valid = valid_wild_card(aconf->user, aconf->host);
			break;
		case CONF_RESV_CHANNEL:
			valid = 1;
			break;
		default:
			valid = valid_wild_card_simple(aconf->host);
			break;
	}
	if (act && hold != created && !valid)
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				       "Ignoring global %d min. %s from %s%s%s for [%s%s%s]: too few non-wildcard characters",
				       (int)((hold - rb_current_time()) / 60),
				       stype,
				       IsServer(source_p) ? source_p->name : get_oper_name(source_p),
				       strcmp(parv[7], "*") ? " on behalf of " : "",
				       strcmp(parv[7], "*") ? parv[7] : "",
				       aconf->user ? aconf->user : "",
				       aconf->user ? "@" : "",
				       aconf->host);
		if(IsPerson(source_p))
			sendto_one_notice(source_p,
					":Your %s [%s%s%s] has too few non-wildcard characters",
					stype,
					aconf->user ? aconf->user : "",
					aconf->user ? "@" : "",
					aconf->host);
		/* Propagate it, but do not apply it locally. */
	}
	else if (act && hold != created)
	{
		/* Keep the notices in sync with modules/m_kline.c etc. */
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				       "%s added global %d min. %s%s%s for [%s%s%s] [%s]",
				       IsServer(source_p) ? source_p->name : get_oper_name(source_p),
				       (int)((hold - rb_current_time()) / 60),
				       stype,
				       strcmp(parv[7], "*") ? " from " : "",
				       strcmp(parv[7], "*") ? parv[7] : "",
				       aconf->user ? aconf->user : "",
				       aconf->user ? "@" : "",
				       aconf->host,
				       parv[parc - 1]);
		ilog(L_KLINE, "%s %s %d %s%s%s %s", parv[1],
				IsServer(source_p) ? source_p->name : get_oper_name(source_p),
				(int)((hold - rb_current_time()) / 60),
				aconf->user ? aconf->user : "",
				aconf->user ? " " : "",
				aconf->host,
				parv[parc - 1]);
		aconf->status &= ~CONF_ILLEGAL;
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
				strcmp(parv[7], "*") ? " on behalf of " : "",
				strcmp(parv[7], "*") ? parv[7] : "");
		ilog(L_KLINE, "U%s %s %s%s %s", parv[1],
				IsServer(source_p) ? source_p->name : get_oper_name(source_p),
				aconf->user ? aconf->user : "",
				aconf->user ? " " : "",
				aconf->host);
	}
	/* If CONF_ILLEGAL is still set at this point, remove entries from the
	 * reject cache (for klines and xlines).
	 * If CONF_ILLEGAL is not set, add the ban to the type-specific data
	 * structure and take action on matched clients/channels.
	 */
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
		case CONF_XLINE:
			if (aconf->status & CONF_ILLEGAL)
				remove_reject_mask(aconf->host, NULL);
			else
			{
				rb_dlinkAddAlloc(aconf, &xline_conf_list);
				check_xlines();
			}
			break;
		case CONF_RESV_CHANNEL:
			if (!(aconf->status & CONF_ILLEGAL))
			{
				add_to_resv_hash(aconf->host, aconf);
				resv_chan_forcepart(aconf->host, aconf->passwd, hold - rb_current_time());
			}
			break;
		case CONF_RESV_NICK:
			if (!(aconf->status & CONF_ILLEGAL))
				rb_dlinkAddAlloc(aconf, &resv_conf_list);
			break;
	}
	sendto_server(client_p, NULL, CAP_BAN|CAP_TS6, NOCAPS,
			":%s BAN %s %s %s %s %s %s %s :%s",
			source_p->id,
			parv[1],
			parv[2],
			parv[3],
			parv[4],
			parv[5],
			parv[6],
			parv[7],
			parv[parc - 1]);
	return 0;
}
