/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_kline.c: Bans/unbans a user.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 *
 *  $Id$
 */

#include "stdinc.h"
#include "channel.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "match.h"
#include "ircd.h"
#include "hostmask.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "logger.h"
#include "send.h"
#include "hash.h"
#include "s_serv.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "reject.h"
#include "bandbi.h"
#include "operhash.h"

static int mo_kline(struct Client *, struct Client *, int, const char **);
static int ms_kline(struct Client *, struct Client *, int, const char **);
static int me_kline(struct Client *, struct Client *, int, const char **);
static int mo_unkline(struct Client *, struct Client *, int, const char **);
static int ms_unkline(struct Client *, struct Client *, int, const char **);
static int me_unkline(struct Client *, struct Client *, int, const char **);

struct Message kline_msgtab = {
	"KLINE", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, {ms_kline, 5}, {ms_kline, 5}, {me_kline, 5}, {mo_kline, 3}}
};

struct Message unkline_msgtab = {
	"UNKLINE", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, {ms_unkline, 4}, {ms_unkline, 4}, {me_unkline, 3}, {mo_unkline, 2}}
};

mapi_clist_av1 kline_clist[] = { &kline_msgtab, &unkline_msgtab, NULL };

DECLARE_MODULE_AV1(kline, NULL, NULL, kline_clist, NULL, NULL, "$Revision$");

/* Local function prototypes */
static int find_user_host(struct Client *source_p, const char *userhost, char *user, char *host);
static int valid_comment(struct Client *source_p, char *comment);
static int valid_user_host(struct Client *source_p, const char *user, const char *host);

static void handle_remote_kline(struct Client *source_p, int tkline_time,
				const char *user, const char *host, const char *reason);
static void apply_kline(struct Client *source_p, struct ConfItem *aconf,
			const char *reason, const char *oper_reason);
static void apply_tkline(struct Client *source_p, struct ConfItem *aconf,
			 const char *, const char *, int);
static void apply_prop_kline(struct Client *source_p, struct ConfItem *aconf,
			 const char *, const char *, int);
static int already_placed_kline(struct Client *, const char *, const char *, int);

static void handle_remote_unkline(struct Client *source_p, const char *user, const char *host);
static void remove_permkline_match(struct Client *, struct ConfItem *);
static int remove_temp_kline(struct Client *, struct ConfItem *);
static void remove_prop_kline(struct Client *, struct ConfItem *);

/* mo_kline()
 *
 *   parv[1] - temp time or user@host
 *   parv[2] - user@host, "ON", or reason
 *   parv[3] - "ON", reason, or server to target
 *   parv[4] - server to target, or reason
 *   parv[5] - reason
 */
