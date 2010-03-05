/*
 *  ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 *  m_stats.c: Sends the user statistics or config information.
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
 *  $Id: m_stats.c 1608 2006-06-04 02:11:40Z jilles $
 */

#include "stdinc.h"
#include "class.h"		/* report_classes */
#include "client.h"		/* Client */
#include "common.h"		/* TRUE/FALSE */
#include "match.h"
#include "ircd.h"		/* me */
#include "listener.h"		/* show_ports */
#include "msg.h"		/* Message */
#include "hostmask.h"		/* report_mtrie_conf_links */
#include "numeric.h"		/* ERR_xxx */
#include "scache.h"		/* list_scache */
#include "send.h"		/* sendto_one */
#include "s_conf.h"		/* ConfItem */
#include "s_serv.h"		/* hunt_server */
#include "s_stats.h"
#include "s_user.h"		/* show_opers */
#include "blacklist.h"		/* dnsbl stuff */
#include "parse.h"
#include "modules.h"
#include "hook.h"
#include "s_newconf.h"
#include "hash.h"
#include "reject.h"
#include "whowas.h"

static int m_stats (struct Client *, struct Client *, int, const char **);

struct Message stats_msgtab = {
	"STATS", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_stats, 2}, {m_stats, 3}, mg_ignore, mg_ignore, {m_stats, 2}}
};

int doing_stats_hook;
int doing_stats_p_hook;

mapi_clist_av1 stats_clist[] = { &stats_msgtab, NULL };
mapi_hlist_av1 stats_hlist[] = {
	{ "doing_stats",	&doing_stats_hook },
	{ "doing_stats_p",	&doing_stats_p_hook },
	{ NULL, NULL }
};

DECLARE_MODULE_AV1(stats, NULL, NULL, stats_clist, stats_hlist, NULL, "$Revision: 1608 $");

const char *Lformat = "%s %u %u %u %u %u :%u %u %s";

static void stats_l_list(struct Client *s, const char *, int, int, rb_dlink_list *, char);
static void stats_l_client(struct Client *source_p, struct Client *target_p,
				char statchar);

static void stats_spy(struct Client *, char, const char *);
static void stats_p_spy(struct Client *);

/* Heres our struct for the stats table */
struct StatsStruct
{
	char letter;
	void (*handler) (struct Client *source_p);
	int need_oper;
	int need_admin;
};

static void stats_dns_servers(struct Client *);
static void stats_delay(struct Client *);
static void stats_hash(struct Client *);
static void stats_connect(struct Client *);
static void stats_tdeny(struct Client *);
static void stats_deny(struct Client *);
static void stats_exempt(struct Client *);
static void stats_events(struct Client *);
static void stats_prop_klines(struct Client *);
static void stats_hubleaf(struct Client *);
static void stats_auth(struct Client *);
static void stats_tklines(struct Client *);
static void stats_klines(struct Client *);
static void stats_messages(struct Client *);
static void stats_dnsbl(struct Client *);
static void stats_oper(struct Client *);
static void stats_privset(struct Client *);
static void stats_operedup(struct Client *);
static void stats_ports(struct Client *);
static void stats_tresv(struct Client *);
static void stats_resv(struct Client *);
static void stats_usage(struct Client *);
static void stats_tstats(struct Client *);
static void stats_uptime(struct Client *);
static void stats_shared(struct Client *);
static void stats_servers(struct Client *);
static void stats_tgecos(struct Client *);
static void stats_gecos(struct Client *);
static void stats_class(struct Client *);
static void stats_memory(struct Client *);
static void stats_servlinks(struct Client *);
static void stats_ltrace(struct Client *, int, const char **);
static void stats_ziplinks(struct Client *);
static void stats_comm(struct Client *);
/* This table contains the possible stats items, in order:
 * stats letter,  function to call, operonly? adminonly?
 * case only matters in the stats letter column.. -- fl_
 */
static struct StatsStruct stats_cmd_table[] = {
    /* letter     function        need_oper need_admin */
	{'a', stats_dns_servers,	1, 1, },
	{'A', stats_dns_servers,	1, 1, },
	{'b', stats_delay,		1, 1, },
	{'B', stats_hash,		1, 1, },
	{'c', stats_connect,		0, 0, },
	{'C', stats_connect,		0, 0, },
	{'d', stats_tdeny,		1, 0, },
	{'D', stats_deny,		1, 0, },
	{'e', stats_exempt,		1, 0, },
	{'E', stats_events,		1, 1, },
	{'f', stats_comm,		1, 1, },
	{'F', stats_comm,		1, 1, },
	{'g', stats_prop_klines,	1, 0, },
	{'h', stats_hubleaf,		0, 0, },
	{'H', stats_hubleaf,		0, 0, },
	{'i', stats_auth,		0, 0, },
	{'I', stats_auth,		0, 0, },
	{'k', stats_tklines,		0, 0, },
	{'K', stats_klines,		0, 0, },
	{'l', NULL /* special */,	0, 0, },
	{'L', NULL /* special */,	0, 0, },
	{'m', stats_messages,		0, 0, },
	{'M', stats_messages,		0, 0, },
	{'n', stats_dnsbl,		0, 0, },
	{'o', stats_oper,		0, 0, },
	{'O', stats_privset,		1, 0, },
	{'p', stats_operedup,		0, 0, },
	{'P', stats_ports,		0, 0, },
	{'q', stats_tresv,		1, 0, },
	{'Q', stats_resv,		1, 0, },
	{'r', stats_usage,		1, 0, },
	{'R', stats_usage,		1, 0, },
	{'t', stats_tstats,		1, 0, },
	{'T', stats_tstats,		1, 0, },
	{'u', stats_uptime,		0, 0, },
	{'U', stats_shared,		1, 0, },
	{'v', stats_servers,		0, 0, },
	{'V', stats_servers,		0, 0, },
	{'x', stats_tgecos,		1, 0, },
	{'X', stats_gecos,		1, 0, },
	{'y', stats_class,		0, 0, },
	{'Y', stats_class,		0, 0, },
	{'z', stats_memory,		1, 0, },
	{'Z', stats_ziplinks,		1, 0, },
	{'?', stats_servlinks,		0, 0, },
	{(char) 0, (void (*)()) 0, 	0, 0, }
};

/*
 * m_stats by fl_
 *      parv[1] = stat letter/command
 *      parv[2] = (if present) server/mask in stats L, or target
 *
 * This will search the tables for the appropriate stats letter,
 * if found execute it.  
 */
