/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_dline.c: Bans/unbans a user.
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
 */

#include <ircd/stdinc.h>
#include <ircd/channel.h>
#include <ircd/class.h>
#include <ircd/client.h>
#include <ircd/match.h>
#include <ircd/ircd.h>
#include <ircd/hostmask.h>
#include <ircd/numeric.h>
#include <ircd/s_conf.h>
#include <ircd/s_newconf.h>
#include <ircd/logger.h>
#include <ircd/send.h>
#include <ircd/hash.h>
#include <ircd/s_serv.h>
#include <ircd/msg.h>
#include <ircd/parse.h>
#include <ircd/modules.h>
#include <ircd/bandbi.h>
#include <ircd/operhash.h>

using namespace ircd;

static const char dline_desc[] = "Provides the DLINE facility to ban users via IP address";

static void mo_dline(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void me_dline(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void mo_undline(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void me_undline(struct MsgBuf *, struct Client *, struct Client *, int, const char **);

struct Message dline_msgtab = {
	"DLINE", 0, 0, 0, 0,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {me_dline, 4}, {mo_dline, 2}}
};

struct Message undline_msgtab = {
	"UNDLINE", 0, 0, 0, 0,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {me_undline, 2}, {mo_undline, 2}}
};

mapi_clist_av1 dline_clist[] = { &dline_msgtab, &undline_msgtab, NULL };

DECLARE_MODULE_AV2(dline, NULL, NULL, dline_clist, NULL, NULL, NULL, NULL, dline_desc);

static bool remove_temp_dline(struct ConfItem *);
static void apply_dline(struct Client *, const char *, int, char *);
static void apply_undline(struct Client *, const char *);

/* mo_dline()
 *
 *   parv[1] - dline to add
 *   parv[2] - reason
 */
static void
mo_dline(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	char def[] = "No Reason";
	const char *dlhost;
	char *reason = def;
	char cidr_form_host[HOSTLEN + 1];
	int tdline_time = 0;
	const char *target_server = NULL;
	int loc = 1;

	if(!IsOperK(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "kline");
		return;
	}

	if((tdline_time = valid_temp_time(parv[loc])) >= 0)
		loc++;

	dlhost = parv[loc];
	rb_strlcpy(cidr_form_host, dlhost, sizeof(cidr_form_host));
	loc++;

	/* would break the protocol */
	if (*dlhost == ':')
	{
		sendto_one_notice(source_p, ":Invalid D-Line");
		return;
	}

	if(parc >= loc + 2 && !irccmp(parv[loc], "ON"))
	{
		if(!IsOperRemoteBan(source_p))
		{
			sendto_one(source_p, form_str(ERR_NOPRIVS),
				   me.name, source_p->name, "remoteban");
			return;
		}

		target_server = parv[loc + 1];
		loc += 2;
	}

	if(parc >= loc + 1 && !EmptyString(parv[loc]))
		reason = LOCAL_COPY(parv[loc]);

	if(target_server != NULL)
	{
		sendto_match_servs(source_p, target_server,
				   CAP_ENCAP, NOCAPS,
				   "ENCAP %s DLINE %d %s :%s",
				   target_server, tdline_time, dlhost, reason);

		if(!match(target_server, me.name))
			return;
	}

	apply_dline(source_p, dlhost, tdline_time, reason);

	check_dlines();
}

/* mo_undline()
 *
 *      parv[1] = dline to remove
 */
static void
mo_undline(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	const char *cidr;
	const char *target_server = NULL;

	if(!IsOperK(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "unkline");
		return;
	}

	cidr = parv[1];

	if(parc >= 4 && !irccmp(parv[2], "ON"))
	{
		if(!IsOperRemoteBan(source_p))
		{
			sendto_one(source_p, form_str(ERR_NOPRIVS),
				   me.name, source_p->name, "remoteban");
			return;
		}

		target_server = parv[3];
		sendto_match_servs(source_p, target_server,
				   CAP_ENCAP, NOCAPS, "ENCAP %s UNDLINE %s", target_server, cidr);

		if(!match(target_server, me.name))
			return;
	}

	apply_undline(source_p, cidr);
}

static void
me_dline(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	int tdline_time = atoi(parv[1]);
	/* Since this is coming over a server link, assume that the originating
	 * server did the relevant permission/sanity checks...
	 */

	if(!IsPerson(source_p))
		return;

	if(!find_shared_conf(source_p->username, source_p->host,
			     source_p->servptr->name,
			     tdline_time > 0 ? SHARED_TDLINE : SHARED_PDLINE))
		return;

	apply_dline(source_p, parv[2], tdline_time, LOCAL_COPY(parv[3]));

	check_dlines();
}

static void
me_undline(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	if(!IsPerson(source_p))
		return;

	if(!find_shared_conf(source_p->username, source_p->host,
			     source_p->servptr->name, SHARED_UNDLINE))
		return;

	apply_undline(source_p, parv[1]);
}

static void
apply_dline(struct Client *source_p, const char *dlhost, int tdline_time, char *reason)
{
	struct ConfItem *aconf;
	char *oper_reason;
	struct rb_sockaddr_storage daddr;
	int t = AF_INET, ty, b;
	const char *creason;

	ty = parse_netmask(dlhost, &daddr, &b);
	if(ty == HM_HOST)
	{
		sendto_one(source_p, ":%s NOTICE %s :Invalid D-Line", me.name, source_p->name);
		return;
	}
#ifdef RB_IPV6
	if(ty == HM_IPV6)
		t = AF_INET6;
	else
#endif
		t = AF_INET;

	/* This means dlines wider than /16 cannot be set remotely */
	if(IsOperAdmin(source_p))
	{
		if(b < 8)
		{
			sendto_one_notice(source_p,
					  ":For safety, bitmasks less than 8 require conf access.");
			return;
		}
	}
	else
	{
		if(b < 16)
		{
			sendto_one_notice(source_p,
					  ":Dline bitmasks less than 16 are for admins only.");
			return;
		}
	}

	if(ConfigFileEntry.non_redundant_klines)
	{
		if((aconf = find_dline((struct sockaddr *) &daddr, t)) != NULL)
		{
			int bx;
			parse_netmask(aconf->host, NULL, &bx);
			if(b >= bx)
			{
				creason = aconf->passwd ? aconf->passwd : "<No Reason>";
				if(IsConfExemptKline(aconf))
					sendto_one(source_p,
						   ":%s NOTICE %s :[%s] is (E)d-lined by [%s] - %s",
						   me.name, source_p->name, dlhost, aconf->host,
						   creason);
				else
					sendto_one(source_p,
						   ":%s NOTICE %s :[%s] already D-lined by [%s] - %s",
						   me.name, source_p->name, dlhost, aconf->host,
						   creason);
				return;
			}
		}
	}

	rb_set_time();

	aconf = make_conf();
	aconf->status = CONF_DLINE;
	aconf->created = rb_current_time();
	aconf->host = rb_strdup(dlhost);
	aconf->passwd = rb_strdup(reason);
	aconf->info.oper = operhash_add(get_oper_name(source_p));

	if(strlen(reason) > BANREASONLEN)
		reason[BANREASONLEN] = '\0';

	/* Look for an oper reason */
	if((oper_reason = strchr(reason, '|')) != NULL)
	{
		*oper_reason = '\0';
		oper_reason++;

		if(!EmptyString(oper_reason))
			aconf->spasswd = rb_strdup(oper_reason);
	}

	if(tdline_time > 0)
	{
		aconf->hold = rb_current_time() + tdline_time;
		add_temp_dline(aconf);

		if(EmptyString(oper_reason))
		{
			sendto_realops_snomask(SNO_GENERAL, L_ALL,
					       "%s added temporary %d min. D-Line for [%s] [%s]",
					       get_oper_name(source_p), tdline_time / 60,
					       aconf->host, reason);
			ilog(L_KLINE, "D %s %d %s %s",
			     get_oper_name(source_p), tdline_time / 60, aconf->host, reason);
		}
		else
		{
			sendto_realops_snomask(SNO_GENERAL, L_ALL,
					       "%s added temporary %d min. D-Line for [%s] [%s|%s]",
					       get_oper_name(source_p), tdline_time / 60,
					       aconf->host, reason, oper_reason);
			ilog(L_KLINE, "D %s %d %s %s|%s",
			     get_oper_name(source_p), tdline_time / 60,
			     aconf->host, reason, oper_reason);
		}

		sendto_one(source_p, ":%s NOTICE %s :Added temporary %d min. D-Line for [%s]",
			   me.name, source_p->name, tdline_time / 60, aconf->host);
	}
	else
	{
		add_conf_by_address(aconf->host, CONF_DLINE, NULL, NULL, aconf);

		bandb_add(BANDB_DLINE, source_p, aconf->host, NULL,
			  reason, EmptyString(aconf->spasswd) ? NULL : aconf->spasswd, 0);

		if(EmptyString(oper_reason))
		{
			sendto_realops_snomask(SNO_GENERAL, L_ALL,
					       "%s added D-Line for [%s] [%s]",
					       get_oper_name(source_p), aconf->host, reason);
			ilog(L_KLINE, "D %s 0 %s %s",
			     get_oper_name(source_p), aconf->host, reason);
		}
		else
		{
			sendto_realops_snomask(SNO_GENERAL, L_ALL,
					       "%s added D-Line for [%s] [%s|%s]",
					       get_oper_name(source_p), aconf->host, reason, oper_reason);
			ilog(L_KLINE, "D %s 0 %s %s|%s",
			     get_oper_name(source_p),
			     aconf->host, reason, oper_reason);
		}
	}
}

static void
apply_undline(struct Client *source_p, const char *cidr)
{
	char buf[BUFSIZE];
	struct ConfItem *aconf;

	if(parse_netmask(cidr, NULL, NULL) == HM_HOST)
	{
		sendto_one_notice(source_p, ":Invalid D-Line");
		return;
	}

	aconf = find_exact_conf_by_address(cidr, CONF_DLINE, NULL);
	if(aconf == NULL)
	{
		sendto_one_notice(source_p, ":No D-Line for %s", cidr);
		return;
	}

	rb_strlcpy(buf, aconf->host, sizeof buf);
	if(remove_temp_dline(aconf))
	{
		sendto_one(source_p,
			   ":%s NOTICE %s :Un-dlined [%s] from temporary D-lines",
			   me.name, source_p->name, buf);
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				       "%s has removed the temporary D-Line for: [%s]",
				       get_oper_name(source_p), buf);
		ilog(L_KLINE, "UD %s %s", get_oper_name(source_p), buf);
		return;
	}

	bandb_del(BANDB_DLINE, aconf->host, NULL);

	sendto_one(source_p, ":%s NOTICE %s :D-Line for [%s] is removed", me.name, source_p->name,
		   aconf->host);
	sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s has removed the D-Line for: [%s]",
			       get_oper_name(source_p), aconf->host);
	ilog(L_KLINE, "UD %s %s", get_oper_name(source_p), aconf->host);
	delete_one_address_conf(aconf->host, aconf);
}

/* remove_temp_dline()
 *
 * inputs       - confitem to undline
 * outputs      - true if removed, false otherwise.
 * side effects - tries to undline anything that matches
 */
static bool
remove_temp_dline(struct ConfItem *aconf)
{
	rb_dlink_node *ptr;
	int i;

	for(i = 0; i < LAST_TEMP_TYPE; i++)
	{
		RB_DLINK_FOREACH(ptr, temp_dlines[i].head)
		{
			if(aconf == ptr->data)
			{
				rb_dlinkDestroy(ptr, &temp_dlines[i]);
				delete_one_address_conf(aconf->host, aconf);
				return true;
			}
		}
	}

	return false;
}