static int
mo_kline(struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	char def[] = "No Reason";
	char user[USERLEN + 2];
	char host[HOSTLEN + 2];
	char *reason = def;
	char *oper_reason;
	const char *target_server = NULL;
	struct ConfItem *aconf;
	int tkline_time = 0;
	int loc = 1;
	int propagated = ConfigFileEntry.use_propagated_bans;

	if(!IsOperK(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "kline");
		return 0;
	}

	if((tkline_time = valid_temp_time(parv[loc])) >= 0)
		loc++;
	/* we just set tkline_time to -1! */
	else
		tkline_time = 0;

	if(find_user_host(source_p, parv[loc], user, host) == 0)
		return 0;

	loc++;

	if(parc >= loc + 2 && !irccmp(parv[loc], "ON"))
	{
		if(!IsOperRemoteBan(source_p))
		{
			sendto_one(source_p, form_str(ERR_NOPRIVS),
				   me.name, source_p->name, "remoteban");
			return 0;
		}

		target_server = parv[loc + 1];
		loc += 2;
	}

	if(parc <= loc || EmptyString(parv[loc]))
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
			   me.name, source_p->name, "KLINE");
		return 0;
	}

	reason = LOCAL_COPY(parv[loc]);

	if(target_server != NULL)
	{
		propagate_generic(source_p, "KLINE", target_server, CAP_KLN,
				  "%d %s %s :%s", tkline_time, user, host, reason);

		/* If we are sending it somewhere that doesnt include us, stop */
		if(!match(target_server, me.name))
			return 0;

		/* Set as local-only. */
		propagated = 0;
	}
	/* if we have cluster servers, send it to them.. */
	else if(!propagated && rb_dlink_list_length(&cluster_conf_list) > 0)
		cluster_generic(source_p, "KLINE",
				(tkline_time > 0) ? SHARED_TKLINE : SHARED_PKLINE, CAP_KLN,
				"%lu %s %s :%s", tkline_time, user, host, reason);

	if(!valid_user_host(source_p, user, host) ||
	   !valid_comment(source_p, reason))
		return 0;

	if(!valid_wild_card(user, host))
	{
		sendto_one_notice(source_p,
				  ":Please include at least %d non-wildcard "
				  "characters with the user@host",
				  ConfigFileEntry.min_nonwildcard);
		return 0;
	}

	if(propagated && tkline_time == 0)
	{
		sendto_one_notice(source_p, ":Cannot set a permanent global ban");
		return 0;
	}

	if(already_placed_kline(source_p, user, host, tkline_time))
		return 0;

	rb_set_time();
	aconf = make_conf();
	aconf->status = CONF_KILL;
	aconf->created = rb_current_time();
	aconf->host = rb_strdup(host);
	aconf->user = rb_strdup(user);
	aconf->port = 0;
	aconf->info.oper = operhash_add(get_oper_name(source_p));

	/* Look for an oper reason */
	if((oper_reason = strchr(reason, '|')) != NULL)
	{
		*oper_reason = '\0';
		oper_reason++;

		if(!EmptyString(oper_reason))
			aconf->spasswd = rb_strdup(oper_reason);
	}
	aconf->passwd = rb_strdup(reason);

	if(propagated)
		apply_prop_kline(source_p, aconf, reason, oper_reason, tkline_time);
	else if(tkline_time > 0)
		apply_tkline(source_p, aconf, reason, oper_reason, tkline_time);
	else
		apply_kline(source_p, aconf, reason, oper_reason);

	if(ConfigFileEntry.kline_delay)
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

	return 0;
}

/* ms_kline()
 *
 *   parv[1] - server targeted at
 *   parv[2] - tkline time (0 if perm)
 *   parv[3] - user
 *   parv[4] - host
 *   parv[5] - reason
 */
static int
ms_kline(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	int tkline_time = atoi(parv[2]);

	/* 1.5-3 and earlier contains a bug that allows remote klines to be
	 * sent with an empty reason field.  This is a protocol violation,
	 * but its not worth dropping the link over.. --anfl
	 */
	if(parc < 6 || EmptyString(parv[5]))
		return 0;

	propagate_generic(source_p, "KLINE", parv[1], CAP_KLN,
			  "%d %s %s :%s", tkline_time, parv[3], parv[4], parv[5]);

	if(!match(parv[1], me.name))
		return 0;

	if(!IsPerson(source_p))
		return 0;

	handle_remote_kline(source_p, tkline_time, parv[3], parv[4], parv[5]);
	return 0;
}

static int
me_kline(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	/* <tkline_time> <user> <host> :<reason> */
	if(!IsPerson(source_p))
		return 0;

	handle_remote_kline(source_p, atoi(parv[1]), parv[2], parv[3], parv[4]);
	return 0;
}

static void
handle_remote_kline(struct Client *source_p, int tkline_time,
		    const char *user, const char *host, const char *kreason)
{
	char *reason = LOCAL_COPY(kreason);
	struct ConfItem *aconf = NULL;
	char *oper_reason;

	if(!find_shared_conf(source_p->username, source_p->host,
			     source_p->servptr->name,
			     (tkline_time > 0) ? SHARED_TKLINE : SHARED_PKLINE))
		return;

	if(!valid_user_host(source_p, user, host) ||
	   !valid_comment(source_p, reason))
		return;

	if(!valid_wild_card(user, host))
	{
		sendto_one_notice(source_p,
				  ":Please include at least %d non-wildcard "
				  "characters with the user@host",
				  ConfigFileEntry.min_nonwildcard);
		return;
	}

	if(already_placed_kline(source_p, user, host, tkline_time))
		return;

	aconf = make_conf();

	aconf->status = CONF_KILL;
	aconf->created = rb_current_time();
	aconf->user = rb_strdup(user);
	aconf->host = rb_strdup(host);
	aconf->info.oper = operhash_add(get_oper_name(source_p));

	/* Look for an oper reason */
	if((oper_reason = strchr(reason, '|')) != NULL)
	{
		*oper_reason = '\0';
		oper_reason++;

		if(!EmptyString(oper_reason))
			aconf->spasswd = rb_strdup(oper_reason);
	}
	aconf->passwd = rb_strdup(reason);

	if(tkline_time > 0)
		apply_tkline(source_p, aconf, reason, oper_reason, tkline_time);
	else
		apply_kline(source_p, aconf, reason, oper_reason);

	if(ConfigFileEntry.kline_delay)
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

	return;
}