static int
m_stats(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static time_t last_used = 0;
	int i;
	char statchar;

	statchar = parv[1][0];

	if(MyClient(source_p) && !IsOper(source_p))
	{
		/* Check the user is actually allowed to do /stats, and isnt flooding */
		if((last_used + ConfigFileEntry.pace_wait) > rb_current_time())
		{
			/* safe enough to give this on a local connect only */
			sendto_one(source_p, form_str(RPL_LOAD2HI),
				   me.name, source_p->name, "STATS");
			sendto_one_numeric(source_p, RPL_ENDOFSTATS, 
					   form_str(RPL_ENDOFSTATS), statchar);
			return 0;
		}
		else
			last_used = rb_current_time();
	}

	if(hunt_server (client_p, source_p, ":%s STATS %s :%s", 2, parc, parv) != HUNTED_ISME)
		return 0;

	if((statchar != 'L') && (statchar != 'l'))
		stats_spy(source_p, statchar, NULL);

	for (i = 0; stats_cmd_table[i].letter; i++)
	{
		if(stats_cmd_table[i].letter == statchar)
		{
			/* The stats table says what privs are needed, so check --fl_ */
			/* Called for remote clients and for local opers, so check need_admin
			 * and need_oper
			 */
			if(stats_cmd_table[i].need_oper && !IsOper(source_p))
			{
				sendto_one_numeric(source_p, ERR_NOPRIVILEGES,
						   form_str (ERR_NOPRIVILEGES));
				break;
			}
			if(stats_cmd_table[i].need_admin && !IsOperAdmin(source_p))
			{
				sendto_one(source_p, form_str(ERR_NOPRIVS),
					   me.name, source_p->name, "admin");
				break;
			}

			/* Blah, stats L needs the parameters, none of the others do.. */
			if(statchar == 'L' || statchar == 'l')
				stats_ltrace (source_p, parc, parv);
			else
				stats_cmd_table[i].handler (source_p);
		}
	}

	/* Send the end of stats notice, and the stats_spy */
	sendto_one_numeric(source_p, RPL_ENDOFSTATS, 
			   form_str(RPL_ENDOFSTATS), statchar);

	return 0;
}

static void
stats_dns_servers (struct Client *source_p)
{
	report_dns_servers (source_p);
}

static void
stats_delay(struct Client *source_p)
{
	struct nd_entry *nd;
	struct DictionaryIter iter;

	DICTIONARY_FOREACH(nd, &iter, nd_dict)
	{
		sendto_one_notice(source_p, ":Delaying: %s for %ld",
				nd->name, (long) nd->expire);
	}
}

static void
stats_hash(struct Client *source_p)
{
	hash_stats(source_p);
}

static void
stats_connect(struct Client *source_p)
{
	static char buf[5];
	struct server_conf *server_p;
	char *s;
	rb_dlink_node *ptr;

	if((ConfigFileEntry.stats_c_oper_only || 
	    (ConfigServerHide.flatten_links && !IsExemptShide(source_p))) &&
	    !IsOper(source_p))
	{
		sendto_one_numeric(source_p, ERR_NOPRIVILEGES,
				   form_str(ERR_NOPRIVILEGES));
		return;
	}

	RB_DLINK_FOREACH(ptr, server_conf_list.head)
	{
		server_p = ptr->data;

		if(ServerConfIllegal(server_p))
			continue;

		buf[0] = '\0';
		s = buf;

		if(IsOper(source_p))
		{
			if(ServerConfAutoconn(server_p))
				*s++ = 'A';
			if(ServerConfSSL(server_p))
				*s++ = 'S';
			if(ServerConfTb(server_p))
				*s++ = 'T';
			if(ServerConfCompressed(server_p))
				*s++ = 'Z';
		}

		if(!buf[0])
			*s++ = '*';

		*s = '\0';

		sendto_one_numeric(source_p, RPL_STATSCLINE, 
				form_str(RPL_STATSCLINE),
				"*@127.0.0.1", 
				buf, server_p->name,
				server_p->port, server_p->class_name);
	}
}

/* stats_tdeny()
 *
 * input	- client to report to
 * output	- none
 * side effects - client is given temp dline list.
 */
static void
stats_tdeny (struct Client *source_p)
{
	char *host, *pass, *user, *oper_reason;
	struct AddressRec *arec;
	struct ConfItem *aconf;
	int i;

	for (i = 0; i < ATABLE_SIZE; i++)
	{
		for (arec = atable[i]; arec; arec = arec->next)
		{
			if(arec->type == CONF_DLINE)
			{
				aconf = arec->aconf;

				if(!(aconf->flags & CONF_FLAGS_TEMPORARY))
					continue;

				get_printable_kline(source_p, aconf, &host, &pass, &user, &oper_reason);

				sendto_one_numeric(source_p, RPL_STATSDLINE, 
						   form_str (RPL_STATSDLINE),
						   'd', host, pass,
						   oper_reason ? "|" : "",
						   oper_reason ? oper_reason : "");
			}
		}
	}
}

/* stats_deny()
 *
 * input	- client to report to
 * output	- none
 * side effects - client is given dline list.
 */
static void
stats_deny (struct Client *source_p)
{
	char *host, *pass, *user, *oper_reason;
	struct AddressRec *arec;
	struct ConfItem *aconf;
	int i;

	for (i = 0; i < ATABLE_SIZE; i++)
	{
		for (arec = atable[i]; arec; arec = arec->next)
		{
			if(arec->type == CONF_DLINE)
			{
				aconf = arec->aconf;

				if(aconf->flags & CONF_FLAGS_TEMPORARY)
					continue;

				get_printable_kline(source_p, aconf, &host, &pass, &user, &oper_reason);

				sendto_one_numeric(source_p, RPL_STATSDLINE, 
						   form_str (RPL_STATSDLINE),
						   'D', host, pass,
						   oper_reason ? "|" : "",
						   oper_reason ? oper_reason : "");
			}
		}
	}
}


/* stats_exempt()
 *
 * input	- client to report to
 * output	- none
 * side effects - client is given list of exempt blocks
 */