/* mo_unkline()
 *
 *   parv[1] - kline to remove
 *   parv[2] - optional "ON"
 *   parv[3] - optional target server
 */
static int
mo_unkline(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	const char *user;
	char *host;
	char splat[] = "*";
	char *h = LOCAL_COPY(parv[1]);
	struct ConfItem *aconf;
	int propagated = 1;

	if(!IsOperUnkline(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "unkline");
		return 0;
	}

	if((host = strchr(h, '@')) || *h == '*' || strchr(h, '.') || strchr(h, ':'))
	{
		/* Explicit user@host mask given */

		if(host)	/* Found user@host */
		{
			*host++ = '\0';

			/* check for @host */
			if(*h)
				user = h;
			else
				user = splat;

			/* check for user@ */
			if(!*host)
				host = splat;
		}
		else
		{
			user = splat;	/* no @ found, assume its *@somehost */
			host = h;
		}
	}
	else
	{
		sendto_one_notice(source_p, ":Invalid parameters");
		return 0;
	}

	/* possible remote kline.. */
	if((parc > 3) && (irccmp(parv[2], "ON") == 0))
	{
		if(!IsOperRemoteBan(source_p))
		{
			sendto_one(source_p, form_str(ERR_NOPRIVS),
				   me.name, source_p->name, "remoteban");
			return 0;
		}

		propagate_generic(source_p, "UNKLINE", parv[3], CAP_UNKLN, "%s %s", user, host);

		if(match(parv[3], me.name) == 0)
			return 0;

		propagated = 0;
	}

	aconf = find_exact_conf_by_address(host, CONF_KILL, user);

	/* No clustering for removing a propagated kline */
	if(propagated && (aconf == NULL || !aconf->lifetime) &&
			rb_dlink_list_length(&cluster_conf_list) > 0)
		cluster_generic(source_p, "UNKLINE", SHARED_UNKLINE, CAP_UNKLN,
				"%s %s", user, host);

	if(aconf == NULL)
	{
		sendto_one_notice(source_p, ":No K-Line for %s@%s", user, host);
		return 0;
	}
	
	if(aconf->lifetime)
	{
		if(propagated)
			remove_prop_kline(source_p, aconf);
		else
			sendto_one_notice(source_p, ":Cannot remove global K-Line %s@%s on specific servers", user, host);
		return 0;
	}

	if(remove_temp_kline(source_p, aconf))
		return 0;

	remove_permkline_match(source_p, aconf);

	return 0;
}

/* ms_unkline()
 *
 *   parv[1] - target server
 *   parv[2] - user to unkline
 *   parv[3] - host to unkline
 */
static int
ms_unkline(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	/* parv[0]  parv[1]        parv[2]  parv[3]
	 * oper     target server  user     host    */
	propagate_generic(source_p, "UNKLINE", parv[1], CAP_UNKLN, "%s %s", parv[2], parv[3]);

	if(!match(parv[1], me.name))
		return 0;

	if(!IsPerson(source_p))
		return 0;

	handle_remote_unkline(source_p, parv[2], parv[3]);
	return 0;
}

static int
me_unkline(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	/* user host */
	if(!IsPerson(source_p))
		return 0;

	handle_remote_unkline(source_p, parv[1], parv[2]);
	return 0;
}

static void
handle_remote_unkline(struct Client *source_p, const char *user, const char *host)
{
	struct ConfItem *aconf;

	if(!find_shared_conf(source_p->username, source_p->host,
			     source_p->servptr->name, SHARED_UNKLINE))
		return;

	aconf = find_exact_conf_by_address(host, CONF_KILL, user);
	if(aconf == NULL)
	{
		sendto_one_notice(source_p, ":No K-Line for %s@%s", user, host);
		return;
	}
	if(aconf->lifetime)
	{
		sendto_one_notice(source_p, ":Cannot remove global K-Line %s@%s on specific servers", user, host);
		return;
	}

	if(remove_temp_kline(source_p, aconf))
		return;

	remove_permkline_match(source_p, aconf);
}

/* apply_kline()
 *
 * inputs	- 
 * output	- NONE
 * side effects	- kline as given, is added to the hashtable
 *		  and conf file
 */
static void
apply_kline(struct Client *source_p, struct ConfItem *aconf,
	    const char *reason, const char *oper_reason)
{
	add_conf_by_address(aconf->host, CONF_KILL, aconf->user, NULL, aconf);
	bandb_add(BANDB_KLINE, source_p, aconf->user, aconf->host,
		  reason, EmptyString(oper_reason) ? NULL : oper_reason, 0);

	/* no oper reason.. */
	if(EmptyString(oper_reason))
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				       "%s added K-Line for [%s@%s] [%s]",
				       get_oper_name(source_p), aconf->user, aconf->host, reason);
		ilog(L_KLINE, "K %s 0 %s %s %s",
		     get_oper_name(source_p), aconf->user, aconf->host, reason);
	}
	else
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				       "%s added K-Line for [%s@%s] [%s|%s]",
				       get_oper_name(source_p), aconf->user, aconf->host,
				       reason, oper_reason);
		ilog(L_KLINE, "K %s 0 %s %s %s|%s",
		     get_oper_name(source_p), aconf->user, aconf->host, reason, oper_reason);
	}

	sendto_one_notice(source_p, ":Added K-Line [%s@%s]",
			  aconf->user, aconf->host);
}

/* apply_tkline()
 *
 * inputs	-
 * output	- NONE
 * side effects	- tkline as given is placed
 */
static void
apply_tkline(struct Client *source_p, struct ConfItem *aconf,
	     const char *reason, const char *oper_reason, int tkline_time)
{
	aconf->hold = rb_current_time() + tkline_time;
	add_temp_kline(aconf);

	/* no oper reason.. */
	if(EmptyString(oper_reason))
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				       "%s added temporary %d min. K-Line for [%s@%s] [%s]",
				       get_oper_name(source_p), tkline_time / 60,
				       aconf->user, aconf->host, reason);
		ilog(L_KLINE, "K %s %d %s %s %s",
		     get_oper_name(source_p), tkline_time / 60, aconf->user, aconf->host, reason);
	}
	else
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				       "%s added temporary %d min. K-Line for [%s@%s] [%s|%s]",
				       get_oper_name(source_p), tkline_time / 60,
				       aconf->user, aconf->host, reason, oper_reason);
		ilog(L_KLINE, "K %s %d %s %s %s|%s",
		     get_oper_name(source_p), tkline_time / 60,
		     aconf->user, aconf->host, reason, oper_reason);
	}

	sendto_one_notice(source_p, ":Added temporary %d min. K-Line [%s@%s]",
			  tkline_time / 60, aconf->user, aconf->host);
}

static void
apply_prop_kline(struct Client *source_p, struct ConfItem *aconf,
	     const char *reason, const char *oper_reason, int tkline_time)
{
	aconf->flags |= CONF_FLAGS_MYOPER | CONF_FLAGS_TEMPORARY;
	aconf->hold = rb_current_time() + tkline_time;
	aconf->lifetime = aconf->hold;

	replace_old_ban(aconf);

	rb_dlinkAddAlloc(aconf, &prop_bans);
	add_conf_by_address(aconf->host, CONF_KILL, aconf->user, NULL, aconf);

	/* no oper reason.. */
	if(EmptyString(oper_reason))
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				       "%s added global %d min. K-Line for [%s@%s] [%s]",
				       get_oper_name(source_p), tkline_time / 60,
				       aconf->user, aconf->host, reason);
		ilog(L_KLINE, "K %s %d %s %s %s",
		     get_oper_name(source_p), tkline_time / 60, aconf->user, aconf->host, reason);
	}
	else
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				       "%s added global %d min. K-Line for [%s@%s] [%s|%s]",
				       get_oper_name(source_p), tkline_time / 60,
				       aconf->user, aconf->host, reason, oper_reason);
		ilog(L_KLINE, "K %s %d %s %s %s|%s",
		     get_oper_name(source_p), tkline_time / 60,
		     aconf->user, aconf->host, reason, oper_reason);
	}

	sendto_one_notice(source_p, ":Added global %d min. K-Line [%s@%s]",
			  tkline_time / 60, aconf->user, aconf->host);

	sendto_server(NULL, NULL, CAP_BAN|CAP_TS6, NOCAPS,
			":%s BAN K %s %s %lu %d %d * :%s%s%s",
			source_p->id, aconf->user, aconf->host,
			(unsigned long)aconf->created,
			(int)(aconf->hold - aconf->created),
			(int)(aconf->lifetime - aconf->created),
			reason,
			oper_reason ? "|" : "",
			oper_reason ? oper_reason : "");
}