static void
stats_exempt(struct Client *source_p)
{
	char *name, *host, *pass, *user, *classname;
	struct AddressRec *arec;
	struct ConfItem *aconf;
	int i, port;

	if(ConfigFileEntry.stats_e_disabled)
	{
		sendto_one_numeric(source_p, ERR_DISABLED,
				   form_str(ERR_DISABLED), "STATS e");
		return;
	}

	for (i = 0; i < ATABLE_SIZE; i++)
	{
		for (arec = atable[i]; arec; arec = arec->next)
		{
			if(arec->type == CONF_EXEMPTDLINE)
			{
				aconf = arec->aconf;
				get_printable_conf (aconf, &name, &host, &pass,
						    &user, &port, &classname);

				sendto_one_numeric(source_p, RPL_STATSDLINE, 
						   form_str(RPL_STATSDLINE),
						   'e', host, pass, "", "");
			}
		}
	}}


static void
stats_events_cb(char *str, void *ptr)
{
	sendto_one_numeric(ptr, RPL_STATSDEBUG, "E :%s", str);
}

static void
stats_events (struct Client *source_p)
{
	rb_dump_events(stats_events_cb, source_p);
}

static void
stats_prop_klines(struct Client *source_p)
{
	struct ConfItem *aconf;
	rb_dlink_node *ptr;
	char *user, *host, *pass, *oper_reason;

	RB_DLINK_FOREACH(ptr, prop_bans.head)
	{
		aconf = ptr->data;

		/* Skip non-klines and deactivated klines. */
		if(aconf->status != CONF_KILL)
			continue;

		get_printable_kline(source_p, aconf, &host, &pass, 
				&user, &oper_reason);

		sendto_one_numeric(source_p, RPL_STATSKLINE,
				form_str(RPL_STATSKLINE),
				'g', host, user, pass,
				oper_reason ? "|" : "",
				oper_reason ? oper_reason : "");
	}
}

static void
stats_hubleaf(struct Client *source_p)
{
	struct remote_conf *hub_p;
	rb_dlink_node *ptr;

	if((ConfigFileEntry.stats_h_oper_only || 
	    (ConfigServerHide.flatten_links && !IsExemptShide(source_p))) &&
	    !IsOper(source_p))
	{
		sendto_one_numeric(source_p, ERR_NOPRIVILEGES,
				   form_str (ERR_NOPRIVILEGES));
		return;
	}

	RB_DLINK_FOREACH(ptr, hubleaf_conf_list.head)
	{
		hub_p = ptr->data;

		if(hub_p->flags & CONF_HUB)
			sendto_one_numeric(source_p, RPL_STATSHLINE,
					form_str(RPL_STATSHLINE),
					hub_p->host, hub_p->server);
		else
			sendto_one_numeric(source_p, RPL_STATSLLINE,
					form_str(RPL_STATSLLINE),
					hub_p->host, hub_p->server);
	}
}


static void
stats_auth (struct Client *source_p)
{
	/* Oper only, if unopered, return ERR_NOPRIVS */
	if((ConfigFileEntry.stats_i_oper_only == 2) && !IsOper (source_p))
		sendto_one_numeric(source_p, ERR_NOPRIVILEGES,
				   form_str (ERR_NOPRIVILEGES));

	/* If unopered, Only return matching auth blocks */
	else if((ConfigFileEntry.stats_i_oper_only == 1) && !IsOper (source_p))
	{
		struct ConfItem *aconf;
		char *name, *host, *pass = "*", *user, *classname;
		int port;

		if(MyConnect (source_p))
			aconf = find_conf_by_address (source_p->host, source_p->sockhost, NULL,
						      (struct sockaddr *)&source_p->localClient->ip,
						      CONF_CLIENT,
						      source_p->localClient->ip.ss_family,
						      source_p->username, NULL);
		else
			aconf = find_conf_by_address (source_p->host, NULL, NULL, NULL, CONF_CLIENT,
						      0, source_p->username, NULL);

		if(aconf == NULL)
			return;

		get_printable_conf (aconf, &name, &host, &pass, &user, &port, &classname);
		if(!EmptyString(aconf->spasswd))
			pass = aconf->spasswd;

		sendto_one_numeric(source_p, RPL_STATSILINE, form_str(RPL_STATSILINE),
				   name, pass, show_iline_prefix(source_p, aconf, user),
				   host, port, classname);
	}

	/* Theyre opered, or allowed to see all auth blocks */
	else
		report_auth (source_p);
}


static void
stats_tklines(struct Client *source_p)
{
	/* Oper only, if unopered, return ERR_NOPRIVS */
	if((ConfigFileEntry.stats_k_oper_only == 2) && !IsOper (source_p))
		sendto_one_numeric(source_p, ERR_NOPRIVILEGES,
				   form_str (ERR_NOPRIVILEGES));

	/* If unopered, Only return matching klines */
	else if((ConfigFileEntry.stats_k_oper_only == 1) && !IsOper (source_p))
	{
		struct ConfItem *aconf;
		char *host, *pass, *user, *oper_reason;

		if(MyConnect (source_p))
			aconf = find_conf_by_address (source_p->host, source_p->sockhost, NULL,
						      (struct sockaddr *)&source_p->localClient->ip,
						      CONF_KILL,
						      source_p->localClient->ip.ss_family,
						      source_p->username, NULL);
		else
			aconf = find_conf_by_address (source_p->host, NULL, NULL, NULL, CONF_KILL,
						      0, source_p->username, NULL);

		if(aconf == NULL)
			return;

		/* dont report a permanent kline as a tkline */
		if((aconf->flags & CONF_FLAGS_TEMPORARY) == 0)
			return;

		get_printable_kline(source_p, aconf, &host, &pass, &user, &oper_reason);

		sendto_one_numeric(source_p, RPL_STATSKLINE, 
				   form_str(RPL_STATSKLINE), aconf->flags & CONF_FLAGS_TEMPORARY ? 'k' : 'K',
				   host, user, pass, oper_reason ? "|" : "",
				   oper_reason ? oper_reason : "");
	}
	/* Theyre opered, or allowed to see all klines */
	else
	{
		struct ConfItem *aconf;
		rb_dlink_node *ptr;
		int i;
		char *user, *host, *pass, *oper_reason;

		for(i = 0; i < LAST_TEMP_TYPE; i++)
		{
			RB_DLINK_FOREACH(ptr, temp_klines[i].head)
			{
				aconf = ptr->data;

				get_printable_kline(source_p, aconf, &host, &pass, 
							&user, &oper_reason);

				sendto_one_numeric(source_p, RPL_STATSKLINE,
						   form_str(RPL_STATSKLINE),
						   'k', host, user, pass,
						   oper_reason ? "|" : "",
						   oper_reason ? oper_reason : "");
			}
		}
	}
}

static void
stats_klines(struct Client *source_p)
{
	/* Oper only, if unopered, return ERR_NOPRIVS */
	if((ConfigFileEntry.stats_k_oper_only == 2) && !IsOper (source_p))
		sendto_one_numeric(source_p, ERR_NOPRIVILEGES,
				   form_str (ERR_NOPRIVILEGES));

	/* If unopered, Only return matching klines */
	else if((ConfigFileEntry.stats_k_oper_only == 1) && !IsOper (source_p))
	{
		struct ConfItem *aconf;
		char *host, *pass, *user, *oper_reason;

		/* search for a kline */
		if(MyConnect (source_p))
			aconf = find_conf_by_address (source_p->host, source_p->sockhost, NULL,
						      (struct sockaddr *)&source_p->localClient->ip,
						      CONF_KILL,
						      source_p->localClient->ip.ss_family,
						      source_p->username, NULL);
		else
			aconf = find_conf_by_address (source_p->host, NULL, NULL, NULL, CONF_KILL,
						      0, source_p->username, NULL);

		if(aconf == NULL)
			return;

		get_printable_kline(source_p, aconf, &host, &pass, &user, &oper_reason);

		sendto_one_numeric(source_p, RPL_STATSKLINE, form_str(RPL_STATSKLINE),
				   aconf->flags & CONF_FLAGS_TEMPORARY ? 'k' : 'K',
				   host, user, pass, oper_reason ? "|" : "",
				   oper_reason ? oper_reason : "");
	}
	/* Theyre opered, or allowed to see all klines */
	else
		report_Klines (source_p);
}

static void
stats_messages(struct Client *source_p)
{
	report_messages(source_p);
}

static void
stats_dnsbl(struct Client *source_p)
{
	rb_dlink_node *ptr;
	struct Blacklist *blptr;

	RB_DLINK_FOREACH(ptr, blacklist_list.head)
	{
		blptr = ptr->data;

		/* use RPL_STATSDEBUG for now -- jilles */
		sendto_one_numeric(source_p, RPL_STATSDEBUG, "n :%d %s %s (%d)",
				blptr->hits,
				blptr->host,
				blptr->status & CONF_ILLEGAL ? "disabled" : "active",
				blptr->refcount);
	}
}

static void
stats_oper(struct Client *source_p)
{
	struct oper_conf *oper_p;
	rb_dlink_node *ptr;

	if(!IsOper(source_p) && ConfigFileEntry.stats_o_oper_only)
	{
		sendto_one_numeric(source_p, ERR_NOPRIVILEGES,
				   form_str (ERR_NOPRIVILEGES));
		return;
	}

	RB_DLINK_FOREACH(ptr, oper_conf_list.head)
	{
		oper_p = ptr->data;
		
		sendto_one_numeric(source_p, RPL_STATSOLINE, 
				form_str(RPL_STATSOLINE),
				oper_p->username, oper_p->host, oper_p->name,
				IsOper(source_p) ? oper_p->privset->name : "0", "-1");
	}
}

static void
stats_privset(struct Client *source_p)
{
	privilegeset_report(source_p);
}

/* stats_operedup()
 *
 * input	- client pointer
 * output	- none
 * side effects - client is shown a list of active opers
 */
static void
stats_operedup (struct Client *source_p)
{
	struct Client *target_p;
	rb_dlink_node *oper_ptr;
	unsigned int count = 0;

	RB_DLINK_FOREACH (oper_ptr, oper_list.head)
	{
		target_p = oper_ptr->data;

		if(IsOperInvis(target_p) && !IsOper(source_p))
			continue;

		if(target_p->user->away)
			continue;

		count++;

		sendto_one_numeric(source_p, RPL_STATSDEBUG,
				   "p :%s (%s@%s)",
				   target_p->name, target_p->username, 
				   target_p->host);
	}

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
				"p :%u staff members", count);

	stats_p_spy (source_p);
}

static void
stats_ports (struct Client *source_p)
{
	if(!IsOper (source_p) && ConfigFileEntry.stats_P_oper_only)
		sendto_one_numeric(source_p, ERR_NOPRIVILEGES,
				   form_str (ERR_NOPRIVILEGES));
	else
		show_ports (source_p);
}

static void
stats_tresv(struct Client *source_p)
{
	struct ConfItem *aconf;
	rb_dlink_node *ptr;
	int i;

	RB_DLINK_FOREACH(ptr, resv_conf_list.head)
	{
		aconf = ptr->data;
		if(aconf->hold)
			sendto_one_numeric(source_p, RPL_STATSQLINE, 
					form_str(RPL_STATSQLINE),
					'q', aconf->port, aconf->host, aconf->passwd);
	}

	HASH_WALK(i, R_MAX, ptr, resvTable)
	{
		aconf = ptr->data;
		if(aconf->hold)
			sendto_one_numeric(source_p, RPL_STATSQLINE, 
					form_str(RPL_STATSQLINE),
					'q', aconf->port, aconf->host, aconf->passwd);
	}
	HASH_WALK_END
}


static void
stats_resv(struct Client *source_p)
{
	struct ConfItem *aconf;
	rb_dlink_node *ptr;
	int i;

	RB_DLINK_FOREACH(ptr, resv_conf_list.head)
	{
		aconf = ptr->data;
		if(!aconf->hold)
			sendto_one_numeric(source_p, RPL_STATSQLINE, 
					form_str(RPL_STATSQLINE),
					'Q', aconf->port, aconf->host, aconf->passwd);
	}

	HASH_WALK(i, R_MAX, ptr, resvTable)
	{
		aconf = ptr->data;
		if(!aconf->hold)
			sendto_one_numeric(source_p, RPL_STATSQLINE, 
					form_str(RPL_STATSQLINE),
					'Q', aconf->port, aconf->host, aconf->passwd);
	}
	HASH_WALK_END
}