/* find_user_host()
 * 
 * inputs	- client placing kline, user@host, user buffer, host buffer
 * output	- 0 if not ok to kline, 1 to kline i.e. if valid user host
 * side effects -
 */
static int
find_user_host(struct Client *source_p, const char *userhost, char *luser, char *lhost)
{
	char *hostp;

	hostp = strchr(userhost, '@');

	if(hostp != NULL)	/* I'm a little user@host */
	{
		*(hostp++) = '\0';	/* short and squat */
		if(*userhost)
			rb_strlcpy(luser, userhost, USERLEN + 1);	/* here is my user */
		else
			strcpy(luser, "*");
		if(*hostp)
			rb_strlcpy(lhost, hostp, HOSTLEN + 1);	/* here is my host */
		else
			strcpy(lhost, "*");
	}
	else
	{
		/* no '@', no '.', so its not a user@host or host, therefore
		 * its a nick, which support was removed for.
		 */
		if(strchr(userhost, '.') == NULL && strchr(userhost, ':') == NULL)
		{
			sendto_one_notice(source_p, ":K-Line must be a user@host or host");
			return 0;
		}

		luser[0] = '*';	/* no @ found, assume its *@somehost */
		luser[1] = '\0';
		rb_strlcpy(lhost, userhost, HOSTLEN + 1);
	}

	return 1;
}

/* valid_user_host()
 *
 * inputs       - user buffer, host buffer
 * output	- 0 if invalid, 1 if valid
 * side effects -
 */
static int
valid_user_host(struct Client *source_p, const char *luser, const char *lhost)
{
	/* # is invalid, as are '!' (n!u@h kline) and '@' (u@@h kline) */
	if(strchr(lhost, '#') || strchr(luser, '#') || strchr(luser, '!') || strchr(lhost, '@'))
	{
		sendto_one_notice(source_p, ":Invalid K-Line");
		return 0;
	}

	return 1;
}

/*
 * valid_comment
 * inputs	- pointer to client
 *              - pointer to comment
 * output       - 0 if no valid comment, 1 if valid
 * side effects - NONE
 */
static int
valid_comment(struct Client *source_p, char *comment)
{
	if(strchr(comment, '"'))
	{
		sendto_one_notice(source_p, ":Invalid character '\"' in comment");
		return 0;
	}

	if(strlen(comment) > BANREASONLEN)
		comment[BANREASONLEN] = '\0';

	return 1;
}

/* already_placed_kline()
 *
 * inputs       - source to notify, user@host to check, tkline time
 * outputs      - 1 if a perm kline or a tkline when a tkline is being
 *                set exists, else 0
 * side effects - notifies source_p kline exists
 */
/* Note: This currently works if the new K-line is a special case of an
 *       existing K-line, but not the other way round. To do that we would
 *       have to walk the hash and check every existing K-line. -A1kmm.
 */