static void
stats_usage (struct Client *source_p)
{
	struct rusage rus;
	time_t secs;
	time_t rup;
#ifdef  hz
# define hzz hz
#else
# ifdef HZ
#  define hzz HZ
# else
	int hzz = 1;
# endif
#endif

	if(getrusage(RUSAGE_SELF, &rus) == -1)
	{
		sendto_one_notice(source_p, ":Getruseage error: %s.",
				  strerror(errno));
		return;
	}
	secs = rus.ru_utime.tv_sec + rus.ru_stime.tv_sec;
	if(0 == secs)
		secs = 1;

	rup = (rb_current_time() - startup_time) * hzz;
	if(0 == rup)
		rup = 1;
  
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "R :CPU Secs %d:%d User %d:%d System %d:%d",
			   (int) (secs / 60), (int) (secs % 60),
			   (int) (rus.ru_utime.tv_sec / 60),
			   (int) (rus.ru_utime.tv_sec % 60),
			   (int) (rus.ru_stime.tv_sec / 60), 
			   (int) (rus.ru_stime.tv_sec % 60));
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "R :RSS %ld ShMem %ld Data %ld Stack %ld",
			   rus.ru_maxrss, (rus.ru_ixrss / rup), 
			   (rus.ru_idrss / rup), (rus.ru_isrss / rup));
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "R :Swaps %d Reclaims %d Faults %d",
			   (int) rus.ru_nswap, (int) rus.ru_minflt, (int) rus.ru_majflt);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "R :Block in %d out %d",
			   (int) rus.ru_inblock, (int) rus.ru_oublock);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "R :Msg Rcv %d Send %d",
			   (int) rus.ru_msgrcv, (int) rus.ru_msgsnd);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "R :Signals %d Context Vol. %d Invol %d",
			   (int) rus.ru_nsignals, (int) rus.ru_nvcsw, 
			   (int) rus.ru_nivcsw);
}

static void
stats_tstats (struct Client *source_p)
{
	struct Client *target_p;
	struct ServerStatistics sp;
	rb_dlink_node *ptr;

	memcpy(&sp, &ServerStats, sizeof(struct ServerStatistics));

	RB_DLINK_FOREACH(ptr, serv_list.head)
	{
		target_p = ptr->data;

		sp.is_sbs += target_p->localClient->sendB;
		sp.is_sbr += target_p->localClient->receiveB;
		sp.is_sti += (unsigned long long)(rb_current_time() - target_p->localClient->firsttime);
		sp.is_sv++;
	}

	RB_DLINK_FOREACH(ptr, lclient_list.head)
	{
		target_p = ptr->data;

		sp.is_cbs += target_p->localClient->sendB;
		sp.is_cbr += target_p->localClient->receiveB;
		sp.is_cti += (unsigned long long)(rb_current_time() - target_p->localClient->firsttime);
		sp.is_cl++;
	}

	RB_DLINK_FOREACH(ptr, unknown_list.head)
	{
		sp.is_ni++;
	}

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :accepts %u refused %u", sp.is_ac, sp.is_ref);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			"T :rejected %u delaying %lu", 
			sp.is_rej, delay_exit_length());
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :throttled refused %u throttle list size %lu", sp.is_thr, throttle_size());
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			"T :nicks being delayed %lu",
			get_nd_count());
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :unknown commands %u prefixes %u",
			   sp.is_unco, sp.is_unpf);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :nick collisions %u saves %u unknown closes %u",
			   sp.is_kill, sp.is_save, sp.is_ni);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :wrong direction %u empty %u", 
			   sp.is_wrdi, sp.is_empt);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :numerics seen %u", sp.is_num);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :tgchange blocked msgs %u restricted addrs %lu",
			   sp.is_tgch, rb_dlink_list_length(&tgchange_list));
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :auth successes %u fails %u",
			   sp.is_asuc, sp.is_abad);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :sasl successes %u fails %u",
			   sp.is_ssuc, sp.is_sbad);
	sendto_one_numeric(source_p, RPL_STATSDEBUG, "T :Client Server");
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :connected %u %u", sp.is_cl, sp.is_sv);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
				"T :bytes sent %lluK %lluK",
				sp.is_cbs / 1024, 
				sp.is_sbs / 1024);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
				"T :bytes recv %lluK %lluK",
				sp.is_cbr / 1024, 
				sp.is_sbr / 1024);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
				"T :time connected %llu %llu",
				sp.is_cti, sp.is_sti);
}

static void
stats_uptime (struct Client *source_p)
{
	time_t now;

	now = rb_current_time() - startup_time;
	sendto_one_numeric(source_p, RPL_STATSUPTIME, 
			   form_str (RPL_STATSUPTIME),
			   now / 86400, (now / 3600) % 24, 
			   (now / 60) % 60, now % 60);
	sendto_one_numeric(source_p, RPL_STATSCONN,
			   form_str (RPL_STATSCONN),
			   MaxConnectionCount, MaxClientCount, 
			   Count.totalrestartcount);
}

struct shared_flags
{
	int flag;
	char letter;
};
static struct shared_flags shared_flagtable[] =
{
	{ SHARED_PKLINE,	'K' },
	{ SHARED_TKLINE,	'k' },
	{ SHARED_UNKLINE,	'U' },
	{ SHARED_PXLINE,	'X' },
	{ SHARED_TXLINE,	'x' },
	{ SHARED_UNXLINE,	'Y' },
	{ SHARED_PRESV,		'Q' },
	{ SHARED_TRESV,		'q' },
	{ SHARED_UNRESV,	'R' },
	{ SHARED_LOCOPS,	'L' },
	{ SHARED_REHASH,	'H' },
	{ SHARED_TDLINE,	'd' },
	{ SHARED_PDLINE,	'D' },
	{ SHARED_UNDLINE,	'E' },
	{ 0,			'\0'}
};


static void
stats_shared (struct Client *source_p)
{
	struct remote_conf *shared_p;
	rb_dlink_node *ptr;
	char buf[sizeof(shared_flagtable)/sizeof(shared_flagtable[0])];
	char *p;
	int i;

	RB_DLINK_FOREACH(ptr, shared_conf_list.head)
	{
		shared_p = ptr->data;

		p = buf;

		*p++ = 'c';

		for(i = 0; shared_flagtable[i].flag != 0; i++)
		{
			if(shared_p->flags & shared_flagtable[i].flag)
				*p++ = shared_flagtable[i].letter;
		}

		*p = '\0';

		sendto_one_numeric(source_p, RPL_STATSULINE, 
					form_str(RPL_STATSULINE),
					shared_p->server, shared_p->username,
					shared_p->host, buf);
	}

	RB_DLINK_FOREACH(ptr, cluster_conf_list.head)
	{
		shared_p = ptr->data;

		p = buf;

		*p++ = 'C';

		for(i = 0; shared_flagtable[i].flag != 0; i++)
		{
			if(shared_p->flags & shared_flagtable[i].flag)
				*p++ = shared_flagtable[i].letter;
		}

		*p = '\0';

		sendto_one_numeric(source_p, RPL_STATSULINE, 
					form_str(RPL_STATSULINE),
					shared_p->server, "*", "*", buf);
	}
}

/* stats_servers()
 *
 * input	- client pointer
 * output	- none
 * side effects - client is shown lists of who connected servers
 */
static void
stats_servers (struct Client *source_p)
{
	struct Client *target_p;
	rb_dlink_node *ptr;
	time_t seconds;
	int days, hours, minutes;
	int j = 0;

	if(ConfigServerHide.flatten_links && !IsOper(source_p) &&
	   !IsExemptShide(source_p))
	{
		sendto_one_numeric(source_p, ERR_NOPRIVILEGES,
				   form_str (ERR_NOPRIVILEGES));
		return;
	}

	RB_DLINK_FOREACH (ptr, serv_list.head)
	{
		target_p = ptr->data;

		j++;
		seconds = rb_current_time() - target_p->localClient->firsttime;

		days = (int) (seconds / 86400);
		seconds %= 86400;
		hours = (int) (seconds / 3600);
		seconds %= 3600;
		minutes = (int) (seconds / 60);
		seconds %= 60;

		sendto_one_numeric(source_p, RPL_STATSDEBUG,
				   "V :%s (%s!*@*) Idle: %d SendQ: %d "
				   "Connected: %d day%s, %d:%02d:%02d",
				   target_p->name,
				   (target_p->serv->by[0] ? target_p->serv->by : "Remote."),
				   (int) (rb_current_time() - target_p->localClient->lasttime),
				   (int) rb_linebuf_len (&target_p->localClient->buf_sendq),
				   days, (days == 1) ? "" : "s", hours, minutes, 
				   (int) seconds);
	}

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "V :%d Server(s)", j);
}

static void
stats_tgecos(struct Client *source_p)
{
	struct ConfItem *aconf;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, xline_conf_list.head)
	{
		aconf = ptr->data;

		if(aconf->hold)
			sendto_one_numeric(source_p, RPL_STATSXLINE,
					form_str(RPL_STATSXLINE),
					'x', aconf->port, aconf->host,
					aconf->passwd);
	}
}

static void
stats_gecos(struct Client *source_p)
{
	struct ConfItem *aconf;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, xline_conf_list.head)
	{
		aconf = ptr->data;

		if(!aconf->hold)
			sendto_one_numeric(source_p, RPL_STATSXLINE,
					form_str(RPL_STATSXLINE),
					'X', aconf->port, aconf->host, 
					aconf->passwd);
	}
}

static void
stats_class(struct Client *source_p)
{
	if(ConfigFileEntry.stats_y_oper_only && !IsOper(source_p))
		sendto_one_numeric(source_p, ERR_NOPRIVILEGES,
				   form_str (ERR_NOPRIVILEGES));
	else
		report_classes(source_p);
}

static void
stats_memory (struct Client *source_p)
{
	struct Client *target_p;
	struct Channel *chptr;
	struct Ban *actualBan;
	rb_dlink_node *rb_dlink;
	rb_dlink_node *ptr;
	int channel_count = 0;
	int local_client_conf_count = 0;	/* local client conf links */
	int users_counted = 0;	/* user structs */

	int channel_users = 0;
	int channel_invites = 0;
	int channel_bans = 0;
	int channel_except = 0;
	int channel_invex = 0;
	int channel_quiets = 0;

	int class_count = 0;	/* classes */
	int conf_count = 0;	/* conf lines */
	int users_invited_count = 0;	/* users invited */
	int user_channels = 0;	/* users in channels */
	int aways_counted = 0;
	size_t number_servers_cached;	/* number of servers cached by scache */

	size_t channel_memory = 0;
	size_t channel_ban_memory = 0;
	size_t channel_except_memory = 0;
	size_t channel_invex_memory = 0;
	size_t channel_quiet_memory = 0;

	size_t away_memory = 0;	/* memory used by aways */
	size_t ww = 0;		/* whowas array count */
	size_t wwm = 0;		/* whowas array memory used */
	size_t conf_memory = 0;	/* memory used by conf lines */
	size_t mem_servers_cached;	/* memory used by scache */

	size_t linebuf_count = 0;
	size_t linebuf_memory_used = 0;

	size_t total_channel_memory = 0;
	size_t totww = 0;

	size_t local_client_count = 0;
	size_t local_client_memory_used = 0;

	size_t remote_client_count = 0;
	size_t remote_client_memory_used = 0;

	size_t total_memory = 0;

	count_whowas_memory(&ww, &wwm);

	RB_DLINK_FOREACH(ptr, global_client_list.head)
	{
		target_p = ptr->data;
		if(MyConnect(target_p))
		{
			local_client_conf_count++;
		}

		if(target_p->user)
		{
			users_counted++;
			users_invited_count += rb_dlink_list_length(&target_p->user->invited);
			user_channels += rb_dlink_list_length(&target_p->user->channel);
			if(target_p->user->away)
			{
				aways_counted++;
				away_memory += (strlen(target_p->user->away) + 1);
			}
		}
	}

	/* Count up all channels, ban lists, except lists, Invex lists */
	RB_DLINK_FOREACH(ptr, global_channel_list.head)
	{
		chptr = ptr->data;
		channel_count++;
		channel_memory += (strlen(chptr->chname) + sizeof(struct Channel));

		channel_users += rb_dlink_list_length(&chptr->members);
		channel_invites += rb_dlink_list_length(&chptr->invites);

		RB_DLINK_FOREACH(rb_dlink, chptr->banlist.head)
		{
			actualBan = rb_dlink->data;
			channel_bans++;

			channel_ban_memory += sizeof(rb_dlink_node) + sizeof(struct Ban);
		}

		RB_DLINK_FOREACH(rb_dlink, chptr->exceptlist.head)
		{
			actualBan = rb_dlink->data;
			channel_except++;

			channel_except_memory += (sizeof(rb_dlink_node) + sizeof(struct Ban));
		}

		RB_DLINK_FOREACH(rb_dlink, chptr->invexlist.head)
		{
			actualBan = rb_dlink->data;
			channel_invex++;

			channel_invex_memory += (sizeof(rb_dlink_node) + sizeof(struct Ban));
		}

		RB_DLINK_FOREACH(rb_dlink, chptr->quietlist.head)
		{
			actualBan = rb_dlink->data;
			channel_quiets++;

			channel_quiet_memory += (sizeof(rb_dlink_node) + sizeof(struct Ban));
		}
	}

	/* count up all classes */

	class_count = rb_dlink_list_length(&class_list) + 1;

	rb_count_rb_linebuf_memory(&linebuf_count, &linebuf_memory_used);

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Users %u(%lu) Invites %u(%lu)",
			   users_counted,
			   (unsigned long) users_counted * sizeof(struct User),
			   users_invited_count, 
			   (unsigned long) users_invited_count * sizeof(rb_dlink_node));

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :User channels %u(%lu) Aways %u(%d)",
			   user_channels,
			   (unsigned long) user_channels * sizeof(rb_dlink_node),
			   aways_counted, (int) away_memory);

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Attached confs %u(%lu)",
			   local_client_conf_count,
			   (unsigned long) local_client_conf_count * sizeof(rb_dlink_node));

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Conflines %u(%d)", conf_count, (int) conf_memory);

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Classes %u(%lu)",
			   class_count, 
			   (unsigned long) class_count * sizeof(struct Class));

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Channels %u(%d)",
			   channel_count, (int) channel_memory);

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Bans %u(%d) Exceptions %u(%d) Invex %u(%d) Quiets %u(%d)",
			   channel_bans, (int) channel_ban_memory,
			   channel_except, (int) channel_except_memory,
			   channel_invex, (int) channel_invex_memory,
			   channel_quiets, (int) channel_quiet_memory);

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Channel members %u(%lu) invite %u(%lu)",
			   channel_users,
			   (unsigned long) channel_users * sizeof(rb_dlink_node),
			   channel_invites, 
			   (unsigned long) channel_invites * sizeof(rb_dlink_node));

	total_channel_memory = channel_memory +
		channel_ban_memory +
		channel_users * sizeof(rb_dlink_node) + channel_invites * sizeof(rb_dlink_node);

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Whowas array %ld(%ld)",
			   (long)ww, (long)wwm);

	totww = wwm;

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Hash: client %u(%ld) chan %u(%ld)",
			   U_MAX, (long)(U_MAX * sizeof(rb_dlink_list)), 
			   CH_MAX, (long)(CH_MAX * sizeof(rb_dlink_list)));

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :linebuf %ld(%ld)",
			   (long)linebuf_count, (long)linebuf_memory_used);

	count_scache(&number_servers_cached, &mem_servers_cached);

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :scache %ld(%ld)",
			   (long)number_servers_cached, (long)mem_servers_cached);

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :hostname hash %d(%ld)",
			   HOST_MAX, (long)HOST_MAX * sizeof(rb_dlink_list));

	total_memory = totww + total_channel_memory + conf_memory +
		class_count * sizeof(struct Class);

	total_memory += mem_servers_cached;
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Total: whowas %d channel %d conf %d", 
			   (int) totww, (int) total_channel_memory, 
			   (int) conf_memory);

	count_local_client_memory(&local_client_count, &local_client_memory_used);
	total_memory += local_client_memory_used;

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Local client Memory in use: %ld(%ld)",
			   (long)local_client_count, (long)local_client_memory_used);


	count_remote_client_memory(&remote_client_count, &remote_client_memory_used);
	total_memory += remote_client_memory_used;

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Remote client Memory in use: %ld(%ld)",
			   (long)remote_client_count,
			   (long)remote_client_memory_used);
}