static int
already_placed_kline(struct Client *source_p, const char *luser, const char *lhost, int tkline)
{
	const char *reason, *p;
	struct rb_sockaddr_storage iphost, *piphost;
	struct ConfItem *aconf;
	int t, bits;

	aconf = find_exact_conf_by_address(lhost, CONF_KILL, luser);
	if(aconf == NULL && ConfigFileEntry.non_redundant_klines)
	{
		bits = 0;
		if((t = parse_netmask(lhost, (struct sockaddr *) &iphost, &bits)) != HM_HOST)
		{
#ifdef RB_IPV6
			if(t == HM_IPV6)
				t = AF_INET6;
			else
#endif
				t = AF_INET;

			piphost = &iphost;
		}
		else
			piphost = NULL;

		aconf = find_conf_by_address(lhost, NULL, NULL, (struct sockaddr *) piphost,
					     CONF_KILL, t, luser, NULL);
		if(aconf != NULL)
		{
			/* The above was really a lookup of a single IP,
			 * so check if the new kline is wider than the
			 * existing one.
			 * -- jilles
			 */
			p = strchr(aconf->host, '/');
			if(bits > 0 && (p == NULL || bits < atoi(p + 1)))
				aconf = NULL;
		}
	}
	if(aconf != NULL)
	{
		/* setting a tkline, or existing one is perm */
		if(tkline || ((aconf->flags & CONF_FLAGS_TEMPORARY) == 0))
		{
			reason = aconf->passwd ? aconf->passwd : "<No Reason>";

			sendto_one_notice(source_p,
					  ":[%s@%s] already K-Lined by [%s@%s] - %s",
					  luser, lhost, aconf->user, aconf->host, reason);
			return 1;
		}
	}

	return 0;
}

/* remove_permkline_match()
 *
 * hunts for a permanent kline, and removes it.
 */
static void
remove_permkline_match(struct Client *source_p, struct ConfItem *aconf)
{
	sendto_one_notice(source_p, ":K-Line for [%s@%s] is removed", aconf->user, aconf->host);

	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			       "%s has removed the K-Line for: [%s@%s]",
			       get_oper_name(source_p), aconf->user, aconf->host);

	ilog(L_KLINE, "UK %s %s %s", get_oper_name(source_p), aconf->user, aconf->host);

	remove_reject_mask(aconf->user, aconf->host);
	bandb_del(BANDB_KLINE, aconf->user, aconf->host);
	delete_one_address_conf(aconf->host, aconf);

	return;
}

/* remove_temp_kline()
 *
 * inputs       - username, hostname to unkline
 * outputs      -
 * side effects - tries to unkline anything that matches
 */
static int
remove_temp_kline(struct Client *source_p, struct ConfItem *aconf)
{
	rb_dlink_node *ptr;
	int i;

	for(i = 0; i < LAST_TEMP_TYPE; i++)
	{
		RB_DLINK_FOREACH(ptr, temp_klines[i].head)
		{
			if(aconf == ptr->data)
			{
				sendto_one_notice(source_p,
						  ":Un-klined [%s@%s] from temporary k-lines",
						  aconf->user, aconf->host);
				sendto_realops_snomask(SNO_GENERAL, L_ALL,
						       "%s has removed the temporary K-Line for: [%s@%s]",
						       get_oper_name(source_p), aconf->user,
						       aconf->host);

				ilog(L_KLINE, "UK %s %s %s",
				     get_oper_name(source_p), aconf->user, aconf->host);
				rb_dlinkDestroy(ptr, &temp_klines[i]);
				remove_reject_mask(aconf->user, aconf->host);
				delete_one_address_conf(aconf->host, aconf);
				return YES;
			}
		}
	}

	return NO;
}

static void
remove_prop_kline(struct Client *source_p, struct ConfItem *aconf)
{
	rb_dlink_node *ptr;

	ptr = rb_dlinkFind(aconf, &prop_bans);
	if (!ptr)
		return;
	sendto_one_notice(source_p,
			  ":Un-klined [%s@%s] from global k-lines",
			  aconf->user, aconf->host);
	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			       "%s has removed the global K-Line for: [%s@%s]",
			       get_oper_name(source_p), aconf->user,
			       aconf->host);

	ilog(L_KLINE, "UK %s %s %s",
	     get_oper_name(source_p), aconf->user, aconf->host);
	if(aconf->created < rb_current_time())
		aconf->created = rb_current_time();
	else
		aconf->created++;
	aconf->hold = aconf->created;
	operhash_delete(aconf->info.oper);
	aconf->info.oper = operhash_add(get_oper_name(source_p));
	aconf->flags |= CONF_FLAGS_MYOPER | CONF_FLAGS_TEMPORARY;
	sendto_server(NULL, NULL, CAP_BAN|CAP_TS6, NOCAPS,
			":%s BAN K %s %s %lu %d %d * :*",
			source_p->id, aconf->user, aconf->host,
			(unsigned long)aconf->created,
			0,
			(int)(aconf->lifetime - aconf->created));
	remove_reject_mask(aconf->user, aconf->host);
	deactivate_conf(aconf, ptr);
}