static void
stats_ziplinks (struct Client *source_p)
{
	rb_dlink_node *ptr;
	struct Client *target_p;
	struct ZipStats *zipstats;
	int sent_data = 0;
	char buf[128], buf1[128];
	RB_DLINK_FOREACH (ptr, serv_list.head)
	{
		target_p = ptr->data;
		if(IsCapable (target_p, CAP_ZIP))
		{
			zipstats = target_p->localClient->zipstats;				
			sprintf(buf, "%.2f%%", zipstats->out_ratio);
			sprintf(buf1, "%.2f%%", zipstats->in_ratio);
			sendto_one_numeric(source_p, RPL_STATSDEBUG,
					    "Z :ZipLinks stats for %s send[%s compression "
					    "(%llu kB data/%llu kB wire)] recv[%s compression "
					    "(%llu kB data/%llu kB wire)]",
					    target_p->name,
					    buf, zipstats->out >> 10, 
					    zipstats->out_wire >> 10, buf1, 
					    zipstats->in >> 10, zipstats->in_wire >> 10);
			sent_data++;
		}
	}

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "Z :%u ziplink(s)", sent_data);
}

static void
stats_servlinks (struct Client *source_p)
{
	static char Sformat[] = ":%s %d %s %s %u %u %u %u %u :%u %u %s";
	long uptime, sendK, receiveK;
	struct Client *target_p;
	rb_dlink_node *ptr;
	int j = 0;
	char buf[128];

	if(ConfigServerHide.flatten_links && !IsOper (source_p) &&
	   !IsExemptShide(source_p))
	{
		sendto_one_numeric(source_p, ERR_NOPRIVILEGES,
				   form_str (ERR_NOPRIVILEGES));
		return;
	}

	sendK = receiveK = 0;

	RB_DLINK_FOREACH (ptr, serv_list.head)
	{
		target_p = ptr->data;

		j++;
		sendK += target_p->localClient->sendK;
		receiveK += target_p->localClient->receiveK;

		sendto_one(source_p, Sformat,
			get_id(&me, source_p), RPL_STATSLINKINFO, get_id(source_p, source_p),
			target_p->name,
			(int) rb_linebuf_len (&target_p->localClient->buf_sendq),
			(int) target_p->localClient->sendM,
			(int) target_p->localClient->sendK,
			(int) target_p->localClient->receiveM,
			(int) target_p->localClient->receiveK,
			rb_current_time() - target_p->localClient->firsttime,
			(rb_current_time() > target_p->localClient->lasttime) ? 
			 (rb_current_time() - target_p->localClient->lasttime) : 0,
			IsOper (source_p) ? show_capabilities (target_p) : "TS");
	}

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "? :%u total server(s)", j);

	snprintf(buf, sizeof buf, "%7.2f", _GMKv ((sendK)));
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "? :Sent total : %s %s",
			   buf, _GMKs (sendK));
	snprintf(buf, sizeof buf, "%7.2f", _GMKv ((receiveK)));
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "? :Recv total : %s %s",
			   buf, _GMKs (receiveK));

	uptime = (rb_current_time() - startup_time);
	snprintf(buf, sizeof buf, "%7.2f %s (%4.1f K/s)",
			   _GMKv (me.localClient->sendK), 
			   _GMKs (me.localClient->sendK),
			   (float) ((float) me.localClient->sendK / (float) uptime));
	sendto_one_numeric(source_p, RPL_STATSDEBUG, "? :Server send: %s", buf);
	snprintf(buf, sizeof buf, "%7.2f %s (%4.1f K/s)",
			   _GMKv (me.localClient->receiveK),
			   _GMKs (me.localClient->receiveK),
			   (float) ((float) me.localClient->receiveK / (float) uptime));
	sendto_one_numeric(source_p, RPL_STATSDEBUG, "? :Server recv: %s", buf);
}

static void
stats_ltrace(struct Client *source_p, int parc, const char *parv[])
{
	int doall = 0;
	int wilds = 0;
	const char *name;
	char statchar = parv[1][0];

	/* this is def targeted at us somehow.. */
	if(parc > 2 && !EmptyString(parv[2]))
	{
		/* directed at us generically? */
		if(match(parv[2], me.name) ||
		   (!MyClient(source_p) && !irccmp(parv[2], me.id)))
		{
			name = me.name;
			doall = 1;
		}
		else
		{
			name = parv[2];
			wilds = strchr(name, '*') || strchr(name, '?');
		}

		/* must be directed at a specific person thats not us */
		if(!doall && !wilds)
		{
			struct Client *target_p;

			if(MyClient(source_p))
				target_p = find_named_person(name);
			else
				target_p = find_person(name);

			if(target_p != NULL)
			{
				stats_spy(source_p, statchar, target_p->name);
				stats_l_client(source_p, target_p, statchar);
			}
			else
				sendto_one_numeric(source_p, ERR_NOSUCHSERVER,
						form_str(ERR_NOSUCHSERVER),
						name);

			return;
		}
	}
	else
	{
		name = me.name;
		doall = 1;
	}

	stats_spy(source_p, statchar, name);

	if(doall)
	{
		/* local opers get everyone */
		if(MyOper(source_p))
		{
			stats_l_list(source_p, name, doall, wilds, &unknown_list, statchar);
			stats_l_list(source_p, name, doall, wilds, &lclient_list, statchar);
		}
		else
		{
			/* they still need themselves if theyre local.. */
			if(MyClient(source_p))
				stats_l_client(source_p, source_p, statchar);

			stats_l_list(source_p, name, doall, wilds, &local_oper_list, statchar);
		}

		if (!ConfigServerHide.flatten_links || IsOper(source_p) ||
				IsExemptShide(source_p))
			stats_l_list(source_p, name, doall, wilds, &serv_list, statchar);

		return;
	}

	/* ok, at this point theyre looking for a specific client whos on
	 * our server.. but it contains a wildcard.  --fl
	 */
	stats_l_list(source_p, name, doall, wilds, &lclient_list, statchar);

	return;
}


static void
stats_l_list(struct Client *source_p, const char *name, int doall, int wilds,
	     rb_dlink_list * list, char statchar)
{
	rb_dlink_node *ptr;
	struct Client *target_p;

	/* send information about connections which match.  note, we
	 * dont need tests for IsInvisible(), because non-opers will
	 * never get here for normal clients --fl
	 */
	RB_DLINK_FOREACH(ptr, list->head)
	{
		target_p = ptr->data;

		if(!doall && wilds && !match(name, target_p->name))
			continue;

		stats_l_client(source_p, target_p, statchar);
	}
}

void
stats_l_client(struct Client *source_p, struct Client *target_p,
		char statchar)
{
	if(IsAnyServer(target_p))
	{
		sendto_one_numeric(source_p, RPL_STATSLINKINFO, Lformat,
				target_p->name,
				(int) rb_linebuf_len(&target_p->localClient->buf_sendq),
				(int) target_p->localClient->sendM,
				(int) target_p->localClient->sendK,
				(int) target_p->localClient->receiveM,
				(int) target_p->localClient->receiveK,
				rb_current_time() - target_p->localClient->firsttime,
				(rb_current_time() > target_p->localClient->lasttime) ? 
				 (rb_current_time() - target_p->localClient->lasttime) : 0,
				IsOper(source_p) ? show_capabilities(target_p) : "-");
	}

	else
	{
		sendto_one_numeric(source_p, RPL_STATSLINKINFO, Lformat,
				   show_ip(source_p, target_p) ?
				    (IsUpper(statchar) ?
				     get_client_name(target_p, SHOW_IP) :
				     get_client_name(target_p, HIDE_IP)) :
				    get_client_name(target_p, MASK_IP),
				    (int) rb_linebuf_len(&target_p->localClient->buf_sendq),
				    (int) target_p->localClient->sendM,
				    (int) target_p->localClient->sendK,
				    (int) target_p->localClient->receiveM,
				    (int) target_p->localClient->receiveK,
				    rb_current_time() - target_p->localClient->firsttime,
				    (rb_current_time() > target_p->localClient->lasttime) ? 
				     (rb_current_time() - target_p->localClient->lasttime) : 0,
				    "-");
	}
}

static void
rb_dump_fd_callback(int fd, const char *desc, void *data)
{
	struct Client *source_p = data;
	sendto_one_numeric(source_p, RPL_STATSDEBUG, "F :fd %-3d desc '%s'", fd, desc);
}

static void
stats_comm(struct Client *source_p)
{
	rb_dump_fd(rb_dump_fd_callback, source_p);
}

/*
 * stats_spy
 *
 * inputs	- pointer to client doing the /stats
 *		- char letter they are doing /stats on
 * output	- none
 * side effects -
 * This little helper function reports to opers if configured.
 * personally, I don't see why opers need to see stats requests
 * at all. They are just "noise" to an oper, and users can't do
 * any damage with stats requests now anyway. So, why show them?
 * -Dianora
 */
static void
stats_spy(struct Client *source_p, char statchar, const char *name)
{
	hook_data_int data;

	data.client = source_p;
	data.arg1 = name;
	data.arg2 = (int) statchar;

	call_hook(doing_stats_hook, &data);
}

/* stats_p_spy()
 *
 * input	- pointer to client doing stats
 * ouput	-
 * side effects - call hook doing_stats_p
 */
static void
stats_p_spy (struct Client *source_p)
{
	hook_data data;

	data.client = source_p;
	data.arg1 = data.arg2 = NULL;

	call_hook(doing_stats_p_hook, &data);
}

